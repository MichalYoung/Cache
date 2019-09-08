//
// Simple assembler for Cache byte code
// (work in progress August/September 2019)
//
//  Organized as a very simple recursive descent
//  parser because of if/else/fi and whle/do/od
//  forms, but the grammar is essentially LL(0)
//  rather than LL(1) (there are no precedence decisions).
//

#include "assembler.h"

#include "machineContext.h"
#include "code.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#define DBG 0
static void dbmsg(char *msg, ...) {
    if (DBG) {
        fprintf(stderr, "==>");
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        va_end(args);
        fprintf(stderr, "<==\n");
    }
}


/* --------------------------------------------------------------
 * Lexical analysis.
 *   Usage:
        struct Asm_Parse_State state;
        struct Token token;
        int got_token;
        lex_init(&state, argv[1]);
        while ((got_token = lex_next(&state, &token))) {
            switch (token.kind) {
              // each case does something with token.val.(int_val|float_val|string_val)
            }
        } // EOF token comes with 0 return value from lex_next
 *
 * Hand-written scanner with conventional
 * 'pull' interface through function 'lex_next' to return the
 * next token.  Hand-written because we're reading the program
 * from a memory buffer, which isn't a good fit for 'flex' and
 * similar scanner generators.
 * --------------------------------------------------------------
 */

/* Limit on literals, identifiers, other token text */ 
#define MAX_STR_LEN 500

/* A 'kind' is like a family of related tokens, e.g., k_Ident is all the
 * usual stack-oriented operations like addition and subtraction.
 * They are grouped as I anticipate them to indicate different behaviors
 * in parsing and code generation.   The k_Ident tokens, which are most
 * numerous, are further described in the OPS table which maps them
 * to a function and the number of immediate operands each takes.
 * The rest mostly require special case handling.
 */
enum Token_Kind {
        k_EOF = 0,      // which is the only 'falsy' token kind
        k_Int, k_Float, // Literal constants
        k_Directive,    // Subscriptions, variable declarations, etc
        k_Ident,        // Identifiers and operation names like add, sub, varpush
        k_Control_Push, // if, while
        k_Control_Pop, //  then, do, else, fi, od
        k_Stop,        //  Pseudo-instruction marks block ends
        k_ERROR,        //  This is not a valid token
        k_SENTINEL      // Use this to mark end of sets represented as arrays
};


char *token_names[] = { "EOF", "int", "float", "directive", "ident",
                        "control (enter)",
                        "control(continue or leave)", "STOP",
                        "Invalid Lexeme"};

static char* token_name(enum Token_Kind kind) {
        assert(kind >= 0 && kind < k_SENTINEL);
        return token_names[kind];
}


/* --------------------------------------------------------------
 * Tables of operations for classifying opcodes and creating
 * bytecode.
 */

struct OpCode {
    char *name;
    void (*f)(MachineContext *mc);
    int immediate_operands;    /* e.g., varpush takes 1 arg from instruction stream */
};

/* operations are from code.h;  to distinguish those with 
 * immediate operands we look for incrementing pc in code.c.
 * The control operations 'ifcode' and 'whilecode' are omitted
 * here because they involve recursive block structure.
 */
static struct OpCode OPS[] = {
        {"add",     add,     0},
        {"constpush",constpush, 1},  // 1 operand, float or integer
        {"varpush",  varpush, 1},    // 1 operand, variable -> index
        {"add",  add, 0},
        {"subtract",  subtract, 0},
        {"multiply",  multiply, 0},
        {"divide",  divide, 0},
        {"modulo",  modulo, 0},
        {"negate",  negate, 0},
        {"eval",  eval, 0},
        {"extract",  extract, 0},
        {"gt",  gt, 0},
        {"ge",  ge, 0},
        {"lt",  lt, 0},
        {"le",  le, 0},
        {"eq",  eq, 0},
        {"ne",  ne, 0},
        {"and",  and, 0},
        {"or",  or, 0},
        {"not",  not, 0},
        {"function",  function, 2},  // name, narg
        {"proc", procedure, 2}, // name, narg
        {"print",  print, 0},
        {"assign",  assign, 0},
        {"pluseq",  pluseq, 0},
        {"minuseq",  minuseq, 0},
        {"newmap",  newmap, 1},
        {"newwindow",  newwindow, 3},
        {"destroy",  destroy, 1},
        {"bitOr",  bitOr, 0},
        {"bitAnd",  bitAnd, 0},
        {"SENTINEL VALUE", 0, 0}
};

static const int N_OPS = sizeof(OPS) / sizeof(struct OpCode);


/*  Look up mnemonic to find the corresponding function pointer
 *  and number of immediate DATA operands to expect.  NULL means
 *  not found here (but it could be a control operation, in a
 *  different table).
 */


struct OpCode* find_op(char *mnemonic) {
    int i=0;
    dbmsg("Initializing opcode search for %s", mnemonic);
    // dbmsg("First mnemonic is %s", OPS[0].name);
    for (i=0; OPS[i].f; ++i) {
        struct OpCode* entry = &OPS[i];
        if (strcmp(entry->name, mnemonic) == 0) {
            return entry;
        }
    }
    return 0;
}

void smoketest_opcode_lookup() {
    dbmsg("In smoketest_opcode_lookup");
    struct OpCode *op = 0;
    dbmsg("Looking up constpush");
    op = find_op("constpush");
    assert(op->f == constpush);
    dbmsg("Looking up bitAnd");
    op = find_op("bitAnd");
    assert(op->f == bitAnd);
    dbmsg("Looking up no_such");
    assert(find_op("no_such") == 0);
    dbmsg("Returning from smoketest_opcode_lookup");
}


/*
 * Control structures if/then/else/fi and while/do/od have custom grammar,
 * so less table-driven than the individual ops, but we distinguish between
 * those that start a nested block (.while and .if) and those that close
 * out a block (whether or not they also start the next block, e.g., .else).
 *
 * The ends of blocks are marked by STOP instructions.  Rather than try to
 * reconstruct this block structuring from conventional labels in
 * an assembly language, we embrace it with "do" and "od", "else"
 * and "fi" as pseudo-ops that mark the end of one block and the
 * beginning of the next.  (See design papers on WASM for a similar
 * approach to representing block-structured control flow in a low
 * level language.)
 *
 * The immediate operands are offsets
 * relative to the program counter of the control operation, and
 * are represented in PNTR instructions rather than DATA instructions.
 */

struct ControlOp {
    char *name;
    enum Token_Kind kind;
} CONTROL[] = {
        {".while", k_Control_Push}
        ,{".do", k_Control_Pop}
        ,{".od", k_Control_Pop}
        ,{".if", k_Control_Push}
        ,{".then", k_Control_Pop}
        ,{".else", k_Control_Pop}
        ,{".fi", k_Control_Pop}
};

static const int N_CONTROLS = sizeof( CONTROL ) / sizeof(struct ControlOp);

enum Token_Kind find_control(char *mnemonic) {
    int i=0;
    dbmsg("Initializing control keyword search for %s", mnemonic);
    for (i=0; i < N_CONTROLS; ++i) {
        if (strcmp(CONTROL[i].name, mnemonic) == 0) {
            return CONTROL[i].kind;
        }
    }
    return k_ERROR;
}

/* In case we need a token outside the Asm_Parse_State */
struct Token {
    enum Token_Kind kind;
    char* string_val;
};

/*
 * Lexical and parsing state together in one struct for simplicity
 * as our grammar is LL(0) rather than LL(1).
 */
struct Asm_Parse_State {
    int n_errors;
    int n_warnings;
    /* Scanner state */
    char* buf_ptr;
    char token_buf[MAX_STR_LEN + 1]; 
    struct Token token;   /* Next token to be parsed */
};

/* We can get a copy of the token, along with responsibility
 * for disposal.
 * Usage:
 *     struct Token token;
 *     dup_token(state, &token);
 *     ...
 *     dispose_token(&token);
 */
void dup_token(struct Asm_Parse_State *state, struct Token *token) {
    token->kind = state->token.kind;
    token->string_val = strdup(state->token.string_val);
}

void dispose_token(struct Token *token) {
    free(token->string_val);
}


/* Input error handling.  During development I can just print to
 * stderr, but a deployed version needs a tactic for transmitting
 * messages back to the client, or making them available to the
 * client (perhaps through re-parsing on the client side).
 * For now, simple.
 */
static void syntax_error(struct Asm_Parse_State *state, const char* msg, ...) {
    fprintf(stderr, "\n*** Syntax error in Cache assembly *** \n*** ");
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr,"\n");
    ++state->n_errors;
    if (state->n_errors > 5) {
        fprintf(stderr, "Too many errors!!\n");
        /* FIXME: We need a setjmp/longjmp pair to bail semi-gracefully
         * when we have seen too many errors.  They need to be here,
         * not global to the module, so that concurrent parsers don't
         * clobber each other.
         */
    }
}


static int lex_next(struct Asm_Parse_State *state);

/* Initialize a lexer to scan a buffer of characters */
static void lex_init(struct Asm_Parse_State *state, char *buf) {
    state->buf_ptr = buf;
    /* And the rest of the parser state */
    state->n_errors = 0;
    state->n_warnings = 0;
    /* Prime the pump */
    lex_next(state);
}


static void skip_whitespace(struct Asm_Parse_State *state) {
    char ch;
    while (1) {
        while ((ch = *(state->buf_ptr))) {
            if (isspace(ch)) { state->buf_ptr++; }
            else { break; }
        }
        if (*(state->buf_ptr) == ';') {
            /* Skip comment to end of string */
            while ((ch = *(state->buf_ptr))) {
                if (ch != '\n') { state->buf_ptr++; }
                else { break; }
            }
        } else break;
    }
}




/* Match a string of alphanumeric characters, except
 * the first may not be alphanumeric.  Copies matched
 * string into token structure. 1 = success,
 * 0 = something went wrong.  Result in state->token_buf
 */
static int scan_alstar(struct Asm_Parse_State *state) {
    int v_i = 0;
    state->token_buf[v_i++] = *(state->buf_ptr++);             // Initial char need not be alnum
    while (isalnum(*(state->buf_ptr)) && v_i < MAX_STR_LEN) {  // but we gobble as far as they are
        state->token_buf[v_i++] = *(state->buf_ptr++);
    }
    state->token_buf[v_i] = '\0';
    return 1;
}

/* Gobble a sequence that starts with an unexpected character */
static int scan_bogus(struct Asm_Parse_State *state) {
    int v_i = 0;
    state->token_buf[v_i++] = *(state->buf_ptr++);        // Initial char need not be graphic
    while (isgraph(*(state->buf_ptr)) && v_i < MAX_STR_LEN) {  // but we gobble as far as they are
        state->token_buf[v_i++] = *(state->buf_ptr++);
    }
    state->token_buf[v_i] = '\0';
    return 1;
}

/* Next token: Advance the scanner, leaving
 * the new token in the parse state.
 * Returns 1 for success, 0 for EOF or failure.
 *
 * Don't use directly from parser:  Use lex_peek or lex_get instead.
 */
static int lex_read_next(struct Asm_Parse_State *state) {
    char initial;
    skip_whitespace(state);
    // dbmsg("Lexing from %s", state->buf_ptr);
    if (*(state->buf_ptr) == '\0') {
        state->token.kind = k_EOF;
        state->token.string_val = "EOF";
        return 0;
    }
    initial = *(state->buf_ptr);
    //dbmsg("Initial character '%c'", initial);

    if (initial == '.') {
        // .if, .else, .fi, .while, .do, .od
        scan_alstar(state);
        state->token.string_val = state->token_buf;
        state->token.kind = (find_control(state->token.string_val));
        return 1;
    }
    if (isalpha(initial)) {
        scan_alstar(state);
        state->token.kind = k_Ident;
        state->token.string_val = state->token_buf;
        // 'stop' is a pseudo-instruction, significant to the syntax
        if (strcmp(state->token.string_val, "stop") == 0) {
            state->token.kind = k_Stop;
        }
        return 1;
    }
    if (initial == ':') {
        scan_alstar(state);
        state->token.kind = k_Directive;
        state->token.string_val = state->token_buf;
        return 1;
    }
    if (isdigit(initial) || initial == '-') {
        //dbmsg("Recognized as numeric with initial character '%c'", initial);
        int v_i = 0;
        /* Might be integer or floating point */
        state->token_buf[v_i++] = *(state->buf_ptr++);        // First might not be a digit
        //dbmsg("First character in state->token_buf '%c'", state->token_buf[0]);
        while (isdigit(*(state->buf_ptr)) && v_i < MAX_STR_LEN) {
            //dbmsg("Appending '%c' to state->token_buf", *(state->buf_ptr));
            state->token_buf[v_i++] = *(state->buf_ptr++);
        }
        // Optional fractional part
        if (*(state->buf_ptr) == '.') {
            //dbmsg("Got a decimal point, must be a float");
            state->token_buf[v_i++] = *(state->buf_ptr++);
            while (isdigit(*(state->buf_ptr)) && v_i < MAX_STR_LEN) {
                state->token_buf[v_i++] = *(state->buf_ptr++);
            }
            state->token_buf[v_i] = '\0';
            state->token.kind = k_Float;
            // NO MORE --- token->val.float_val = strtod(state->token_buf, NULL);
            return 1;
        }
        // No fractional part, must be integer
        state->token_buf[v_i] = '\0';
        //dbmsg("No decimal point, just %s", state->token_buf);
        state->token.kind = k_Int;
        // NO MORE --- state->token.int_val = strtol(state->token_buf, NULL, 10);
        return 1;
    }
    scan_bogus(state);
    fprintf(stderr, "Not yet lexing '%s'\n", state->token.string_val);
    return 0;
}

/* Ensure state->token is the next token to be parsed */
static int lex_peek(struct Asm_Parse_State *state) {
    // A token is always buffered, so this has become
    // a no-op.
    return (state->token.kind != k_EOF);
}

/* Consume current token and make state->token be the next */
static int lex_next(struct Asm_Parse_State *state) {
    lex_read_next(state);
    dbmsg("Current token: '%s' (%s)", state->token.string_val,
            token_name(state->token.kind));
    return (state->token.kind != k_EOF);
}

/*  Token kind membership tests.
 * Each should end with a sentinal value of 0.
 * Note we cannot include k_EOF in a set of accepted lookaheads
 *
 */
static int token_kind_in(const enum Token_Kind kind, const enum Token_Kind kind_set[])  {
    int i;
    dbmsg("Checking token %s against expected types", token_name(kind));
    for (i=0; kind_set[i] != k_SENTINEL; ++i) {
        dbmsg("Checking for match to %s", token_name(kind_set[i]));
        if (kind == kind_set[i]) {
            dbmsg("Matched expected token kind!");
            return 1;
        }
    }
    dbmsg("No match with expected token kinds");
    return 0;
}

/* Lookahead checks for LL(1) parsing */
static int lookahead_is_kind(struct Asm_Parse_State *state, enum Token_Kind kind) {
    lex_peek(state);
    return (state->token.kind == kind);
}

/* Lookahead is in set of candidate kinds */
static int lookahead_in_kinds(struct Asm_Parse_State *state, const enum Token_Kind kind_set[]) {
    lex_peek(state);
    return token_kind_in(state->token.kind, kind_set);
}

static int lookahead_is(struct Asm_Parse_State *state, const char *expecting) {
    lex_peek(state);
    return (strcmp(state->token.string_val, expecting) == 0);
}

/* The LL parsing "match" operation lookahead_consume_match
 * advances to the next token after ascertaining that the current
 * lookahead is of the expected kind.   If the token doesn't match,
 * an error message is printed (and perhaps we should set a flag in
 * the lexing state).
 */
static int lookahead_consume_match(struct Asm_Parse_State *state, enum Token_Kind kind) {
    lex_peek(state);
    if (state->token.kind != kind) {
        // FIXME: printing to stderr is not the appropriate strategy in Cache
        syntax_error(state, "Expected %s, but got %s\n",
                token_names[kind], token_names[state->token.kind]);
        return 0; // Is there a better recovery tactic?
    }
    lex_next(state);
    return state->token.kind;
}


/* --------------------------------------------------------------
 * Parsing:  Simple LL(1) recursive descent parser.
 * Note that control structures in GAPL are block-structured even
 * at the bytecode level --- e.g., an 'if' makes three recursive calls
 * to evaluate condition part, then part, and else part, each of which
 * ends with a STOP instruction.
 */

/* Grammar (in progress):
 *   program ::= declarations block
 *   declarations ::= ( vardecl | subscription )*  // There is more ...
 *   block ::= (instruction)*  stop
 *   instruction ::= simple_op | if_block | while_block
 *   if_block ::= ".if"  block ".then" block [".else" block] ".fi"
 *   while_block ::= ".while" block ".do" block ".od"
 *   simple_op ::=  * Any of the simple stack-based operations, e.g., addition
 *   stop ::= the STOP instruction, which ends a block
 */

static void parse_block(struct Asm_Parse_State *state);
static void parse_declarations(struct Asm_Parse_State *state);

/* Simple ll(1) parser of assembly code text */
static void parse_program(struct Asm_Parse_State *state) {
    parse_declarations(state);
    parse_block(state);
}

static void parse_declarations(struct Asm_Parse_State *state) {
    // (vardecl | subscription) *
    //     FIXME: other kinds of declaration
    dbmsg("parse_declarations");
    struct Token* varname;
    lex_peek(state);
    while (lookahead_is_kind(state, k_Directive)) {
        dbmsg("Parsing directive %s", state->token.string_val);
        if (strcmp(state->token.string_val, ":subscribe")==0) {
            // .subscribe variable topic
            lex_next(state);
            if (lookahead_is_kind(state, k_Ident)) {
                dup_token(state, varname);
            } else {
                syntax_error(state, "Expecting identifier after :subscribe");
                // error recovery: We've already advanced scanner, so we can
                // continue from the declaration loop.
                continue;
            }
            lex_next(state);
            if (lookahead_is_kind(state, k_Ident)) {
                // FIXME: codegen for subscription here
                printf("Generate code for :subscribe %s %s\n",
                        varname->string_val, state->token.string_val);
                dispose_token(varname);
                lex_next(state);
            } else {
                syntax_error(state, "Expecting channel name after :subscribe <varname>");
                dispose_token(varname);
            }
        } else if (strcmp(state->token.string_val, ":var") == 0) {
            // :var ident type
            // consume the ":var"
            lookahead_consume_match(state, k_Directive);
            // ... ident type
            if (lookahead_is_kind(state, k_Ident)) {
                dup_token(state, varname);
            } else {
                syntax_error(state, "Expecting identifier after :var");
                continue;
            }
            lex_next(state);
            // ... type
            if (lookahead_is_kind(state, k_Ident)) {
                // FIXME: codegen for variable declaration here
                printf("Generate code for :vardecl %s %s\n",
                       varname->string_val, state->token.string_val);
                dispose_token(varname);
                lex_next(state);
            } else {
                syntax_error(state, "Expecting type name after :vardecl <varname>");
                dispose_token(varname);
            }
        } else {
            syntax_error(state, "Unrecognized directive %s", state->token.string_val);
            // Recover by gobbling any following identifiers
            lex_next(state);
            while (lookahead_is_kind(state, k_Ident)) {
                syntax_error(state, "Skipping identifier after unrecognized directive");
                lex_next(state);
            }
        }
    }
}


static void parse_if(struct Asm_Parse_State *state);
static void parse_instruction(struct Asm_Parse_State *state);


/*   block ::= (instruction)*  stop
 *   first(instruction) is { k_Ident, k_Control_Push }
 */
static void parse_block(struct Asm_Parse_State *state) {
    dbmsg("parse_block()");
    lex_peek(state);
    while (state->token.kind == k_Ident || state->token.kind == k_Control_Push) {
        if (state->token.kind == k_Control_Push) {
            if (strcmp(state->token.string_val, ".if") == 0) {
                parse_if(state);
            } else if (strcmp(state->token.string_val, ".while") == 0) {
                // parse_while(state);
            } else {
                assert(0); // Should not reach here!
            }
        } else {
            parse_instruction(state);
        }
        // Each action above should consume at least one token
        lex_peek(state);
        dbmsg("parse_block now looking at: %s (%s)\n",
                state->token.string_val, token_name(state->token.kind));
    }
    /* Block should be terminated by 'stop' instruction */
    lookahead_consume_match(state, k_Stop);
    // FIXME: generate 'stop' object code here
    return;
}

void parse_immediate(struct Asm_Parse_State *state);

static void parse_instruction(struct Asm_Parse_State *state) {
    dbmsg("parse_instruction()");
    struct OpCode *opCode;
    if ((opCode = find_op(state->token.string_val) )) {
        fprintf(stderr, "Parsed operation %s, expecting %d immediates\n",
                state->token.string_val, opCode->immediate_operands);
        lookahead_consume_match(state, k_Ident);
        for (int i=0; i < opCode->immediate_operands; ++i) {
            const enum Token_Kind immediate_kinds[] = { k_Ident, k_Float, k_Int, k_SENTINEL};
            if (lookahead_in_kinds(state, immediate_kinds)) {
                dbmsg("%s looks like an immediate operand", state->token.string_val);
                parse_immediate(state);
            } else {
                syntax_error(state, "Expecting an immediate operand, got %s", state->token.string_val);
                // We might be short of operands, so let's break to outer parsing loop
                // lex_next(state);  // Don't consume it; it might be the next operation
                break;
            }
        }
    } else {
        syntax_error(state, "Was expecting an operation, got '%s'", state->token.string_val);
        // Recover by skipping token
        lex_next(state);
    }
}

/* An immediate operand:
 *   Possible kinds are variable references (e.g., for varpush),
 *   numeric literal (constpush).  String literal?  Other kinds of
 *   literal?
 * Control operations have their own kind of immediate operands,
 * PNTR instructions which are not handled here.
 */
void parse_immediate(struct Asm_Parse_State *state) {
    switch (state->token.kind) {
        // FIXME:  Float and int values are no longer in the parse state;  OK?
        case k_Int:
        case k_Float:
            dbmsg("Immediate constant '%s' (%s)\n", state->token.string_val, token_name(state->token.kind));
            // FIXME: Generate code for numeric literal here
            break;
        case k_Ident:
            dbmsg("Variable reference '%s' (%s)\n", state->token.string_val, token_name(state->token.kind));
            // FIXME: Generate code for reference to variable slot here, after looking up in environment
            break;
        case k_Directive:
        case k_Control_Pop:
        case k_Control_Push:
            syntax_error(state, "WRONG immediate '%s' (%s)\n", state->token.string_val, token_name(state->token.kind));
            // Leave in buffer for processing by another syntactic routine
            break;
        default:
            printf("Don't know how to print token type %s\n", token_name(state->token.kind));
    }
    // Use early 'return' above if we *don't* want to consume the token
    lex_next(state);
}

static void parse_if(struct Asm_Parse_State *state) {
    lookahead_consume_match(state, k_Control_Push); // More specifically 'if?'
    // Create 'if' block here; we fill it in with thenpart, elsepart, endif
    parse_block(state); /* This part is the condition */
    lookahead_consume_match(state, k_Control_Pop); // More specifically, ".then"
    parse_block(state);  // This is the then block
    // else block is optional
    lex_peek(state);
    if (strcmp(state->token.string_val, ".else") == 0) {
        lookahead_consume_match(state, k_Control_Pop);
        parse_block(state);
        lex_peek(state);
    }
    lookahead_consume_match(state, k_Control_Pop); // ".fi"
    // FIXME: Here we complete patching up the 'ifcode'
}


/* ----------------------- */
/* The public interface    */
/* ----------------------- */

/* Note we shouldn't use any static memory in the
 * module here, because Cache is a multi-threaded system
 * and we cannot rule out multiple concurrent invocations
 * of the parser (unless we want to introduce additional
 * locking).   So we need to consider buffer management.
 *
 * Tactics:
 *    - The program text buffer belongs to the client.  It
 *      allocates it however it likes, and is responsible for
 *      its disposition.
 *    - The client passes a struct that includes a buffer for
 *      for messages.  That buffer is managed here, but the client
 *      must call assembler_close to give us a chance to clean up.
 */

struct Asm_Parse_State *asm_parse(char *text) {
    struct Asm_Parse_State* state = malloc(sizeof(struct Asm_Parse_State));
    lex_init(state, text);
    parse_program(state);
    return state;
}

/* asm_parse returns an opaque structure reference */
struct Asm_Parse_State *asm_parse_old(char *text) {
    struct Asm_Parse_State* state = malloc(sizeof(struct Asm_Parse_State));
    lex_init(state, text);
    int got_token;
    printf("Scanning\n");
    while ((got_token = lex_next(state))) {
        switch (state->token.kind) {
            // FIXME:  Float and int values are no longer in the parse state;  OK?
            case k_Int:
                printf("Int token '%s' (%s)\n", state->token.string_val, token_name(state->token.kind));
                break;
            case k_Float:
                printf("Float token '%s' (%s)\n", state->token.string_val,  token_name(state->token.kind));
                break;
            case k_Directive:
            case k_Ident:
            case k_Control_Pop:
            case k_Control_Push:
                printf("Token '%s' (%s)\n", state->token.string_val, token_name(state->token.kind));
                break;
            default:
                printf("Don't know how to print token type %s\n", token_name(state->token.kind));
        }
    }
    printf("Done\n");
    return state;
}

/* Interrogate status */
enum Asm_Parse_Status asm_parse_status(struct Asm_Parse_State *state) {
    if (state->n_errors == 0) {
        return asm_parse_ok;
    }
    if (state ->n_errors > 0) {
        return asm_parse_syntax_err;
    }
    return asm_parse_internal_err;
}

/* Error messages */
char *asm_parse_messages(struct Asm_Parse_State *state) {
    return "Error message buffer not implemented!\n";
}

/* Client must call close to reclaim resources.
 * The Asm_Parse_State pointer is invalid after
 * the call (like memory that has been freed).
 */
void asm_parse_close(struct Asm_Parse_State *state) {
    free(state);
}


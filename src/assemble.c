//
// Simple assembler for Cache byte code
// (work in progress August 2019)
//

#include "assemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>

#define DBG 1
static void dbmsg(char *msg, ...) {
    if (DBG) {
        fprintf(stderr, "\n==>");
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        va_end(args);
        fprintf(stderr, "<==\n");
    }
}

struct Lex_State {
    char* buf_ptr;
};

enum Token_Kind { k_Int, k_Float, k_Directive, k_Op, k_EOF };
char *token_names[] = { "int", "float", "directive", "op", "EOF"};

static char* token_name(enum Token_Kind kind) {
        assert(kind >= 0 && kind <= k_EOF);
        return token_names[kind];
}

enum Directive { dir_Declare, dir_Subscribe };
char* directive_names[] = { ":declare", ":subscribe" };

struct Token {
    enum Token_Kind kind;
    struct {
        long int_val;
        double float_val;
        char* string_val;
    } val;
};

/* Initialize a lexer to scan a buffer of characters */
static void lex_init(struct Lex_State *state, char *buf) {
    state->buf_ptr = buf;
}

static void skip_whitespace(struct Lex_State *state) {
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

/*
 * Temporary stashes for token values; these become invalid on the
 * next call of lex_next.
 */
char value_str[100] = "INVALID!";
long value_int =  42;
double value_float = 42.42;


/* Match a string of alphanumeric characters, except
 * the first may not be alphanumeric.  Copies matched
 * string into token structure. 1 = success,
 * 0 = something went wrong.  Result in value_str.
 */
static int scan_alstar(struct Lex_State *state, struct Token *token) {
    int v_i = 0;
    value_str[v_i++] = *(state->buf_ptr++);        // Initial char need not be alnum
    while (isalnum(*(state->buf_ptr))) {  // but we gobble as far as they are
        value_str[v_i++] = *(state->buf_ptr++);
    }
    value_str[v_i] = '\0';
    return 1;
}

/* Next token: pass a reference to the Token
 * structure into which the token should be place.
 * Returns 1 for success, 0 for EOF or failure.
 */
static int lex_next(struct Lex_State *state, struct Token *token) {
    char initial;
    skip_whitespace(state);
    if (*(state->buf_ptr) == '\0') {
        token->kind = k_EOF;
        token->val.string_val = "EOF";
        return 0;
    }
    initial = *(state->buf_ptr);
    //dbmsg("Initial character '%c'", initial);
    if (isalpha(initial)) {
        scan_alstar(state, token);
        token->kind = k_Op;
        token->val.string_val = value_str;
        return 1;
    }
    if (initial == ':') {
        scan_alstar(state, token);
        token->kind = k_Directive;
        token->val.string_val = value_str;
        return 1;
    }
    if (isdigit(initial) || initial == '-') {
        //dbmsg("Recognized as numeric with initial character '%c'", initial);
        int v_i = 0;
        /* Might be integer or floating point */
        value_str[v_i++] = *(state->buf_ptr++);        // First might not be a digit
        //dbmsg("First character in value_str '%c'", value_str[0]);
        while (isdigit(*(state->buf_ptr))) {
            //dbmsg("Appending '%c' to value_str", *(state->buf_ptr));
            value_str[v_i++] = *(state->buf_ptr++);
        }
        // Optional fractional part
        if (*(state->buf_ptr) == '.') {
            //dbmsg("Got a decimal point, must be a float");
            value_str[v_i++] = *(state->buf_ptr++);
            while (isdigit(*(state->buf_ptr))) {
                value_str[v_i++] = *(state->buf_ptr++);
            }
            value_str[v_i] = '\0';
            token->kind = k_Float;
            token->val.float_val = strtod(value_str, NULL);
            return 1;
        }
        // No fractional part, must be integer
        value_str[v_i] = '\0';
        //dbmsg("No decimal point, just %s", value_str);
        token->kind = k_Int;
        token->val.int_val = strtol(value_str, NULL, 10);
        return 1;
    }
    fprintf(stderr, "Not yet lexing '%s'\n", state->buf_ptr);
    return 0;
}

/* Simple ll(1) parser of assembly code text */
static void parse(char *text) {
    //
}


/* Dummy main for testing */
int main(int argc, char **argv) {
    struct Lex_State state;
    struct Token token;
    if (argc != 2) {
        fprintf(stderr, "Usage: assemble 'put your program here'\n");
        fprintf(stderr, "Got %d arguments\n", argc);
        exit(1);
    }
    lex_init(&state, argv[1]);
    int got_token;
    printf("Scanning %s\n", argv[1]);
    while ((got_token = lex_next(&state, &token))) {
        switch (token.kind) {
            case k_Int:
                printf("Token '%ld' (%s)\n", token.val.int_val, token_name(token.kind));
                break;
            case k_Float:
                printf("Token '%f' (%s)\n", token.val.float_val, token_name(token.kind));
                break;
            case k_Directive:
            case k_Op:
                printf("Token '%s' (%s)\n", token.val.string_val, token_name(token.kind));
                break;
            default:
                printf("Don't know how to print token type %s\n", token_name(token.kind));
        }
    }
    printf("Done\n");
}

//k_Int, k_Float, k_Directive, k_Op, k_EOF
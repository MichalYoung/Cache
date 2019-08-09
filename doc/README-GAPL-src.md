# Cache source code notes

Reading notes made by M Young in December 2018 as
I navigate the source code to understand how things fit together. 
In particular I am trying to understand how the different parts of GAPL
fit together:

## How the Parser is Invoked

* testclient.c calls install_file_automata with command RF.
   First argument is file path. Second argument is a handler,
   `dumpToStdout`, which calls `print_cache_response(r, stdout)`,
   in `cacheconnect.h`:
   `void print_cache_response(CacheResponse r, FILE* fd)`.


A cache response is

```
/* Response Data */
struct cache_response_t {
    int retcode;
    char* message;
    int ncols;
    int nrows;

    char** headers;
    char** data;
};
```

* cacheconnect.c has install_file_automata, which reads the automaton
source file and calls `install_automata`, defined as
`install_automata(char* automata, AutomataHandler_t ahandle)`.

`Q_Decl` and `Q_Arg` in srpc.h are defined as

```
#define Q_Decl(QUERY,SIZE) char QUERY[SIZE]; \
                           const struct qdecl QUERY ## _struct = {SIZE,QUERY}
#define Q_Arg(QUERY) (&QUERY ## _struct)
```

The transmitted query appears to be `SQL:register \"%s\" %s %hu %s",automata,lhost,lport,MY_SERVICE_NAME`

`register_callback.c` creates a similar query and appears to be a small test program

* The SQL parser in `parser.c` recognizes the query and translates it to `SQL_TYPE_REGISTER`.
  This is one of the query cases in hwdb.c, which does:

   ```
       case SQL_TYPE_REGISTER: {
           int v;
           if (isreadonly || !(v = hwdb_register(&stmt.sql.regist))) {
               results = rtab_new_msg(RTAB_MSG_REGISTER_FAILED, NULL);
           } else {
               static char buf[20];
               sprintf(buf, "%d", v);
               results = rtab_new_msg(RTAB_MSG_SUCCESS, buf);
           }
           break;
    ```

  Then `hwdb_register` does

  ```
      /* compile automaton */
      /* automaton has leading and trailing quotes */
      strcpy(buf, regist->automaton+1);
      /* convert '\r' back into '\n' */
      for (p = buf; *p != '\0'; p++)
          if (*p == '\r')
              *p = '\n';
      /* remove trailing quote */
      p = strrchr(buf, '\"');
      *p = '\0';
      debugf("automaton: %s\n", buf);
      au = au_create(buf, rpc, ebuf);
      if (! au) {
          errorf("Error creating automaton - %s\n", ebuf);
          rpc_disconnect(rpc);
          return 0;
      }
    ```


* au_create in automaton.c is called by hwdb.c:

    `Automaton *au_create(char *program, RpcConnection rpc, char *ebuf)`

* au_create in automaton.c calls a_parse, which is the symbol yyparse
becomes in agram.y.  (au_create could therefore be a good
place to put the dumping code.)

* Program code can be dumped by function in code.c:

    ```
    void dumpCompilationResults(unsigned long id, ArrayList *v, ArrayList *i2v,
                                     InstructionEntry *init, int initSize,
                                     InstructionEntry *behav, int behavSize) {
    ```

    It appears that au_create has all the arguments needed for dumpCompilationResults.



* GAPL parser:  agram.y --- includes the lexer (function a_lex). 
    Highly table-driven; includes table of keywords for the lexer. 
    
* GAPL code generation:  semantic routines in agram.y call function 
   ```code``` in code.c to produce InstructionEntry objects.
   This appears to be by side-effect: Although code() returns a pointer
   to an InstructionEntry, this is (always?) not used in the 
   semantic routines.
   
* GAPL instruction code
    Entries are tagged as being either instructions or data. 
    This being a stack machine, I assume being data means it 
    should be pushed onto the stack.  Instructions codes 
    appear to be function pointers (so this is a variety of 
    threaded code ... direct threaded code, I believe). 
    
* GAPL instruction functions
     Since this is threaded code, each virtual machine instruction
     is a function pointer, or perhaps an index into a table of 
     functions ... still exploring. 
     
Threaded code, with no table from opcodes
to instructions, but rather function pointers directly in the instruction stream. The execution of an instruction is 
```
(*((mc->pc++)->u.op))(mc)
```

* mc is the machine context (state of virtual machine)
* pc is the program counter, which is incremented after execution
of the instruction.  (A little confused about this access ... 
I think pc is an entry in an array, so the increment moves us 
to the next instruction.)  
* u.op must be a member of the 'pc' structure that points
to the function that executes the code.  Does the structure 
contain anything else? Guess:  Since we are not testing for 
data vs code here, each instruction must have a slot for an 
operand, and 'push' must be a possible instruction. 
Yes, the instruction is 
```void constpush(MachineContext *mc);```.  There is likewise 
a ```varpush``` function. 

* The instruction functions are declared in code.h, just after ```execute```. 


First argument to ```code``` (called from parser) is a boolean with true indicating code
(type FUNC) and false indicating data (type DATA): 
```
        if (ifInst) {
            progp->type = FUNC;
            progp->u.op = f;
        } else {
            progp->type = DATA;
            progp->u.immediate = *d;
        }
```

There are richer type tags in the instructions.  
An instructionEntry is: 
```
struct instructionEntry {
    int type;
    union {
        Inst op;
        DataStackEntry immediate;
        int offset;			/* offset from current PC */
    } u;
    char *label;
    int lineno;				/* corresponding source line no */
};
```

The type entries are defined in dataStackEntry.h.  They are 
dNULL , dBOOLEAN, dINTEGER, dDOUBLE, dTSTAMP, dSTRING, dEVENT, 
dMAP, dIDENT, dTIDENT, dWINDOW, dITERATOR, dSEQUENCE, dPTABLE.  

Inst is the function pointer: 
```
typedef void (*Inst)(MachineContext *mc);  /* actual machine instruction */
```


Instruction set (all stack-based, so e.g., plus is push(pop() + pop()): 

|Instruction  |  Action                                 |
|-------------|-----------------------------------------|
|constpush          | Push constant value                     |
| varpush	           | Push variable | 
| add	               | +
| subtract	       | -
| multiply       	   | *
| divide            	| / 
| modulo	| %
| negate	| -
| eval	|
| extract	|
| gt	| >
| ge	| >=
| lt	| <
| le	| <=
| eq	| ==
| ne	| !=
| and	| &&
| or	| ||
| not	| !
| function	|
| print	|
| assign	| =
| pluseq	| +=
| minuseq	| -=
| whilecode	|
| ifcode	|
| procedure	|
| newmap	|
| newwindow	|
| destroy	|
| bitOr	| |
| bitAnd	| &

Code generator often calls "InitDSE", which is a macro: 

```
#define initDSE(d,t,f) {(d)->type=(t); (d)->flags=(f); }

```

## Conditional branching

IF and WHILE work by back-patching relative offsets.  Considering the if/else, 
the basic structure is established when the keyword "IF" is parsed: 

```yacc
if:		  IF {
                    InstructionEntry *spc;
                    if (iflog)
                      fprintf(stderr, "Starting code generation for if\n");
                    spc = code(TRUE, ifcode, NULL, "ifcode", lineno);
                    code(TRUE, STOP, NULL, "thenpart", lineno);
                    code(TRUE, STOP, NULL, "elsepart", lineno);
                    code(TRUE, STOP, NULL, "nextstatement", lineno);
                    $$ = spc;
                  }
                ;
```

The (relative) offsets are patched in when the rest of the IF construct is 
recognized: 

```yacc
                | if condition begin body else begin body end {
                    ($1)[1].type = PNTR;
                    ($1)[1].u.offset = $3 - $1;
                    ($1)[2].type = PNTR;
                    ($1)[2].u.offset = $6 - $1;
                    ($1)[3].type = PNTR;
                    ($1)[3].u.offset = $8 - $1;
                  }
```

Note $1 is the instruction built by 'if'.  $3 - $1 is instructions 
forward to the 'begin'.  $6 - $1 is instructions forward to the 'else' part. 
$8 - $1 is instructions forward to the endif.   If there is no else, 
this becomes: 

```yacc
                | if condition begin body end {
                    ($1)[1].type = PNTR;
                    ($1)[1].u.offset = $3 - $1;
                    ($1)[2].type = PNTR;
                    ($1)[2].u.offset = 0;
                    ($1)[3].type = PNTR;
                    ($1)[3].u.offset = $5 - $1;
                  }
```

Question:  Why is the 'else' offset set to 0 rather than the endif? 

The instruction is executed by 'ifcode': 

```C
void ifcode(MachineContext *mc) {
    DataStackEntry d;
    InstructionEntry *base = mc->pc - 1;
    InstructionEntry *thenpart = base + mc->pc->u.offset;
    InstructionEntry *elsepart = base + (mc->pc+1)->u.offset;
    InstructionEntry *nextStmt = base + (mc->pc+2)->u.offset;
    InstructionEntry *condition = mc->pc + 3;

    if (iflog) {
        logit("ifcode called\n", mc->au);
        fprintf(stderr, "   PC        address %p\n", mc->pc);
        fprintf(stderr, "   condition address %p\n", condition);
        fprintf(stderr, "   thenpart  address %p\n", thenpart);
        fprintf(stderr, "   elsepart  address %p\n", elsepart);
        fprintf(stderr, "   nextstmt  address %p\n", nextStmt);
        logit("=====> Dumping condition\n", mc->au);
        dumpProgram(condition);
        logit("=====> Dumping thenpart\n", mc->au);
        dumpProgram(thenpart);
        if(elsepart != base) {
            logit("=====> Dumping elsepart\n", mc->au);
            dumpProgram(elsepart);
        } else
            logit("=====> No elsepart to if\n", mc->au);
    }
    execute(mc, condition);
    d = pop(mc->stack);
    if (iflog)
        fprintf(stderr, "%08lx: condition is %d\n", au_id(mc->au), d.value.bool_v);
    if (d.value.bool_v)
        execute(mc, thenpart);
    else if (elsepart != base)
        execute(mc, elsepart);
    mc->pc = nextStmt;
}
```

The 0 is because it doesn't actually jump to thenpart or elsepart (it 
doesn't set the pc to those addresses), but rather calls 'execute'
on the corresponding address.  The 0 disables calling execute on an else part. 

## How `publish` works

Code generation for the Cache has a set of operations that are 
implemented not directly as function pointers (with one parameter being 
machine context) but indirectly as execution of a procedure-calling 
instruction.  The known procedures can have multiple arguments.  They
are in a table in agram.y: 

```
static struct fpstruct procedures[] = {
    {"topOfHeap", 0, 0, 0},     /* void topOfHeap() */
    {"insert", 3, 3, 1},	/* void insert(map, ident, map.type) */
    {"remove", 2, 2, 2},	/* void remove(map, ident) */
    {"send", 1, MAX_ARGS, 3},   /* void send(arg, ...) */
    {"append", 2, MAX_ARGS, 4}, /* void append(window, window.dtype[, tstamp]) */
    /* if wtype == SECS, must provide tstamp */
    /* void append(sequence, basictype[, ...]) */
    {"publish", 2, MAX_ARGS, 5},/* void publish(topic, arg, ...) */
    {"frequent", 3, 3, 6}       /* void frequent(map, ident, int) */
};
```

`publish` is not a keyword, but rather is matched by the grammar rule
for a procedure call: 

```
| PROCEDURE '(' argumentlist ')' ';' {
                    if (iflog)
                      fprintf(stderr, "%s called, #args = %lld\n", $1, $3);
                    code(TRUE, procedure, NULL, "procedure", lineno);
                    initDSE(&dse, dSTRING, 0);
                    dse.value.str_v = $1;
                    code(FALSE, STOP, &dse, "procname", lineno);
                    initDSE(&dse, dINTEGER, 0);
                    dse.value.int_v = $3;
                    code(FALSE, STOP, &dse, "nargs", lineno);
                  }
```

In `code.c`, the `procedure` function has a specific case for the 
`publish` operation: 

```
    case 5: {		/* void publish(topic, arg, ...) [max 20 args] */
        publishevent(mc, narg.value.int_v, args);
        break;
    }
```

`mc` is the machine context provided to each threaded action (called 
by the interpreter through the sequence of function pointers).  Additional 
information is extracted from the machine context: 

```
    DataStackEntry name = mc->pc->u.immediate;
    DataStackEntry narg = (mc->pc+1)->u.immediate;
    DataStackEntry args[MAX_ARGS];
    int i;
    unsigned int nargs;
    struct fpargs *range;

    nargs = narg.value.int_v;
```

The arguments to publish go on the `args` stack, after extracting them 
from the data stack maintained in the machine context: 

```
    for (i = narg.value.int_v -1; i >= 0; i--)
        args[i] = pop(mc->stack);
```

`publishevent` transforms the channel name and arguments into an SQL 
query, which is sent to the database part of the system. 

```
static void publishevent(MachineContext *mc, long long nargs,
                         DataStackEntry *args) {
    int i, n, ans, error;
    sqlinsert sqli;
    char *colval[NCOLUMNS];
    int *coltype[NCOLUMNS];
    char buf[2048];
    //  Logging, check that first argument is a valid topic ... 
    //  Build up colval and coltype, one entry per argument
        ... 
        ans = hwdb_insert(&sqli);
        // Error checking, freeing storage, etc
 }
```

`hwdb` is the "homework database", because "homework" was the project 
name (probably for the home router management project).  Files `hwdb.h`
and `hwdb.c` are the main (temporal) database managers.  (TBD: Read enough 
of hwdb.h and hwdb.c to understand requirements of the sql insertion). 

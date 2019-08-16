//
// Disassemble (dump) byte code for an autoamton in a human-readable
// format.
// MY 2019-08-08, extracted from the 'dump' functionality of code.c
// and edited to be a bit more concise and human-readable.
// We need a round trip to 'automata' because that is where
// knowledge of the automaton struct lives.
//

#include "disassemble.h"
#include "adts/hashmap.h"
#include "adts/iterator.h"
#include <string.h>

static void print_instruction(InstructionEntry *p, ArrayList *i2v, FILE *fd) {
    char* ident;
    long long int var_slot;
    switch(p->type) {
        case FUNC:
            fprintf(fd, "\t :%s (%p)\n", p->label, p->u.op);
            break;
        case DATA:
            /* Special casing based on label is inadequate --- there are multiple
             * labels depending on context.  We need to do it based on prior
             * instruction slot being varpush.
             */
            if (strcmp(p->label, "variable name") == 0 || strcmp(p->label, "variable") == 0 ) {
                /* Special case for variable indexes: What is it's name? */
                var_slot = p->u.immediate.value.int_v;
                (void) al_get(i2v, var_slot, (void **)&ident);
                fprintf(fd, "\t [%lld] => %s\n", var_slot, ident);
            } else {
                fprintf(fd, "\t data (%s), ", p->label);
                dumpDataStackEntry(&(p->u.immediate), 1);
            }
            break;
        case PNTR:
            fprintf(fd, "Offset from PC (%s), %d\n", p->label, p->u.offset);
            break;
    }
}

static void print_block(InstructionEntry *p, int n, ArrayList *i2v, FILE *fd) {
    while (n > 0) {
        print_instruction(p, i2v, fd);
        p++;
        n--;
    }
}

static void print_variables(ArrayList *v, ArrayList *i2v, FILE *fd) {
    long i, n;
    n = al_size(v);
    for (i = 0; i < n; i++) {
        DataStackEntry *d;
        char *s;
        (void) al_get(v, i, (void **)&d);
        (void) al_get(i2v, i, (void **)&s);
        fprintf(fd, "%s: ", s);
        switch(d->type) {
            case dBOOLEAN:
                fprintf(fd, "bool");
                break;
            case dINTEGER:
                fprintf(fd, "int");
                break;
            case dDOUBLE:
                fprintf(fd, "real");
                break;
            case dTSTAMP:
                fprintf(fd, "tstamp");
                break;
            case dSTRING:
                fprintf(fd, "string");
                break;
            case dEVENT:
                fprintf(fd, "tuple");
                break;
            case dMAP:
                fprintf(fd, "map");
                break;
            case dIDENT:
                fprintf(fd, "identifier");
                break;
            case dWINDOW:
                fprintf(fd, "window");
                break;
            case dITERATOR:
                fprintf(fd, "iterator");
                break;
            case dSEQUENCE:
                fprintf(fd, "sequence");
                break;
            case dPTABLE:
                fprintf(fd, "PTable");
                break;
        }
        fprintf(fd, "\n");
    }
}

void print_topics(HashMap *topics, ArrayList *i2v, FILE *fd) {
    Iterator *it = hm_it_create(topics);
    if (!it) {
        fprintf(fd, "No topics\n");
        return;
    }
    while (it_hasNext(it)) {
        HMEntry *hme;
        char *key = "Has not been assigned";
        long var_index = 42L;
        char *var_name = "Has not been assigned";
        (void) it_next(it, (void **)&hme);
        key = hmentry_key(hme);
        var_index = (long) hmentry_value(hme);
        al_get(i2v, var_index, (void **)&var_name);
        fprintf(fd, "%s => %s [slot %d]\n", key, var_name, var_index);
        // top_subscribe(hmentry_key(hme), au->id);
    }
    it_destroy(it);
}

void do_disassemble(unsigned long id, HashMap *topics,
                            ArrayList *v, ArrayList *i2v,
                            InstructionEntry *init, int initSize,
                            InstructionEntry *behav, int behavSize, 
                            FILE *fd) {

    fprintf(fd, "=== Compilation results for automaton %08lx\n", id);
    fprintf(fd, "====== variables\n");
    print_variables(v, i2v, fd);
    fprintf(fd, "=== Subscribed to topics ===\n");
    print_topics(topics, i2v, fd);
    fprintf(fd, "====== initialization instructions\n");
    print_block(init, initSize, i2v, fd);  /* MY added variable name table */
    fprintf(fd, "====== behavior instructions\n");
    print_block(behav, behavSize, i2v, fd);
    fprintf(fd, "=== End of comp results for automaton %08lx\n", id);
}


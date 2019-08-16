//
// Disassemble (dump) byte code for an autoamton in a human-readable
// format.
// MY, August 2019, extracted from the 'dump' functionality of code.c
// and edited to be a bit more concise and human-readable.
//

#ifndef CACHE_PROJECT_DISASSEMBLE_H
#define CACHE_PROJECT_DISASSEMBLE_H

#include "code.h"
#include "automaton.h"
#include <stdio.h>

/* void disassemble(Automaton *au, FILE *fd);
 *     is in automaton.h, because disassembly needs
 *     the symbolic information (variable names, etc)
 *     from the automaton structure.
 */

/* The actual disassembly depends primarily on code.h; normally
 * called from 'disassemble' in automaton.c.
 */
void do_disassemble(unsigned long id, HashMap *topics,
                            ArrayList *v, ArrayList *i2v,
                            InstructionEntry *init, int initSize,
                            InstructionEntry *behav, int behavSize,
                            FILE *fd);

#endif //CACHE_PROJECT_DISASSEMBLE_H

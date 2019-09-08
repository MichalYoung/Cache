//
// Public API for assembler will go here.
//

#ifndef CACHE_PROJECT_ASSEMBLE_H
#define CACHE_PROJECT_ASSEMBLE_H

struct Asm_Parse_State;

/* asm_parse returns an opaque structure reference */
struct Asm_Parse_State *asm_parse(char *text);

/* Interrogate the returned structure:
 * asm_parse_ok returns 1 for ok, 0 for errors
 */
enum Asm_Parse_Status {
    asm_parse_ok = 1,
    asm_parse_syntax_err = 2,
    asm_parse_internal_err = 3
};
enum Asm_Parse_Status asm_parse_status(struct Asm_Parse_State *state);
char *asm_parse_messages(struct Asm_Parse_State *state);

/* Client must call close to reclaim resources.
 * The Asm_Parse_State pointer is invalid after
 * the call (like memory that has been freed).
 */
void asm_parse_close(struct Asm_Parse_State *state);


#endif //CACHE_PROJECT_ASSEMBLE_H

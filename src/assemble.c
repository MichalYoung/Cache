//
// Command line interface to assembler.c  (test harness)
//
// assembler.{h,c} is designed to be invoked by the Cache system
// in response to commands and program text sent over a network port. It has no
// 'main' function.  This file, assemble.c, provides a simple
// 'main' function for testing it.
//

#include "assembler.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>

char* USAGE = "Usage: assemble my/asm/file_name\n";

int main(int argc, char** argv) {

    if (argc != 2) {
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
    char* fname = argv[1];
    FILE* f;
    int slen;
    struct stat st;
    int okstat = stat(fname, &st);
    if (okstat == -1) {
        fprintf(stderr, "Couldn't open %s\n", fname);
        exit(1);
    }
    int buflen=st.st_size;
    char* buf=(char*)malloc(sizeof(char)*(buflen+1));

    f = fopen(fname,"r");
    if (f == NULL) {
        fprintf(stderr,"Failed to open %s for reading.\n", fname);
        exit(1);
    }
    slen = fread(buf, buflen, buflen, f);
    assert (slen == 1); // Number of *chunks*, not number of bytes
    buf[buflen]='\0';

    struct Asm_Parse_State *status = asm_parse(buf);
    if (asm_parse_status(status) == asm_parse_ok) {
        fprintf(stderr, "Successful parse\n");
    } else if (asm_parse_status(status) == asm_parse_syntax_err) {
        fprintf(stderr, "Syntax errors\n");
        char *messages = asm_parse_messages(status);
        fprintf(stderr, "%s\n", messages);
        exit(1);
    } else if (asm_parse_status(status) == asm_parse_internal_err) {
        fprintf(stderr, "Internal error\n");
        exit(3);
    }
    asm_parse_close(status);
    fprintf(stderr, "assemble done\n");
    exit(0);
}





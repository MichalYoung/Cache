/*
 * Copyright (c) 2016, University of Oregon
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the University of Glasgow nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * testclient for the Homework Database
 *
 * usage: ./testclient [-f stream] [-h host] [-p port] [-l packets]
 *
 * each line from standard input (or a fifo) is assumed to be a complete
 * query.  It is sent to the Homework database server, and the results
 * are printed on the standard output
 */

#include "config.h"
#include "util.h"
#include <srpc/srpc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdarg.h>

#include "cacheconnect.h"

#define USAGE "./testclient [-f fifo] [-h host] [-p port] [-l packets]"
#define MAX_LINE 4096
#define MAX_INSERTS 500

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

static char bf[MAX_LINE]; /* buffer to hold pushed-back line */
static int pback = 0; /* if bf holds a pushed-back line */

static int ifstream = 0;
static FILE *f;

static char *fetchline(char *line) {
    char* ret;
    if (ifstream) {
        ret=fgets(line, MAX_LINE, f);
    } else {
        ret=fgets(line, MAX_LINE, stdin);
    }
    int t = strlen(ret);
    ret[t-1]='\0';
    return ret;
}

int dumpToStdout(CacheResponse r) {
    print_cache_response(r, stdout);
    return 0;
}

int main(int argc, char *argv[]) {
    RpcConnection rpc;
    char inb[MAX_LINE];
    Q_Decl(query, SOCK_RECV_BUF_LEN);
    char resp[SOCK_RECV_BUF_LEN];
    int n;
    unsigned len;
    char *host;
    unsigned short port;
    struct timeval start, stop;
    unsigned long count = 0;
    unsigned long msec;
    double mspercall;
    int i, j, log;
    char *service;
    CacheResponse cr;

    dbmsg("testclient connecting to service");

    connect_env(&host, &port, &service);
    if(service == NULL) {
        service = "HWDB";
    }
    dbmsg("testclient: Service is '%s'", service);
    if(host == NULL) {
        host = HWDB_SERVER_ADDR;
    }
    if(port == 0) {
        port = HWDB_SERVER_PORT;
    }

    dbmsg("testclient connected");
    log = 1;
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "usage: %s\n", USAGE);
            exit(1);
        }
        if (strcmp(argv[i], "-h") == 0)
            host = argv[j];
        else if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[j]);
        else if (strcmp(argv[i], "-s") == 0)
            service = argv[j];
        else if (strcmp(argv[i], "-f") == 0) {
            ifstream = 1;
            if ((f = fopen(argv[j], "r")) == NULL) {
                fprintf(stderr, "Failed to open %s\n", argv[j]);
                exit(-1);
            }
        } else if (strcmp(argv[i], "-l") == 0) {
            if (strcmp(argv[j], "packets") == 0)
                log++;
            else
                fprintf(stderr, "usage: %s\n", USAGE);
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]);
        }
        i = j + 1;
    }
    int err = init_cache(host, port, service);
    if(err) {
        fprintf(stderr, "connection failed\n");
        return 1;
    }
    gettimeofday(&start, NULL);

    dbmsg("testclient starting fetch loop");

    while (fetchline(inb) != NULL) {
        if (strcmp(inb, "\n") == 0) /* ignore blank lines */
            continue;
        if(inb[0]=='S') {
            if(inb[1]=='F') {
                cr=file_sql(inb+2);
            } else {
                cr=raw_sql(inb+1);
            }
            if(cr) {
                print_cache_response(cr, stdout);
            } else {
                err=1;
            }
        } else if (inb[0]=='R') {
            if(inb[1]=='F') {
                dbmsg("testclient: recognized command to install automaton '%s'", inb);
                fprintf(stderr, "testclient: automaton to install will be '%s'\n", inb+2);
                err=install_file_automata(inb+2, dumpToStdout);
            } else {
                err=install_automata(inb+1, dumpToStdout);
            }
        } else {
            fprintf(stderr,"unrecognized action %c\n",inb[0]);
            continue;
        }
        if(err) {
            fprintf(stderr,"Could not process command: %s\n",inb);
            continue;
        }

        count++;
    }
    gettimeofday(&stop, NULL);
    if (stop.tv_usec < start.tv_usec) {
        stop.tv_usec += 1000000;
        stop.tv_sec--;
    }
    msec = 1000 * (stop.tv_sec - start.tv_sec) +
           (stop.tv_usec - start.tv_usec) / 1000;
    mspercall = 0;
    if (count > 0)
        mspercall = (double)msec / (double)count;
    fprintf(stderr, "%ld queries processed in %ld.%03ld seconds, %.3fms/call\n",
            count, msec/1000, msec%1000, mspercall);
    if (ifstream && f) fclose(f);
    rpc_disconnect(rpc);
    return 0;
}


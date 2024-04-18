/*
* DLA-VVTB : Deep Learning Accelerator - Vulnerability Verification Test Board
* -------------------------------------------------------------------------------
# Copyrights (c) 2023 TSN Lab. All rights reserved.
# Programmed by hounjoung@tsnlab.com
#
# Revision history
# 2023-xx-xx    hounjoung   create this file.
# $Id$
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pcap.h>
#include <unistd.h>
#include <fcntl.h>
#include <log.h>
#include <libcom.h>
#include <dla_intf.h>
#include <dla_main.h>
#include <dla_LargeRead.h>

#include "command_parser.h"

int main_bitmode(int argc, char *argv[]);

int verbose = 0;
char * progname = NULL;

void print_testResult(enum testProgress s, char *r) {
    FILE *fp;

    fp = fopen(RESULT_DIRECTORY"/"RESULT_FILE, "wt");
    if(fp == NULL) {
        lprintf(LOG_ERR, "Cannot open %s file !", 
				RESULT_DIRECTORY"/"RESULT_FILE);
        return;
    }

    if(s == PROG_E) {
        fprintf(fp, "dlatool: running\n");
        fprintf(fp, "%s\n", r);
    } else if(s == STOP_E) {
        fprintf(fp, "dlatool: terminated\n");
        fprintf(fp, "%s\n", r);
    }

    fclose(fp);
}

int main(int argc, char *argv[]) {
    int        rc=0, t_argc;
    char     **pav = NULL;
    int dbgId;

    for(dbgId=0; dbgId<argc; dbgId++) {
		lprintf(LOG_DEBUG, "argv[%d] : %s", dbgId, argv[dbgId]);
	}

    /* save program name */
    progname = strrchr(argv[0], '/');
    progname = ((!progname) ? argv[0] : progname+1);

    /* setup log */
    log_init(progname, 0, 0);

    t_argc = argc, pav = argv;
    pav++, t_argc--;

    make_directory(RESULT_DIRECTORY);
    print_testResult(PROG_E, "Start ...");
    rc = commandParser(t_argc, pav);

    if (rc < 0) {
        print_testResult(STOP_E, "FAIL");
        exit(EXIT_FAILURE); 
    } else {
        print_testResult(STOP_E, "SUCCESS");
        exit(EXIT_SUCCESS);
    }
}

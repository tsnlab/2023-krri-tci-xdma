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
#include <lib_menu.h>
#include <log.h>
#include <helper.h>

#include "api_ftd2xx.h"

menu_command_t  mainCommand_tbl[] = {
    { "board",     EXECUTION_ATTR,   process_main_boardCmd, \
        "   board <test-item>", \
        "   Validate the DLA-VVTB board and verify the engine through this board\n"
        "   The test items are listed below:\n"
        "        loopback: Writes a known sequence of bytes then expects to read them back.\n"
        "        register: Writes or reads data values to or from registers in FPGAs on the board.\n"
        "                  Or show register list in FPGAs on the board.\n"},
    { 0,           EXECUTION_ATTR,   NULL, " ", " "}
};

int
commandParser(int argc, char ** argv) {
    char  **pav = NULL;
    int dbgId;

    for(dbgId=0; dbgId<argc; dbgId++) {
        lprintf(LOG_DEBUG, "argv[%d] : %s", dbgId, argv[dbgId]);
    }

    pav = argv;

    return lookup_cmd_tbl(argc, (const char **)pav, mainCommand_tbl, ECHO);
}


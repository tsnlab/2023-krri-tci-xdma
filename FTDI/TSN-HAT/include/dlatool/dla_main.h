#ifndef __DLA_MAIN_H__
#define __DLA_MAIN_H__

#define RESULT_DIRECTORY ".result"
#define RESULT_FILE      ".execution.result"

enum testProgress { PROG_E = 0x0, STOP_E = 0x1 };

void print_testResult(enum testProgress s, char *r);

#endif    // __DLA_MAIN_H__

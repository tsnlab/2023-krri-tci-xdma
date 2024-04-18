#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <version.h>
#include <ftd2xx.h>
#include <lib_menu.h>
#include <log.h>
#include <helper.h>
#include <dla_intf.h>

extern int verbose;

#define OPTION_STRING  "p:hv"

static struct dla_intf * dla_main_intf = NULL;

int dla_port_open(struct dla_intf * intf) {
    int iport;
    FT_STATUS ftStatus;

    if(intf->opened) {
        return -1;
    }

    iport = intf->port;
    ftStatus = FT_Open(iport, &intf->ftHandle);
    if(ftStatus != FT_OK) {
        /* 
        This can fail if the ftdi_sio driver is loaded
        use lsmod to check this and rmmod ftdi_sio to remove
        also rmmod usbserial
        */
        lprintf(LOG_ERR, "FT_Open(%d) failed\n", iport);
        return 1;
    }

    intf->opened = 1;

    return 0;
}

int dla_port_close(struct dla_intf * intf) {

    if(!intf->opened) {
        return -1;
    }

    FT_Close(intf->ftHandle);

    intf->opened = 0;

    return 0;
}

/*
 * Print all the commands in the above table to stderr
 * used for help text on command line and shell
 */
void dla_cmd_print(struct dla_cmd * cmdlist) {
    struct dla_cmd * cmd;
    int hdr = 0;

    if (!cmdlist) {
        return;
    }
    for (cmd=cmdlist; cmd->func; cmd++) {
        if (!cmd->desc) {
            continue;
        }
        if (hdr == 0) {
            lprintf(LOG_NOTICE, "Commands:");
            hdr = 1;
        }
        lprintf(LOG_NOTICE, "\t%-12s  %s", cmd->name, cmd->desc);
    }
    lprintf(LOG_NOTICE, "");
}


/* dla_cmd_run - run a command from list based on parameters
 *                called from main()
 *
 *                1. iterate through dla_cmd_list matching on name
 *                2. call func() for that command
 *
 * @intf: dla interface
 * @name: command name
 * @argc: command argument count
 * @argv: command argument list
 *
 * returns value from func() of that command if found
 * returns -1 if command is not found
 */
int dla_cmd_run(struct dla_intf * intf, char * name, int argc, char ** argv) {
    struct dla_cmd * cmd = intf->cmdlist;

    /* hook to run a default command if nothing specified */
    if (!name) {
        if (!cmd->func || !cmd->name) {
            return -1;
        }

        if (!strcmp(cmd->name, "default")) {
            return cmd->func(intf, 0, NULL);
        }

        lprintf(LOG_ERR, "No command provided!");
        dla_cmd_print(intf->cmdlist);
        return -1;
    }

    for (cmd=intf->cmdlist; cmd->func; cmd++) {
        if (!strcmp(name, cmd->name)) {
            break;
        }
    }
    if (!cmd->func) {
        cmd = intf->cmdlist;
        if (!strcmp(cmd->name, "default")) {
            return cmd->func(intf, argc+1, argv-1);
        }

        lprintf(LOG_ERR, "Invalid command: %s", name);
        dla_cmd_print(intf->cmdlist);
        return -1;
    }
    return cmd->func(intf, argc, argv);
}

static void dla_option_usage(const char * progname, struct dla_cmd * cmdlist, 
                             struct dla_intf_support * intflist) {
    lprintf(LOG_NOTICE, "%s version %s\n", progname, VERSION_STRING);
    lprintf(LOG_NOTICE, "usage: %s [options...] <command>\n", progname);
    lprintf(LOG_NOTICE, "       -h         This help");
    lprintf(LOG_NOTICE, "       -p port    USB port [default=0]");
    lprintf(LOG_NOTICE, "       -v         Verbose (can use multiple times)");
    lprintf(LOG_NOTICE, "");

    if (cmdlist) {
        dla_cmd_print(cmdlist);
    }
}

extern char * progname;

int dla_main(int argc, char ** argv,
             struct dla_cmd * cmdlist,
             struct dla_intf_support * intflist) {
    int argflag;
    int rc = -1;
    int iport = 0;
    char * portname = NULL;
    struct dla_intf intf;

    while ((argflag = getopt(argc, (char **)argv, OPTION_STRING)) != -1)
    {
        switch (argflag) {
        case 'p':
            if (str2int(optarg, &iport) != 0) {
                lprintf(LOG_ERR, "Invalid parameter given or out of range for '-p'.");
                rc = -1;
                goto out_free;
            }
            /* Check if port is -gt 0 && port is -lt 65535 */
            if (iport < 0 || iport > 65535) {
                lprintf(LOG_ERR, "Port number %i is out of range.", iport);
                rc = -1;
                goto out_free;
            }
            break;
        case 'h':
            dla_option_usage(progname, cmdlist, intflist);
            rc = 0;
            goto out_free;
            break;
        case 'v':
            log_level_set(++verbose);
            if (verbose == 2) {
                /* add version info to debug output */
                lprintf(LOG_DEBUG, "%s version %s\n", progname, VERSION_STRING);
            }
            break;
        }
    }

    memset(&intf, 0, sizeof(struct dla_intf));

    dla_main_intf = &intf;

    /* check for command before doing anything */
    if (argc-optind > 0
            && !strcmp(argv[optind], "help")) {
        dla_cmd_print(cmdlist);
        rc = 0;
        goto out_free;
    }

    dla_main_intf->cmdlist = cmdlist;
    dla_main_intf->port = iport;

    if(dla_port_open(dla_main_intf)) {
        rc = 0;
        goto out_free;
    }
    /* now we finally run the command */
    if (argc-optind > 0) {
        rc = dla_cmd_run(dla_main_intf, argv[optind], argc-optind-1,
                                            &(argv[optind+1]));
    } else {
        rc = dla_cmd_run(dla_main_intf, NULL, 0, NULL);
    }

    dla_port_close(dla_main_intf);

out_free:
    log_halt();

    if (portname) {
        free(portname);
        portname = NULL;
    }

    return rc;
}


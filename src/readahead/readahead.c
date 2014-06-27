/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2012 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdio.h>
#include <getopt.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "util.h"
#include "def.h"
#include "readahead-common.h"

unsigned arg_files_max = 16*1024;
off_t arg_file_size_max = READAHEAD_FILE_SIZE_MAX;
usec_t arg_timeout = 2*USEC_PER_MINUTE;
FILE *finputlist;

static int help(void) {

        printf("%s [OPTIONS...] collect [DIRECTORY]\n\n"
               "Collect read-ahead data on early boot.\n\n"
               "  -h --help                 Show this help\n"
               "     --max-files=INT        Maximum number of files to read ahead\n"
               "     --file-size-max=BYTES  Maximum size of files to read ahead\n"
               "     --timeout=USEC         Maximum time to spend collecting data\n"
               "     --filelist=ABSOLUTE_PATH         Inclusive list of files to be used in creating the pack\n\n\n",
               program_invocation_short_name);

        printf("%s [OPTIONS...] replay [DIRECTORY]\n\n"
               "Replay collected read-ahead data on early boot.\n\n"
               "  -h --help                 Show this help\n"
               "     --file-size-max=BYTES  Maximum size of files to read ahead\n\n\n",
               program_invocation_short_name);

        printf("%s [OPTIONS...] analyze [PACK FILE]\n\n"
               "Analyze collected read-ahead data.\n\n"
               "  -h --help                 Show this help\n",
               program_invocation_short_name);

        return 0;
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_FILES_MAX = 0x100,
                ARG_FILE_SIZE_MAX,
                ARG_TIMEOUT,
                ARG_FILELIST
        };

        static const struct option options[] = {
                { "help",          no_argument,       NULL, 'h'                },
                { "files-max",     required_argument, NULL, ARG_FILES_MAX      },
                { "file-size-max", required_argument, NULL, ARG_FILE_SIZE_MAX  },
                { "timeout",       required_argument, NULL, ARG_TIMEOUT        },
                { "filelist",      required_argument, NULL, ARG_FILELIST       },
                { NULL,            0,                 NULL, 0                  }
        };

        int c, ret;
        char filelistname[LINE_MAX];

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "h", options, NULL)) >= 0) {

                switch (c) {

                case 'h':
                        help();
                        return 0;

                case ARG_FILES_MAX:
                        if (safe_atou(optarg, &arg_files_max) < 0 || arg_files_max <= 0) {
                                log_error("Failed to parse maximum number of files %s.", optarg);
                                ret = -EINVAL;
                                goto out;
                        }
                        break;

                case ARG_FILE_SIZE_MAX: {
                        unsigned long long ull;

                        if (safe_atollu(optarg, &ull) < 0 || ull <= 0) {
                                log_error("Failed to parse maximum file size %s.", optarg);
                                ret = -EINVAL;
                                goto out;
                        }

                        arg_file_size_max = (off_t) ull;
                        break;
                }

                case ARG_TIMEOUT:
                        if (parse_sec(optarg, &arg_timeout) < 0 || arg_timeout <= 0) {
                                log_error("Failed to parse timeout %s.", optarg);
                                ret = -EINVAL;
                                goto out;
                        }

                        break;

                case ARG_FILELIST:
                        if (sscanf(optarg, "%s", filelistname) != 1) {
                                log_error("Invalid filelistname.\n");
                                ret = -EINVAL;
                                /* in unlikely event that two of these switches
                                   are given on the command line */
                                goto out;
                        }
                        else {
                                finputlist = fopen(filelistname,"r");
                                if (!finputlist) {
                                        log_error("Cannot read list %s of collect-file names.\n", filelistname);
                                        return -ENOENT;
                                }
                                log_info("Using files in %s to generate pack.\n", filelistname);
                        }
                        break;
                case '?':
                        ret = -EINVAL;
                        goto out;

                default:
                        log_error("Unknown option code %c", c);
                        ret = -EINVAL;
                        goto out;
                }
        }

        if (optind != argc-1 &&
            optind != argc-2) {
                help();
                ret = -EINVAL;
                goto out;
        }

        return 1;
out:
        if (finputlist) {
                fclose(finputlist);
                finputlist = NULL;
        }

        return ret;
}

int main(int argc, char *argv[]) {
        int ret, r = EXIT_FAILURE;

        log_set_target(LOG_TARGET_SAFE);
        log_parse_environment();
        log_open();

        umask(0022);
        finputlist = NULL;

        ret = parse_argv(argc, argv);
        if (ret == 0)
                r = EXIT_SUCCESS;
        if (ret <= 0)
                goto out;

        if (streq(argv[optind], "collect"))
                /* finputlist is NULL if not --filelist is given */
                r = main_collect(argv[optind+1], &finputlist);
        else if (streq(argv[optind], "replay"))
                r = main_replay(argv[optind+1]);
        else if (streq(argv[optind], "analyze"))
                r = main_analyze(argv[optind+1]);
        else {
                log_error("Unknown verb %s.", argv[optind]);
                r = EXIT_FAILURE;
        }

out:
        if (finputlist) {
                fclose(finputlist);
                finputlist = NULL;
        }

        return r;
}

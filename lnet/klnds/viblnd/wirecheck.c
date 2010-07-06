/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lnet/api-support.h>

/* This ghastly hack to allows me to include lib-types.h It doesn't affect any
 * assertions generated here (but fails-safe if it ever does) */
typedef struct {
        int     counter;
} atomic_t;

#include <lnet/lib-types.h>

#define IBNAL_USE_FMR 1
#include "viblnd_wire.h"

#ifndef HAVE_STRNLEN
#define strnlen(s, i) strlen(s)
#endif

#define BLANK_LINE()                            \
do {                                            \
        printf ("\n");                          \
} while (0)

#define COMMENT(c)                              \
do {                                            \
        printf ("        /* "c" */\n");         \
} while (0)

#undef STRINGIFY
#define STRINGIFY(a) #a

#define CHECK_DEFINE(a)                                         \
do {                                                            \
        printf ("        CLASSERT ("#a" == "STRINGIFY(a)");\n"); \
} while (0)

#define CHECK_VALUE(a)                                  \
do {                                                    \
        printf ("        CLASSERT ("#a" == %d);\n", a);  \
} while (0)

#define CHECK_MEMBER_OFFSET(s,m)                \
do {                                            \
        CHECK_VALUE((int)offsetof(s, m));       \
} while (0)

#define CHECK_MEMBER_SIZEOF(s,m)                \
do {                                            \
        CHECK_VALUE((int)sizeof(((s *)0)->m));  \
} while (0)

#define CHECK_MEMBER(s,m)                       \
do {                                            \
        CHECK_MEMBER_OFFSET(s, m);              \
        CHECK_MEMBER_SIZEOF(s, m);              \
} while (0)

#define CHECK_STRUCT(s)                         \
do {                                            \
        BLANK_LINE ();                          \
        COMMENT ("Checks for struct "#s);       \
        CHECK_VALUE((int)sizeof(s));            \
} while (0)

void
system_string (char *cmdline, char *str, int len)
{
        int   fds[2];
        int   rc;
        pid_t pid;

        rc = pipe (fds);
        if (rc != 0)
                abort ();

        pid = fork ();
        if (pid == 0) {
                /* child */
                int   fd = fileno(stdout);

                rc = dup2(fds[1], fd);
                if (rc != fd)
                        abort();

                exit(system(cmdline));
                /* notreached */
        } else if ((int)pid < 0) {
                abort();
        } else {
                FILE *f = fdopen (fds[0], "r");

                if (f == NULL)
                        abort();

                close(fds[1]);

                if (fgets(str, len, f) == NULL)
                        abort();

                if (waitpid(pid, &rc, 0) != pid)
                        abort();

                if (!WIFEXITED(rc) ||
                    WEXITSTATUS(rc) != 0)
                        abort();

                if (strnlen(str, len) == len)
                        str[len - 1] = 0;

                if (str[strlen(str) - 1] == '\n')
                        str[strlen(str) - 1] = 0;

                fclose(f);
        }
}

int
main (int argc, char **argv)
{
        char unameinfo[80];
        char gccinfo[80];

        system_string("uname -a", unameinfo, sizeof(unameinfo));
        system_string("gcc -v 2>&1 | tail -1", gccinfo, sizeof(gccinfo));

        printf ("void vibnal_assert_wire_constants (void)\n"
                "{\n"
                "        /* Wire protocol assertions generated by 'wirecheck'\n"
                "         * running on %s\n"
                "         * with %s */\n"
                "\n", unameinfo, gccinfo);

        BLANK_LINE ();
        
        COMMENT ("Constants...");
        CHECK_DEFINE (IBNAL_MSG_MAGIC);
        CHECK_DEFINE (IBNAL_MSG_VERSION);

        CHECK_DEFINE (IBNAL_MSG_CONNREQ);
        CHECK_DEFINE (IBNAL_MSG_CONNACK);
        CHECK_DEFINE (IBNAL_MSG_NOOP);
        CHECK_DEFINE (IBNAL_MSG_IMMEDIATE);
        CHECK_DEFINE (IBNAL_MSG_PUT_REQ);
        CHECK_DEFINE (IBNAL_MSG_PUT_NAK);
        CHECK_DEFINE (IBNAL_MSG_PUT_ACK);
        CHECK_DEFINE (IBNAL_MSG_PUT_DONE);
        CHECK_DEFINE (IBNAL_MSG_GET_REQ);
        CHECK_DEFINE (IBNAL_MSG_GET_DONE);

        CHECK_DEFINE (IBNAL_REJECT_CONN_RACE);
        CHECK_DEFINE (IBNAL_REJECT_NO_RESOURCES);
        CHECK_DEFINE (IBNAL_REJECT_FATAL);

        CHECK_STRUCT (kib_connparams_t);
        CHECK_MEMBER (kib_connparams_t, ibcp_queue_depth);
        CHECK_MEMBER (kib_connparams_t, ibcp_max_msg_size);
        CHECK_MEMBER (kib_connparams_t, ibcp_max_frags);

        CHECK_STRUCT (kib_immediate_msg_t);
        CHECK_MEMBER (kib_immediate_msg_t, ibim_hdr);
        CHECK_MEMBER (kib_immediate_msg_t, ibim_payload[13]);

        CHECK_DEFINE (IBNAL_USE_FMR);
#if IBNAL_USE_FMR
        CHECK_STRUCT (kib_rdma_desc_t);
        CHECK_MEMBER (kib_rdma_desc_t, rd_addr);
        CHECK_MEMBER (kib_rdma_desc_t, rd_nob);
        CHECK_MEMBER (kib_rdma_desc_t, rd_key);
#else
        CHECK_STRUCT (kib_rdma_frag_t);
        CHECK_MEMBER (kib_rdma_frag_t, rf_nob);
        CHECK_MEMBER (kib_rdma_frag_t, rf_addr_lo);
        CHECK_MEMBER (kib_rdma_frag_t, rf_addr_hi);

        CHECK_STRUCT (kib_rdma_desc_t);
        CHECK_MEMBER (kib_rdma_desc_t, rd_key);
        CHECK_MEMBER (kib_rdma_desc_t, rd_nfrag);
        CHECK_MEMBER (kib_rdma_desc_t, rd_frags[13]);
#endif
        CHECK_STRUCT (kib_putreq_msg_t);
        CHECK_MEMBER (kib_putreq_msg_t, ibprm_hdr);
        CHECK_MEMBER (kib_putreq_msg_t, ibprm_cookie);

        CHECK_STRUCT (kib_putack_msg_t);
        CHECK_MEMBER (kib_putack_msg_t, ibpam_src_cookie);
        CHECK_MEMBER (kib_putack_msg_t, ibpam_dst_cookie);
        CHECK_MEMBER (kib_putack_msg_t, ibpam_rd);

        CHECK_STRUCT (kib_get_msg_t);
        CHECK_MEMBER (kib_get_msg_t, ibgm_hdr);
        CHECK_MEMBER (kib_get_msg_t, ibgm_cookie);
        CHECK_MEMBER (kib_get_msg_t, ibgm_rd);

        CHECK_STRUCT (kib_completion_msg_t);
        CHECK_MEMBER (kib_completion_msg_t, ibcm_cookie);
        CHECK_MEMBER (kib_completion_msg_t, ibcm_status);

        CHECK_STRUCT (kib_msg_t);
        CHECK_MEMBER (kib_msg_t, ibm_magic);
        CHECK_MEMBER (kib_msg_t, ibm_version);
        CHECK_MEMBER (kib_msg_t, ibm_type);
        CHECK_MEMBER (kib_msg_t, ibm_credits);
        CHECK_MEMBER (kib_msg_t, ibm_nob);
        CHECK_MEMBER (kib_msg_t, ibm_cksum);
        CHECK_MEMBER (kib_msg_t, ibm_srcnid);
        CHECK_MEMBER (kib_msg_t, ibm_srcstamp);
        CHECK_MEMBER (kib_msg_t, ibm_dstnid);
        CHECK_MEMBER (kib_msg_t, ibm_dststamp);
        CHECK_MEMBER (kib_msg_t, ibm_seq);
        CHECK_MEMBER (kib_msg_t, ibm_u.connparams);
        CHECK_MEMBER (kib_msg_t, ibm_u.immediate);
        CHECK_MEMBER (kib_msg_t, ibm_u.putreq);
        CHECK_MEMBER (kib_msg_t, ibm_u.putack);
        CHECK_MEMBER (kib_msg_t, ibm_u.get);
        CHECK_MEMBER (kib_msg_t, ibm_u.completion);

        printf ("}\n\n");

        return (0);
}

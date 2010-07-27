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
 * Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lustre/ptlrpc/sec_lproc.c
 *
 * Author: Eric Mei <ericm@clusterfs.com>
 */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif
#define DEBUG_SUBSYSTEM S_SEC

#include <libcfs/libcfs.h>
#ifndef __KERNEL__
#include <liblustre.h>
#include <libcfs/list.h>
#else
#include <linux/crypto.h>
#endif

#include <obd.h>
#include <obd_class.h>
#include <obd_support.h>
#include <lustre_net.h>
#include <lustre_import.h>
#include <lustre_dlm.h>
#include <lustre_sec.h>

#include "ptlrpc_internal.h"

#ifdef __KERNEL__

libcfs_param_entry_t *sptlrpc_proc_root = NULL;
EXPORT_SYMBOL(sptlrpc_proc_root);

char *sec_flags2str(unsigned long flags, char *buf, int bufsize)
{
        buf[0] = '\0';

        if (flags & PTLRPC_SEC_FL_REVERSE)
                strncat(buf, "reverse,", bufsize);
        if (flags & PTLRPC_SEC_FL_ROOTONLY)
                strncat(buf, "rootonly,", bufsize);
        if (flags & PTLRPC_SEC_FL_UDESC)
                strncat(buf, "udesc,", bufsize);
        if (flags & PTLRPC_SEC_FL_BULK)
                strncat(buf, "bulk,", bufsize);
        if (buf[0] == '\0')
                strncat(buf, "-,", bufsize);

        buf[strlen(buf) - 1] = '\0';
        return buf;
}

static int sptlrpc_info_lprocfs_seq_show(libcfs_seq_file_t *seq, void *v)
{
        struct obd_device *dev = LIBCFS_SEQ_PRIVATE(seq);
        struct client_obd *cli = &dev->u.cli;
        struct ptlrpc_sec *sec = NULL;
        char               str[32];

        LASSERT(strcmp(dev->obd_type->typ_name, LUSTRE_OSC_NAME) == 0 ||
                strcmp(dev->obd_type->typ_name, LUSTRE_MDC_NAME) == 0 ||
                strcmp(dev->obd_type->typ_name, LUSTRE_MGC_NAME) == 0);

        if (cli->cl_import)
                sec = sptlrpc_import_sec_ref(cli->cl_import);
        if (sec == NULL)
                goto out;

        sec_flags2str(sec->ps_flvr.sf_flags, str, sizeof(str));

        LIBCFS_SEQ_PRINTF(seq, "rpc flavor:    %s\n",
                   sptlrpc_flavor2name_base(sec->ps_flvr.sf_rpc));
        LIBCFS_SEQ_PRINTF(seq, "bulk flavor:   %s\n",
                   sptlrpc_flavor2name_bulk(&sec->ps_flvr, str, sizeof(str)));
        LIBCFS_SEQ_PRINTF(seq, "flags:         %s\n",
                   sec_flags2str(sec->ps_flvr.sf_flags, str, sizeof(str)));
        LIBCFS_SEQ_PRINTF(seq, "id:            %d\n", sec->ps_id);
        LIBCFS_SEQ_PRINTF(seq, "refcount:      %d\n",
                          cfs_atomic_read(&sec->ps_refcount));
        LIBCFS_SEQ_PRINTF(seq, "nctx:          %d\n",
                          cfs_atomic_read(&sec->ps_nctx));
        LIBCFS_SEQ_PRINTF(seq, "gc internal    %ld\n", sec->ps_gc_interval);
        LIBCFS_SEQ_PRINTF(seq, "gc next        %ld\n",
                   sec->ps_gc_interval ?
                   sec->ps_gc_next - cfs_time_current_sec() : 0);

        sptlrpc_sec_put(sec);
out:
        return 0;
}
LPROC_SEQ_FOPS_RO(sptlrpc_info_lprocfs);

static int sptlrpc_ctxs_lprocfs_seq_show(libcfs_seq_file_t *seq, void *v)
{
        struct obd_device *dev = LIBCFS_SEQ_PRIVATE(seq);
        struct client_obd *cli = &dev->u.cli;
        struct ptlrpc_sec *sec = NULL;

        LASSERT(strcmp(dev->obd_type->typ_name, LUSTRE_OSC_NAME) == 0 ||
                strcmp(dev->obd_type->typ_name, LUSTRE_MDC_NAME) == 0 ||
                strcmp(dev->obd_type->typ_name, LUSTRE_MGC_NAME) == 0);

        if (cli->cl_import)
                sec = sptlrpc_import_sec_ref(cli->cl_import);
        if (sec == NULL)
                goto out;

        if (sec->ps_policy->sp_cops->display)
                sec->ps_policy->sp_cops->display(sec, seq);

        sptlrpc_sec_put(sec);
out:
        return 0;
}
LPROC_SEQ_FOPS_RO(sptlrpc_ctxs_lprocfs);

int sptlrpc_lprocfs_cliobd_attach(struct obd_device *dev)
{
        int     rc;

        if (strcmp(dev->obd_type->typ_name, LUSTRE_OSC_NAME) != 0 &&
            strcmp(dev->obd_type->typ_name, LUSTRE_MDC_NAME) != 0 &&
            strcmp(dev->obd_type->typ_name, LUSTRE_MGC_NAME) != 0) {
                CERROR("can't register lproc for obd type %s\n",
                       dev->obd_type->typ_name);
                return -EINVAL;
        }

        rc = lprocfs_obd_seq_create(dev, "srpc_info", 0444,
                                    &sptlrpc_info_lprocfs_fops, dev);
        if (rc) {
                CERROR("create proc entry srpc_info for %s: %d\n",
                       dev->obd_name, rc);
                return rc;
        }

        rc = lprocfs_obd_seq_create(dev, "srpc_contexts", 0444,
                                    &sptlrpc_ctxs_lprocfs_fops, dev);
        if (rc) {
                CERROR("create proc entry srpc_contexts for %s: %d\n",
                       dev->obd_name, rc);
                return rc;
        }

        return 0;
}
EXPORT_SYMBOL(sptlrpc_lprocfs_cliobd_attach);

static struct lprocfs_vars sptlrpc_lprocfs_vars[] = {
        { "encrypt_page_pools", sptlrpc_proc_read_enc_pool, NULL, NULL },
        { NULL }
};

int sptlrpc_lproc_init(void)
{
        int     rc;

        LASSERT(sptlrpc_proc_root == NULL);

        sptlrpc_proc_root = lprocfs_register("sptlrpc", proc_lustre_root,
                                             sptlrpc_lprocfs_vars, NULL);
        if (IS_ERR(sptlrpc_proc_root)) {
                rc = PTR_ERR(sptlrpc_proc_root);
                sptlrpc_proc_root = NULL;
                return rc;
        }
        lprocfs_put_lperef(sptlrpc_proc_root);
        return 0;
}

void sptlrpc_lproc_fini(void)
{
        if (sptlrpc_proc_root) {
                lprocfs_remove(&sptlrpc_proc_root);
                sptlrpc_proc_root = NULL;
        }
}

#else /* !__KERNEL__ */

int sptlrpc_lprocfs_cliobd_attach(struct obd_device *dev)
{
        return 0;
}

int sptlrpc_lproc_init(void)
{
        return 0;
}

void sptlrpc_lproc_fini(void)
{
}

#endif

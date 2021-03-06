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
 * lustre/fid/fid_handler.c
 *
 * Lustre Sequence Manager
 *
 * Author: Yury Umanets <umka@clusterfs.com>
 */

#ifndef EXPORT_SYMTAB
# define EXPORT_SYMTAB
#endif
#define DEBUG_SUBSYSTEM S_FID

#ifdef __KERNEL__
# include <libcfs/libcfs.h>
# include <linux/module.h>
#else /* __KERNEL__ */
# include <liblustre.h>
#endif

#include <obd.h>
#include <obd_class.h>
#include <lu_target.h>
#include <dt_object.h>
#include <md_object.h>
#include <obd_support.h>
#include <lustre_req_layout.h>
#include <lustre_fid.h>
#include "fid_internal.h"

#ifdef __KERNEL__
/* Assigns client to sequence controller node. */
int seq_server_set_cli(struct lu_server_seq *seq,
                       struct lu_client_seq *cli,
                       const struct lu_env *env)
{
        int rc = 0;
        ENTRY;

        /*
         * Ask client for new range, assign that range to ->seq_space and write
         * seq state to backing store should be atomic.
         */
        cfs_down(&seq->lss_sem);

        if (cli == NULL) {
                CDEBUG(D_INFO, "%s: Detached sequence client %s\n",
                       seq->lss_name, cli->lcs_name);
                seq->lss_cli = cli;
                GOTO(out_up, rc = 0);
        }

        if (seq->lss_cli != NULL) {
                CERROR("%s: Sequence controller is already "
                       "assigned\n", seq->lss_name);
                GOTO(out_up, rc = -EINVAL);
        }

        CDEBUG(D_INFO, "%s: Attached sequence controller %s\n",
               seq->lss_name, cli->lcs_name);

        seq->lss_cli = cli;
        cli->lcs_space.lsr_index = seq->lss_site->ms_node_id;
        EXIT;
out_up:
        cfs_up(&seq->lss_sem);
        return rc;
}
EXPORT_SYMBOL(seq_server_set_cli);
/*
 * allocate \a w units of sequence from range \a from.
 */
static inline void range_alloc(struct lu_seq_range *to,
                               struct lu_seq_range *from,
                               __u64 width)
{
        width = min(range_space(from), width);
        to->lsr_start = from->lsr_start;
        to->lsr_end = from->lsr_start + width;
        from->lsr_start += width;
}

/**
 * On controller node, allocate new super sequence for regular sequence server.
 * As this super sequence controller, this node suppose to maintain fld
 * and update index.
 * \a out range always has currect mds node number of requester.
 */

static int __seq_server_alloc_super(struct lu_server_seq *seq,
                                    struct lu_seq_range *out,
                                    const struct lu_env *env)
{
        struct lu_seq_range *space = &seq->lss_space;
        int rc;
        ENTRY;

        LASSERT(range_is_sane(space));

        if (range_is_exhausted(space)) {
                CERROR("%s: Sequences space is exhausted\n",
                       seq->lss_name);
                RETURN(-ENOSPC);
        } else {
                range_alloc(out, space, seq->lss_width);
        }

        rc = seq_store_update(env, seq, out, 1 /* sync */);

        CDEBUG(D_INFO, "%s: super-sequence allocation rc = %d "
               DRANGE"\n", seq->lss_name, rc, PRANGE(out));

        RETURN(rc);
}

int seq_server_alloc_super(struct lu_server_seq *seq,
                           struct lu_seq_range *out,
                           const struct lu_env *env)
{
        int rc;
        ENTRY;

        cfs_down(&seq->lss_sem);
        rc = __seq_server_alloc_super(seq, out, env);
        cfs_up(&seq->lss_sem);

        RETURN(rc);
}

static int __seq_set_init(const struct lu_env *env,
                            struct lu_server_seq *seq)
{
        struct lu_seq_range *space = &seq->lss_space;
        int rc;

        range_alloc(&seq->lss_lowater_set, space, seq->lss_set_width);
        range_alloc(&seq->lss_hiwater_set, space, seq->lss_set_width);

        rc = seq_store_update(env, seq, NULL, 1);
        seq->lss_set_transno = 0;

        return rc;
}

/*
 * This function implements new seq allocation algorithm using async
 * updates to seq file on disk. ref bug 18857 for details.
 * there are four variable to keep track of this process
 *
 * lss_space; - available lss_space
 * lss_lowater_set; - lu_seq_range for all seqs before barrier, i.e. safe to use
 * lss_hiwater_set; - lu_seq_range after barrier, i.e. allocated but may be
 *                    not yet committed
 *
 * when lss_lowater_set reaches the end it is replaced with hiwater one and
 * a write operation is initiated to allocate new hiwater range.
 * if last seq write opearion is still not commited, current operation is
 * flaged as sync write op.
 */
static int range_alloc_set(const struct lu_env *env,
                            struct lu_seq_range *out,
                            struct lu_server_seq *seq)
{
        struct lu_seq_range *space = &seq->lss_space;
        struct lu_seq_range *loset = &seq->lss_lowater_set;
        struct lu_seq_range *hiset = &seq->lss_hiwater_set;
        int rc = 0;

        if (range_is_zero(loset))
                __seq_set_init(env, seq);

        if (OBD_FAIL_CHECK(OBD_FAIL_SEQ_ALLOC)) /* exhaust set */
                loset->lsr_start = loset->lsr_end;

        if (range_is_exhausted(loset)) {
                /* reached high water mark. */
                struct lu_device *dev = seq->lss_site->ms_lu.ls_top_dev;
                struct lu_target *tg = dev->ld_obd->u.obt.obt_lut;
                int obd_num_clients = dev->ld_obd->obd_num_exports;
                __u64 set_sz;
                int sync = 0;

                /* calculate new seq width based on number of clients */
                set_sz = max(seq->lss_set_width,
                               obd_num_clients * seq->lss_width);
                set_sz = min(range_space(space), set_sz);

                /* Switch to hiwater range now */
                loset = hiset;
                /* allocate new hiwater range */
                range_alloc(hiset, space, set_sz);

                if (seq->lss_set_transno > dev->ld_obd->obd_last_committed)
                        sync = 1;

                /* update ondisk seq with new *space */
                rc = seq_store_update(env, seq, NULL, sync);

                /* set new hiwater transno */
                cfs_spin_lock(&tg->lut_translock);
                seq->lss_set_transno = tg->lut_last_transno;
                cfs_spin_unlock(&tg->lut_translock);
        }

        LASSERTF(!range_is_exhausted(loset) || range_is_sane(loset),
                 DRANGE"\n", PRANGE(loset));

        if (rc == 0)
                range_alloc(out, loset, seq->lss_width);

        RETURN(rc);
}

static int __seq_server_alloc_meta(struct lu_server_seq *seq,
                                   struct lu_seq_range *out,
                                   const struct lu_env *env)
{
        struct lu_seq_range *space = &seq->lss_space;
        int rc = 0;

        ENTRY;

        LASSERT(range_is_sane(space));

        /* Check if available space ends and allocate new super seq */
        if (range_is_exhausted(space)) {
                if (!seq->lss_cli) {
                        CERROR("%s: No sequence controller is attached.\n",
                               seq->lss_name);
                        RETURN(-ENODEV);
                }

                rc = seq_client_alloc_super(seq->lss_cli, env);
                if (rc) {
                        CERROR("%s: Can't allocate super-sequence, rc %d\n",
                               seq->lss_name, rc);
                        RETURN(rc);
                }

                /* Saving new range to allocation space. */
                *space = seq->lss_cli->lcs_space;
                LASSERT(range_is_sane(space));
        }

        rc = range_alloc_set(env, out, seq);
        if (rc == 0) {
                CDEBUG(D_INFO, "%s: Allocated meta-sequence "
                       DRANGE"\n", seq->lss_name, PRANGE(out));
        }

        RETURN(rc);
}

int seq_server_alloc_meta(struct lu_server_seq *seq,
                          struct lu_seq_range *out,
                          const struct lu_env *env)
{
        int rc;
        ENTRY;

        cfs_down(&seq->lss_sem);
        rc = __seq_server_alloc_meta(seq, out, env);
        cfs_up(&seq->lss_sem);

        RETURN(rc);
}
EXPORT_SYMBOL(seq_server_alloc_meta);

static int seq_server_handle(struct lu_site *site,
                             const struct lu_env *env,
                             __u32 opc, struct lu_seq_range *out)
{
        int rc;
        struct md_site *mite;
        ENTRY;

        mite = lu_site2md(site);
        switch (opc) {
        case SEQ_ALLOC_META:
                if (!mite->ms_server_seq) {
                        CERROR("Sequence server is not "
                               "initialized\n");
                        RETURN(-EINVAL);
                }
                rc = seq_server_alloc_meta(mite->ms_server_seq, out, env);
                break;
        case SEQ_ALLOC_SUPER:
                if (!mite->ms_control_seq) {
                        CERROR("Sequence controller is not "
                               "initialized\n");
                        RETURN(-EINVAL);
                }
                rc = seq_server_alloc_super(mite->ms_control_seq, out, env);
                break;
        default:
                rc = -EINVAL;
                break;
        }

        RETURN(rc);
}

static int seq_req_handle(struct ptlrpc_request *req,
                          const struct lu_env *env,
                          struct seq_thread_info *info)
{
        struct lu_seq_range *out, *tmp;
        struct lu_site *site;
        int rc = -EPROTO;
        __u32 *opc;
        ENTRY;

	LASSERT(!(lustre_msg_get_flags(req->rq_reqmsg) & MSG_REPLAY));
        site = req->rq_export->exp_obd->obd_lu_dev->ld_site;
        LASSERT(site != NULL);

        rc = req_capsule_server_pack(info->sti_pill);
        if (rc)
                RETURN(err_serious(rc));

        opc = req_capsule_client_get(info->sti_pill, &RMF_SEQ_OPC);
        if (opc != NULL) {
                out = req_capsule_server_get(info->sti_pill, &RMF_SEQ_RANGE);
                if (out == NULL)
                        RETURN(err_serious(-EPROTO));

                tmp = req_capsule_client_get(info->sti_pill, &RMF_SEQ_RANGE);

                /* seq client passed mdt id, we need to pass that using out
                 * range parameter */

                out->lsr_index = tmp->lsr_index;
                out->lsr_flags = tmp->lsr_flags;
                rc = seq_server_handle(site, env, *opc, out);
        } else
                rc = err_serious(-EPROTO);

        RETURN(rc);
}

/* context key constructor/destructor: seq_key_init, seq_key_fini */
LU_KEY_INIT_FINI(seq, struct seq_thread_info);

/* context key: seq_thread_key */
LU_CONTEXT_KEY_DEFINE(seq, LCT_MD_THREAD);

static void seq_thread_info_init(struct ptlrpc_request *req,
                                 struct seq_thread_info *info)
{
        info->sti_pill = &req->rq_pill;
        /* Init request capsule */
        req_capsule_init(info->sti_pill, req, RCL_SERVER);
        req_capsule_set(info->sti_pill, &RQF_SEQ_QUERY);
}

static void seq_thread_info_fini(struct seq_thread_info *info)
{
        req_capsule_fini(info->sti_pill);
}

static int seq_handle(struct ptlrpc_request *req)
{
        const struct lu_env *env;
        struct seq_thread_info *info;
        int rc;

        env = req->rq_svc_thread->t_env;
        LASSERT(env != NULL);

        info = lu_context_key_get(&env->le_ctx, &seq_thread_key);
        LASSERT(info != NULL);

        seq_thread_info_init(req, info);
        rc = seq_req_handle(req, env, info);
        /* XXX: we don't need replay but MDT assign transno in any case,
         * remove it manually before reply*/
        lustre_msg_set_transno(req->rq_repmsg, 0);
        seq_thread_info_fini(info);

        return rc;
}

/*
 * Entry point for handling FLD RPCs called from MDT.
 */
int seq_query(struct com_thread_info *info)
{
        return seq_handle(info->cti_pill->rc_req);
}
EXPORT_SYMBOL(seq_query);

static void seq_server_proc_fini(struct lu_server_seq *seq);

#ifdef LPROCFS
static int seq_server_proc_init(struct lu_server_seq *seq)
{
        int rc;
        ENTRY;

        seq->lss_proc_dir = lprocfs_register(seq->lss_name,
                                             seq_type_proc_dir,
                                             NULL, NULL);
        if (IS_ERR(seq->lss_proc_dir)) {
                rc = PTR_ERR(seq->lss_proc_dir);
                RETURN(rc);
        }

        rc = lprocfs_add_vars(seq->lss_proc_dir,
                              seq_server_proc_list, seq);
        if (rc) {
                CERROR("%s: Can't init sequence manager "
                       "proc, rc %d\n", seq->lss_name, rc);
                GOTO(out_cleanup, rc);
        }

        RETURN(0);

out_cleanup:
        seq_server_proc_fini(seq);
        return rc;
}

static void seq_server_proc_fini(struct lu_server_seq *seq)
{
        ENTRY;
        if (seq->lss_proc_dir != NULL) {
                if (!IS_ERR(seq->lss_proc_dir))
                        lprocfs_remove(&seq->lss_proc_dir);
                seq->lss_proc_dir = NULL;
        }
        EXIT;
}
#else
static int seq_server_proc_init(struct lu_server_seq *seq)
{
        return 0;
}

static void seq_server_proc_fini(struct lu_server_seq *seq)
{
        return;
}
#endif


int seq_server_init(struct lu_server_seq *seq,
                    struct dt_device *dev,
                    const char *prefix,
                    enum lu_mgr_type type,
                    struct md_site *ms,
                    const struct lu_env *env)
{
        int rc, is_srv = (type == LUSTRE_SEQ_SERVER);
        ENTRY;

        LASSERT(dev != NULL);
        LASSERT(prefix != NULL);

        seq->lss_cli = NULL;
        seq->lss_type = type;
        seq->lss_site = ms;
        range_init(&seq->lss_space);

        range_init(&seq->lss_lowater_set);
        range_init(&seq->lss_hiwater_set);
        seq->lss_set_width = LUSTRE_SEQ_BATCH_WIDTH;

        cfs_sema_init(&seq->lss_sem, 1);

        seq->lss_width = is_srv ?
                LUSTRE_SEQ_META_WIDTH : LUSTRE_SEQ_SUPER_WIDTH;

        snprintf(seq->lss_name, sizeof(seq->lss_name),
                 "%s-%s", (is_srv ? "srv" : "ctl"), prefix);

        rc = seq_store_init(seq, env, dev);
        if (rc)
                GOTO(out, rc);
        /* Request backing store for saved sequence info. */
        rc = seq_store_read(seq, env);
        if (rc == -ENODATA) {

                /* Nothing is read, init by default value. */
                seq->lss_space = is_srv ?
                        LUSTRE_SEQ_ZERO_RANGE:
                        LUSTRE_SEQ_SPACE_RANGE;

                seq->lss_space.lsr_index = ms->ms_node_id;
                CDEBUG(D_INFO, "%s: No data found "
                       "on store. Initialize space\n",
                       seq->lss_name);

                rc = seq_store_update(env, seq, NULL, 0);
                if (rc) {
                        CERROR("%s: Can't write space data, "
                               "rc %d\n", seq->lss_name, rc);
                }
        } else if (rc) {
                CERROR("%s: Can't read space data, rc %d\n",
                       seq->lss_name, rc);
                GOTO(out, rc);
        }

        if (is_srv) {
                LASSERT(range_is_sane(&seq->lss_space));
        } else {
                LASSERT(!range_is_zero(&seq->lss_space) &&
                        range_is_sane(&seq->lss_space));
        }

        rc  = seq_server_proc_init(seq);
        if (rc)
                GOTO(out, rc);

        EXIT;
out:
        if (rc)
                seq_server_fini(seq, env);
        return rc;
}
EXPORT_SYMBOL(seq_server_init);

void seq_server_fini(struct lu_server_seq *seq,
                     const struct lu_env *env)
{
        ENTRY;

        seq_server_proc_fini(seq);
        seq_store_fini(seq, env);

        EXIT;
}
EXPORT_SYMBOL(seq_server_fini);

cfs_proc_dir_entry_t *seq_type_proc_dir = NULL;

static struct lu_local_obj_desc llod_seq_srv = {
        .llod_name      = LUSTRE_SEQ_SRV_NAME,
        .llod_oid       = FID_SEQ_SRV_OID,
        .llod_is_index  = 0,
};

static struct lu_local_obj_desc llod_seq_ctl = {
        .llod_name      = LUSTRE_SEQ_CTL_NAME,
        .llod_oid       = FID_SEQ_CTL_OID,
        .llod_is_index  = 0,
};

static int __init fid_mod_init(void)
{
        seq_type_proc_dir = lprocfs_register(LUSTRE_SEQ_NAME,
                                             proc_lustre_root,
                                             NULL, NULL);
        if (IS_ERR(seq_type_proc_dir))
                return PTR_ERR(seq_type_proc_dir);

        llo_local_obj_register(&llod_seq_srv);
        llo_local_obj_register(&llod_seq_ctl);

        LU_CONTEXT_KEY_INIT(&seq_thread_key);
        lu_context_key_register(&seq_thread_key);
        return 0;
}

static void __exit fid_mod_exit(void)
{
        llo_local_obj_unregister(&llod_seq_srv);
        llo_local_obj_unregister(&llod_seq_ctl);

        lu_context_key_degister(&seq_thread_key);
        if (seq_type_proc_dir != NULL && !IS_ERR(seq_type_proc_dir)) {
                lprocfs_remove(&seq_type_proc_dir);
                seq_type_proc_dir = NULL;
        }
}

MODULE_AUTHOR("Sun Microsystems, Inc. <http://www.lustre.org/>");
MODULE_DESCRIPTION("Lustre FID Module");
MODULE_LICENSE("GPL");

cfs_module(fid, "0.1.0", fid_mod_init, fid_mod_exit);
#endif

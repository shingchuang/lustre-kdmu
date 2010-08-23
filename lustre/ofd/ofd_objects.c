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
 * Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lustre/ofd/ofd_objects.c
 *
 * Author: Alex Tomas <alex@clusterfs.com>
 * Author: Mike Pershin <tappro@sun.com>
 */


#define DEBUG_SUBSYSTEM S_FILTER

#include <libcfs/libcfs.h>
#include "ofd_internal.h"

struct filter_object *filter_object_find(const struct lu_env *env,
                                         struct filter_device *ofd,
                                         const struct lu_fid *fid)
{
        struct filter_object *fo;
        struct lu_object *o;
        ENTRY;

        o = lu_object_find(env, &ofd->ofd_dt_dev.dd_lu_dev, fid, NULL);
        if (likely(!IS_ERR(o)))
                fo = filter_obj(o);
        else
                fo = (struct filter_object *)o; /* return error */
        RETURN(fo);
}

struct filter_object *filter_object_find_or_create(const struct lu_env *env,
                                                   struct filter_device *ofd,
                                                   const struct lu_fid *fid,
                                                   struct lu_attr *attr)
{
        struct filter_object *fo;
        struct dt_object *next;
        struct thandle *th;
        struct dt_object_format dof;
        int rc;
        ENTRY;

        fo = filter_object_find(env, ofd, fid);
        if (IS_ERR(fo))
                RETURN(fo);

        LASSERT(fo != NULL);
        if (filter_object_exists(fo))
                RETURN(fo);

        dof.dof_type = dt_mode_to_dft(S_IFREG);

        next = filter_object_child(fo);
        LASSERT(next != NULL);

        th = filter_trans_create(env, ofd);
        if (IS_ERR(th))
                GOTO(out, rc = PTR_ERR(th));

        rc = dt_declare_create(env, next, attr, NULL, &dof, th);
        LASSERT(rc == 0);

        rc = filter_trans_start(env, ofd, th);
        if (rc)
                GOTO(trans_stop, rc);

        filter_write_lock(env, fo);
        if (filter_object_exists(fo))
                GOTO(unlock, rc = 0);

        CDEBUG(D_OTHER, "create new object %lu:%llu\n",
               (unsigned long) fid->f_oid, fid->f_seq);

        rc = dt_create(env, next, attr, NULL, &dof, th);
        LASSERT(rc == 0);
        LASSERT(filter_object_exists(fo));

unlock:
        filter_write_unlock(env, fo);

trans_stop:
        filter_trans_stop(env, ofd, th);
out:
        if (rc) {
                filter_object_put(env, fo);
                RETURN(ERR_PTR(rc));
        }
        RETURN(fo);
}

void filter_object_put(const struct lu_env *env, struct filter_object *fo)
{
        lu_object_put(env, &fo->ofo_obj.do_lu);
}

int filter_precreate_object(const struct lu_env *env, struct filter_device *ofd,
                            obd_id id, obd_seq group)
{
        struct dt_object_format  dof;
        struct filter_object    *fo;
        struct dt_object        *next;
        struct lu_attr           attr;
        struct thandle          *th;
        struct lu_buf            buf;
        struct lu_fid            fid;
        struct ost_id            ostid;
        obd_id                   tmp;
        loff_t                   off;
        int                      rc;

        ostid.oi_id = id;
        ostid.oi_seq = group;
        fid_ostid_unpack(&fid, &ostid, 0);

        fo = filter_object_find(env, ofd, &fid);
        if (IS_ERR(fo))
                RETURN(PTR_ERR(fo));

        attr.la_valid = LA_TYPE | LA_MODE;
        attr.la_mode = S_IFREG | S_ISUID | S_ISGID | 0666;
        dof.dof_type = dt_mode_to_dft(S_IFREG);

        next = filter_object_child(fo);
        LASSERT(next != NULL);

        buf.lb_buf = &tmp;
        buf.lb_len = sizeof(tmp);
        off = group * sizeof(tmp);

        th = filter_trans_create(env, ofd);
        if (IS_ERR(th))
                GOTO(out_unlock, rc = PTR_ERR(th));

        rc = dt_declare_create(env, next, &attr, NULL, &dof, th);
        if (rc)
                GOTO(trans_stop, rc);
        rc = dt_declare_attr_set(env, next, &attr, th);
        if (rc)
                GOTO(trans_stop, rc);

        rc = dt_declare_record_write(env, ofd->ofd_lastid_obj[group],
                                     sizeof(tmp), off, th);
        if (rc)
                GOTO(trans_stop, rc);

        rc = filter_trans_start(env, ofd, th);
        if (rc)
                GOTO(trans_stop, rc);

        filter_write_lock(env, fo);
        if (filter_object_exists(fo)) {
                /* underlying filesystem is broken - object must not exist */
                CERROR("object %u/"LPD64" exists: "DFID"\n",
                       (unsigned) group, id, PFID(&fid));
                GOTO(out_unlock, rc = -EEXIST);
        }

        CDEBUG(D_OTHER, "create new object %lu:%llu\n",
               (unsigned long) fid.f_oid, fid.f_seq);

        rc = dt_create(env, next, &attr, NULL, &dof, th);
        if (rc)
                GOTO(out_unlock, rc);
        LASSERT(filter_object_exists(fo));

        attr.la_valid &= ~LA_TYPE;
        rc = dt_attr_set(env, next, &attr, th, BYPASS_CAPA);
        if (rc)
                GOTO(out_unlock, rc);

        filter_last_id_set(ofd, id, group);

        rc = filter_last_id_write(env, ofd, group, th);

out_unlock:
        filter_write_unlock(env, fo);
trans_stop:
        filter_trans_stop(env, ofd, th);
        filter_object_put(env, fo);
        RETURN(rc);
}

int filter_attr_set(const struct lu_env *env, struct filter_object *fo,
                    const struct lu_attr *la)
{
        struct thandle *th;
        struct filter_device *ofd = filter_obj2dev(fo);
        struct filter_thread_info *info = filter_info(env);
        struct filter_mod_data *fmd;
        int rc;
        ENTRY;

        if (la->la_valid & (LA_ATIME | LA_MTIME | LA_CTIME)) {
                fmd = filter_fmd_get(info->fti_exp, &fo->ofo_header.loh_fid);
                if (fmd && fmd->fmd_mactime_xid < info->fti_xid)
                        fmd->fmd_mactime_xid = info->fti_xid;
                filter_fmd_put(info->fti_exp, fmd);
        }

        th = filter_trans_create(env, ofd);
        if (IS_ERR(th))
                RETURN(PTR_ERR(th));

        rc = dt_declare_attr_set(env, filter_object_child(fo), la, th);
        LASSERT(rc == 0);

        rc = filter_trans_start(env, ofd, th);
        if (rc)
                RETURN(rc);

        rc = dt_attr_set(env, filter_object_child(fo), la, th,
                        filter_object_capa(env, fo));

        filter_trans_stop(env, ofd, th);

        RETURN(rc);
}

int filter_object_punch(const struct lu_env *env, struct filter_object *fo,
                        __u64 start, __u64 end, struct obdo *oa)
{
        struct filter_thread_info *info = filter_info(env);
        struct filter_device      *ofd = filter_obj2dev(fo);
        struct filter_mod_data    *fmd;
        struct dt_object          *dob = filter_object_child(fo);
        struct thandle            *th;
        struct lu_attr             attr;
        int rc;
        ENTRY;

        /* we support truncate, not punch yet */
        LASSERT(end == OBD_OBJECT_EOF);

        fmd = filter_fmd_get(info->fti_exp, &fo->ofo_header.loh_fid);
        if (fmd && fmd->fmd_mactime_xid < info->fti_xid)
                fmd->fmd_mactime_xid = info->fti_xid;
        filter_fmd_put(info->fti_exp, fmd);

        la_from_obdo(&attr, oa, OBD_MD_FLMTIME | OBD_MD_FLATIME | OBD_MD_FLCTIME);
        attr.la_size = start;
        attr.la_valid |= LA_SIZE;
        attr.la_valid &= ~LA_TYPE;

        filter_write_lock(env, fo);

        th = filter_trans_create(env, ofd);
        if (IS_ERR(th))
                GOTO(unlock, rc = PTR_ERR(th));

        rc = dt_declare_attr_set(env, dob, &attr, th);
        if (rc)
                GOTO(stop, rc);

        rc = dt_declare_punch(env, dob, start, OBD_OBJECT_EOF, th);
        if (rc)
                GOTO(stop, rc);

        rc = filter_trans_start(env, ofd, th);
        if (rc)
                GOTO(unlock, rc);

        rc = dt_punch(env, dob, start, OBD_OBJECT_EOF, th,
                      filter_object_capa(env, fo));
        if (rc)
                GOTO(unlock, rc);

        rc = dt_attr_set(env, dob, &attr, th, filter_object_capa(env, fo));

stop:
        filter_trans_stop(env, ofd, th);

unlock:
        filter_write_unlock(env, fo);

        RETURN(rc);

}

int filter_object_destroy(const struct lu_env *env, struct filter_object *fo)
{
        struct thandle *th;
        int rc = 0;
        ENTRY;

        th = filter_trans_create(env, filter_obj2dev(fo));
        if (IS_ERR(th))
                RETURN(PTR_ERR(th));
        dt_declare_ref_del(env, filter_object_child(fo), th);
        rc = filter_trans_start(env, filter_obj2dev(fo), th);
        if (rc)
                RETURN(rc);

        filter_fmd_drop(filter_info(env)->fti_exp, &fo->ofo_header.loh_fid);

        filter_write_lock(env, fo);
        dt_ref_del(env, filter_object_child(fo), th);
        filter_write_unlock(env, fo);

        filter_trans_stop(env, filter_obj2dev(fo), th);

        RETURN(rc);
}

int filter_attr_get(const struct lu_env *env, struct filter_object *fo,
                    struct lu_attr *la)
{
        int rc = 0;

        /* CROW allow object to don't exist */
        if (filter_object_exists(fo)) {
                rc = dt_attr_get(env, filter_object_child(fo), la,
                                 filter_object_capa(env, fo));
        } else {
                la->la_size = 0;
                la->la_blocks = 0;
                la->la_atime = 0;
                la->la_ctime = 0;
                la->la_mtime = 0;
                la->la_valid = LA_SIZE | LA_BLOCKS |
                               LA_ATIME | LA_CTIME | LA_MTIME;
        }

        return rc;
}
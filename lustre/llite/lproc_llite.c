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
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 */
#define DEBUG_SUBSYSTEM S_LLITE

#include <linux/version.h>
#include <lustre_lite.h>
#include <lprocfs_status.h>
#include <linux/seq_file.h>
#include <obd_support.h>

#include "llite_internal.h"

struct libcfs_param_entry *proc_lustre_fs_root;

#ifdef __KERNEL__
/* /proc/lustre/llite mount point registration */
extern libcfs_file_ops_t vvp_dump_pgcache_file_ops;
libcfs_file_ops_t ll_rw_extents_stats_fops;
libcfs_file_ops_t ll_rw_extents_stats_pp_fops;
libcfs_file_ops_t ll_rw_offset_stats_fops;

static int ll_rd_blksize(char *page, char **start, off_t off, int count,
                         int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc)
              rc = libcfs_param_snprintf(page, count, data, LP_U32,
                                         "%u\n", osfs.os_bsize);

        return rc;
}

static int ll_rd_kbytestotal(char *page, char **start, off_t off, int count,
                             int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc) {
                __u32 blk_size = osfs.os_bsize >> 10;
                __u64 result = osfs.os_blocks;

                while (blk_size >>= 1)
                        result <<= 1;

                rc = libcfs_param_snprintf(page, count, data, LP_U64,
                                           LPU64"\n", result);
        }
        return rc;

}

static int ll_rd_kbytesfree(char *page, char **start, off_t off, int count,
                            int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc) {
                __u32 blk_size = osfs.os_bsize >> 10;
                __u64 result = osfs.os_bfree;

                while (blk_size >>= 1)
                        result <<= 1;

                rc = libcfs_param_snprintf(page, count, data, LP_U64,
                                           LPU64"\n", result);
        }
        return rc;
}

static int ll_rd_kbytesavail(char *page, char **start, off_t off, int count,
                             int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc) {
                __u32 blk_size = osfs.os_bsize >> 10;
                __u64 result = osfs.os_bavail;

                while (blk_size >>= 1)
                        result <<= 1;

                rc = libcfs_param_snprintf(page, count, data, LP_U64,
                                           LPU64"\n", result);
        }
        return rc;
}

static int ll_rd_filestotal(char *page, char **start, off_t off, int count,
                            int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc)
                rc = libcfs_param_snprintf(page, count, data, LP_U64,
                                           LPU64"\n", osfs.os_files);
        return rc;
}

static int ll_rd_filesfree(char *page, char **start, off_t off, int count,
                           int *eof, void *data)
{
        struct super_block *sb;
        struct obd_statfs osfs;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);
        rc = ll_statfs_internal(sb, &osfs, cfs_time_current_64() - CFS_HZ,
                                OBD_STATFS_NODELAY);
        if (!rc)
                rc = libcfs_param_snprintf(page, count, data, LP_U64,
                                           LPU64"\n", osfs.os_ffree);
        return rc;

}

static int ll_rd_client_type(char *page, char **start, off_t off, int count,
                            int *eof, void *data)
{
        struct ll_sb_info *sbi;
        int rc;

        LIBCFS_PARAM_GET_DATA(sbi, data, NULL);
        LASSERT(sbi != NULL);
        if (sbi->ll_flags & LL_SBI_RMT_CLIENT)
                rc = libcfs_param_snprintf(page, count, data, LP_STR,
                                           "%s", "remote client\n");
        else
                rc = libcfs_param_snprintf(page, count, data, LP_STR,
                                           "%s", "local client\n");

        return rc;
}

static int ll_rd_fstype(char *page, char **start, off_t off, int count,
                        int *eof, void *data)
{
        struct super_block *sb;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);

        return libcfs_param_snprintf(page, count, data, LP_STR,
                                     "%s\n", sb->s_type->name);
}

static int ll_rd_sb_uuid(char *page, char **start, off_t off, int count,
                         int *eof, void *data)
{
        struct super_block *sb;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        LASSERT(sb != NULL);

        return libcfs_param_snprintf(page, count, data, LP_STR, "%s\n",
                                     ll_s2sbi(sb)->ll_sb_uuid.uuid);
}

static int ll_rd_site_stats(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
        struct super_block *sb;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        /*
         * See description of statistical counters in struct cl_site, and
         * struct lu_site.
         */
        rc = cl_site_stats_print(lu2cl_site(ll_s2sbi(sb)->ll_site),
                                 page, count);
        rc = libcfs_param_snprintf(page, count, data, LP_STR, NULL, NULL);

        return rc;
}

static int ll_rd_max_readahead_mb(char *page, char **start, off_t off,
                                   int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        unsigned long pages_number;
        int mult;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        cfs_spin_lock(&sbi->ll_lock);
        pages_number = sbi->ll_ra_info.ra_max_pages;
        cfs_spin_unlock(&sbi->ll_lock);

        mult = 1 << (20 - PAGE_CACHE_SHIFT);
        rc = lprocfs_read_frac_helper(page, count, pages_number, mult);
        if (rc > 0)
                rc = libcfs_param_snprintf(page, count, data, LP_DB,
                                           NULL, NULL);

        return rc;
}

static int ll_wr_max_readahead_mb(libcfs_file_t *file, const char *buffer,
                                   unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int mult, rc, pages_number, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_write_frac_helper(buffer, count, &pages_number, mult, flag);
        if (rc)
                return rc;

        if (pages_number < 0 || pages_number > cfs_num_physpages / 2) {
                CERROR("can't set file readahead more than %lu MB\n",
                       cfs_num_physpages >> (20 - CFS_PAGE_SHIFT + 1)); /*1/2 of RAM*/
                return -ERANGE;
        }

        cfs_spin_lock(&sbi->ll_lock);
        sbi->ll_ra_info.ra_max_pages = pages_number;
        cfs_spin_unlock(&sbi->ll_lock);

        return count;
}

static int ll_rd_max_readahead_per_file_mb(char *page, char **start, off_t off,
                                           int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        unsigned long pages_number;
        int mult;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        cfs_spin_lock(&sbi->ll_lock);
        pages_number = sbi->ll_ra_info.ra_max_pages_per_file;
        cfs_spin_unlock(&sbi->ll_lock);

        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_read_frac_helper(page, count, pages_number, mult);
        if (rc > 0)
                rc = libcfs_param_snprintf(page, count, data, LP_DB,
                                           NULL, NULL);

        return rc;
}

static int ll_wr_max_readahead_per_file_mb(libcfs_file_t *file, const char *buffer,
                                          unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int mult, rc, pages_number, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_write_frac_helper(buffer, count, &pages_number, mult, flag);
        if (rc)
                return rc;

        if (pages_number < 0 ||
                pages_number > sbi->ll_ra_info.ra_max_pages) {
                CERROR("can't set file readahead more than"
                       "max_read_ahead_mb %lu MB\n",
                       sbi->ll_ra_info.ra_max_pages);
                return -ERANGE;
        }

        cfs_spin_lock(&sbi->ll_lock);
        sbi->ll_ra_info.ra_max_pages_per_file = pages_number;
        cfs_spin_unlock(&sbi->ll_lock);

        return count;
}

static int ll_rd_max_read_ahead_whole_mb(char *page, char **start, off_t off,
                                         int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        unsigned long pages_number;
        int mult;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        cfs_spin_lock(&sbi->ll_lock);
        pages_number = sbi->ll_ra_info.ra_max_read_ahead_whole_pages;
        cfs_spin_unlock(&sbi->ll_lock);

        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_read_frac_helper(page, count, pages_number, mult);
        if (rc > 0)
                rc = libcfs_param_snprintf(page, count, data, LP_DB,
                                           NULL, NULL);

        return rc;
}

static int ll_wr_max_read_ahead_whole_mb(libcfs_file_t *file,
                                         const char *buffer,
                                         unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int mult, rc, pages_number, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_write_frac_helper(buffer, count, &pages_number, mult,
                                       flag);
        if (rc)
                return rc;

        /* Cap this at the current max readahead window size, the readahead
         * algorithm does this anyway so it's pointless to set it larger. */
        if (pages_number < 0 ||
            pages_number > sbi->ll_ra_info.ra_max_pages_per_file) {
                CERROR("can't set max_read_ahead_whole_mb more than "
                       "max_read_ahead_per_file_mb: %lu\n",
                        sbi->ll_ra_info.ra_max_pages_per_file >> (20 - CFS_PAGE_SHIFT));
                return -ERANGE;
        }

        cfs_spin_lock(&sbi->ll_lock);
        sbi->ll_ra_info.ra_max_read_ahead_whole_pages = pages_number;
        cfs_spin_unlock(&sbi->ll_lock);

        return count;
}

static int ll_rd_max_cached_mb(char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        unsigned long pages_number;
        int mult;
        int rc;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        cfs_spin_lock(&sbi->ll_lock);
        pages_number = sbi->ll_async_page_max;
        cfs_spin_unlock(&sbi->ll_lock);

        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_read_frac_helper(page, count, pages_number, mult);
        if (rc > 0)
                rc = libcfs_param_snprintf(page, count, data, LP_DB,
                                           NULL, NULL);

        return rc;
}

static int ll_wr_max_cached_mb(libcfs_file_t *file, const char *buffer,
                               unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int mult, rc, pages_number, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        mult = 1 << (20 - CFS_PAGE_SHIFT);
        rc = lprocfs_write_frac_helper(buffer, count, &pages_number, mult,flag);
        if (rc)
                return rc;

        if (pages_number < 0 || pages_number > cfs_num_physpages) {
                CERROR("can't set max cache more than %lu MB\n",
                        cfs_num_physpages >> (20 - CFS_PAGE_SHIFT));
                return -ERANGE;
        }

        cfs_spin_lock(&sbi->ll_lock);
        sbi->ll_async_page_max = pages_number ;
        cfs_spin_unlock(&sbi->ll_lock);

        if (!sbi->ll_dt_exp)
                /* Not set up yet, don't call llap_shrink_cache */
                return count;

        return count;
}

static int ll_rd_checksum(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int temp;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        temp = (sbi->ll_flags & LL_SBI_CHECKSUM) ? 1 : 0;

        return libcfs_param_snprintf(page, count, data, LP_U32, "%d\n", temp);
}

static int ll_wr_checksum(libcfs_file_t *file, const char *buffer,
                          unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int val, rc, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        if (!sbi->ll_dt_exp)
                /* Not set up yet */
                return -EAGAIN;

        rc = lprocfs_write_helper(buffer, count, &val, flag);
        if (rc)
                return rc;
        if (val)
                sbi->ll_flags |= LL_SBI_CHECKSUM;
        else
                sbi->ll_flags &= ~LL_SBI_CHECKSUM;

        rc = obd_set_info_async(sbi->ll_dt_exp, sizeof(KEY_CHECKSUM),
                                KEY_CHECKSUM, sizeof(val), &val, NULL);
        if (rc)
                CWARN("Failed to set OSC checksum flags: %d\n", rc);

        return count;
}

static int ll_rd_max_rw_chunk(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
        struct super_block *sb;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);

        return libcfs_param_snprintf(page, count, data, LP_U32, "%lu\n",
                                     ll_s2sbi(sb)->ll_max_rw_chunk);
}

static int ll_wr_max_rw_chunk(libcfs_file_t *file, const char *buffer,
                          unsigned long count, void *data)
{
        struct super_block *sb;
        int rc, val;
        int flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        rc = lprocfs_write_helper(buffer, count, &val, flag);
        if (rc)
                return rc;
        ll_s2sbi(sb)->ll_max_rw_chunk = val;
        return count;
}

static int ll_rd_track_id(char *page, int count, void *data,
                          enum stats_track_type type)
{
        struct super_block *sb;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        if (ll_s2sbi(sb)->ll_stats_track_type == type)
                return libcfs_param_snprintf(page, count, data, LP_D32, "%d\n",
                                             ll_s2sbi(sb)->ll_stats_track_id);
        if (ll_s2sbi(sb)->ll_stats_track_type == STATS_TRACK_ALL)
                return libcfs_param_snprintf(page, count, data, LP_STR,
                                             "%s\n", "0 (all)");
        return libcfs_param_snprintf(page, count, data, LP_STR,
                                     "%s\n", "untracked");
}

static int ll_wr_track_id(const char *buffer, unsigned long count, void *data,
                          enum stats_track_type type)
{
        struct super_block *sb;
        int rc, pid, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        rc = lprocfs_write_helper(buffer, count, &pid, flag);
        if (rc)
                return rc;
        ll_s2sbi(sb)->ll_stats_track_id = pid;
        if (pid == 0)
                ll_s2sbi(sb)->ll_stats_track_type = STATS_TRACK_ALL;
        else
                ll_s2sbi(sb)->ll_stats_track_type = type;
        lprocfs_clear_stats(ll_s2sbi(sb)->ll_stats);
        return count;
}

static int ll_rd_track_pid(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
        return (ll_rd_track_id(page, count, data, STATS_TRACK_PID));
}

static int ll_wr_track_pid(libcfs_file_t *file, const char *buffer,
                          unsigned long count, void *data)
{
        return (ll_wr_track_id(buffer, count, data, STATS_TRACK_PID));
}

static int ll_rd_track_ppid(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
        return (ll_rd_track_id(page, count, data, STATS_TRACK_PPID));
}

static int ll_wr_track_ppid(libcfs_file_t *file, const char *buffer,
                          unsigned long count, void *data)
{
        return (ll_wr_track_id(buffer, count, data, STATS_TRACK_PPID));
}

static int ll_rd_track_gid(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
        return (ll_rd_track_id(page, count, data, STATS_TRACK_GID));
}

static int ll_wr_track_gid(libcfs_file_t *file, const char *buffer,
                          unsigned long count, void *data)
{
        return (ll_wr_track_id(buffer, count, data, STATS_TRACK_GID));
}

static int ll_rd_statahead_max(char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);

        return libcfs_param_snprintf(page, count, data, LP_U32,
                                     "%u\n", sbi->ll_sa_max);
}

static int ll_wr_statahead_max(libcfs_file_t *file, const char *buffer,
                               unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int val, rc, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);
        rc = lprocfs_write_helper(buffer, count, &val, flag);
        if (rc)
                return rc;

        if (val >= 0 && val <= LL_SA_RPC_MAX)
                sbi->ll_sa_max = val;
        else
                CERROR("Bad statahead_max value %d. Valid values are in the "
                       "range [0, %d]\n", val, LL_SA_RPC_MAX);

        return count;
}

static int ll_rd_statahead_stats(char *page, char **start, off_t off,
                                 int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        return libcfs_param_snprintf(page, count, data, LP_STR,
                                     "statahead total: %u\n"
                                     "statahead wrong: %u\n",
                                     atomic_read(&sbi->ll_sa_total),
                                     atomic_read(&sbi->ll_sa_wrong));
}

static int ll_rd_lazystatfs(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int temp;

        LIBCFS_PARAM_GET_DATA(sb, data, NULL);
        sbi = ll_s2sbi(sb);
        temp = (sbi->ll_flags & LL_SBI_LAZYSTATFS) ? 1 : 0;

        return libcfs_param_snprintf(page, count, data, LP_D32, "%d\n", temp);
}

static int ll_wr_lazystatfs(libcfs_file_t *file, const char *buffer,
                            unsigned long count, void *data)
{
        struct super_block *sb;
        struct ll_sb_info *sbi;
        int val, rc, flag = 0;

        LIBCFS_PARAM_GET_DATA(sb, data, &flag);
        sbi = ll_s2sbi(sb);

        rc = lprocfs_write_helper(buffer, count, &val, flag);
        if (rc)
                return rc;

        if (val)
                sbi->ll_flags |= LL_SBI_LAZYSTATFS;
        else
                sbi->ll_flags &= ~LL_SBI_LAZYSTATFS;

        return count;
}

static struct lprocfs_vars lprocfs_llite_obd_vars[] = {
        { "uuid",         ll_rd_sb_uuid,          0, 0 },
        //{ "mntpt_path",   ll_rd_path,             0, 0 },
        { "fstype",       ll_rd_fstype,           0, 0 },
        { "site",         ll_rd_site_stats,       0, 0 },
        { "blocksize",    ll_rd_blksize,          0, 0 },
        { "kbytestotal",  ll_rd_kbytestotal,      0, 0 },
        { "kbytesfree",   ll_rd_kbytesfree,       0, 0 },
        { "kbytesavail",  ll_rd_kbytesavail,      0, 0 },
        { "filestotal",   ll_rd_filestotal,       0, 0 },
        { "filesfree",    ll_rd_filesfree,        0, 0 },
        { "client_type",  ll_rd_client_type,      0, 0 },
        //{ "filegroups",   lprocfs_rd_filegroups,  0, 0 },
        { "max_read_ahead_mb", ll_rd_max_readahead_mb,
                               ll_wr_max_readahead_mb, 0 },
        { "max_read_ahead_per_file_mb", ll_rd_max_readahead_per_file_mb,
                                        ll_wr_max_readahead_per_file_mb, 0 },
        { "max_read_ahead_whole_mb", ll_rd_max_read_ahead_whole_mb,
                                     ll_wr_max_read_ahead_whole_mb, 0 },
        { "max_cached_mb",    ll_rd_max_cached_mb, ll_wr_max_cached_mb, 0 },
        { "checksum_pages",   ll_rd_checksum, ll_wr_checksum, 0 },
        { "max_rw_chunk",     ll_rd_max_rw_chunk, ll_wr_max_rw_chunk, 0 },
        { "stats_track_pid",  ll_rd_track_pid, ll_wr_track_pid, 0 },
        { "stats_track_ppid", ll_rd_track_ppid, ll_wr_track_ppid, 0 },
        { "stats_track_gid",  ll_rd_track_gid, ll_wr_track_gid, 0 },
        { "statahead_max",    ll_rd_statahead_max, ll_wr_statahead_max, 0 },
        { "statahead_stats",  ll_rd_statahead_stats, 0, 0 },
        { "lazystatfs",         ll_rd_lazystatfs, ll_wr_lazystatfs, 0 },
        { 0 }
};

#define MAX_STRING_SIZE 128

struct llite_file_opcode {
        __u32       opcode;
        __u32       type;
        const char *opname;
} llite_opcode_table[LPROC_LL_FILE_OPCODES] = {
        /* file operation */
        { LPROC_LL_DIRTY_HITS,     LPROCFS_TYPE_REGS, "dirty_pages_hits" },
        { LPROC_LL_DIRTY_MISSES,   LPROCFS_TYPE_REGS, "dirty_pages_misses" },
        { LPROC_LL_WB_WRITEPAGE,   LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "writeback_from_writepage" },
        { LPROC_LL_WB_PRESSURE,    LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "writeback_from_pressure" },
        { LPROC_LL_WB_OK,          LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "writeback_ok_pages" },
        { LPROC_LL_WB_FAIL,        LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "writeback_failed_pages" },
        { LPROC_LL_READ_BYTES,     LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_BYTES,
                                   "read_bytes" },
        { LPROC_LL_WRITE_BYTES,    LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_BYTES,
                                   "write_bytes" },
        { LPROC_LL_BRW_READ,       LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "brw_read" },
        { LPROC_LL_BRW_WRITE,      LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "brw_write" },

        { LPROC_LL_IOCTL,          LPROCFS_TYPE_REGS, "ioctl" },
        { LPROC_LL_OPEN,           LPROCFS_TYPE_REGS, "open" },
        { LPROC_LL_RELEASE,        LPROCFS_TYPE_REGS, "close" },
        { LPROC_LL_MAP,            LPROCFS_TYPE_REGS, "mmap" },
        { LPROC_LL_LLSEEK,         LPROCFS_TYPE_REGS, "seek" },
        { LPROC_LL_FSYNC,          LPROCFS_TYPE_REGS, "fsync" },
        /* inode operation */
        { LPROC_LL_SETATTR,        LPROCFS_TYPE_REGS, "setattr" },
        { LPROC_LL_TRUNC,          LPROCFS_TYPE_REGS, "truncate" },
        { LPROC_LL_LOCKLESS_TRUNC, LPROCFS_TYPE_REGS, "lockless_truncate"},
        { LPROC_LL_FLOCK,          LPROCFS_TYPE_REGS, "flock" },
        { LPROC_LL_GETATTR,        LPROCFS_TYPE_REGS, "getattr" },
        /* special inode operation */
        { LPROC_LL_STAFS,          LPROCFS_TYPE_REGS, "statfs" },
        { LPROC_LL_ALLOC_INODE,    LPROCFS_TYPE_REGS, "alloc_inode" },
        { LPROC_LL_SETXATTR,       LPROCFS_TYPE_REGS, "setxattr" },
        { LPROC_LL_GETXATTR,       LPROCFS_TYPE_REGS, "getxattr" },
        { LPROC_LL_LISTXATTR,      LPROCFS_TYPE_REGS, "listxattr" },
        { LPROC_LL_REMOVEXATTR,    LPROCFS_TYPE_REGS, "removexattr" },
        { LPROC_LL_INODE_PERM,     LPROCFS_TYPE_REGS, "inode_permission" },
        { LPROC_LL_DIRECT_READ,    LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "direct_read" },
        { LPROC_LL_DIRECT_WRITE,   LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_PAGES,
                                   "direct_write" },
        { LPROC_LL_LOCKLESS_READ,  LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_BYTES,
                                   "lockless_read_bytes" },
        { LPROC_LL_LOCKLESS_WRITE, LPROCFS_CNTR_AVGMINMAX|LPROCFS_TYPE_BYTES,
                                   "lockless_write_bytes" },

};

void ll_stats_ops_tally(struct ll_sb_info *sbi, int op, int count)
{
        if (!sbi->ll_stats)
                return;
        if (sbi->ll_stats_track_type == STATS_TRACK_ALL)
                lprocfs_counter_add(sbi->ll_stats, op, count);
        else if (sbi->ll_stats_track_type == STATS_TRACK_PID &&
                 sbi->ll_stats_track_id == current->pid)
                lprocfs_counter_add(sbi->ll_stats, op, count);
        else if (sbi->ll_stats_track_type == STATS_TRACK_PPID &&
                 sbi->ll_stats_track_id == current->parent->pid)
                lprocfs_counter_add(sbi->ll_stats, op, count);
        else if (sbi->ll_stats_track_type == STATS_TRACK_GID &&
                 sbi->ll_stats_track_id == cfs_curproc_gid())
                lprocfs_counter_add(sbi->ll_stats, op, count);
}
EXPORT_SYMBOL(ll_stats_ops_tally);

static const char *ra_stat_string[] = {
        [RA_STAT_HIT] = "hits",
        [RA_STAT_MISS] = "misses",
        [RA_STAT_DISTANT_READPAGE] = "readpage not consecutive",
        [RA_STAT_MISS_IN_WINDOW] = "miss inside window",
        [RA_STAT_FAILED_GRAB_PAGE] = "failed grab_cache_page",
        [RA_STAT_FAILED_MATCH] = "failed lock match",
        [RA_STAT_DISCARDED] = "read but discarded",
        [RA_STAT_ZERO_LEN] = "zero length file",
        [RA_STAT_ZERO_WINDOW] = "zero size window",
        [RA_STAT_EOF] = "read-ahead to EOF",
        [RA_STAT_MAX_IN_FLIGHT] = "hit max r-a issue",
        [RA_STAT_WRONG_GRAB_PAGE] = "wrong page from grab_cache_page",
};

int lprocfs_register_mountpoint(void *parent,
                                struct super_block *sb, char *osc, char *mdc)
{
        struct lprocfs_vars lvars[2];
        struct lustre_sb_info *lsi = s2lsi(sb);
        struct ll_sb_info *sbi = ll_s2sbi(sb);
        struct obd_device *obd;
        char name[MAX_STRING_SIZE + 1], *ptr;
        int err, id, len, rc;
        ENTRY;

        memset(lvars, 0, sizeof(lvars));

        name[MAX_STRING_SIZE] = '\0';
        lvars[0].name = name;

        LASSERT(sbi != NULL);
        LASSERT(mdc != NULL);
        LASSERT(osc != NULL);

        /* Get fsname */
        len = strlen(lsi->lsi_lmd->lmd_profile);
        ptr = strrchr(lsi->lsi_lmd->lmd_profile, '-');
        if (ptr && (strcmp(ptr, "-client") == 0))
                len -= 7;

        /* Mount info */
        snprintf(name, MAX_STRING_SIZE, "%.*s-%p", len,
                 lsi->lsi_lmd->lmd_profile, sb);

        sbi->ll_proc_root = lprocfs_register(name, parent, NULL, NULL);
        if (IS_ERR(sbi->ll_proc_root)) {
                err = PTR_ERR(sbi->ll_proc_root);
                sbi->ll_proc_root = NULL;
                RETURN(err);
        }

        rc = lprocfs_seq_create(sbi->ll_proc_root, "dump_page_cache", 0444,
                                &vvp_dump_pgcache_file_ops, sbi);
        if (rc)
                CWARN("Error adding the dump_page_cache file\n");

        rc = lprocfs_seq_create(sbi->ll_proc_root, "extents_stats", 0644,
                                &ll_rw_extents_stats_fops, sbi);
        if (rc)
                CWARN("Error adding the extent_stats file\n");

        rc = lprocfs_seq_create(sbi->ll_proc_root, "extents_stats_per_process",
                                0644, &ll_rw_extents_stats_pp_fops, sbi);
        if (rc)
                CWARN("Error adding the extents_stats_per_process file\n");

        rc = lprocfs_seq_create(sbi->ll_proc_root, "offset_stats", 0644,
                                &ll_rw_offset_stats_fops, sbi);
        if (rc)
                CWARN("Error adding the offset_stats file\n");

        /* File operations stats */
        sbi->ll_stats = lprocfs_alloc_stats(LPROC_LL_FILE_OPCODES,
                                            LPROCFS_STATS_FLAG_NONE);
        if (sbi->ll_stats == NULL)
                GOTO(out, err = -ENOMEM);
        /* do counter init */
        for (id = 0; id < LPROC_LL_FILE_OPCODES; id++) {
                __u32 type = llite_opcode_table[id].type;
                void *ptr = NULL;
                if (type & LPROCFS_TYPE_REGS)
                        ptr = "regs";
                else if (type & LPROCFS_TYPE_BYTES)
                        ptr = "bytes";
                else if (type & LPROCFS_TYPE_PAGES)
                        ptr = "pages";
                lprocfs_counter_init(sbi->ll_stats,
                                     llite_opcode_table[id].opcode,
                                     (type & LPROCFS_CNTR_AVGMINMAX),
                                     llite_opcode_table[id].opname, ptr);
        }
        err = lprocfs_register_stats(sbi->ll_proc_root, "stats", sbi->ll_stats);
        if (err)
                GOTO(out, err);

        sbi->ll_ra_stats = lprocfs_alloc_stats(ARRAY_SIZE(ra_stat_string),
                                               LPROCFS_STATS_FLAG_NONE);
        if (sbi->ll_ra_stats == NULL)
                GOTO(out, err = -ENOMEM);

        for (id = 0; id < ARRAY_SIZE(ra_stat_string); id++)
                lprocfs_counter_init(sbi->ll_ra_stats, id, 0,
                                     ra_stat_string[id], "pages");
        err = lprocfs_register_stats(sbi->ll_proc_root, "read_ahead_stats",
                                     sbi->ll_ra_stats);
        if (err)
                GOTO(out, err);


        err = lprocfs_add_vars(sbi->ll_proc_root, lprocfs_llite_obd_vars, sb);
        if (err)
                GOTO(out, err);

        /* MDC info */
        obd = class_name2obd(mdc);

        LASSERT(obd != NULL);
        LASSERT(obd->obd_magic == OBD_DEVICE_MAGIC);
        LASSERT(obd->obd_type->typ_name != NULL);

        snprintf(name, MAX_STRING_SIZE, "%s/common_name",
                 obd->obd_type->typ_name);
        lvars[0].read_fptr = lprocfs_rd_name;
        err = lprocfs_add_vars(sbi->ll_proc_root, lvars, obd);
        if (err)
                GOTO(out, err);

        snprintf(name, MAX_STRING_SIZE, "%s/uuid", obd->obd_type->typ_name);
        lvars[0].read_fptr = lprocfs_rd_uuid;
        err = lprocfs_add_vars(sbi->ll_proc_root, lvars, obd);
        if (err)
                GOTO(out, err);

        /* OSC */
        obd = class_name2obd(osc);

        LASSERT(obd != NULL);
        LASSERT(obd->obd_magic == OBD_DEVICE_MAGIC);
        LASSERT(obd->obd_type->typ_name != NULL);

        snprintf(name, MAX_STRING_SIZE, "%s/common_name",
                 obd->obd_type->typ_name);
        lvars[0].read_fptr = lprocfs_rd_name;
        err = lprocfs_add_vars(sbi->ll_proc_root, lvars, obd);
        if (err)
                GOTO(out, err);

        snprintf(name, MAX_STRING_SIZE, "%s/uuid", obd->obd_type->typ_name);
        lvars[0].read_fptr = lprocfs_rd_uuid;
        err = lprocfs_add_vars(sbi->ll_proc_root, lvars, obd);
out:
        lprocfs_put_lperef(sbi->ll_proc_root);
        if (err) {
                lprocfs_remove(&sbi->ll_proc_root);
                lprocfs_free_stats(&sbi->ll_ra_stats);
                lprocfs_free_stats(&sbi->ll_stats);
        }
        RETURN(err);
}

void lprocfs_unregister_mountpoint(struct ll_sb_info *sbi)
{
        if (sbi->ll_proc_root) {
                lprocfs_remove(&sbi->ll_proc_root);
                lprocfs_free_stats(&sbi->ll_ra_stats);
                lprocfs_free_stats(&sbi->ll_stats);
        }
}
#undef MAX_STRING_SIZE

#define pct(a,b) (b ? a * 100 / b : 0)

static void ll_display_extents_info(struct ll_rw_extents_info *io_extents,
                                   libcfs_seq_file_t *seq, int which)
{
        unsigned long read_tot = 0, write_tot = 0, read_cum, write_cum;
        unsigned long start, end, r, w;
        char *unitp = "KMGTPEZY";
        int i, units = 10;
        struct per_process_info *pp_info = &io_extents->pp_extents[which];

        read_cum = 0;
        write_cum = 0;
        start = 0;

        for(i = 0; i < LL_HIST_MAX; i++) {
                read_tot += pp_info->pp_r_hist.oh_buckets[i];
                write_tot += pp_info->pp_w_hist.oh_buckets[i];
        }

        for(i = 0; i < LL_HIST_MAX; i++) {
                r = pp_info->pp_r_hist.oh_buckets[i];
                w = pp_info->pp_w_hist.oh_buckets[i];
                read_cum += r;
                write_cum += w;
                end = 1 << (i + LL_HIST_START - units);
                LIBCFS_SEQ_PRINTF(seq, "%4lu%c - %4lu%c%c: %14lu %4lu %4lu  | "
                           "%14lu %4lu %4lu\n", start, *unitp, end, *unitp,
                           (i == LL_HIST_MAX - 1) ? '+' : ' ',
                           r, pct(r, read_tot), pct(read_cum, read_tot),
                           w, pct(w, write_tot), pct(write_cum, write_tot));
                start = end;
                if (start == 1<<10) {
                        start = 1;
                        units += 10;
                        unitp++;
                }
                if (read_cum == read_tot && write_cum == write_tot)
                        break;
        }
}

static int ll_rw_extents_stats_pp_seq_show(libcfs_seq_file_t *seq, void *v)
{
        struct timeval now;
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_extents_info *io_extents = &sbi->ll_rw_extents_info;
        int k;

        cfs_gettimeofday(&now);

        if (!sbi->ll_rw_stats_on) {
                LIBCFS_SEQ_PRINTF(seq, "disabled\n"
                                "write anything in this file to activate, "
                                "then 0 or \"[D/d]isabled\" to deactivate\n");
                return 0;
        }
        LIBCFS_SEQ_PRINTF(seq, "snapshot_time:         %lu.%lu (secs.usecs)\n",
                   now.tv_sec, now.tv_usec);
        LIBCFS_SEQ_PRINTF(seq, "%15s %19s       | %20s\n", " ", "read", "write");
        LIBCFS_SEQ_PRINTF(seq, "%13s   %14s %4s %4s  | %14s %4s %4s\n",
                   "extents", "calls", "%", "cum%",
                   "calls", "%", "cum%");
        cfs_spin_lock(&sbi->ll_pp_extent_lock);
        for(k = 0; k < LL_PROCESS_HIST_MAX; k++) {
                if(io_extents->pp_extents[k].pid != 0) {
                        LIBCFS_SEQ_PRINTF(seq, "\nPID: %d\n",
                                   io_extents->pp_extents[k].pid);
                        ll_display_extents_info(io_extents, seq, k);
                }
        }
        cfs_spin_unlock(&sbi->ll_pp_extent_lock);
        return 0;
}
static ssize_t
ll_rw_extents_stats_pp_seq_write(libcfs_file_t *file, const char *buf,
                                 size_t len, loff_t *off)
{
        libcfs_seq_file_t *seq = LIBCFS_FILE_PRIVATE(file);
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_extents_info *io_extents = &sbi->ll_rw_extents_info;
        int i;
        int value = 1, rc = 0;

        rc = lprocfs_write_helper(buf, len, &value, 0);
        if (rc < 0 && (strcmp(buf, "disabled") == 0 ||
                       strcmp(buf, "Disabled") == 0))
                value = 0;

        if (value == 0)
                sbi->ll_rw_stats_on = 0;
        else
                sbi->ll_rw_stats_on = 1;

        cfs_spin_lock(&sbi->ll_pp_extent_lock);
        for(i = 0; i < LL_PROCESS_HIST_MAX; i++) {
                io_extents->pp_extents[i].pid = 0;
                lprocfs_oh_clear(&io_extents->pp_extents[i].pp_r_hist);
                lprocfs_oh_clear(&io_extents->pp_extents[i].pp_w_hist);
        }
        cfs_spin_unlock(&sbi->ll_pp_extent_lock);
        return len;
}
LPROC_SEQ_FOPS(ll_rw_extents_stats_pp);

static int ll_rw_extents_stats_seq_show(libcfs_seq_file_t *seq, void *v)
{
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_extents_info *io_extents = &sbi->ll_rw_extents_info;
        struct timeval now;

        cfs_gettimeofday(&now);

        if (!sbi->ll_rw_stats_on) {
                LIBCFS_SEQ_PRINTF(seq, "disabled\n"
                                "write anything in this file to activate, "
                                "then 0 or \"[D/d]isabled\" to deactivate\n");
                return 0;
        }
        LIBCFS_SEQ_PRINTF(seq, "snapshot_time:         %lu.%lu (secs.usecs)\n",
                   now.tv_sec, now.tv_usec);
        LIBCFS_SEQ_PRINTF(seq, "%15s %19s       | %20s\n", " ", "read", "write");
        LIBCFS_SEQ_PRINTF(seq, "%13s   %14s %4s %4s  | %14s %4s %4s\n",
                   "extents", "calls", "%", "cum%",
                   "calls", "%", "cum%");
        cfs_spin_lock(&sbi->ll_lock);
        ll_display_extents_info(io_extents, seq, LL_PROCESS_HIST_MAX);
        cfs_spin_unlock(&sbi->ll_lock);

        return 0;
}

static ssize_t
ll_rw_extents_stats_seq_write(libcfs_file_t *file, const char *buf,
                              size_t len, loff_t *off)
{
        libcfs_seq_file_t *seq = LIBCFS_FILE_PRIVATE(file);
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_extents_info *io_extents = &sbi->ll_rw_extents_info;
        int i;
        int value = 1, rc = 0;

        rc = lprocfs_write_helper(buf, len, &value, 0);
        if (rc < 0 && (strcmp(buf, "disabled") == 0 ||
                       strcmp(buf, "Disabled") == 0))
                value = 0;

        if (value == 0)
                sbi->ll_rw_stats_on = 0;
        else
                sbi->ll_rw_stats_on = 1;
        cfs_spin_lock(&sbi->ll_pp_extent_lock);
        for(i = 0; i <= LL_PROCESS_HIST_MAX; i++)
        {
                io_extents->pp_extents[i].pid = 0;
                lprocfs_oh_clear(&io_extents->pp_extents[i].pp_r_hist);
                lprocfs_oh_clear(&io_extents->pp_extents[i].pp_w_hist);
        }
        cfs_spin_unlock(&sbi->ll_pp_extent_lock);

        return (int)len;
}
LPROC_SEQ_FOPS(ll_rw_extents_stats);

void ll_rw_stats_tally(struct ll_sb_info *sbi, pid_t pid,
                       struct ll_file_data *file, loff_t pos,
                       size_t count, int rw)
{
        int i, cur = -1;
        struct ll_rw_process_info *process;
        struct ll_rw_process_info *offset;
        int *off_count = &sbi->ll_rw_offset_entry_count;
        int *process_count = &sbi->ll_offset_process_count;
        struct ll_rw_extents_info *io_extents = &sbi->ll_rw_extents_info;

        if(!sbi->ll_rw_stats_on)
                return;
        process = sbi->ll_rw_process_info;
        offset = sbi->ll_rw_offset_info;

        cfs_spin_lock(&sbi->ll_pp_extent_lock);
        /* Extent statistics */
        for(i = 0; i < LL_PROCESS_HIST_MAX; i++) {
                if(io_extents->pp_extents[i].pid == pid) {
                        cur = i;
                        break;
                }
        }

        if (cur == -1) {
                /* new process */
                sbi->ll_extent_process_count =
                        (sbi->ll_extent_process_count + 1) % LL_PROCESS_HIST_MAX;
                cur = sbi->ll_extent_process_count;
                io_extents->pp_extents[cur].pid = pid;
                lprocfs_oh_clear(&io_extents->pp_extents[cur].pp_r_hist);
                lprocfs_oh_clear(&io_extents->pp_extents[cur].pp_w_hist);
        }

        for(i = 0; (count >= (1 << LL_HIST_START << i)) &&
             (i < (LL_HIST_MAX - 1)); i++);
        if (rw == 0) {
                io_extents->pp_extents[cur].pp_r_hist.oh_buckets[i]++;
                io_extents->pp_extents[LL_PROCESS_HIST_MAX].pp_r_hist.oh_buckets[i]++;
        } else {
                io_extents->pp_extents[cur].pp_w_hist.oh_buckets[i]++;
                io_extents->pp_extents[LL_PROCESS_HIST_MAX].pp_w_hist.oh_buckets[i]++;
        }
        cfs_spin_unlock(&sbi->ll_pp_extent_lock);

        cfs_spin_lock(&sbi->ll_process_lock);
        /* Offset statistics */
        for (i = 0; i < LL_PROCESS_HIST_MAX; i++) {
                if (process[i].rw_pid == pid) {
                        if (process[i].rw_last_file != file) {
                                process[i].rw_range_start = pos;
                                process[i].rw_last_file_pos = pos + count;
                                process[i].rw_smallest_extent = count;
                                process[i].rw_largest_extent = count;
                                process[i].rw_offset = 0;
                                process[i].rw_last_file = file;
                                cfs_spin_unlock(&sbi->ll_process_lock);
                                return;
                        }
                        if (process[i].rw_last_file_pos != pos) {
                                *off_count =
                                    (*off_count + 1) % LL_OFFSET_HIST_MAX;
                                offset[*off_count].rw_op = process[i].rw_op;
                                offset[*off_count].rw_pid = pid;
                                offset[*off_count].rw_range_start =
                                        process[i].rw_range_start;
                                offset[*off_count].rw_range_end =
                                        process[i].rw_last_file_pos;
                                offset[*off_count].rw_smallest_extent =
                                        process[i].rw_smallest_extent;
                                offset[*off_count].rw_largest_extent =
                                        process[i].rw_largest_extent;
                                offset[*off_count].rw_offset =
                                        process[i].rw_offset;
                                process[i].rw_op = rw;
                                process[i].rw_range_start = pos;
                                process[i].rw_smallest_extent = count;
                                process[i].rw_largest_extent = count;
                                process[i].rw_offset = pos -
                                        process[i].rw_last_file_pos;
                        }
                        if(process[i].rw_smallest_extent > count)
                                process[i].rw_smallest_extent = count;
                        if(process[i].rw_largest_extent < count)
                                process[i].rw_largest_extent = count;
                        process[i].rw_last_file_pos = pos + count;
                        cfs_spin_unlock(&sbi->ll_process_lock);
                        return;
                }
        }
        *process_count = (*process_count + 1) % LL_PROCESS_HIST_MAX;
        process[*process_count].rw_pid = pid;
        process[*process_count].rw_op = rw;
        process[*process_count].rw_range_start = pos;
        process[*process_count].rw_last_file_pos = pos + count;
        process[*process_count].rw_smallest_extent = count;
        process[*process_count].rw_largest_extent = count;
        process[*process_count].rw_offset = 0;
        process[*process_count].rw_last_file = file;
        cfs_spin_unlock(&sbi->ll_process_lock);
}

static int ll_rw_offset_stats_seq_show(libcfs_seq_file_t *seq, void *v)
{
        struct timeval now;
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_process_info *offset = sbi->ll_rw_offset_info;
        struct ll_rw_process_info *process = sbi->ll_rw_process_info;
        int i;

        cfs_gettimeofday(&now);

        if (!sbi->ll_rw_stats_on) {
                LIBCFS_SEQ_PRINTF(seq, "disabled\n"
                                "write anything in this file to activate, "
                                "then 0 or \"[D/d]isabled\" to deactivate\n");
                return 0;
        }
        cfs_spin_lock(&sbi->ll_process_lock);

        LIBCFS_SEQ_PRINTF(seq, "snapshot_time:         %lu.%lu (secs.usecs)\n",
                   now.tv_sec, now.tv_usec);
        LIBCFS_SEQ_PRINTF(seq, "%3s %10s %14s %14s %17s %17s %14s\n",
                   "R/W", "PID", "RANGE START", "RANGE END",
                   "SMALLEST EXTENT", "LARGEST EXTENT", "OFFSET");
        /* We stored the discontiguous offsets here; print them first */
        for(i = 0; i < LL_OFFSET_HIST_MAX; i++) {
                if (offset[i].rw_pid != 0)
                        LIBCFS_SEQ_PRINTF(seq,
                                   "%3c %10d %14Lu %14Lu %17lu %17lu %14Lu",
                                   offset[i].rw_op ? 'W' : 'R',
                                   offset[i].rw_pid,
                                   offset[i].rw_range_start,
                                   offset[i].rw_range_end,
                                   (unsigned long)offset[i].rw_smallest_extent,
                                   (unsigned long)offset[i].rw_largest_extent,
                                   offset[i].rw_offset);
        }
        /* Then print the current offsets for each process */
        for(i = 0; i < LL_PROCESS_HIST_MAX; i++) {
                if (process[i].rw_pid != 0)
                        LIBCFS_SEQ_PRINTF(seq,
                                   "%3c %10d %14Lu %14Lu %17lu %17lu %14Lu",
                                   process[i].rw_op ? 'W' : 'R',
                                   process[i].rw_pid,
                                   process[i].rw_range_start,
                                   process[i].rw_last_file_pos,
                                   (unsigned long)process[i].rw_smallest_extent,
                                   (unsigned long)process[i].rw_largest_extent,
                                   process[i].rw_offset);
        }
        cfs_spin_unlock(&sbi->ll_process_lock);

        return 0;
}

static ssize_t
ll_rw_offset_stats_seq_write(libcfs_file_t *file, const char *buf,
                             size_t len, loff_t *off)
{
        libcfs_seq_file_t *seq = LIBCFS_FILE_PRIVATE(file);
        struct ll_sb_info *sbi = LIBCFS_SEQ_PRIVATE(seq);
        struct ll_rw_process_info *process_info = sbi->ll_rw_process_info;
        struct ll_rw_process_info *offset_info = sbi->ll_rw_offset_info;
        int value = 1, rc = 0;

        rc = lprocfs_write_helper(buf, len, &value, 0);

        if (rc < 0 && (strcmp(buf, "disabled") == 0 ||
                           strcmp(buf, "Disabled") == 0))
                value = 0;

        if (value == 0)
                sbi->ll_rw_stats_on = 0;
        else
                sbi->ll_rw_stats_on = 1;

        cfs_spin_lock(&sbi->ll_process_lock);
        sbi->ll_offset_process_count = 0;
        sbi->ll_rw_offset_entry_count = 0;
        memset(process_info, 0, sizeof(struct ll_rw_process_info) *
               LL_PROCESS_HIST_MAX);
        memset(offset_info, 0, sizeof(struct ll_rw_process_info) *
               LL_OFFSET_HIST_MAX);
        cfs_spin_unlock(&sbi->ll_process_lock);

        return len;
}

LPROC_SEQ_FOPS(ll_rw_offset_stats);

void lprocfs_llite_init_vars(struct lprocfs_static_vars *lvars)
{
    lvars->module_vars  = NULL;
    lvars->obd_vars     = lprocfs_llite_obd_vars;
}
#endif /* __KERNEL__ */

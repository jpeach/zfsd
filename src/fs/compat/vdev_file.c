/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2011, 2015 by Delphix. All rights reserved.
 */

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/zio.h>
#include <sys/fs/zfs.h>
#include <sys/stat.h>

extern int vdev_disk_physio(vdev_t *,
    caddr_t, size_t, uint64_t, int, boolean_t);

typedef struct vdev_file {
        int             vf_fd;
} vdev_file_t;

/*
 * Virtual device vector for files.
 */

static vdev_file_t *
vdev_file_alloc(void)
{
        vdev_file_t *vf;

	vf = kmem_zalloc(sizeof (vdev_file_t), KM_SLEEP);
        vf->vf_fd = -1;

        return vf;
}

static int
vdev_file_openat(vdev_file_t *vf, const char *path, int spa_mode)
{
        int flags = 0;

        VERIFY3(vf->vf_fd, ==, -1);

        // Do synchronous IO to provide the guarantees the upper layers want.
        flags |= O_SYNC;

        // Try to minimize kernel IO bufferring.
        flags |= O_DIRECT;

        if ((spa_mode & (FREAD|FWRITE)) == (FREAD|FWRITE)) {
                flags |= O_RDWR;
        } else if (spa_mode & FREAD) {
                flags |= O_RDONLY;
        } else if (spa_mode & FWRITE) {
                flags |= O_WRONLY;
        }

        // If we can't open with O_DIRECT, try without it since many
        // filesystems don't support O_DIRECT.
        vf->vf_fd = open(path, flags);
        if (vf->vf_fd == -1 && errno == EINVAL) {
                flags &= ~O_DIRECT;
                vf->vf_fd = open(path, flags);
        }

        return vf->vf_fd == -1 ? errno : ESUCCESS;
}

static void
vdev_file_hold(vdev_t *vd)
{
	ASSERT(vd->vdev_path != NULL);
}

static void
vdev_file_rele(vdev_t *vd)
{
	ASSERT(vd->vdev_path != NULL);
}

static int
vdev_file_open(vdev_t *vd, uint64_t *psize, uint64_t *max_psize,
    uint64_t *ashift)
{
	vdev_file_t *vf;
	struct stat vattr;
	int error;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (SET_ERROR(EINVAL));
	}

	/*
	 * Reopen the device if it's not currently open.  Otherwise,
	 * just update the physical size of the device.
	 */
	if (vd->vdev_tsd != NULL) {
		ASSERT(vd->vdev_reopening);
		vf = vd->vdev_tsd;
		goto skip_open;
	}

	vf = vd->vdev_tsd = vdev_file_alloc();

	/*
	 * We always open the files from the root of the global zone, even if
	 * we're in a local zone.  If the user has gotten to this point, the
	 * administrator has already decided that the pool should be available
	 * to local zone users, so the underlying devices should be as well.
	 */
	ASSERT(vd->vdev_path != NULL && vd->vdev_path[0] == '/');
        error = vdev_file_openat(vf, vd->vdev_path, spa_mode(vd->vdev_spa) | FOFFMAX);
	if (error) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (error);
	}

skip_open:
	/*
	 * Determine the physical size of the file.
	 */
        error = fstat(vf->vf_fd, &vattr);
        if (error == -1) {
                error = errno;
        }

	if (error) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (error);
	}

	/*
	 * Make sure it's a regular file.
	 */
	if (!S_ISREG(vattr.st_mode)) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (SET_ERROR(ENODEV));
	}

        /* TODO: Add disk geometry support for block devices. */

	*max_psize = *psize = vattr.st_size;
	*ashift = SPA_MINBLOCKSHIFT;

	return (0);
}

static void
vdev_file_close(vdev_t *vd)
{
	vdev_file_t *vf = vd->vdev_tsd;

	if (vd->vdev_reopening || vf == NULL)
		return;

        if (vf->vf_fd != -1) {
                fsync(vf->vf_fd);
                close(vf->vf_fd);
        }

	vd->vdev_delayed_close = B_FALSE;
	kmem_free(vf, sizeof (vdev_file_t));
	vd->vdev_tsd = NULL;
}

static void
vdev_file_io_strategy(void *arg)
{
        zio_t *zio = arg;
	vdev_t *vd = zio->io_vd;
	vdev_file_t *vf = vd->vdev_tsd;
	ssize_t resid;

	switch (zio->io_type) {
        case ZIO_TYPE_READ:
                resid = pread(vf->vf_fd, zio->io_data, zio->io_size, zio->io_offset);
                break;
        case ZIO_TYPE_WRITE:
                resid = pwrite(vf->vf_fd, zio->io_data, zio->io_size, zio->io_offset);
                break;
        default:
                panic("invalid zio io_type=%d", zio->io_type);
        }

        if (resid == -1) {
		zio->io_error = SET_ERROR(errno);
        }

        if (resid != zio->io_size) {
		zio->io_error = SET_ERROR(EIO);
        }

	zio_delay_interrupt(zio);
}

static void
vdev_file_io_start(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	vdev_file_t *vf = vd->vdev_tsd;

	if (zio->io_type == ZIO_TYPE_IOCTL) {
		/* XXPOLICY */
		if (!vdev_readable(vd)) {
			zio->io_error = SET_ERROR(ENXIO);
			zio_interrupt(zio);
			return;
		}

		if (zio->io_cmd == DKIOCFLUSHWRITECACHE) {
			zio->io_error = fsync(vf->vf_fd) == -1 ? ESUCCESS : errno;
                } else {
			zio->io_error = SET_ERROR(ENOTSUP);
		}

		zio_execute(zio);
		return;
	}

	ASSERT(zio->io_type == ZIO_TYPE_READ || zio->io_type == ZIO_TYPE_WRITE);
	zio->io_target_timestamp = zio_handle_io_delay(zio);

	VERIFY3U(taskq_dispatch(system_taskq, vdev_file_io_strategy, zio,
	    TQ_SLEEP), !=, 0);
}

/* ARGSUSED */
static void
vdev_file_io_done(zio_t *zio)
{
}

int
vdev_disk_physio(vdev_t *vd, caddr_t data,
    size_t size, uint64_t offset, int flags, boolean_t isdump)
{
        int error;
        vdev_file_t *vf = vd->vdev_tsd;

        // No support for kernel core dumps. Of course, the call path for this
        // routine only occurs when we use a zvol for dumps (via zvol_dump()),
        // so the whole ting it probably moot :-/
        VERIFY3(isdump, ==, B_FALSE);

        // This is supposed to be a leaf vdev.
        VERIFY3(vd->vdev_ops->vdev_op_leaf, ==, B_TRUE);
        VERIFY(vd->vdev_ops == &vdev_file_ops || vd->vdev_ops == &vdev_disk_ops);
        VERIFY(flags & B_READ || flags & B_WRITE);

        if (flags & B_READ) {
                if (pread(vf->vf_fd, data, size, offset) == -1) {
                        error = errno;
                }
        }

        if (flags & B_WRITE) {
                if (pwrite(vf->vf_fd, data, size, offset) == -1) {
                        error = errno;
                }
        }

        if (error == 0 && (flags & B_WRITE)) {
                fsync(vf->vf_fd);
        }

        return error;
}

vdev_ops_t vdev_file_ops = {
	vdev_file_open,
	vdev_file_close,
	vdev_default_asize,
	vdev_file_io_start,
	vdev_file_io_done,
	NULL,
	vdev_file_hold,
	vdev_file_rele,
	VDEV_TYPE_FILE,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};

/*
 * From userland we access disks just like files.
 */

vdev_ops_t vdev_disk_ops = {
	vdev_file_open,
	vdev_file_close,
	vdev_default_asize,
	vdev_file_io_start,
	vdev_file_io_done,
	NULL,
	vdev_file_hold,
	vdev_file_rele,
	VDEV_TYPE_DISK,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};

/* vim: set sw=8 ts=8 et: */

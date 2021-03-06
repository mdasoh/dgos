#include "sys_fd.h"
#include "process.h"
#include "thread.h"
#include "fileio.h"
#include "syscall_helper.h"

static int err(errno_t errno)
{
    thread_set_error(errno);
    return -1;
}

static int err(int negerr)
{
    assert(negerr < 0 && -negerr < int(errno_t::MAX_ERRNO));
    thread_set_error(errno_t(-negerr));
    return -1;
}

static int badf_err()
{
    return err(errno_t::EBADF);
}

// == APIs that take file descriptors ==

ssize_t sys_read(int fd, void *bufaddr, size_t count)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    ssize_t sz = file_read(id, bufaddr, count);

    if (sz < 0)
        thread_set_error(errno_t(-sz));

    return sz;
}

ssize_t sys_write(int fd, void const *bufaddr, size_t count)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    if (uintptr_t(bufaddr) >= 0x800000000000)
        return err(errno_t::EFAULT);

    if (!verify_accessible(bufaddr, count, false))
        return err(errno_t::EFAULT);

    ssize_t sz = file_write(id, bufaddr, count);

    if (sz >= 0)
        return sz;

    return err(sz);
}

int sys_close(int fd)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_close(id);
    if (status != 0)
        return 0;

    return err(status);
}

ssize_t sys_pread64(int fd, void *bufaddr, size_t count, off_t ofs)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int sz = file_pread(id, bufaddr, count, ofs);
    if (sz >= 0)
        return sz;

    return err(sz);
}

ssize_t sys_pwrite64(int fd, void const *bufaddr,
                     size_t count, off_t ofs)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int sz = file_pwrite(id, bufaddr, count, ofs);
    if (sz >= 0)
        return sz;

    return err(sz);
}

off_t sys_lseek(int fd, off_t ofs, int whence)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int pos = file_seek(id, ofs, whence);
    if (pos >= 0)
        return pos;

    return err(pos);
}

int sys_fsync(int fd)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_fsync(id);
    if (status >= 0)
        return status;

    return err(status);
}

int sys_fdatasync(int fd)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_fdatasync(id);
    if (status >= 0)
        return status;

    return err(status);
}

int sys_ftruncate(int fd, off_t size)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_ftruncate(id, size);
    if (status >= 0)
        return status;

    return err(status);
}

int sys_dup(int oldfd)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(oldfd);

    int newfd = p->ids.desc_alloc.alloc();

    if (file_ref_filetab(id)) {
        p->ids.ids[newfd] = id;
        return newfd;
    }

    p->ids.desc_alloc.free(newfd);

    return badf_err();
}

int sys_dup3(int oldfd, int newfd, int flags)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(oldfd);

    int newid = p->fd_to_id(newfd);

    if (newid >= 0)
        file_close(newid);
    else if (!p->ids.desc_alloc.take(newfd))
        return err(errno_t::EMFILE);


    if (likely(file_ref_filetab(id))) {
        p->ids.ids[newfd].set(id, flags);
        return newfd;
    }

    p->ids.desc_alloc.free(newfd);

    return badf_err();
}

int sys_dup2(int oldfd, int newfd)
{
    return sys_dup3(oldfd, newfd, 0);
}

// == APIs that take paths ==

int sys_open(char const* pathname, int flags, mode_t mode)
{
    process_t *p = fast_cur_process();

    int fd = p->ids.desc_alloc.alloc();

    int id = file_open(pathname, flags, mode);

    if (likely(id >= 0)) {
        p->ids.ids[fd] = id;
        return fd;
    }

    p->ids.desc_alloc.free(fd);
    return err(-id);
}

int sys_creat(char const *path, mode_t mode)
{
    process_t *p = fast_cur_process();

    int fd = p->ids.desc_alloc.alloc();

    int id = file_creat(path, mode);

    if (likely(id >= 0)) {
        p->ids.ids[fd] = id;
        return fd;
    }

    p->ids.desc_alloc.free(fd);
    return err(-id);
}

int sys_truncate(char const *path, off_t size)
{
    int fd = sys_open(path, O_RDWR, 0);
    int err = sys_ftruncate(fd, size);
    sys_close(fd);
    return err;
}

int sys_rename(char const *old_path, char const *new_path)
{
    int status = file_rename(old_path, new_path);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_mkdir(char const *path, mode_t mode)
{
    int status = file_mkdir(path, mode);
    if (status >= 0)
        return status;

    return err(status);
}

int sys_rmdir(char const *path)
{
    int status = file_rmdir(path);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_unlink(char const *path)
{
    int status = file_unlink(path);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_mknod(char const *path, mode_t mode, int rdev)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_link(char const *from, char const *to)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_chmod(char const *path, mode_t mode)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_fchmod(int fd, mode_t mode)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_chown(char const *path, int uid, int gid)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_fchown(int fd, int uid, int gid)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_setxattr(char const *path,
                 char const *name, char const *value,
                 size_t size, int flags)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_getxattr(char const *path,
                 char const *name, char *value,
                 size_t size)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_listxattr(char const *path, char const *list, size_t size)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

int sys_access(char const *path, int mask)
{
    // FIXME: implement me
    return -int(errno_t::ENOSYS);
}

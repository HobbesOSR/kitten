#ifndef _LWK_UIO_H
#define _LWK_UIO_H

struct iovec
{
        void __user *iov_base;  /* BSD uses caddr_t (1003.1g requires void *) */
        __kernel_size_t iov_len; /* Must be size_t (1003.1g) */
};

struct kiocb {
	struct file             *ki_filp;

};

#endif

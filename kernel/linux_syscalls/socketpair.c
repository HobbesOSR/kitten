#include <arch/uaccess.h>

int sys_socketpair(int family, int type, int protocol, int * usockvec)
{
  int fds[2];
  fds[0] = 0;
  fds[1] = 1;

  if (copy_to_user(usockvec, fds, sizeof(fds))) {
      printk("s: couldn't copy socket vector\n", __func__);
    }

    return 0;
}

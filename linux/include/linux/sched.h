#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#include <lwk/sched.h>

#define wake_up_process(p)      sched_wakeup_task((p), TASK_ALL)

#endif

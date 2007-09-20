#include <lwk/task.h>
#include <lwk/init_task.h>

union task_union init_task_union
	__attribute__((__section__(".data.init_task"))) =
		{
			/* Initialize task_union.task_info */
			{
				/* arch independent portion */
				INIT_TASK(init_task)

				/* x86_64 specific portion */
				.arch = {
					.cpu = 0,
				}
			}
		};


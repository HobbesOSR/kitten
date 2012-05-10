#include <lwk/list.h>
#include <arch/atomic.h>

//#undef _KDBG
//#define _KDBG(fmt,arg...)

extern void workq_init(void);

struct workqueue_struct;
struct work_struct;

typedef void (*work_func_t)(struct work_struct *work);

enum {
	WORK_STRUCT_PENDING = 0,        /* T if work item pending execution */
	WORK_STRUCT_FLAG_MASK = (3UL),
	WORK_STRUCT_WQ_DATA_MASK = (~WORK_STRUCT_FLAG_MASK)
};

struct work_struct {
	atomic_long_t data;
	struct list_head entry;
	work_func_t func;
};

/*
 * The first word is the work queue pointer and the flags rolled into
 * one
 */
#define work_data_bits(work) ((unsigned long *)(&(work)->data))

#define WORK_DATA_INIT()    ATOMIC_LONG_INIT(0)

#define __WORK_INITIALIZER(n, f)  				\
	{											\
		.data = WORK_DATA_INIT(),				\
		.entry  = { &(n).entry, &(n).entry },	\
		.func = (f),							\
    }

#define DECLARE_WORK(n, function)	\
	struct work_struct n = __WORK_INITIALIZER(n, function)

#define PREPARE_WORK(_work, _func)	\
	do {							\
		(_work)->func = (_func);	\
	} while (0)

#define INIT_WORK(_work, _func)								\
	do {													\
		(_work)->data = (atomic_long_t) WORK_DATA_INIT();	\
		INIT_LIST_HEAD(&(_work)->entry);					\
		PREPARE_WORK((_work), (_func));						\
	} while (0)

/**
 * work_pending - Find out whether a work item is currently pending
 * @work: The work item in question
 */
#define work_pending(work) \
    test_bit(WORK_STRUCT_PENDING, work_data_bits(work))

/**
 * work_clear_pending - for internal use only, mark a work item as not pending
 * @work: The work item in question
 */
#define work_clear_pending(work) \
    clear_bit(WORK_STRUCT_PENDING, work_data_bits(work))


/* Create a worker thread */
struct workqueue_struct *create_workqueue(const char *name);
/* Destroy a worker thread */
void destroy_workqueue(struct workqueue_struct *wq);

/* Queue work to a given worker thread */
int queue_work(struct workqueue_struct *wq, struct work_struct *work);
/* Wait on all pending work on the given worker therad */
void flush_workqueue(struct workqueue_struct *wq);

/* Schedule work to the default worker thread */
int schedule_work(struct work_struct *work);

/* Wait on all pending work on the default worker thread */
void flush_scheduled_work(void);

int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);

/* vim: set noexpandtab noai ts=4 sw=4: */

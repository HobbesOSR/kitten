
/*
 * linux-2.6.27.57 workq code
 */

#include <lwk/workq.h>
#include <lwk/spinlock.h>
#include <lwk/kthread.h>
#include <lwk/smp.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>

#undef _KDBG
#define _KDBG(...)

struct workqueue_struct *keventd_wq;

#define dump_stack()

struct cpu_workqueue_struct {
    spinlock_t lock;
    struct list_head worklist;
    waitq_t more_work;

    struct work_struct *current_work;
    struct workqueue_struct *wq;
    struct task_struct *thread;
	int run_depth;
} ____cacheline_aligned;

struct workqueue_struct {
    struct cpu_workqueue_struct *cpu_wq;
    struct list_head list;
    const char *name;
};

/* Serializes the accesses to the list of workqueues. */
static DEFINE_SPINLOCK(workqueue_lock);
static LIST_HEAD(workqueues);

#if 0
/*
 * _cpu_down() first removes CPU from cpu_online_map, then CPU_DEAD
 * flushes cwq->worklist. This means that flush_workqueue/wait_on_work
 * which comes in between can't use for_each_online_cpu(). We could
 * use cpu_possible_map, the cpumask below is more a documentation
 * than optimization.
 */
static cpumask_t cpu_populated_map __read_mostly;

static const cpumask_t *wq_cpu_map(struct workqueue_struct *wq)
{
    return &cpu_populated_map;
}
#endif

static
struct cpu_workqueue_struct *wq_per_cpu(struct workqueue_struct *wq, int cpu)
{
    return percpu_ptr(wq->cpu_wq, cpu);
}

/*
 * Set the workqueue on which a work item is to be run
 * - Must *only* be called if the pending flag is set
 */
static inline void set_wq_data(struct work_struct *work,
                struct cpu_workqueue_struct *cwq)
{
    unsigned long new;

    BUG_ON(!work_pending(work));

    new = (unsigned long) cwq | (1UL << WORK_STRUCT_PENDING);
    new |= WORK_STRUCT_FLAG_MASK & *work_data_bits(work);
    atomic_long_set(&work->data, new);
}

static inline
struct cpu_workqueue_struct *get_wq_data(struct work_struct *work)
{
    return (void *) (atomic_long_read(&work->data) & WORK_STRUCT_WQ_DATA_MASK);
}

static void insert_work(struct cpu_workqueue_struct *cwq,
            struct work_struct *work, struct list_head *head)
{
	_KDBG("\n");
    set_wq_data(work, cwq);
    /*
     * Ensure that we get the right work->data if we see the
     * result of list_add() below, see try_to_grab_pending().
     */
    smp_wmb();
    list_add_tail(&work->entry, head);
    waitq_wake_nr(&cwq->more_work,1);
}

static void __queue_work(struct cpu_workqueue_struct *cwq,
             struct work_struct *work)
{
    unsigned long flags;

    spin_lock_irqsave(&cwq->lock, flags);
    insert_work(cwq, work, &cwq->worklist);
    spin_unlock_irqrestore(&cwq->lock, flags);
}

/**
 * queue_work - queue work on a workqueue
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to the CPU on which it was submitted, but if the CPU dies
 * it can be processed by another CPU.
 */
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    int ret;

	_KDBG("\n");
    ret = queue_work_on(get_cpu(), wq, work);
    put_cpu();

    return ret;
}

/**
 * queue_work_on - queue work on specific cpu
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to a specific CPU, the caller must ensure it
 * can't go away.
 */
int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
    int ret = 0;

	_KDBG("\n");
    if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
        BUG_ON(!list_empty(&work->entry));
        __queue_work(wq_per_cpu(wq, cpu), work);
        ret = 1;
    }
    return ret;
}

static void delayed_work_timer_fn(unsigned long __data)
{
    struct delayed_work *dwork = (struct delayed_work *)__data;
    struct cpu_workqueue_struct *cwq = get_wq_data(&dwork->work);
    struct workqueue_struct *wq = cwq->wq;

    __queue_work(wq_per_cpu(wq, smp_processor_id()), &dwork->work);
}


/**
 * queue_delayed_work - queue work on a workqueue after delay
 * @wq: workqueue to use
 * @dwork: delayable work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work(struct workqueue_struct *wq,
            struct delayed_work *dwork, unsigned long delay)
{
    if (delay == 0)
        return queue_work(wq, &dwork->work);

    return queue_delayed_work_on(-1, wq, dwork, delay);
}

/**
 * queue_delayed_work_on - queue work on specific CPU after delay
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @dwork: work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
            struct delayed_work *dwork, unsigned long delay)
{
    int ret = 0;
    struct timer *timer = &dwork->timer;
    struct work_struct *work = &dwork->work;

    if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
        BUG_ON(timer_pending(timer));
        BUG_ON(!list_empty(&work->entry));

#ifdef CONFIG_TIMER_STATS
        timer_stats_timer_set_start_info(&dwork->timer);
#endif

        /* This stores cwq for the moment, for the timer_fn */
        set_wq_data(work, wq_per_cpu(wq, raw_smp_processor_id()));
        timer->expires = get_time() + delay;
        timer->data = (unsigned long)dwork;
        timer->function = delayed_work_timer_fn;

        if (unlikely(cpu >= 0))
            timer_add_on(timer, cpu);
        else
            timer_add(timer);
        ret = 1;
    }
    return ret;
}

/* Wait on all pending work on the given worker therad */
void flush_workqueue(struct workqueue_struct *wq)
{
	panic("%s() not implemented\n",__func__);
#if 0
    const cpumask_t *cpu_map = wq_cpu_map(wq);
    int cpu;

    might_sleep();
    lock_map_acquire(&wq->lockdep_map);
    lock_map_release(&wq->lockdep_map);
    for_each_cpu_mask(cpu, *cpu_map)
        flush_cpu_workqueue(per_cpu_ptr(wq->cpu_wq, cpu));
#endif
}

int flush_work(struct work_struct *work)
{
	panic("%s() not implemented\n",__func__);
}

/* Schedule work to the default worker thread */
int schedule_work(struct work_struct *work)
{
	_KDBG("\n");
	return queue_work( keventd_wq,  work );
}

/* Wait on all pending work on the default worker thread */
void flush_scheduled_work(void)
{
	_KDBG("\n");
	flush_workqueue(keventd_wq);
}

static void run_workqueue(struct cpu_workqueue_struct *cwq)
{
	_KDBG("\n");
    spin_lock_irq(&cwq->lock);
    cwq->run_depth++;
    if (cwq->run_depth > 3) {
        /* morton gets to eat his hat */
        printk("%s: recursion depth exceeded: %d\n",
            __func__, cwq->run_depth);
        dump_stack();
    }
    while (!list_empty(&cwq->worklist)) {
        struct work_struct *work = list_entry(cwq->worklist.next,
                        struct work_struct, entry);
        work_func_t f = work->func;

        cwq->current_work = work;
        list_del_init(cwq->worklist.next);
        spin_unlock_irq(&cwq->lock);

        BUG_ON(get_wq_data(work) != cwq);
        work_clear_pending(work);
#if 0
        lock_map_acquire(&cwq->wq->lockdep_map);
        lock_map_acquire(&lockdep_map);
#endif
        f(work);
#if 0
        lock_map_release(&lockdep_map);
        lock_map_release(&cwq->wq->lockdep_map);
#endif
        spin_lock_irq(&cwq->lock);
        cwq->current_work = NULL;
    }
    cwq->run_depth--;
    spin_unlock_irq(&cwq->lock);
}

static int worker_thread(void *__cwq)
{
    struct cpu_workqueue_struct *cwq = __cwq;
    DECLARE_WAITQ_ENTRY(wait,current);

	_KDBG("\n");

    for (;;) {
        waitq_prepare_to_wait(&cwq->more_work, &wait, TASK_INTERRUPTIBLE);
        if (
            !kthread_should_stop() &&
            list_empty(&cwq->worklist))
            schedule();
        waitq_finish_wait(&cwq->more_work, &wait);

        if (kthread_should_stop())
            break;
        run_workqueue(cwq);
    }

    return 0;
}


static struct cpu_workqueue_struct *
init_cpu_workqueue(struct workqueue_struct *wq, int cpu)
{
    struct cpu_workqueue_struct *cwq = percpu_ptr(wq->cpu_wq, cpu);

	_KDBG("\n");
    cwq->wq = wq;
    spin_lock_init(&cwq->lock);
    INIT_LIST_HEAD(&cwq->worklist);
    waitq_init(&cwq->more_work);

    return cwq;
}

static int create_workqueue_thread(struct cpu_workqueue_struct *cwq, int cpu)
{
    struct workqueue_struct *wq = cwq->wq;
    const char *fmt = "%s/%d";
    struct task_struct *p;

	_KDBG("\n");
    p = kthread_run(worker_thread, cwq, fmt, wq->name, cpu);
    /*
     * Nobody can add the work_struct to this cwq,
     *  if (caller is __create_workqueue)
     *      nobody should see this wq
     *  else // caller is CPU_UP_PREPARE
     *      cpu is not on cpu_online_map
     * so we can abort safely.
     */
    if ( !p ) return -1;
    
    cwq->thread = p;
    
    return 0;
}


static void start_workqueue_thread(struct cpu_workqueue_struct *cwq, int cpu)
{
    struct task_struct *p = cwq->thread;
	_KDBG("\n");

    if (p != NULL) {
        if (cpu >= 0)
            kthread_bind(p, cpu);
		sched_wakeup_task(p,TASK_ALL);
    }
}

struct workqueue_struct *__create_workqueue_key(const char *name,
                        int singlethread,
                        int freezeable,
                        struct lock_class_key *key,
                        const char *lock_name)
{
       return create_workqueue(name);
}

/* Create a worker thread */
struct workqueue_struct *create_workqueue(const char *name)
{
    struct workqueue_struct *wq;
    struct cpu_workqueue_struct *cwq;
    int err = 0, cpu;
    
	_KDBG("name=%s\n",name);
    wq = kmem_alloc(sizeof(*wq));
    if (!wq)
        return NULL;
        
    wq->cpu_wq = percpu_alloc(sizeof(struct cpu_workqueue_struct));
    if (!wq->cpu_wq) {
        kmem_free(wq);
        return NULL;
    }   
    
    wq->name = name;
	INIT_LIST_HEAD(&wq->list);

//    cpu_maps_update_begin();
    /*
     * We must place this wq on list even if the code below fails.
     * cpu_down(cpu) can remove cpu from cpu_populated_map before
     * destroy_workqueue() takes the lock, in that case we leak
     * cwq[cpu]->thread.
     */
	spin_lock(&workqueue_lock);
	list_add(&wq->list, &workqueues);
    spin_unlock(&workqueue_lock);
    /*
     * We must initialize cwqs for each possible cpu even if we
     * are going to call destroy_workqueue() finally. Otherwise
     * cpu_up() can hit the uninitialized cwq once we drop the
     * lock.
     */
    for_each_present_cpu(cpu) {
        cwq = init_cpu_workqueue(wq, cpu);
        if (err || !cpu_online(cpu))
            continue;
        err = create_workqueue_thread(cwq, cpu);
        start_workqueue_thread(cwq, cpu);
    }
//    cpu_maps_update_done();

    if (err) {
        destroy_workqueue(wq);
        wq = NULL;
    }
    return wq;
}

/* Destroy a worker thread */
void destroy_workqueue(struct workqueue_struct *wq)
{
	panic("%s()\n",__func__);
#if 0
    const cpumask_t *cpu_map = wq_cpu_map(wq);
    int cpu;

    cpu_maps_update_begin();
    spin_lock(&workqueue_lock);
    list_del(&wq->list);
    spin_unlock(&workqueue_lock);

    for_each_cpu_mask(cpu, *cpu_map)
        cleanup_workqueue_thread(per_cpu_ptr(wq->cpu_wq, cpu));
    cpu_maps_update_done();
    
    free_percpu(wq->cpu_wq);
    kmem_free(wq);
#endif
}

void workq_init(void)
{
	_KDBG("\n");
	keventd_wq = create_workqueue("events");
	BUG_ON(!keventd_wq);
}

int cancel_work_sync(struct work_struct *work)
{
    panic("%s() not implemented\n",__func__);
    //return __cancel_work_timer(work, NULL);
}

/* vim: set noexpandtab noai ts=4 sw=4: */

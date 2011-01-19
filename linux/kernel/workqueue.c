#include <lwk/linux_compat.h>

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/wait.h>

#undef _KDBG
#define _KDBG(fmt,arg...)

struct cpu_workqueue_struct {
	spinlock_t lock;
	struct list_head worklist;
	wait_queue_head_t more_work;
	struct work_struct *current_work;

	struct workqueue_struct *wq;
	struct task_struct *thread;
} ____cacheline_aligned;


struct workqueue_struct {
	struct cpu_workqueue_struct *cpu_wq;
	spinlock_t		lock;
	struct list_head	list;
	const char		name[64];
};

static struct workqueue_struct *keventd_wq;

#define NUM_WQS 40
static struct workqueue_struct *_wqs[NUM_WQS] = {0,};

static struct workqueue_struct *
get_wq_data(struct work_struct *work)
{
    return (void *) (atomic_long_read(&work->data) & WORK_STRUCT_WQ_DATA_MASK);
}

/*
 * Set the workqueue on which a work item is to be run
 * - Must *only* be called if the pending flag is set
 */
static void
set_wq_data(struct work_struct *work, struct workqueue_struct *wq)
{
    unsigned long new;

    BUG_ON(!work_pending(work));

    new = (unsigned long) wq | (1UL << WORK_STRUCT_PENDING);
    new |= WORK_STRUCT_FLAG_MASK & *work_data_bits(work);
    atomic_long_set(&work->data, new);
}


static void
insert_work(struct workqueue_struct *wq, struct work_struct *work, int tail)
{
    //_KDBG("wq=%p work=%p name=%s\n", wq, work, wq->name );
    set_wq_data(work, wq);
    //smp_wmb();
    if (tail)
	list_add_tail(&work->entry, &wq->list);
    else
	list_add(&work->entry, &wq->list);
    //wake_up(&cwq->more_work);
}

/* Preempt must be disabled. */
static void
__queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    unsigned long flags;

    spin_lock_irqsave(&wq->lock, flags);
    insert_work(wq, work, 1);
    spin_unlock_irqrestore(&wq->lock, flags);

    /* wake up linux worker thread */
    linux_wakeup();
}

int
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    int ret = 0;

    _KDBG("name=%s work=%p\n",wq->name,work);
    if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
	BUG_ON(!list_empty(&work->entry));
	__queue_work(wq, work);
	put_cpu();
	ret = 1;
    }
    return ret;
}

struct cpu_workqueue_struct *wq_per_cpu(struct workqueue_struct *wq, int cpu)
{
	_KDBG("name=%s cpu=%d\n",wq->name,cpu);
/*	if (unlikely(is_wq_single_threaded(wq)))
	cpu = singlethread_cpu; */
	return per_cpu_ptr(wq->cpu_wq, cpu);
}

int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	int ret = 0;

	_KDBG("cpu=%d name=%s work=%p\n",cpu,wq->name,work);
	if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
		BUG_ON(!list_empty(&work->entry));
		__queue_work(wq_per_cpu(wq, cpu), work);
		ret = 1;
	}
	return ret;
}


void
flush_workqueue(struct workqueue_struct *wq)
{
    panic("In flush_workqueue()... needs to be implemented.");
}

void
flush_scheduled_work(void)
{
    _KDBG("\n");
    flush_workqueue(keventd_wq);
}

int
schedule_work(struct work_struct *work)
{
    _KDBG("\n");
    return queue_work(keventd_wq, work);
}

struct workqueue_struct *
__create_workqueue_key(const char *name, int singlethread, int freezeable,
		       struct lock_class_key *key, const char *lock_name)
{
    struct workqueue_struct *wq = kmem_alloc(sizeof(struct workqueue_struct ));

    _KDBG("wq=%p name=%s\n",wq,name);

    INIT_LIST_HEAD(&wq->list);
    spin_lock_init(&wq->lock);
    strcpy( (char*) wq->name, name);

    int i;
    for ( i = 0; i < NUM_WQS; i++ ) {
	if ( ! _wqs[i] ) {
	    _wqs[i] = wq;
	    break;
	}
    }
    if ( i == NUM_WQS ) {
	printk("__create_workqueue() failed\n");while(1);
    }
    return wq;
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
    int i;
    _KDBG("wq=%p\n",wq);
    for ( i = 0; i < NUM_WQS; i++ ) {
	if ( _wqs[i] == wq ) {
	    _wqs[i] = NULL;
	    break;
	}
    }
    if ( i == NUM_WQS ) {
	printk("destroy_workqueue() failed\n");while(1);
    }
}

void
delayed_work_timer_fn(unsigned long __data)
{
    struct delayed_work *dwork = (struct delayed_work *)__data;
    struct workqueue_struct *wq = get_wq_data(&dwork->work);

    _KDBG("\n");
    __queue_work(wq, &dwork->work);
}

int
queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
		      struct delayed_work *dwork, unsigned long delay)
{
    int ret = 0;
    struct timer_list *timer = &dwork->timer;
    struct work_struct *work = &dwork->work;
    _KDBG("\n");

    if (!test_and_set_bit(WORK_STRUCT_PENDING, work_data_bits(work))) {
	BUG_ON(timer_pending(timer));
	BUG_ON(!list_empty(&work->entry));

	set_wq_data(work, wq );

	timer->expires = jiffies + delay;
	timer->data = (unsigned long)dwork;
	timer->function = delayed_work_timer_fn;

	add_timer(timer);
	ret = 1;
    }
    return ret;
}

int
queue_delayed_work(struct workqueue_struct *wq,
		   struct delayed_work *dwork, unsigned long delay)
{
    _KDBG("wq=%p work=%p name=%s delay=%ld\n", wq, dwork, wq->name, delay );
    if ( delay == 0 )
	return queue_work( wq, &dwork->work );

    return queue_delayed_work_on(-1, wq, dwork, delay);
}

int schedule_delayed_work(struct delayed_work *dwork,
			  unsigned long delay)
{
	_KDBG("\n");
	return queue_delayed_work(keventd_wq, dwork, delay);
}


static int __cancel_work_timer(struct work_struct *work,
				struct timer_list* timer)
{
	int ret;

	do {
		ret = (timer && likely(del_timer(timer)));
		//if (!ret)
		//	ret = try_to_grab_pending(work);
		//wait_on_work(work);
	} while (unlikely(ret < 0));

	work_clear_pending(work);
	return ret;
}

int cancel_delayed_work_sync(struct delayed_work *dwork)
{
	_KDBG("\n");
	return __cancel_work_timer(&dwork->work, &dwork->timer);
}

int cancel_work_sync(struct work_struct *work)
{
	_KDBG("\n");
	return __cancel_work_timer(work, NULL);
}

void
init_workqueues(void)
{
    // create_workqueue sets a pointer in _wqs[] but we don't want
    // kevent_wq to be in _wqs[] so we zero _wqs[] after we create kevent_wq
    keventd_wq = create_workqueue("events");
    int i;
    for ( i = 0; i < NUM_WQS; i++ ) {
	_wqs[i] = NULL;
    }
}

void
run_workqueues(void) {

    int i;

    unsigned long flags;
    _KDBG("\n");
    for ( i = 0; i < NUM_WQS; i++ ) {
	if ( _wqs[i] ) {
	    struct work_struct *work = NULL;
	    spin_lock_irqsave(&_wqs[i]->lock,flags);
	    if ( ! list_empty( &_wqs[i]->list ) ) {
		//_KDBG("wq=%p name=%s\n", _wqs[i], _wqs[i]->name);
		work =
		    list_entry( _wqs[i]->list.next, struct work_struct, entry);
		list_del_init( _wqs[i]->list.next );
	    }
	    spin_unlock_irqrestore(&_wqs[i]->lock,flags);
	    if ( work ) {
		work_clear_pending(work);
#if 0
		_KDBG("wq=%p wq_name=%s work=%p func=%p\n",
					_wqs[i],_wqs[i]->name,work,work->func);
#endif
		work->func((struct work_struct *)work);
    //            _KDBG("done\n");
	    }
	}
    }
    struct work_struct *work = NULL;
    spin_lock_irqsave(&keventd_wq->lock,flags);
    if ( ! list_empty( &keventd_wq->list ) ) {
	//_KDBG("wq=%p name=%s\n", keventd_wq, keventd_wq->name);
	work = list_entry( keventd_wq->list.next, struct work_struct, entry);
	list_del_init( keventd_wq->list.next );
    }
    spin_unlock_irqrestore(&keventd_wq->lock,flags);
    if ( work ) {
	work_clear_pending(work);
#if 0
	_KDBG("wq=%p wq_name=%s work=%p func=%p\n",
				keventd_wq,keventd_wq->name, work, work->func);
#endif
	work->func((struct work_struct *)work);
    //    _KDBG("done\n");
    }
}

void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
	unsigned long flags;

	_KDBG("\n");
	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	if (list_empty(&wait->task_list))
		__add_wait_queue(q, wait);
	set_current_state(state);
	spin_unlock_irqrestore(&q->lock, flags);
}

void finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;

	_KDBG("\n");
	__set_current_state(TASK_RUNNING);

	if (!list_empty_careful(&wait->task_list)) {
		spin_lock_irqsave(&q->lock, flags);
		list_del_init(&wait->task_list);
		spin_unlock_irqrestore(&q->lock, flags);
	}
}

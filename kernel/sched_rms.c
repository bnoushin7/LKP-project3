/*
 * RMS event logging information
 */
#include <linux/sched.h>
#include <linux/slab.h>

struct rms_event_log *rms_event_log;// = { .head = 0, .tail = 0 };

struct rms_event_log *get_rms_event_log(void)
{
    return rms_event_log;
}

void init_rms_event_log(void)
{
    char msg[RMS_MSG_SIZE];

    rms_event_log = kmalloc(sizeof(*rms_event_log), GFP_KERNEL);
    if (!rms_event_log) {
        printk(KERN_ALERT "Failed to allocate rms_event_log!");
        rms_event_log = NULL;
    }
    else {
        snprintf(msg, RMS_MSG_SIZE, "init_rms_event_log");
        rms_event_log->head = rms_event_log->tail = 0;
        register_rms_event(sched_clock(), msg, RMS_DEBUG);
    }
}

void _do_register_rms_event(unsigned long long t, char *m, int a)
{
    struct rms_event *event;
    event = &rms_event_log->rms_events[rms_event_log->tail];
    event->action = a;
    event->timestamp = t;
    strncpy(event->msg, m, RMS_MSG_SIZE - 1);
    rms_event_log->tail++;
}


void register_rms_event(unsigned long long t, char *m, int a)
{
    if (rms_event_log->tail < RMS_MAX_EVENTS)
        _do_register_rms_event(t, m, a);
    else 
        printk(KERN_ALERT "rms_event_log: full\n");
}

void cleanup_rms_event_log(void) {
    kfree(rms_event_log);
    rms_event_log = NULL;
}

/* 
 * RMS scheduling functions 
 */

void init_rms_rq(struct rms_rq *rms_rq)
{
	rms_rq->rms_rb_root=RB_ROOT;
	INIT_LIST_HEAD(&rms_rq->rms_list_head);
	atomic_set(&rms_rq->nr_running,0);
}

void add_rms_task_2_list(struct rms_rq *rq, struct task_struct *p)
{
	struct list_head *ptr=NULL;
	struct rms_task *new=NULL, *rms_task=NULL;

	if (rq && p) {
		new=(struct rms_task *) kzalloc(sizeof(struct rms_task),GFP_KERNEL);
		if (new) {
			rms_task=NULL;
			new->task=p;
			
            list_for_each(ptr,&rq->rms_list_head){
				rms_task=list_entry(ptr,struct rms_task, rms_list_node);
				if (rms_task) {
					if (new->task->rms_id < rms_task->task->rms_id) {
						list_add(&new->rms_list_node,ptr);
					}
				}
			}
			list_add(&new->rms_list_node,&rq->rms_list_head);
		}
		else {
			printk(KERN_ALERT "add_rms_task_2_list: kzalloc\n");
		}
	}
	else {
		printk(KERN_ALERT "add_rms_task_2_list: null pointers\n");
	}
}

struct rms_task * find_rms_task_list(struct rms_rq *rq, struct task_struct *p)
{
	struct list_head *ptr=NULL;
	struct rms_task *rms_task=NULL;

	if (rq && p) {
		list_for_each(ptr,&rq->rms_list_head){
			rms_task=list_entry(ptr,struct rms_task, rms_list_node);
			if (rms_task) {
				if (rms_task->task->rms_id == p->rms_id) {
					return rms_task;
				}
			}
		}
	}

	return NULL;
}

void rem_rms_task_list(struct rms_rq *rq, struct task_struct *p)
{
	struct list_head *ptr=NULL,*next=NULL;
	struct rms_task *rms_task=NULL;

	if (rq && p) {
		list_for_each_safe(ptr,next,&rq->rms_list_head){
			rms_task=list_entry(ptr,struct rms_task, rms_list_node);
			if (rms_task) {
				if (rms_task->task->rms_id == p->rms_id) {
					list_del(ptr);
					kfree(rms_task);
					return;
				}
			}
		}
	}
}

/*
 * rb_tree functions.
 */

void remove_rms_task_rb_tree(struct rms_rq *rq, struct rms_task *p)
{
	rb_erase(&(p->rms_rb_node),&(rq->rms_rb_root));
	p->rms_rb_node.rb_left=p->rms_rb_node.rb_right=NULL;
}

void insert_rms_task_rb_tree(struct rms_rq *rq, struct rms_task *p)
{
	struct rb_node **node=NULL;
	struct rb_node *parent=NULL;
	struct rms_task *entry=NULL;

	node=&rq->rms_rb_root.rb_node;

	while (*node!=NULL) {
		parent=*node;
		entry=rb_entry(parent, struct rms_task,rms_rb_node);
		if (entry) {
			if (p->task->period < entry->task->period) { /* question*/
				node=&parent->rb_left;
			} else {
				node=&parent->rb_right;
			}
		}
	}

	rb_link_node(&p->rms_rb_node,parent,node);
	rb_insert_color(&p->rms_rb_node,&rq->rms_rb_root);
}

struct rms_task * shortest_period_rms_task_rb_tree(struct rms_rq *rq)
{
	struct rb_node *node=NULL;
	struct rms_task *p=NULL;

	node=rq->rms_rb_root.rb_node;

	if (node==NULL)
		return NULL;

	while(node->rb_left!=NULL){
		node=node->rb_left;
	}

	p=rb_entry(node, struct rms_task,rms_rb_node);
	return p;
}



static void check_preempt_curr_rms(struct rq *rq, struct task_struct *p, 
                                   int flags)
{
	struct rms_task *t=NULL,*curr=NULL;

	if(rq->curr->policy!=SCHED_RMS){
		resched_task(rq->curr);
	}
	else{
		t=shortest_period_rms_task_rb_tree(&rq->rms_rq);
		if(t){
			curr=find_rms_task_list(&rq->rms_rq,rq->curr);
			if(curr){
				if(t->task->period < curr->task->period) /* question  */
					resched_task(rq->curr);
			}
			else{
				printk(KERN_ALERT "check_preempt_curr_rms\n");
			}
		}
	}
}



static struct task_struct *pick_next_task_rms(struct rq *rq)
{
	struct rms_task *t=NULL;

	t=shortest_period_rms_task_rb_tree(&rq->rms_rq);
	if(t){
		return t->task;
	}

	return NULL;
}



static void enqueue_task_rms(struct rq *rq, struct task_struct *p, int wakeup, 
                             bool head)
{
	struct rms_task *t=NULL;
    char msg[RMS_MSG_SIZE];

	if(p){
		t=find_rms_task_list(&rq->rms_rq,p);
		if(t){
		/*question*/	//t->absolute_deadline=sched_clock()+p->deadline;
			insert_rms_task_rb_tree(&rq->rms_rq, t);
			atomic_inc(&rq->rms_rq.nr_running);
            snprintf(msg, RMS_MSG_SIZE, "Task (%d, %d), period %llu)",
                     p->rms_id, p->pid, p->period);
            register_rms_event(sched_clock(), msg, RMS_ENQUEUE);
		}
		else{
			printk(KERN_ALERT "enqueue_task_rms\n");
		}
	}
}



static void dequeue_task_rms(struct rq *rq, struct task_struct *p, int sleep)
{
	struct rms_task *t=NULL;
    char msg[RMS_MSG_SIZE];

	if(p){
		t=find_rms_task_list(&rq->rms_rq,p);
		if(t){
            snprintf(msg, RMS_MSG_SIZE, "Task (%d, %d), period %llu)",
                     p->rms_id, p->pid, p->period);
            register_rms_event(sched_clock(), msg, RMS_DEQUEUE);
			remove_rms_task_rb_tree(&rq->rms_rq, t);
			atomic_dec(&rq->rms_rq.nr_running);
			if(t->task->state==TASK_DEAD || t->task->state==EXIT_DEAD 
               || t->task->state==EXIT_ZOMBIE){
				rem_rms_task_list(&rq->rms_rq,t->task);
			}
		}
		else{
			printk(KERN_ALERT "dequeue_task_rms\n");
		}
	}
}



unsigned int get_rr_interval_rms(struct rq *rq, struct task_struct *task)
{
    /*question*/
	/*
     * Time slice is 0 for SCHED_FIFO tasks
     */
        if (task->policy == SCHED_RR)
                return DEF_TIMESLICE;
        else
                return 0;
}


static void prio_changed_rms(struct rq *rq, struct task_struct *p,
			    int oldprio, int running)
{
}

static void yield_task_rms(struct rq *rq)
{
}

static void switched_to_rms(struct rq *rq, struct task_struct *p,
                           int running)
{
}

static void put_prev_task_rms(struct rq *rq, struct task_struct *prev)
{
}

static void task_tick_rms(struct rq *rq, struct task_struct *p, int queued)
{
}

static void set_curr_task_rms(struct rq *rq)
{
}

#ifdef CONFIG_SMP
static int select_task_rq_rms(struct rq *rq, struct task_struct *p, int sd_flag,
                              int flags)
{
    /*question*/
    //	struct rq *rq = task_rq(p);
	if (sd_flag != SD_BALANCE_WAKE)
		return smp_processor_id();
	return task_cpu(p);
}
#endif
static const struct sched_class rms_sched_class = {
	.next 			= &rt_sched_class,
	.enqueue_task		= enqueue_task_rms,
	.dequeue_task		= dequeue_task_rms,
	.check_preempt_curr	= check_preempt_curr_rms,
	.pick_next_task		= pick_next_task_rms,
	.put_prev_task		= put_prev_task_rms,
	.set_curr_task          = set_curr_task_rms,
	.task_tick		= task_tick_rms,
	.switched_to		= switched_to_rms,
	.yield_task		= yield_task_rms,
	.get_rr_interval	= get_rr_interval_rms,
	.prio_changed		= prio_changed_rms,
};

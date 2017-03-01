#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>

static void *rms_seq_start(struct seq_file *s, loff_t *pos)
{
    struct rms_event_log *log = NULL;
    log = get_rms_event_log();
    if (!log || (log->head >= log->tail))
        return NULL;

    return &log->rms_events[log->head];
}

static int rms_seq_show(struct seq_file *s, void *v)
{
    struct rms_event *val_ptr = NULL; 
    val_ptr = (struct rms_event *) v;

    if (val_ptr->action == RMS_ENQUEUE)
        seq_printf(s, "@t=%llu: ENQUEUED %s\n", val_ptr->timestamp, 
                   val_ptr->msg);
    else if (val_ptr->action == RMS_DEQUEUE)
        seq_printf(s, "@t=%llu: DEQUEUED %s\n", val_ptr->timestamp, 
                   val_ptr->msg);
    else if (val_ptr->action == RMS_CONTEXT_SWITCH)
        seq_printf(s, "@t=%llu: C_SWITCH %s\n", val_ptr->timestamp, 
                   val_ptr->msg);
    else
        seq_printf(s, "@t=%llu: (%d)     %s\n", val_ptr->timestamp, 
                   val_ptr->action, val_ptr->msg);
    return 0;
}

static void *rms_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    struct rms_event_log *log = NULL;
    log = get_rms_event_log();

    if (!log)
        return NULL;

    log->head++;
    if (log->head >= log->tail)
        return NULL;
    
    return &log->rms_events[log->head];

}

static void rms_seq_stop(struct seq_file *s, void *v)
{
    return;
}

static const struct seq_operations rms_seq_ops = {
    .start  = rms_seq_start,
    .next   = rms_seq_next,
    .stop   = rms_seq_stop,
    .show   = rms_seq_show
};

static int rms_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &rms_seq_ops);
}

static const struct file_operations proc_rms_operations = {
    .open       = rms_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release,
};

static int __init proc_rms_init(void)
{
	proc_create("rms_event", 0, NULL, &proc_rms_operations);
	return 0;
}

static void __exit proc_rms_exit(void)
{
    remove_proc_entry("rms_event", NULL);
    cleanup_rms_event_log();
}

module_init(proc_rms_init);

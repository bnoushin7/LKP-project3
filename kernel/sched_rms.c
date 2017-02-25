/*
 * RMS event logging information
 */
#include <linux/sched.h>

struct rms_event_log rms_event_log = { .head = 0, .tail = 0 };

struct rms_event_log *get_rms_event_log(void)
{
    return &rms_event_log;
}

void init_rms_event_log(void)
{
    char msg[RMS_MSG_SIZE];
    snprintf(msg, RMS_MSG_SIZE, "init_rms_event_log");
    register_rms_event(sched_clock(), msg, RMS_DEBUG);
}

void _do_register_rms_event(unsigned long long t, char *m, int a)
{
    struct rms_event *event;
    event = &rms_event_log.rms_events[rms_event_log.tail];
    event->action = a;
    event->timestamp = t;
    strncpy(event->msg, m, RMS_MSG_SIZE - 1);
    rms_event_log.tail++;
}


void register_rms_event(unsigned long long t, char *m, int a)
{
    if (rms_event_log.tail < RMS_MAX_EVENTS)
        _do_register_rms_event(t, m, a);
    //else 
    //    printk(KERN_ALERT "rms_event_log: full\n");
}

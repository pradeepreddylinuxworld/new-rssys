#ifndef _LID_SWITCH_H
#define _LID_SWITCH_H

extern int lid_close;
extern int lcm_is_resume;
extern bool hdmi_connected;
extern bool hdmi_disconnected;
static DECLARE_WAIT_QUEUE_HEAD(waiter1);

extern wait_queue_head_t waiter2;
extern wait_queue_head_t waiter3;
extern wait_queue_head_t waiter4;
#endif

/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2017-2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 *  process, and are protected by trade secret or copyright law.  Dissemination
 *  of this information or reproduction of this material is strictly forbidden
 *  unless prior written permission is obtained from Syntiant Corporation.
 */

#ifndef LINUX_BACKPORTS
#define LINUX_BACKPORTS

#ifndef WRITE_ONCE
#ifdef ASSIGN_ONCE
#define WRITE_ONCE(x,val) ASSIGN_ONCE(val, x)
#else
#include <linux/compiler.h>

static __always_inline
void __write_once_size(volatile void *p, void *res, int size)
{
       switch (size) {
       case 1: *(volatile __u8 *)p = *(__u8 *)res; break;
       case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
       case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
       case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
       default:
               barrier();
               __builtin_memcpy((void *)p, (const void *)res, size);
               barrier();
       }
}

#define WRITE_ONCE(x, val) \
({                                                     \
       union { typeof(x) __val; char __c[1]; } __u =   \
               { .__val = (__force typeof(x)) (val) }; \
       __write_once_size(&(x), __u.__c, sizeof(x));    \
       __u.__val;                                      \
})
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0))
#define u64_to_user_ptr(x) (           \
{                                      \
       typecheck(u64, x);              \
       (void __user *)(uintptr_t)x;    \
}                                      \
)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0))
#include <linux/irq.h>
#include <linux/irqdesc.h>

/* Two alternatives:
 * A) back-port the possibility to call irq_wake_thread from
      3.15 tree to enable external calls
 * B) implement the logic here. This is generally discouraged
      since it requires sync of enum values with kernel
 *
 * A) is the cleanest solution, but requires kernel changes, so for now, do B)
 * Following is copied from  kernel/kernel/irq/internals.h -
 * make sure this matches that file
 * NB: we are ignoring the explicit warning in kernel/kernel/irq/internals.h
 *     with respect to re-using
 *     any information from that file exernally...
 */
enum {
	IRQTF_RUNTHREAD,
	IRQTF_WARNED,
	IRQTF_AFFINITY,
	IRQTF_FORCED_THREAD,
};

static void __irq_wake_thread(struct irq_desc *desc, struct irqaction *action)
{
	if (action->thread->flags & PF_EXITING)
		return;

	if (test_and_set_bit(IRQTF_RUNTHREAD, &action->thread_flags))
		return;

	desc->threads_oneshot |= action->thread_mask;
	atomic_inc(&desc->threads_active);
	wake_up_process(action->thread);
}

void irq_wake_thread(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	unsigned long flags;

	if (!desc)
		return;

	raw_spin_lock_irqsave(&desc->lock, flags);
	for (action = desc->action; action; action = action->next) {
		if (action->dev_id == dev_id) {
			if (action->thread)
				__irq_wake_thread(desc, action);
			break;
		}
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

#endif
#endif

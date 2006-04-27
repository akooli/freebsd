/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <sys/param.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_pthread_setschedparam, pthread_setschedparam);

/*
 * Set a thread's scheduling parameters, this should be done
 * in kernel, doing it in userland is no-op.
 */
int
_pthread_setschedparam(pthread_t pthread, int policy, 
	const struct sched_param *param)
{
	struct pthread *curthread = _get_curthread();
	int	ret = 0;

	if ((param == NULL) || (policy < SCHED_FIFO) || (policy > SCHED_RR)) {
		ret = EINVAL;
	} else if (param->sched_priority < _thr_priorities[policy-1].pri_min ||
		   param->sched_priority > _thr_priorities[policy-1].pri_max) {
		ret = ENOTSUP;
	} else if ((ret = _thr_ref_add(curthread, pthread, /*include dead*/0))
	    == 0) {
		/*
		 * Lock the threads scheduling queue while we change
		 * its priority:
		 */
		THR_THREAD_LOCK(curthread, pthread);
		if (pthread->state == PS_DEAD) {
			THR_THREAD_UNLOCK(curthread, pthread);
			_thr_ref_delete(curthread, pthread);
			return (ESRCH);
		}

		/* Set the scheduling policy: */
		pthread->attr.sched_policy = policy;

		if (param->sched_priority == pthread->base_priority)
			/*
			 * There is nothing to do; unlock the threads
			 * scheduling queue.
			 */
			THR_THREAD_UNLOCK(curthread, pthread);
		else {
			pthread->base_priority = param->sched_priority;

			/* Recalculate the active priority: */
			pthread->active_priority = MAX(pthread->base_priority,
			    pthread->inherited_priority);

			THR_THREAD_UNLOCK(curthread, pthread);
		}
		_thr_ref_delete(curthread, pthread);
	}
	return (ret);
}

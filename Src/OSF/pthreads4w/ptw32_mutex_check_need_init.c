/*
 * ptw32_mutex_check_need_init.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
 *
 * --------------------------------------------------------------------------
 *
 *   Pthreads4w - POSIX Threads for Windows
 *   Copyright 1998 John E. Bossom
 *   Copyright 1999-2018, Pthreads4w contributors
 *
 *   Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *   The current list of contributors is contained
 *   in the file CONTRIBUTORS included with the source
 *   code distribution. The list can also be seen at the
 *   following World Wide Web location:
 *
 *   https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sl_pthreads4w.h>
#pragma hdrstop

static struct pthread_mutexattr_t_ __ptw32_recursive_mutexattr_s = {PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_STALLED};
static struct pthread_mutexattr_t_ __ptw32_errorcheck_mutexattr_s = {PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_ERRORCHECK, PTHREAD_MUTEX_STALLED};
static pthread_mutexattr_t __ptw32_recursive_mutexattr = &__ptw32_recursive_mutexattr_s;
static pthread_mutexattr_t __ptw32_errorcheck_mutexattr = &__ptw32_errorcheck_mutexattr_s;

INLINE int __ptw32_mutex_check_need_init(pthread_mutex_t * mutex)
{
	int result = 0;
	pthread_mutex_t mtx;
	__ptw32_mcs_local_node_t node;
	__ptw32_mcs_lock_acquire(&__ptw32_mutex_test_init_lock, &node);
	/*
	 * We got here possibly under race
	 * conditions. Check again inside the critical section
	 * and only initialise if the mutex is valid (not been destroyed).
	 * If a static mutex has been destroyed, the application can
	 * re-initialise it only by calling pthread_mutex_init()
	 * explicitly.
	 */
	mtx = *mutex;
	if(mtx == PTHREAD_MUTEX_INITIALIZER) {
		result = pthread_mutex_init(mutex, NULL);
	}
	else if(mtx == PTHREAD_RECURSIVE_MUTEX_INITIALIZER) {
		result = pthread_mutex_init(mutex, &__ptw32_recursive_mutexattr);
	}
	else if(mtx == PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) {
		result = pthread_mutex_init(mutex, &__ptw32_errorcheck_mutexattr);
	}
	else if(mtx == NULL) {
		/*
		 * The mutex has been destroyed while we were waiting to
		 * initialise it, so the operation that caused the
		 * auto-initialisation should fail.
		 */
		result = EINVAL;
	}
	__ptw32_mcs_lock_release(&node);
	return result;
}

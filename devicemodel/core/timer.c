/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/timerfd.h>

#include "vmmapi.h"
#include "mevent.h"
#include "timer.h"

/* We can use timerfd and epoll mechanism to emulate kinds of timers like
 * PIT/RTC/WDT/PMTIMER/... in device model under Linux.
 * Compare with sigevent mechanism, timerfd has a advantage that it could
 * avoid race condition on resource accessing in the async sigev thread.
 *
 * Please note timerfd and epoll are all Linux specific. If the code need to be
 * ported to other OS, we can modify the api with POSIX timers and sigevent
 * mechanism.
 */

static void
timer_handler(int fd __attribute__((unused)),
		  enum ev_type t __attribute__((unused)),
		  void *arg)
{
	struct acrn_timer *timer = arg;
	uint64_t buf;
	int32_t size;

	if (timer == NULL) {
		return;
	}

	/* Consume I/O event for default EPOLLLT type.
	 * Here is a temporary solution, the processing could be moved to
	 * mevent.c once EVF_TIMER is supported.
	 */
	size = read(timer->fd, &buf, sizeof(buf));
	if (size < 1) {
		fprintf(stderr, "acrn_timer read timerfd error!");
		return;
	}

	if (timer->callback != NULL) {
		(*timer->callback)(timer->callback_param);
	}
}

int32_t
acrn_timer_init(struct acrn_timer *timer, void (*cb)(void *), void *param)
{
	if ((timer == NULL) || (cb == NULL)) {
		return -1;
	}

	timer->fd = -1;
	if ((timer->clockid == CLOCK_REALTIME) ||
			(timer->clockid == CLOCK_MONOTONIC)) {
		timer->fd = timerfd_create(timer->clockid,
					TFD_NONBLOCK | TFD_CLOEXEC);
	} else {
		perror("acrn_timer clockid is not supported.\n");
	}

	if (timer->fd <= 0) {
		perror("acrn_timer create failed.\n");
		return -1;
	}

	timer->mevp = mevent_add(timer->fd, EVF_READ, timer_handler, timer, NULL, NULL);
	if (timer->mevp == NULL) {
		close(timer->fd);
		perror("acrn_timer mevent add failed.\n");
		return -1;
	}

	timer->callback = cb;
	timer->callback_param = param;

	return 0;
}

void
acrn_timer_deinit(struct acrn_timer *timer)
{
	if (timer == NULL) {
		return;
	}

	if (timer->mevp != NULL) {
		mevent_delete_close(timer->mevp);
		timer->mevp = NULL;
	}

	timer->fd = -1;
	timer->callback = NULL;
	timer->callback_param = NULL;
}

int32_t
acrn_timer_settime(struct acrn_timer *timer, struct itimerspec *new_value)
{
	if (timer == NULL) {
		return -1;
	}

	return timerfd_settime(timer->fd, 0, new_value, NULL);
}

int32_t
acrn_timer_gettime(struct acrn_timer *timer, struct itimerspec *cur_value)
{
	if (timer == NULL) {
		return -1;
	}

	return timerfd_gettime(timer->fd, cur_value);
}

#include <stdint.h>

#include "frame_limiter.h"

#define NS_PER_SECOND 1000000000

/*
 * run this at the beginning of a frame
 * 	so there is a reference for how long the frame took to process
 */
void frame_limiter_frame_start (Frame_limiter* limiter)
{
	timespec_get (&limiter->start_time, TIME_UTC);
}

/*
 * run this at the end of a frame
 *
 * will block the thread for the difference between the time needed to hit the target framerate
 * 	and how long the frame took to process
 */
void frame_limiter_frame_end (Frame_limiter* limiter, uint32_t target)
{
	// hard limit framerate to 200, just because
	if (target < 1 || 200 < target)
	{
		target = 200;
	}

	timespec_get (&limiter->current_time, TIME_UTC);

	// how long the frame took to process
	long delta = limiter->current_time.tv_nsec - limiter->start_time.tv_nsec;
	if (delta < 0)
	{
		delta += NS_PER_SECOND;
	}

	limiter->start_time = limiter->current_time;
	// since pthread_cond_timedwait takes an absolute time, not a relative time
	// 	set next_time to the current time, then add to it later
	limiter->next_time = limiter->current_time;

	// the time to wait, in nanoseconds
	long time_to_block = NS_PER_SECOND / target;

	time_to_block -= delta;

	// if it took this frame longer than our target time
	// 	go to the next frame
	if (time_to_block < 0)
	{
		return;
	}

	limiter->next_time.tv_nsec += time_to_block;
	// timespec stores seconds and nanosecds separately
	// 	so if our nanoseconds rolled over to the next second
	// 		we need to set seconds and nanoseconds accordingly
	if (limiter->next_time.tv_nsec > NS_PER_SECOND)
	{
		limiter->next_time.tv_sec += 1;
		limiter->next_time.tv_nsec -= NS_PER_SECOND;
	}

	// all the pthreads stuff needed to do a timedwait
	pthread_mutex_lock (&limiter->fps_lock);
	pthread_cond_timedwait (&limiter->fps_lock_condition, &limiter->fps_lock, &limiter->next_time);
	pthread_mutex_unlock (&limiter->fps_lock);
}

/*
 * initialize a frame limiter
 */
void frame_limiter_initialize (Frame_limiter* limiter)
{
	pthread_mutex_init (&limiter->fps_lock, NULL);
	pthread_cond_init (&limiter->fps_lock_condition, NULL);

	struct timespec zero = {0, 0};
	limiter->start_time = zero;
	limiter->current_time = zero;
	limiter->next_time = zero;
}

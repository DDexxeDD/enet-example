#ifndef _frame_limiter_h
#define _frame_limiter_h

#include <time.h>
#include <pthread.h>
#include <stdint.h>

/*
 * this is a frames per second limiter
 * it uses pthread_cond_timedwait to block the calling thread
 * 	for an amount of time that will get close to the desired (target) frame rate
 */

// pthreads use timespec, so instead of worrying about converting between sokol time and timespec
// 	just use timespecs here
typedef struct Frame_limiter_s
{
	pthread_mutex_t fps_lock;
	pthread_cond_t fps_lock_condition;
	struct timespec start_time;
	struct timespec current_time;
	struct timespec next_time;
} Frame_limiter;

void frame_limiter_frame_end (Frame_limiter* limiter, uint32_t target);
void frame_limiter_initialize (Frame_limiter* limiter);
void frame_limiter_frame_start (Frame_limiter* limiter);

#endif

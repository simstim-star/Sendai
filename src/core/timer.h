#pragma once
#include <basetsd.h>
#include <stdbool.h>
#include <windows.h>

// Integer format represents time using 10,000,000 ticks per second.
#define TICKS_PER_SECOND 10000000

inline double ticks_to_seconds(UINT64 ticks) {
	return ((double)ticks) / TICKS_PER_SECOND;
}
inline UINT64 seconds_to_ticks(double seconds) {
	return (UINT64)(seconds * TICKS_PER_SECOND);
}

// Helper class for animation and simulation timing.
typedef struct snc_step_timer_t {
	// Source timing data uses QueryPerformanceCounter units.
	LARGE_INTEGER qpc_frequency;
	LARGE_INTEGER qpc_last_time;
	UINT64 qpc_max_delta;
	// Derived timing data uses a canonical tick format.
	UINT64 elapsed_ticks;
	UINT64 total_ticks;
	UINT64 leftover_ticks;
	// Members for tracking the framerate.
	UINT32 frame_count;
	UINT32 frames_per_second;
	UINT32 frames_this_second;
	UINT64 qpc_second_counter;
	// Members for configuring fixed timestep mode.
	bool is_fixed_time_step;
	UINT64 target_elapsed_ticks;
} snc_step_timer_t;

void snc_steptimer_init(snc_step_timer_t *st);

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to avoid having the fixed timestep logic attempt a set of catch-up
// Update calls.
inline void snc_reset_elapsed_time(snc_step_timer_t *st);

typedef void (*LPUPDATEFUNC)(void);

// Update timer state, calling the specified Update function the appropriate number of times.
void snc_tick_with_update_fn(snc_step_timer_t *st, LPUPDATEFUNC update);

// Only updates timer state, but doesn't do anything else
inline void snc_tick(snc_step_timer_t *st) {
	snc_tick_with_update_fn(st, NULL);
}
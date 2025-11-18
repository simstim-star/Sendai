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
typedef struct SC_Step_Timer {
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
} SC_Step_Timer;

void SC_steptimer_init(SC_Step_Timer *st);

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to avoid having the fixed timestep logic attempt a set of catch-up
// Update calls.
inline void SC_reset_elapsed_time(SC_Step_Timer *st);

typedef void (*LPUPDATEFUNC)(void);

// Update timer state, calling the specified Update function the appropriate number of times.
void SC_tick_with_update_fn(SC_Step_Timer *st, LPUPDATEFUNC update);

// Only updates timer state, but doesn't do anything else
inline void SC_tick(SC_Step_Timer *st) {
	SC_tick_with_update_fn(st, NULL);
}
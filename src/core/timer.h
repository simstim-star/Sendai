/*
 * Based on https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MeshShaders/src/DynamicLOD/StepTimer.h
 */

#pragma once
#include <basetsd.h>
#include <stdbool.h>
#include <windows.h>

// Integer format represents time using 10,000,000 ticks per second.
#define TICKS_PER_SECOND 10000000

inline float ticks_to_seconds_FLOAT(UINT64 ticks) {
	return ((float)ticks) / TICKS_PER_SECOND;
}

inline double ticks_to_seconds_DOUBLE(UINT64 ticks) {
	return ((double)ticks) / TICKS_PER_SECOND;
}

inline UINT64 seconds_to_ticks(double seconds) {
	return (UINT64)(seconds * TICKS_PER_SECOND);
}

// Helper class for animation and simulation timing.
typedef struct Sendai_Step_Timer {
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
} Sendai_Step_Timer;

void SendaiTimer_init(Sendai_Step_Timer *st);

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to avoid having the fixed timestep logic attempt a set of catch-up
// Update calls.
inline void Sendai_reset_elapsed_time(Sendai_Step_Timer *st);

typedef void (*LPUPDATEFUNC)(void);

// Update timer state, calling the specified Update function the appropriate number of times.
void Sendai_tick_with_update_fn(Sendai_Step_Timer *st, LPUPDATEFUNC update);

// Only updates timer state, but doesn't do anything else
inline void Sendai_tick(Sendai_Step_Timer *st) {
	Sendai_tick_with_update_fn(st, NULL);
}
#include "timer.h"

void snc_steptimer_init(snc_step_timer_t *st) {
	st->elapsed_ticks = 0;
	st->total_ticks = 0;
	st->leftover_ticks = 0;
	st->frame_count = 0;
	st->frames_per_second = 0;
	st->frames_this_second = 0;
	st->qpc_second_counter = 0;
	st->is_fixed_time_step = false;
	st->target_elapsed_ticks = TICKS_PER_SECOND / 60;

	QueryPerformanceFrequency(&st->qpc_frequency);
	QueryPerformanceCounter(&st->qpc_last_time);

	// Initialize max delta to 1/10 of a second.
	st->qpc_max_delta = st->qpc_frequency.QuadPart / 10;
}

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to avoid having the fixed timestep logic attempt a set of catch-up
// Update calls.
void snc_reset_elapsed_time(snc_step_timer_t *st) {
	QueryPerformanceCounter(&st->qpc_last_time);

	st->leftover_ticks = 0;
	st->frames_per_second = 0;
	st->frames_this_second = 0;
	st->qpc_second_counter = 0;
}

// Update timer state, calling the specified Update function the appropriate number of times.
void snc_tick_with_update_fn(snc_step_timer_t *st, LPUPDATEFUNC update) {
	// Query the current time.
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);
	// Update delta since last call
	UINT64 delta = current_time.QuadPart - st->qpc_last_time.QuadPart;
	st->qpc_last_time = current_time;
	st->qpc_second_counter += delta;
	// Clamp excessively large time deltas (e.g. after paused in the debugger).
	if (delta > st->qpc_max_delta) {
		delta = st->qpc_max_delta;
	}
	// Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
	delta *= TICKS_PER_SECOND;
	delta /= st->qpc_frequency.QuadPart;

	UINT32 last_frame_count = st->frame_count;

	if (st->is_fixed_time_step) {
		// Fixed timestep update logic

		// If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
		// the clock to exactly match the target value. This prevents tiny and irrelevant errors
		// from accumulating over time. Without this clamping, a game that requested a 60 fps
		// fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
		// accumulate enough tiny errors that it would drop a frame. It is better to just round
		// small deviations down to zero to leave things running smoothly.
		if (abs((int)(delta - st->target_elapsed_ticks)) < TICKS_PER_SECOND / 4000) {
			delta = st->target_elapsed_ticks;
		}
		// Add to the leftover. If the accumulated leftover passes a certain target, we will need to take some actions to
		// ensure that the time step is still fixed.
		st->leftover_ticks += delta;

		// This will be done every time the leftover reaches the target. In case it has reached the target exaclty, it will do what
		// is expected: just step with the target. If it surpases, we will still step with target, but we will also register a leftover
		// that will propagate to the next tick (this leftover will be the difference timeDelta - target)
		while (st->leftover_ticks >= st->target_elapsed_ticks) {
			st->elapsed_ticks = st->target_elapsed_ticks;
			st->total_ticks += st->target_elapsed_ticks;
			st->leftover_ticks -= st->target_elapsed_ticks; // will be zero if st->leftOverTicks == st->targetElapsedTicks
			st->frame_count++;

			if (update) {
				update();
			}
		}
	} else {
		// Variable timestep update logic.
		st->elapsed_ticks = delta;
		st->total_ticks += delta;
		st->leftover_ticks = 0;
		st->frame_count++;

		if (update) {
			update();
		}
	}

	// Track the current framerate.
	if (st->frame_count != last_frame_count) {
		st->frames_this_second++;
	}

	if (st->qpc_second_counter >= (UINT64)(st->qpc_frequency.QuadPart)) {
		st->frames_per_second = st->frames_this_second;
		st->frames_this_second = 0;
		st->qpc_second_counter %= st->qpc_frequency.QuadPart;
	}
}
/*
 * Based on
 * https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MeshShaders/src/DynamicLOD/StepTimer.h
 */

#include "pch.h"

#include "timer.h"

VOID
S_TimerInit(S_StepTimer *StepTimer)
{
	StepTimer->ElapsedTicks = 0;
	StepTimer->TotalTicks = 0;
	StepTimer->LeftoverTicks = 0;
	StepTimer->FrameCount = 0;
	StepTimer->FramesPerSecond = 0;
	StepTimer->FramesThisSecond = 0;
	StepTimer->QPCSecondCounter = 0;
	StepTimer->bFixedTimeStep = false;
	StepTimer->TargetElapsedTicks = TICKS_PER_SECOND / 60;

	QueryPerformanceFrequency(&StepTimer->QPCFrequency);
	QueryPerformanceCounter(&StepTimer->QPCLastTime);

	// Initialize max delta to 1/10 of a second.
	StepTimer->QPCMaxDelta = StepTimer->QPCFrequency.QuadPart / 10;
}

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to aVOID having the fixed timestep logic attempt a set of catch-up
// Update calls.
VOID
S_ResetElapsedTime(S_StepTimer *StepTimer)
{
	QueryPerformanceCounter(&StepTimer->QPCLastTime);

	StepTimer->LeftoverTicks = 0;
	StepTimer->FramesPerSecond = 0;
	StepTimer->FramesThisSecond = 0;
	StepTimer->QPCSecondCounter = 0;
}

// Update timer state, calling the specified Update function the appropriate number of times.
VOID
S_TickWithUpdateFn(S_StepTimer *StepTimer, LPUPDATEFUNC Update)
{
	// Query the current time.
	LARGE_INTEGER CurrentTime;
	QueryPerformanceCounter(&CurrentTime);
	// Update delta since last call
	UINT64 Delta = CurrentTime.QuadPart - StepTimer->QPCLastTime.QuadPart;
	StepTimer->QPCLastTime = CurrentTime;
	StepTimer->QPCSecondCounter += Delta;
	// Clamp excessively large time deltas (e.g. after paused in the debugger).
	if (Delta > StepTimer->QPCMaxDelta) {
		Delta = StepTimer->QPCMaxDelta;
	}
	// Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
	Delta *= TICKS_PER_SECOND;
	Delta /= StepTimer->QPCFrequency.QuadPart;

	UINT32 LastFrameCount = StepTimer->FrameCount;

	if (StepTimer->bFixedTimeStep) {
		// Fixed timestep update logic

		// If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
		// the clock to exactly match the target value. This prevents tiny and irrelevant errors
		// from accumulating over time. Without this clamping, a game that requested a 60 fps
		// fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
		// accumulate enough tiny errors that it would drop a frame. It is better to just round
		// small deviations down to zero to leave things running smoothly.
		if (abs((INT)(Delta - StepTimer->TargetElapsedTicks)) < TICKS_PER_SECOND / 4000) {
			Delta = StepTimer->TargetElapsedTicks;
		}
		// Add to the leftover. If the accumulated leftover passes a certain target, we will need to take some actions to
		// ensure that the time step is still fixed.
		StepTimer->LeftoverTicks += Delta;

		// This will be done every time the leftover reaches the target. In case it has reached the target exaclty, it will do
		// what is expected: just step with the target. If it surpases, we will still step with target, but we will also register
		// a leftover that will propagate to the next tick (this leftover will be the difference timeDelta - target)
		while (StepTimer->LeftoverTicks >= StepTimer->TargetElapsedTicks) {
			StepTimer->ElapsedTicks = StepTimer->TargetElapsedTicks;
			StepTimer->TotalTicks += StepTimer->TargetElapsedTicks;
			StepTimer->LeftoverTicks -= StepTimer->TargetElapsedTicks; // will be zero if st->leftOverTicks == st->targetElapsedTicks
			StepTimer->FrameCount++;

			if (Update) {
				Update();
			}
		}
	} else {
		// Variable timestep update logic.
		StepTimer->ElapsedTicks = Delta;
		StepTimer->TotalTicks += Delta;
		StepTimer->LeftoverTicks = 0;
		StepTimer->FrameCount++;

		if (Update) {
			Update();
		}
	}

	// Track the current framerate.
	if (StepTimer->FrameCount != LastFrameCount) {
		StepTimer->FramesThisSecond++;
	}

	if (StepTimer->QPCSecondCounter >= (UINT64)(StepTimer->QPCFrequency.QuadPart)) {
		StepTimer->FramesPerSecond = StepTimer->FramesThisSecond;
		StepTimer->FramesThisSecond = 0;
		StepTimer->QPCSecondCounter %= StepTimer->QPCFrequency.QuadPart;
	}
}
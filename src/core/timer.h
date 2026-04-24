/*
 * Based on
 * https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MeshShaders/src/DynamicLOD/StepTimer.h
 */

#pragma once

// Integer format represents time using 10,000,000 ticks per second.
#define TICKS_PER_SECOND 10000000

inline FLOAT
TicksToSeconds_FLOAT(UINT64 Ticks)
{
	return ((FLOAT)Ticks) / TICKS_PER_SECOND;
}

inline DOUBLE
TicksToSeconds_DOUBLE(UINT64 Ticks)
{
	return ((DOUBLE)Ticks) / TICKS_PER_SECOND;
}

inline UINT64
SecondsToTicks(DOUBLE Seconds)
{
	return (UINT64)(Seconds * TICKS_PER_SECOND);
}

// Helper class for animation and simulation timing.
typedef struct S_StepTimer {
	// Source timing data uses QueryPerformanceCounter units.
	LARGE_INTEGER QPCFrequency;
	LARGE_INTEGER QPCLastTime;
	UINT64 QPCMaxDelta;
	// Derived timing data uses a canonical tick format.
	UINT64 ElapsedTicks;
	UINT64 TotalTicks;
	UINT64 LeftoverTicks;
	// Members for tracking the framerate.
	UINT32 FrameCount;
	UINT32 FramesPerSecond;
	UINT32 FramesThisSecond;
	UINT64 QPCSecondCounter;
	// Members for configuring fixed timestep mode.
	BOOL bFixedTimeStep;
	UINT64 TargetElapsedTicks;
} S_StepTimer;

VOID S_TimerInit(S_StepTimer *StepTimer);

// After an intentional timing discontinuity (for instance a blocking IO operation)
// call this to avoid having the fixed timestep logic attempt a set of catch-up
// Update calls.
inline VOID S_ResetElapsedTime(S_StepTimer *StepTimer);

typedef VOID (*LPUPDATEFUNC)(VOID);

// Update timer state, calling the specified Update function the appropriate number of times.
void S_TickWithUpdateFn(S_StepTimer *StepTimer, LPUPDATEFUNC update);

// Only updates timer state, but doesn't do anything else
inline VOID
S_Tick(S_StepTimer *StepTimer)
{
	S_TickWithUpdateFn(StepTimer, NULL);
}
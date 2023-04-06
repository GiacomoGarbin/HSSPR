#pragma once

class Timer
{
public:
	Timer();

	void Reset();
	void Start();
	void Stop();
	void Tick();

	float GetDeltaTime() const;
	float GetTotalTime() const;

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPauseTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};
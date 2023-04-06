#include "Timer.h"

#include <windows.h>

Timer::Timer()
    : mSecondsPerCount(0.0)
    , mDeltaTime(-1.0)
    , mBaseTime(0)
    , mPauseTime(0)
    , mStopTime(0)
    , mPrevTime(0)
    , mCurrTime(0)
    , mStopped(false)
{
    LARGE_INTEGER CountsPerSecond;
    QueryPerformanceFrequency(&CountsPerSecond);

    mSecondsPerCount = 1.0 / static_cast<double>(CountsPerSecond.QuadPart);
}

void Timer::Reset()
{
    LARGE_INTEGER CurrTime;
    QueryPerformanceCounter(&CurrTime);

    mBaseTime = static_cast<__int64>(CurrTime.QuadPart);
    mPrevTime = static_cast<__int64>(CurrTime.QuadPart);
    mStopTime = 0;
    mStopped = false;
}

void Timer::Start()
{
    LARGE_INTEGER StartTime;
    QueryPerformanceCounter(&StartTime);

    if (mStopped)
    {
        __int64 StartTime64 = static_cast<__int64>(StartTime.QuadPart);

        mPauseTime += (StartTime64 - mStopTime);

        mPrevTime = StartTime64;
        mStopTime = 0;
        mStopped = false;
    }
}

void Timer::Stop()
{
    if (!mStopped)
    {
        LARGE_INTEGER CurrTime;
        QueryPerformanceCounter(&CurrTime);

        mStopTime = static_cast<__int64>(CurrTime.QuadPart);
        mStopped = true;
    }
}

void Timer::Tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0;
        return;
    }

    LARGE_INTEGER CurrTime;
    QueryPerformanceCounter(&CurrTime);

    mCurrTime = static_cast<__int64>(CurrTime.QuadPart);

    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

    mPrevTime = mCurrTime;

    if (mDeltaTime < 0.0)
    {
        mDeltaTime = 0.0;
    }
}

float Timer::GetDeltaTime() const
{
    return static_cast<float>(mDeltaTime);
}

float Timer::GetTotalTime() const
{
    if (mStopped)
    {
        return static_cast<float>(((mStopTime - mPauseTime) - mBaseTime) * mSecondsPerCount);
    }
    else
    {
        return static_cast<float>(((mCurrTime - mPauseTime) - mBaseTime) * mSecondsPerCount);
    }
}
#include "Zuma_Prefix.pch"

#include "WorkerThread.h"

using namespace Sexy;

WorkerThread::WorkerThread()
{
	mShutdown = false;
	mTaskRunning = false;
	mTaskProc = NULL;
	mParam = NULL;

	mThread = std::thread([this]() { ThreadLoop(); });
}

WorkerThread::~WorkerThread()
{
	{
		std::unique_lock<std::mutex> aLock(mMutex);
		mShutdown = true;
	}
	mTaskCv.notify_all();

	if (mThread.joinable())
		mThread.join();
}

void WorkerThread::WaitForTask()
{
	// Same semantics as the original's WaitForSingleObject(mUnk2, 1000):
	// give the in-flight task up to a second to finish.
	std::unique_lock<std::mutex> aLock(mMutex);
	mDoneCv.wait_for(aLock, std::chrono::milliseconds(1000),
					 [this]() { return !mTaskRunning; });
}

void WorkerThread::DoTask(void (*theTaskProc)(void*), void* theParam)
{
	WaitForTask();

	{
		std::unique_lock<std::mutex> aLock(mMutex);
		mTaskProc = theTaskProc;
		mParam = theParam;
		mTaskRunning = true;
	}

	mTaskCv.notify_one();
}

void WorkerThread::ThreadLoop()
{
	for (;;)
	{
		void (*aTask)(void*) = NULL;
		void* aParam = NULL;

		{
			std::unique_lock<std::mutex> aLock(mMutex);
			mTaskCv.wait(aLock, [this]() { return mShutdown || mTaskProc != NULL; });

			if (mShutdown)
				break;

			aTask = mTaskProc;
			aParam = mParam;
			mTaskProc = NULL;
		}

		if (aTask != NULL)
			aTask(aParam);

		{
			std::unique_lock<std::mutex> aLock(mMutex);
			mTaskRunning = false;
		}

		mDoneCv.notify_all();
	}
}

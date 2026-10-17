#ifndef __SEXY_WORKER_THREAD_H__
#define __SEXY_WORKER_THREAD_H__

#include "CircleCommon.h"

#include <vector>
#include <list>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Sexy
{
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////

	// Port note: the original drove this with Win32 events (HANDLE /
	// CreateEventA / _beginthread). This port uses std::thread and condition
	// variables so it works on Windows and Android alike. The original also never
	// set mShutdown, so its thread ran until process exit and the destructor
	// leaked it; the destructor here stops and joins properly.
	class WorkerThread
	{
	public:
		std::thread mThread;
		std::mutex mMutex;
		std::condition_variable mTaskCv;	// signals a task is ready to run
		std::condition_variable mDoneCv;	// signals the running task finished
		void (*mTaskProc)(void *);
		void *mParam;
		bool mShutdown;
		bool mTaskRunning;

		WorkerThread();

		virtual ~WorkerThread();

		void WaitForTask();
		void DoTask(void (*func)(void *), void *unk);

		void ThreadLoop();
	};
};

#endif

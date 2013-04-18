/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2007 Erwin Coumans  http://bulletphysics.com

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "Win32ThreadSupport.h"

#ifdef USE_WIN32_THREADING

#include <windows.h>
#include <stdio.h>

#include "SpuCollisionTaskProcess.h"
#include "SpuNarrowPhaseCollisionTask/SpuGatheringCollisionTask.h"

#ifdef _MSC_VER
// Stolen from http://msdn.microsoft.com/en-us/library/xcb2z8hs%28v=vs.110%29.aspx
const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadName(DWORD dwThreadID, const char* threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
}
#endif // _MSC_VER


///The number of threads should be equal to the number of available cores
///@todo: each worker should be linked to a single core, using SetThreadIdealProcessor.

///Win32ThreadSupport helps to initialize/shutdown libspe2, start/stop SPU tasks and communication
///Setup and initialize SPU/CELL/Libspe2
Win32ThreadSupport::Win32ThreadSupport(const Win32ThreadConstructionInfo & threadConstructionInfo)
{
	m_maxNumTasks = threadConstructionInfo.m_numThreads;
	startThreads(threadConstructionInfo);
}

///cleanup/shutdown Libspe2
Win32ThreadSupport::~Win32ThreadSupport()
{
	stopSPU();
}

static DWORD WINAPI ThreadFunc(LPVOID lpParam) 
{
	Win32ThreadSupport::btSpuStatus *status = (Win32ThreadSupport::btSpuStatus *)lpParam;

	if (!status) {
		// Not good.
		printf("Thread started with NULL param!\n");
		return 0;
	}
	
	while (true)
	{
		WaitForSingleObject(status->m_eventStartHandle, INFINITE);
		
		void* userPtr = status->m_userPtr;

		if (userPtr)
		{
			btAssert(status->m_status);
			status->m_userThreadFunc(userPtr, status->m_lsMemory);
			status->m_status = 2;
			SetEvent(status->m_eventCompletedHandle);
		}
		else
		{
			//exit Thread
			status->m_status = 3;
			printf("Thread with taskId %i with handle %p exiting\n", status->m_taskId, status->m_threadHandle);
			SetEvent(status->m_eventCompletedHandle);
			break;
		}
		
	}

	printf("Thread TERMINATED\n");
	return 0;

}

///send messages to SPUs
void Win32ThreadSupport::sendRequest(uint32_t uiCommand, ppu_address_t uiArgument0, uint32_t taskId)
{
	///we should spawn an SPU task here, and in 'waitForResponse' it should wait for response of the (one of) the first tasks that finished

	btAssert(taskId >= 0);
	btAssert(int(taskId) < m_activeSpuStatus.size());

	btSpuStatus&	spuStatus = m_activeSpuStatus[taskId];

	spuStatus.m_commandId = uiCommand;
	spuStatus.m_status = 1;
	spuStatus.m_userPtr = (void *)uiArgument0;

	///fire event to start new task
	SetEvent(spuStatus.m_eventStartHandle);
}


///check for messages from SPUs
void Win32ThreadSupport::waitForResponse(unsigned int *puiArgument0, unsigned int *puiArgument1)
{
	///We should wait for (one of) the first tasks to finish (or other SPU messages), and report its response
	
	///A possible response can be 'yes, SPU handled it', or 'no, please do a PPU fallback'


	btAssert(m_activeSpuStatus.size());

	int last = -1;
#ifndef SINGLE_THREADED
	DWORD res = WaitForMultipleObjects(m_completeHandles.size(), &m_completeHandles[0], FALSE, INFINITE);
	btAssert(res != WAIT_FAILED);
	last = res - WAIT_OBJECT_0;

	btSpuStatus& spuStatus = m_activeSpuStatus[last];
	btAssert(spuStatus.m_threadHandle);
	btAssert(spuStatus.m_eventCompletedHandle);

	//WaitForSingleObject(spuStatus.m_eventCompletedHandle, INFINITE);
	btAssert(spuStatus.m_status > 1);
	spuStatus.m_status = 0;

	///need to find an active spu
	btAssert(last>=0);

#else
	last=0;
	btSpuStatus& spuStatus = m_activeSpuStatus[last];
#endif //SINGLE_THREADED

	
	if (puiArgument0)
		*puiArgument0 = spuStatus.m_taskId;

	if (puiArgument1)
		*puiArgument1 = spuStatus.m_status;
}

///check for messages from SPUs
bool Win32ThreadSupport::isTaskCompleted(unsigned int *puiArgument0, unsigned int *puiArgument1, int timeOutInMilliseconds)
{
	///We should wait for (one of) the first tasks to finish (or other SPU messages), and report its response
	
	///A possible response can be 'yes, SPU handled it', or 'no, please do a PPU fallback'


	btAssert(m_activeSpuStatus.size());

	int last = -1;
#ifndef SINGLE_THREADED
	DWORD res = WaitForMultipleObjects(m_completeHandles.size(), &m_completeHandles[0], FALSE, timeOutInMilliseconds);
	
	if ((res != STATUS_TIMEOUT) && (res != WAIT_FAILED))
	{
		last = res - WAIT_OBJECT_0;

		btSpuStatus& spuStatus = m_activeSpuStatus[last];
		btAssert(spuStatus.m_threadHandle);
		btAssert(spuStatus.m_eventCompletedHandle);

		//WaitForSingleObject(spuStatus.m_eventCompletedHandle, INFINITE);
		btAssert(spuStatus.m_status > 1);
		spuStatus.m_status = 0;

		///need to find an active spu
		btAssert(last>=0);

		if (puiArgument0)
			*puiArgument0 = spuStatus.m_taskId;

		if (puiArgument1)
			*puiArgument1 = spuStatus.m_status;

		return true;
	}

	return false;
#else
	last=0;
	btSpuStatus& spuStatus = m_activeSpuStatus[last];

	if (puiArgument0)
		*puiArgument0 = spuStatus.m_taskId;

	if (puiArgument1)
		*puiArgument1 = spuStatus.m_status;

	return true;
#endif //SINGLE_THREADED
}


void Win32ThreadSupport::startThreads(const Win32ThreadConstructionInfo& threadConstructionInfo)
{

	m_activeSpuStatus.resize(threadConstructionInfo.m_numThreads);
	m_completeHandles.resize(threadConstructionInfo.m_numThreads);

	m_maxNumTasks = threadConstructionInfo.m_numThreads;

	for (int i = 0; i < threadConstructionInfo.m_numThreads; i++)
	{
		printf("starting thread %d\n", i);

		btSpuStatus&	spuStatus = m_activeSpuStatus[i];

		LPSECURITY_ATTRIBUTES lpThreadAttributes = NULL;
		SIZE_T dwStackSize = threadConstructionInfo.m_threadStackSize;
		LPTHREAD_START_ROUTINE lpStartAddress = &ThreadFunc;
		LPVOID lpParameter = &spuStatus;
		DWORD dwCreationFlags = 0;
		LPDWORD lpThreadId = 0;

		spuStatus.m_userPtr=0;

		sprintf_s(spuStatus.m_eventStartHandleName, "eventStart%s%d", threadConstructionInfo.m_uniqueName, i);
		spuStatus.m_eventStartHandle = CreateEventA(0, false, false, spuStatus.m_eventStartHandleName);

		sprintf_s(spuStatus.m_eventCompletedHandleName, "eventComplete%s%d", threadConstructionInfo.m_uniqueName, i);
		spuStatus.m_eventCompletedHandle = CreateEventA(0, false, false, spuStatus.m_eventCompletedHandleName);

		m_completeHandles[i] = spuStatus.m_eventCompletedHandle;

		HANDLE hThread = CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,	dwCreationFlags, lpThreadId);
		SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
		//SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);

#ifdef _MSC_VER
		// For easier debugging.
		char threadName[32];
		sprintf_s(threadName, "%s-%d", threadConstructionInfo.m_uniqueName, i);
		SetThreadName(GetThreadId(hThread), threadName);
#endif

		SetThreadAffinityMask(hThread, 1<<i);
		

		spuStatus.m_taskId = i;
		spuStatus.m_commandId = 0;
		spuStatus.m_status = 0;
		spuStatus.m_threadHandle = hThread;
		spuStatus.m_lsMemory = threadConstructionInfo.m_lsMemoryFunc();
		spuStatus.m_userThreadFunc = threadConstructionInfo.m_userThreadFunc;

		printf("started thread %d with threadHandle %p\n", i, hThread);
	}
}

void Win32ThreadSupport::startSPU()
{
}


///tell the task scheduler we are done with the SPU tasks
void Win32ThreadSupport::stopSPU()
{
	int i;
	for (i=0;i<m_activeSpuStatus.size();i++)
	{
		btSpuStatus& spuStatus = m_activeSpuStatus[i];
		if (spuStatus.m_status>0)
		{
			WaitForSingleObject(spuStatus.m_eventCompletedHandle, INFINITE);
		}
		

		spuStatus.m_userPtr = 0;
		SetEvent(spuStatus.m_eventStartHandle);
		WaitForSingleObject(spuStatus.m_eventCompletedHandle, INFINITE);

		CloseHandle(spuStatus.m_eventCompletedHandle);
		CloseHandle(spuStatus.m_eventStartHandle);
		CloseHandle(spuStatus.m_threadHandle);

	}

	m_activeSpuStatus.clear();
	m_completeHandles.clear();

}



class btWin32Barrier : public btBarrier
{
private:
	CRITICAL_SECTION mExternalCriticalSection;
	CRITICAL_SECTION mLocalCriticalSection;
	HANDLE mRunEvent, mNotifyEvent;
	int mCounter, mEnableCounter;
	int mMaxCount;

public:
	btWin32Barrier()
	{
		mCounter = 0;
		mMaxCount = 1;
		mEnableCounter = 0;
		InitializeCriticalSection(&mExternalCriticalSection);
		InitializeCriticalSection(&mLocalCriticalSection);
		mRunEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		mNotifyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	virtual ~btWin32Barrier()
	{
		DeleteCriticalSection(&mExternalCriticalSection);
		DeleteCriticalSection(&mLocalCriticalSection);
		CloseHandle(mRunEvent);
		CloseHandle(mNotifyEvent);
	}

	void sync()
	{
		int eventId;

		EnterCriticalSection(&mExternalCriticalSection);

		//PFX_PRINTF("enter taskId %d count %d stage %d phase %d mEnableCounter %d\n", taskId, mCounter, debug&0xff, debug>>16, mEnableCounter);

		if(mEnableCounter > 0) {
			ResetEvent(mNotifyEvent);
			LeaveCriticalSection(&mExternalCriticalSection);
			WaitForSingleObject(mNotifyEvent, INFINITE); 
			EnterCriticalSection(&mExternalCriticalSection);
		}

		eventId = mCounter;
		mCounter++;

		if(eventId == mMaxCount-1) {
			SetEvent(mRunEvent);

			mEnableCounter = mCounter-1;
			mCounter = 0;
		}
		else {
			ResetEvent(mRunEvent);
			LeaveCriticalSection(&mExternalCriticalSection);
			WaitForSingleObject(mRunEvent, INFINITE); 
			EnterCriticalSection(&mExternalCriticalSection);
			mEnableCounter--;
		}

		if(mEnableCounter == 0) {
			SetEvent(mNotifyEvent);
		}

		//PFX_PRINTF("leave taskId %d count %d stage %d phase %d mEnableCounter %d\n", taskId, mCounter, debug&0xff, debug>>16, mEnableCounter);

		LeaveCriticalSection(&mExternalCriticalSection);
	}

	virtual void setMaxCount(int n) {mMaxCount = n;}
	virtual int  getMaxCount() {return mMaxCount;}
};

class btWin32CriticalSection : public btCriticalSection
{
private:
	CRITICAL_SECTION mCriticalSection;

public:
	btWin32CriticalSection()
	{
		InitializeCriticalSection(&mCriticalSection);
	}

	~btWin32CriticalSection()
	{
		DeleteCriticalSection(&mCriticalSection);
	}

	unsigned int getSharedParam(int i)
	{
		btAssert(i>=0&&i<31);
		return mCommonBuff[i+1];
	}

	void setSharedParam(int i, unsigned int p)
	{
		btAssert(i>=0&&i<31);
		mCommonBuff[i+1] = p;
	}

	void lock()
	{
		EnterCriticalSection(&mCriticalSection);
		mCommonBuff[0] = 1;
	}

	void unlock()
	{
		mCommonBuff[0] = 0;
		LeaveCriticalSection(&mCriticalSection);
	}
};


btBarrier*	Win32ThreadSupport::createBarrier()
{
	unsigned char* mem = (unsigned char*)btAlignedAlloc(sizeof(btWin32Barrier),16);
	btWin32Barrier* barrier = new(mem) btWin32Barrier();
	barrier->setMaxCount(getNumTasks());
	return barrier;
}

btCriticalSection* Win32ThreadSupport::createCriticalSection()
{
	unsigned char* mem = (unsigned char*) btAlignedAlloc(sizeof(btWin32CriticalSection),16);
	btWin32CriticalSection* cs = new(mem) btWin32CriticalSection();
	return cs;
}

void Win32ThreadSupport::deleteBarrier(btBarrier* barrier)
{
	barrier->~btBarrier();
	btAlignedFree(barrier);
}

void Win32ThreadSupport::deleteCriticalSection(btCriticalSection* criticalSection)
{
	criticalSection->~btCriticalSection();
	btAlignedFree(criticalSection);
}


#endif //USE_WIN32_THREADING


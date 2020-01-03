#ifndef PHYSICS_MULTITHREADING_H
#define PHYSICS_MULTITHREADING_H
#if defined(_MSC_VER) || (defined(__GNUC__) && __GNUC__ > 3)
	#pragma once
#endif

// #define USE_PARALLEL_DISPATCHER // TODO: See why collisions are broken
#define USE_PARALLEL_SOLVER 
#define USE_PARALLEL_DYNAMICS

#ifdef BT_THREADSAFE
	#include "LinearMath/btThreads.h"
#endif

#ifdef USE_PARALLEL_DISPATCHER
	#include "BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h"
#else
	#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#endif

#ifdef USE_PARALLEL_DYNAMICS
	#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorldMt.h"
#else
	#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#endif

#ifdef USE_PARALLEL_SOLVER
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h"
#else
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#endif

#include "Physics.h"
#include "Physics_Environment.h"

// Environment ConVars
// Change the hardcoded min and max if you change the min and max here!
#ifdef BT_THREADSAFE
static void vphysics_numthreads_Change(IConVar *var, const char *pOldValue, float flOldValue);
static ConVar vphysics_numthreads("vphysics_numthreads", "8", FCVAR_ARCHIVE, "Amount of threads to use in simulation (don't set this too high).", true, 1, true, 16, vphysics_numthreads_Change);

static void vphysics_numthreads_Change(IConVar *var, const char *pOldValue, float flOldValue) {
	const int newVal = vphysics_numthreads.GetInt();
	if (newVal <= 0 || newVal > 8) 
		return;

	Msg("VPhysics: Resizing to %d threads\n", newVal);

	for (int i = 0; i < g_Physics.GetActiveEnvironmentCount(); i++) 
	{
		dynamic_cast<CPhysicsEnvironment*>(g_Physics.GetActiveEnvironmentByIndex(i))->ChangeThreadCount(newVal);
	}
}
#endif

///
/// btThreadManager -- manage a number of task schedulers so we can switch between them
///
class CPhysThreadManager
{
	btAlignedObjectArray<btITaskScheduler*> m_taskSchedulers;
	btAlignedObjectArray<btITaskScheduler*> m_allocatedTaskSchedulers;
#ifdef USE_PARALLEL_DYNAMICS
	btConstraintSolverPoolMt*				m_constraintSolverPool{};
#endif

public:
	CPhysThreadManager() {}
	void init()
	{
		addTaskScheduler(btGetSequentialTaskScheduler());
#ifdef BT_THREADSAFE
		if (btITaskScheduler* ts = btCreateDefaultTaskScheduler())
		{
			m_allocatedTaskSchedulers.push_back(ts);
			addTaskScheduler(ts);
		}
		// We will be using OpenMP for now
		addTaskScheduler(btGetOpenMPTaskScheduler());
		// addTaskScheduler(btGetTBBTaskScheduler());
		// addTaskScheduler(btGetPPLTaskScheduler());
		if (getNumTaskSchedulers() > 1)
		{
			// prefer a non-sequential scheduler if available
			btSetTaskScheduler(m_taskSchedulers[1]);
		}
		else
		{
			btSetTaskScheduler(m_taskSchedulers[0]);
		}

#ifdef USE_PARALLEL_DYNAMICS
		m_constraintSolverPool = new btConstraintSolverPoolMt(vphysics_numthreads.GetInt());
#endif
#endif  // #ifdef BT_THREADSAFE
	}
	void shutdown()
	{
		for (int i = 0; i < m_allocatedTaskSchedulers.size(); ++i)
		{
			delete m_allocatedTaskSchedulers[i];
		}
		m_allocatedTaskSchedulers.clear();

#ifdef USE_PARALLEL_DYNAMICS
		delete m_constraintSolverPool;
#endif
	}

	void addTaskScheduler(btITaskScheduler* ts)
	{
		if (ts)
		{
#ifdef BT_THREADSAFE
			// if initial number of threads is 0 or 1,
			if (ts->getNumThreads() <= 1)
			{
				// for OpenMP, TBB, PPL set num threads to number of logical cores
				ts->setNumThreads(ts->getMaxNumThreads());
			}
#endif  // #ifdef BT_THREADSAFE
			m_taskSchedulers.push_back(ts);
		}
	}
	
	int getNumTaskSchedulers() const { return m_taskSchedulers.size(); }
#ifdef USE_PARALLEL_DYNAMICS
	btConstraintSolverPoolMt* GetConstraintSolverPool() const { return m_constraintSolverPool; }
#endif
	btITaskScheduler* getTaskScheduler(int i) { return m_taskSchedulers[i]; }
};

#endif // PHYSICS_MULTITHREADING_H
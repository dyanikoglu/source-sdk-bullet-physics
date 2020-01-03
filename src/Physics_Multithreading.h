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
#ifdef USE_PARALLEL_DYNAMICS
	btConstraintSolverPoolMt*				m_constraintSolverPool{};
#endif

public:
	CPhysThreadManager() {}
	void init()
	{
#ifdef BT_THREADSAFE
		btSetTaskScheduler(btGetTBBTaskScheduler());
#endif // BT_THREADSAFE
#ifdef USE_PARALLEL_DYNAMICS
		m_constraintSolverPool = new btConstraintSolverPoolMt(vphysics_numthreads.GetInt());
#endif // #ifdef USE_PARALLEL_DYNAMICS
	}
	void shutdown()
	{
#ifdef USE_PARALLEL_DYNAMICS
		delete m_constraintSolverPool;
#endif
	}
#ifdef USE_PARALLEL_DYNAMICS
	btConstraintSolverPoolMt* GetConstraintSolverPool() const { return m_constraintSolverPool; }
#endif
};

#endif // PHYSICS_MULTITHREADING_H
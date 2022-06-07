// Engine.cpp: implementation of the CEngine class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "cpuid.h"
#include "Engine.h"

CEngine				Engine;

extern	void msCreate		(LPCSTR name);

void CEngine::Initialize()
{
	// Bind PSGP
	/*hPSGP		= LoadLibrary("xrCPU_Pipe.dll");
	R_ASSERT	(hPSGP);
	xrBinder*	bindCPU	= (xrBinder*)	GetProcAddress(hPSGP,"xrBind_PSGP");	R_ASSERT(bindCPU);
	bindCPU		(&PSGP, 0);
	*/
	// Other stuff
	Engine.Sheduler.Initialize			( );
	// 
#ifdef DEBUG
	msCreate							("game");
#endif
}

void CEngine::Destroy	()
{
	Engine.Sheduler.Destroy				( );
#ifdef DEBUG_MEMORY_MANAGER
	extern void	dbg_dump_leaks_prepare	( );
	if (Memory.debug_mode)				dbg_dump_leaks_prepare	();
#endif // DEBUG_MEMORY_MANAGER
	Engine.External.Destroy				( );
}

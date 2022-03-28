#include "stdafx.h"
#include "frustum.h"

#pragma warning(disable:4995)
// mmsystem.h
#define MMNOSOUND
#define MMNOMIDI
#define MMNOAUX
#define MMNOMIXER
#define MMNOJOY
#include <mmsystem.h>
// d3dx9.h
#include <d3dx/d3dx9.h>
#pragma warning(default:4995)

#include "x_ray.h"
#include "render.h"

// must be defined before include of FS_impl.h
#define INCLUDE_FROM_ENGINE
#include "../xrCore/FS_impl.h"

#ifdef INGAME_EDITOR
#	include "../include/editor/ide.hpp"
#	include "engine_impl.hpp"
#endif // #ifdef INGAME_EDITOR

#include "xrSash.h"
#include "igame_persistent.h"

ENGINE_API CRenderDevice Device;
ENGINE_API CLoadScreenRenderer load_screen_renderer;
ENGINE_API BOOL g_bRendering = FALSE; 
u32 g_dwFPSlimit = 60;

BOOL		g_bLoaded = FALSE;
//ref_light precache_light = 0;

BOOL CRenderDevice::Begin	()
{
	switch (m_pRender->GetDeviceState())
	{
	case IRenderDeviceRender::dsOK:
		break;

	case IRenderDeviceRender::dsLost:
		// If the device was lost, do not render until we get it back
		Sleep(33);
		return FALSE;
		break;

	case IRenderDeviceRender::dsNeedReset:
		// Check if the device is ready to be reset
		Reset();
		break;

	default:
		R_ASSERT(0);
	}

	m_pRender->Begin();

	FPU::m24r	();
	g_bRendering = 	TRUE;

	return		TRUE;
}

void CRenderDevice::Clear	()
{
	m_pRender->Clear();
	/*
	CHK_DX(HW.pDevice->Clear(0,0,
		D3DCLEAR_ZBUFFER|
		(psDeviceFlags.test(rsClearBB)?D3DCLEAR_TARGET:0)|
		(HW.Caps.bStencil?D3DCLEAR_STENCIL:0),
		D3DCOLOR_XRGB(0,0,0),1,0
		));
		*/
}

extern void CheckPrivilegySlowdown();
//#include "resourcemanager.h"

void CRenderDevice::End		(void)
{
	//	Moved to m_pRenderEnd()
	//VERIFY	(HW.pDevice);

	//if (HW.Caps.SceneMode)	overdrawEnd		();

	// 
#ifdef INGAME_EDITOR
	bool							load_finished = false;
#endif // #ifdef INGAME_EDITOR
	if (dwPrecacheFrame)
	{
		::Sound->set_master_volume	(0.f);
		dwPrecacheFrame	--;
		pApp->load_draw_internal	();
        if (!dwPrecacheFrame)
		{

#ifdef INGAME_EDITOR
			load_finished			= true;
#endif // #ifdef INGAME_EDITOR
			//Gamma.Update		();
			m_pRender->updateGamma();

			//if(precache_light) precache_light->set_active	(false);
			//if(precache_light) precache_light.destroy		();
			::Sound->set_master_volume						(1.f);
			//pApp->destroy_loading_shaders					();

			m_pRender->ResourcesDestroyNecessaryTextures	();
			Memory.mem_compact								();
			Msg												("* MEMORY USAGE: %d K",Memory.mem_usage()/1024);
			Msg												("* End of synchronization A[%d] R[%d]",b_is_Active, b_is_Ready);

#ifdef FIND_CHUNK_BENCHMARK_ENABLE
			g_find_chunk_counter.flush();
#endif // FIND_CHUNK_BENCHMARK_ENABLE

			CheckPrivilegySlowdown							();
			
			if(g_pGamePersistent->GameType()==1)//haCk
			{
				WINDOWINFO	wi;
				GetWindowInfo(m_hWnd,&wi);
				if(wi.dwWindowStatus!=WS_ACTIVECAPTION)
					Pause(TRUE,TRUE,TRUE,"application start");
			}
		}
	}

	g_bRendering		= FALSE;
	// end scene
	//	Present goes here, so call OA Frame end.
	//if (g_SASH.IsBenchmarkRunning())
	//	g_SASH.DisplayFrame(Device.fTimeGlobal);
	m_pRender->End();
	//RCache.OnFrameEnd	();
	//Memory.dbg_check		();
    //CHK_DX				(HW.pDevice->EndScene());

	//HRESULT _hr		= HW.pDevice->Present( NULL, NULL, NULL, NULL );
	//if				(D3DERR_DEVICELOST==_hr)	return;			// we will handle this later
	//R_ASSERT2		(SUCCEEDED(_hr),	"Presentation failed. Driver upgrade needed?");
#	ifdef INGAME_EDITOR
		if (load_finished && m_editor)
			m_editor->on_load_finished	();
#	endif // #ifdef INGAME_EDITOR
}


volatile u32	mt_Thread_marker		= 0x12345678;
void 			mt_Thread	(void *ptr)	{
	while (true) {
		// waiting for Device permission to execute
		Device.mt_csEnter.Enter	();

		if (Device.mt_bMustExit) {
			Device.mt_bMustExit = FALSE;				// Important!!!
			Device.mt_csEnter.Leave();					// Important!!!
			return;
		}
		// we has granted permission to execute
		mt_Thread_marker			= Device.dwFrame;
 
		for (u32 pit=0; pit<Device.seqParallel.size(); pit++)
			Device.seqParallel[pit]	();
		Device.seqParallel.clear_not_free	();
		Device.seqFrameMT.Process	(rp_Frame);

		// now we give control to device - signals that we are ended our work
		Device.mt_csEnter.Leave	();
		// waits for device signal to continue - to start again
		Device.mt_csLeave.Enter	();
		// returns sync signal to device
		Device.mt_csLeave.Leave	();
	}
}

#include "igame_level.h"
void CRenderDevice::PreCache	(u32 amount, bool b_draw_loadscreen, bool b_wait_user_input)
{
	//if (HW.Caps.bForceGPU_REF)	amount=0;
	if (m_pRender->GetForceGPU_REF()) amount=0;
	// Msg			("* PCACHE: start for %d...",amount);
	dwPrecacheFrame	= dwPrecacheTotal = amount;
	/*if (amount && !precache_light && g_pGameLevel && g_loading_events.empty()) {
		precache_light					= ::Render->light_create();
		precache_light->set_shadow		(false);
		precache_light->set_position	(vCameraPosition);
		precache_light->set_color		(255,255,255);
		precache_light->set_range		(5.0f);
		precache_light->set_active		(true);
	}*/

    if (amount && b_draw_loadscreen && !load_screen_renderer.b_registered)
	{
		load_screen_renderer.start	(b_wait_user_input);
	}
}

int g_svDedicateServerUpdateReate = 100;

ENGINE_API xr_list<LOADING_EVENT>			g_loading_events;

void CRenderDevice::on_idle		()
{
	if (!b_is_Ready) {
		Sleep	(100);
		return;
	}

	// FPS Lock
	constexpr u32 menuFPSlimit = 60, pauseFPSlimit = 60;
	u32 curFPSLimit = IsMainMenuActive() ? menuFPSlimit : Device.Paused() ? pauseFPSlimit : g_dwFPSlimit;
	if (curFPSLimit > 0)
	{
		static DWORD dwLastFrameTime = 0;
		DWORD dwCurrentTime = timeGetTime();
		if (dwCurrentTime - dwLastFrameTime < 1000 / (curFPSLimit + 1))
			return;
		dwLastFrameTime = dwCurrentTime;
	}

	if (psDeviceFlags.test(rsStatistic))	g_bEnableStatGather	= TRUE;
	else									g_bEnableStatGather	= FALSE;
	if(g_loading_events.size())
	{
		if( g_loading_events.front()() )
			g_loading_events.pop_front();
		pApp->LoadDraw				();
		return;
	}
	/*else 
	{
		if ( (!Device.dwPrecacheFrame) && (!g_SASH.IsBenchmarkRunning())
			&& g_bLoaded)
			g_SASH.StartBenchmark();*/

		FrameMove						( );
	//}

	// Precache
	if (dwPrecacheFrame)
	{
		float factor					= float(dwPrecacheFrame)/float(dwPrecacheTotal);
		float angle						= PI_MUL_2 * factor;
		vCameraDirection.set			(_sin(angle),0,_cos(angle));	vCameraDirection.normalize	();
		vCameraTop.set					(0,1,0);
		vCameraRight.crossproduct		(vCameraTop,vCameraDirection);

		mView.build_camera_dir			(vCameraPosition,vCameraDirection,vCameraTop);
	}

	// Matrices
	mFullTransform.mul			( mProject,mView	);
	m_pRender->SetCacheXform(mView, mProject);
	//RCache.set_xform_view		( mView				);
	//RCache.set_xform_project	( mProject			);
	D3DXMatrixInverse			( (D3DXMATRIX*)&mInvFullTransform, 0, (D3DXMATRIX*)&mFullTransform);

	// *** Resume threads
	// Capture end point - thread must run only ONE cycle
	// Release start point - allow thread to run
	mt_csLeave.Enter			();
	mt_csEnter.Leave			();
	Sleep						(0);

	Statistic->RenderTOTAL_Real.FrameStart	();
	Statistic->RenderTOTAL_Real.Begin		();
	if (b_is_Active)							{
		if (Begin())				{

			seqRender.Process						(rp_Render);
			if (psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || Statistic->errors.size())	
				Statistic->Show						();
			//	TEST!!!
			//Statistic->RenderTOTAL_Real.End			();
			//	Present goes here
			End										();
		}
	}
	Statistic->RenderTOTAL_Real.End			();
	Statistic->RenderTOTAL_Real.FrameEnd	();
	Statistic->RenderTOTAL.accum	= Statistic->RenderTOTAL_Real.accum;

	// *** Suspend threads
	// Capture startup point
	// Release end point - allow thread to wait for startup point
	mt_csEnter.Enter						();
	mt_csLeave.Leave						();

	// Ensure, that second thread gets chance to execute anyway
	if (dwFrame!=mt_Thread_marker)			{
		for (u32 pit=0; pit<Device.seqParallel.size(); pit++)
			Device.seqParallel[pit]			();
		Device.seqParallel.clear_not_free	();
		seqFrameMT.Process					(rp_Frame);
	}

	if (!b_is_Active)
		Sleep		(1);
}

#ifdef INGAME_EDITOR
void CRenderDevice::message_loop_editor	()
{
	m_editor->run			();
	m_editor_finalize		(m_editor);
	xr_delete				(m_engine);
}
#endif // #ifdef INGAME_EDITOR

void CRenderDevice::message_loop()
{
#ifdef INGAME_EDITOR
	if (editor()) {
		message_loop_editor	();
		return;
	}
#endif // #ifdef INGAME_EDITOR

	MSG						msg;
    PeekMessage				(&msg, NULL, 0U, 0U, PM_NOREMOVE );
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage	(&msg);
			continue;
		}

		on_idle				();
    }
}

void CRenderDevice::Run			()
{
//	DUMP_PHASE;
	g_bLoaded		= FALSE;
	Log				("Starting engine...");
	thread_name		("X-RAY Primary thread");

	// Startup timers and calculate timer delta
	dwTimeGlobal				= 0;
	Timer_MM_Delta				= 0;
	{
		u32 time_mm			= timeGetTime	();
		while (timeGetTime()==time_mm);			// wait for next tick
		u32 time_system		= timeGetTime	();
		u32 time_local		= TimerAsync	();
		Timer_MM_Delta		= time_system-time_local;
	}

	// Start all threads
//	InitializeCriticalSection	(&mt_csEnter);
//	InitializeCriticalSection	(&mt_csLeave);
	mt_csEnter.Enter			();
	mt_bMustExit				= FALSE;
	thread_spawn				(mt_Thread,"X-RAY Secondary thread",0,0);

	// Message cycle
	seqAppStart.Process			(rp_AppStart);

	//CHK_DX(HW.pDevice->Clear(0,0,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1,0));
	m_pRender->ClearTarget		();

	message_loop				();

	seqAppEnd.Process		(rp_AppEnd);

	// Stop Balance-Thread
	mt_bMustExit			= TRUE;
	mt_csEnter.Leave		();
	while (mt_bMustExit)	Sleep(0);
//	DeleteCriticalSection	(&mt_csEnter);
//	DeleteCriticalSection	(&mt_csLeave);
}

u32 app_inactive_time		= 0;
u32 app_inactive_time_start = 0;

void ProcessLoading(RP_FUNC *f);
void CRenderDevice::FrameMove()
{
	dwFrame			++;

	dwTimeContinual	= TimerMM.GetElapsed_ms() - app_inactive_time;

	if (psDeviceFlags.test(rsConstantFPS))	{
		// 20ms = 50fps
		//fTimeDelta		=	0.020f;			
		//fTimeGlobal		+=	0.020f;
		//dwTimeDelta		=	20;
		//dwTimeGlobal	+=	20;
		// 33ms = 30fps
		fTimeDelta		=	0.033f;			
		fTimeGlobal		+=	0.033f;
		dwTimeDelta		=	33;
		dwTimeGlobal	+=	33;
	} else {
		// Timer
		float fPreviousFrameTime = Timer.GetElapsed_sec(); Timer.Start();	// previous frame
		fTimeDelta = 0.1f * fTimeDelta + 0.9f*fPreviousFrameTime;			// smooth random system activity - worst case ~7% error
		//fTimeDelta = 0.7f * fTimeDelta + 0.3f*fPreviousFrameTime;			// smooth random system activity
		if (fTimeDelta>.1f)    fTimeDelta = .1f;							// limit to 15fps minimum
		if (fTimeDelta <= 0.f) fTimeDelta = EPS_S + EPS_S;					// limit to 15fps minimum
		if(Paused())	fTimeDelta = 0.0f;

//		u64	qTime		= TimerGlobal.GetElapsed_clk();
		fTimeGlobal		= TimerGlobal.GetElapsed_sec(); //float(qTime)*CPU::cycles2seconds;
		u32	_old_global	= dwTimeGlobal;
		dwTimeGlobal	= TimerGlobal.GetElapsed_ms	();	//u32((qTime*u64(1000))/CPU::cycles_per_second);
		dwTimeDelta		= dwTimeGlobal-_old_global;
	}

	// Frame move
	Statistic->EngineTOTAL.Begin	();

	//	TODO: HACK to test loading screen.
	//if(!g_bLoaded) 
		ProcessLoading				(rp_Frame);
	//else
	//	seqFrame.Process			(rp_Frame);
	Statistic->EngineTOTAL.End	();
}

void ProcessLoading				(RP_FUNC *f)
{
	Device.seqFrame.Process				(rp_Frame);
	g_bLoaded							= TRUE;
}

ENGINE_API BOOL bShowPauseString = TRUE;
#include "IGame_Persistent.h"

void CRenderDevice::Pause(BOOL bOn, BOOL bTimer, BOOL bSound, LPCSTR reason)
{
	static int snd_emitters_ = -1;

	if (g_bBenchmark)	return;


#ifdef DEBUG
//	Msg("pause [%s] timer=[%s] sound=[%s] reason=%s",bOn?"ON":"OFF", bTimer?"ON":"OFF", bSound?"ON":"OFF", reason);
#endif // DEBUG

	if(bOn)
	{
		if(!Paused())						
			bShowPauseString				= 
#ifdef INGAME_EDITOR
				editor() ? FALSE : 
#endif // #ifdef INGAME_EDITOR
				TRUE;

		if( bTimer && (!g_pGamePersistent || g_pGamePersistent->CanBePaused()) )
			g_pauseMngr.Pause				(TRUE);
	
		if (bSound && ::Sound) {
			snd_emitters_ =					::Sound->pause_emitters(true);
#ifdef DEBUG
//			Log("snd_emitters_[true]",snd_emitters_);
#endif // DEBUG
		}
	}else
	{
		if( bTimer && /*g_pGamePersistent->CanBePaused() &&*/ g_pauseMngr.Paused() )
		{
			fTimeDelta						= EPS_S + EPS_S;
			g_pauseMngr.Pause				(FALSE);
		}
		
		if(bSound)
		{
			if(snd_emitters_>0) //avoid crash
			{
				snd_emitters_ =				::Sound->pause_emitters(false);
#ifdef DEBUG
//				Log("snd_emitters_[false]",snd_emitters_);
#endif // DEBUG
			}else {
#ifdef DEBUG
				Log("Sound->pause_emitters underflow");
#endif // DEBUG
			}
		}
	}

}

BOOL CRenderDevice::Paused()
{
	return g_pauseMngr.Paused();
};

void CRenderDevice::OnWM_Activate(WPARAM wParam, LPARAM lParam)
{
	const u16 fActive						= LOWORD(wParam);
	const BOOL fMinimized					= (BOOL) HIWORD(wParam);
	const BOOL bActive					= ((fActive!=WA_INACTIVE) && (!fMinimized))?TRUE:FALSE;
	
	if (bActive!=Device.b_is_Active)
	{
		Device.b_is_Active			= bActive;

		if (Device.b_is_Active)	
		{
			Device.seqAppActivate.Process(rp_AppActivate);
			app_inactive_time		+= TimerMM.GetElapsed_ms() - app_inactive_time_start;

#	ifdef INGAME_EDITOR
			if (!editor())
#	endif // #ifdef INGAME_EDITOR
				ShowCursor			(FALSE);
		}else	
		{
			app_inactive_time_start	= TimerMM.GetElapsed_ms();
			Device.seqAppDeactivate.Process(rp_AppDeactivate);
			ShowCursor				(TRUE);
		}
	}
}

CLoadScreenRenderer::CLoadScreenRenderer()
:b_registered(false)
{}

void CLoadScreenRenderer::start(bool b_user_input) 
{
	Device.seqRender.Add			(this, 0);
	b_registered					= true;
	b_need_user_input				= b_user_input;
}

void CLoadScreenRenderer::stop()
{
	if(!b_registered)				return;
	Device.seqRender.Remove			(this);
	pApp->destroy_loading_shaders	();
	b_registered					= false;
	b_need_user_input				= false;
}

void CLoadScreenRenderer::OnRender() 
{
	pApp->load_draw_internal();
}

void CRenderDevice::time_factor(const float& time_factor)
{
    Timer.time_factor(time_factor);
    TimerGlobal.time_factor(time_factor);
    psSoundTimeFactor = time_factor; //--#SM+#--
}

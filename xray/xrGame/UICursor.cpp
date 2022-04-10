#include "stdafx.h"
#include "uicursor.h"

#include "../xrEngine/IInputReceiver.h"
#include "UI.h"
#include "HUDManager.h"
#include "ui/UIStatic.h"

CUICursor::CUICursor()
:m_static(NULL),m_b_use_win_cursor(false)
{    
	bVisible				= false;
	vPrevPos.set			(0.0f, 0.0f);
	vPos.set				(0.f,0.f);
	InitInternal			();
	Device.seqRender.Add	(this,-3/*2*/);
	Device.seqResolutionChanged.Add(this);
}
//--------------------------------------------------------------------
CUICursor::~CUICursor	()
{
	xr_delete				(m_static);
	Device.seqRender.Remove	(this);
	Device.seqResolutionChanged.Remove(this);
}

void CUICursor::OnScreenResolutionChanged()
{
	xr_delete					(m_static);
	InitInternal				();
}

void CUICursor::InitInternal()
{
	m_static					= xr_new<CUIStatic>();
	m_static->InitTextureEx		("ui\\a_menu_cursor", "hud\\cursor");
	constexpr Frect rect		{ 0.0f, 0.0f, 40.0f, 40.0f };
	m_static->SetOriginalRect	(rect);
	Fvector2 sz					{ rect.rb };
	sz.x *= UI()->get_current_kx();

	m_static->SetWndSize		(sz);
	m_static->SetStretchTexture	(true);

	u32 screen_size_x	= GetSystemMetrics( SM_CXSCREEN );
	u32 screen_size_y	= GetSystemMetrics( SM_CYSCREEN );
	m_b_use_win_cursor	= (screen_size_y >=Device.dwHeight && screen_size_x>=Device.dwWidth);
}

//--------------------------------------------------------------------
void CUICursor::OnRender	()
{
	if( !IsVisible() ) return;
#ifdef DEBUG
	static u32 last_render_frame = 0;
	VERIFY(last_render_frame != Device.dwFrame);
	last_render_frame = Device.dwFrame;

	if(bDebug)
	{
	CGameFont* F		= UI()->Font()->pFontDI;
	F->SetAligment		(CGameFont::alCenter);
	F->SetHeightI		(0.02f);
	F->OutSetI			(0.f,-0.9f);
	F->SetColor			(0xffffffff);
	Fvector2			pt = GetCursorPosition();
	F->OutNext			("%f-%f",pt.x, pt.y);
	}
#endif

	m_static->SetWndPos	(vPos);
	m_static->Update	();
	m_static->Draw		();
}

Fvector2 CUICursor::GetCursorPosition() const
{
	return  vPos;
}

Fvector2 CUICursor::GetCursorPositionDelta() const
{
	return Fvector2	{ vPos.x - vPrevPos.x , vPos.y - vPrevPos.y };
}

void CUICursor::UpdateCursorPosition(const int _dx, const int _dy)
{
	Fvector2	p;
	vPrevPos = vPos;
	if (m_b_use_win_cursor)
	{
		Ivector2 pti;
		IInputReceiver::IR_GetMousePosReal(pti);

		p.x		= (float)pti.x;
		p.y		= (float)pti.y;
		vPos.x	= p.x * (UI_BASE_WIDTH / (float)Device.dwWidth);
		vPos.y	= p.y * (UI_BASE_HEIGHT / (float)Device.dwHeight);
	}
	else
	{
		constexpr float sens = 1.0f;
		vPos.x += _dx*sens;
		vPos.y += _dy*sens;
	}
	clamp(vPos.x, 0.f, UI_BASE_WIDTH);
	clamp(vPos.y, 0.f, UI_BASE_HEIGHT);
}

void CUICursor::SetUICursorPosition(const Fvector2& pos)
{
	vPos		= pos;
	POINT		p;
	p.x			= iFloor(vPos.x / (UI_BASE_WIDTH/(float)Device.dwWidth));
	p.y			= iFloor(vPos.y / (UI_BASE_HEIGHT/(float)Device.dwHeight));
    if (m_b_use_win_cursor)
        ClientToScreen(Device.m_hWnd, (LPPOINT)&p);

	SetCursorPos(p.x, p.y);
}

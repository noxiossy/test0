#pragma once

#include "UIProgressBar.h"
#include "UIGameLog.h"
#include "UIZoneMap.h"
#include "UICarPanel.h"
#include "UIMotionIcon.h"

#include "../hudsound.h"
#include "../../xrServerEntities/alife_space.h"

class	CUIPdaMsgListItem;
class	CLAItem;
class	CUIZoneMap;
struct	GAME_NEWS_DATA;
class	CActor;
class	CWeapon;
class	CMissile;
class	CInventoryItem;
class CUIHudStatesWnd;

class CUIMainIngameWnd: public CUIWindow  
{
public:
			CUIMainIngameWnd();
	virtual ~CUIMainIngameWnd();

	virtual void Init();
	virtual void Draw();
	virtual void Update();

			bool OnKeyboardPress(int dik);

protected:
	
	CUIStatic			UIStaticQuickHelp;
	CUIZoneMap*			UIZoneMap;

	CUIHudStatesWnd*	m_ui_hud_states;

public:
	IC	void			ShowZoneMap( bool status ) { UIZoneMap->visible = status; }
		void			DrawZoneMap() { UIZoneMap->Render(); }
		void			UpdateZoneMap() { UIZoneMap->Update(); }
	
	CUIHudStatesWnd*	get_hud_states() { return m_ui_hud_states; } //temp
	void				OnSectorChanged			(int sector);

public:
	void				AnimateContacts					(bool b_snd);
	HUD_SOUND_ITEM		m_contactSnd;

	void				ReceiveNews						(GAME_NEWS_DATA* news);
	
protected:
	CMissile*			m_pGrenade;
	CInventoryItem*		m_pItem;

	// Отображение подсказок при наведении прицела на объект
	void				RenderQuickInfos();

public:
	void				OnConnected							();
	void				reset_ui							();

protected:
	CInventoryItem*		m_pPickUpItem;
	CUIStatic			UIPickUpItemIcon;

	float				m_iPickUpItemIconX;
	float				m_iPickUpItemIconY;
	float				m_iPickUpItemIconWidth;
	float				m_iPickUpItemIconHeight;

	void				UpdatePickUpItem();
public:
	void				SetPickUpItem	(CInventoryItem* PickUpItem);
#ifdef DEBUG
	void				draw_adjust_mode					();
#endif
};

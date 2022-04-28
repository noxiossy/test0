#include "stdafx.h"

#include "UIMainIngameWnd.h"
#include "UIMessagesWindow.h"
#include "../UIZoneMap.h"


#include <dinput.h>
#include "../actor.h"
#include "../ActorCondition.h"
#include "../CustomOutfit.h"
#include "../HUDManager.h"
#include "../PDA.h"
//.7#include "../WeaponHUD.h"
#include "../xrServerEntities/character_info.h"
#include "../inventory.h"
#include "../UIGameSP.h"
#include "../weaponmagazined.h"
#include "../missile.h"
#include "../Grenade.h"
#include "../xrServerEntities/xrServer_objects_ALife.h"
#include "../alife_simulator.h"
#include "../alife_object_registry.h"
#include "../game_cl_base.h"
#include "../level.h"
#include "../seniority_hierarchy_holder.h"

#include "../date_time.h"
#include "../xrServerEntities/xrServer_Objects_ALife_Monsters.h"
#include "../../xrEngine/LightAnimLibrary.h"

#include "UIInventoryUtilities.h"


#include "UIXmlInit.h"
#include "UIPdaMsgListItem.h"
#include "../alife_registry_wrappers.h"
#include "../actorcondition.h"

#include "../string_table.h"

#ifdef DEBUG
#	include "../attachable_item.h"
#	include "../../xrEngine/xr_input.h"
#endif

#include "UIScrollView.h"
#include "map_hint.h"
#include "UIColorAnimatorWrapper.h"
#include "../game_news.h"

#include "static_cast_checked.hpp"
#include "game_cl_capture_the_artefact.h"
#include "UIHudStatesWnd.h"
#include "UIActorMenu.h"

void test_draw	();
void test_key	(int dik);

#include "../Include/xrRender/Kinematics.h"
#include <functional>

using namespace InventoryUtilities;
//BOOL		g_old_style_ui_hud			= FALSE;
const u32	g_clWhite					= 0xffffffff;

#define		DEFAULT_MAP_SCALE			1.f

#define		C_SIZE						0.025f
#define		NEAR_LIM					0.5f

#define		SHOW_INFO_SPEED				0.5f
#define		HIDE_INFO_SPEED				10.f
#define		C_ON_ENEMY					D3DCOLOR_XRGB(0xff,0,0)
#define		C_DEFAULT					D3DCOLOR_XRGB(0xff,0xff,0xff)


CUIMainIngameWnd::CUIMainIngameWnd()
{
//	m_pWeapon					= NULL;
	m_pGrenade					= NULL;
	m_pItem						= NULL;
	UIZoneMap					= xr_new<CUIZoneMap>();
	m_pPickUpItem				= NULL;
}

#include "UIProgressShape.h"
extern CUIProgressShape* g_MissileForceShape;

CUIMainIngameWnd::~CUIMainIngameWnd()
{
	xr_delete					(UIZoneMap);
	HUD_SOUND_ITEM::DestroySound(m_contactSnd);
	xr_delete					(g_MissileForceShape);
}

void CUIMainIngameWnd::Init()
{
	CUIXml						uiXml;
	uiXml.Load					(CONFIG_PATH, UI_PATH, "a_hud.xml");
	
	CUIXmlInit					xml_init;
	xml_init.InitWindow			(uiXml,"main",0,this);

	Enable(false);

	AttachChild					(&UIPickUpItemIcon);
	xml_init.InitStatic			(uiXml, "pick_up_item", 0, &UIPickUpItemIcon);
	UIPickUpItemIcon.SetShader	(GetEquipmentIconsShader());
	UIPickUpItemIcon.ClipperOn	();

	m_iPickUpItemIconWidth		= UIPickUpItemIcon.GetWidth();
	m_iPickUpItemIconHeight		= UIPickUpItemIcon.GetHeight();
	m_iPickUpItemIconX			= UIPickUpItemIcon.GetWndRect().left;
	m_iPickUpItemIconY			= UIPickUpItemIcon.GetWndRect().top;
	//---------------------------------------------------------

	//индикаторы 
	UIZoneMap->Init				();

	// Подсказки, которые возникают при наведении прицела на объект
	AttachChild					(&UIStaticQuickHelp);
	xml_init.InitStatic			(uiXml, "quick_info", 0, &UIStaticQuickHelp);

	uiXml.SetLocalRoot			(uiXml.GetRoot());

	m_ui_hud_states							= xr_new<CUIHudStatesWnd>();
	m_ui_hud_states->SetAutoDelete			(true);
	AttachChild								(m_ui_hud_states);
	m_ui_hud_states->InitFromXml			(uiXml, "hud_states");

	HUD_SOUND_ITEM::LoadSound					("maingame_ui", "snd_new_contact", m_contactSnd, SOUND_TYPE_IDLE);
}

void CUIMainIngameWnd::Draw()
{
	CActor* m_pActor		= smart_cast<CActor*>(Level().CurrentViewEntity());
#ifdef DEBUG
	test_draw				();
#endif
	if ( !m_pActor || !m_pActor->g_Alive() ) return;

	CUIWindow::Draw();

	UIZoneMap->visible = true;
	UIZoneMap->Render();

	RenderQuickInfos();		
}

void CUIMainIngameWnd::Update()
{
	CUIWindow::Update();
	CActor* m_pActor = smart_cast<CActor*>(Level().CurrentViewEntity());

	if ( !m_pActor )
	{
		m_pItem				= NULL;
		m_pGrenade			= NULL;
		return;
	}

	UIZoneMap->Update();

	UpdatePickUpItem			();

	if( Device.dwFrame % 10 )
	{
		return;
	}
	return false;
}

void CUIMainIngameWnd::RenderQuickInfos()
{
	CActor* m_pActor		= smart_cast<CActor*>(Level().CurrentViewEntity());
	if (!m_pActor)
		return;

	static CGameObject *pObject			= NULL;
	LPCSTR actor_action					= m_pActor->GetDefaultActionForObject();
	UIStaticQuickHelp.Show				(NULL!=actor_action);

	if(NULL!=actor_action){
		if(stricmp(actor_action,UIStaticQuickHelp.GetText()))
			UIStaticQuickHelp.SetTextST				(actor_action);
	}

	if(pObject!=m_pActor->ObjectWeLookingAt())
	{
		UIStaticQuickHelp.SetTextST				(actor_action?actor_action:" ");
		UIStaticQuickHelp.ResetClrAnimation		();
		pObject	= m_pActor->ObjectWeLookingAt	();
	}
}

void CUIMainIngameWnd::ReceiveNews(GAME_NEWS_DATA* news)
{
	VERIFY(news->texture_name.size());

//	HUD().GetUI()->m_pMessagesWnd->AddIconedPdaMessage(news->texture_name.c_str(), news->tex_rect, news->SingleLineText(), news->show_time);
	HUD().GetUI()->m_pMessagesWnd->AddIconedPdaMessage(news);
	HUD().GetUI()->UpdatePda();
}

void CUIMainIngameWnd::AnimateContacts(bool b_snd)
{
	UIZoneMap->Counter_ResetClrAnimation();

	if(b_snd)
		HUD_SOUND_ITEM::PlaySound	(m_contactSnd, Fvector().set(0,0,0), 0, true );

}

void CUIMainIngameWnd::SetPickUpItem	(CInventoryItem* PickUpItem)
{
	m_pPickUpItem = PickUpItem;
};

void CUIMainIngameWnd::UpdatePickUpItem	()
{
	if (!m_pPickUpItem || !Level().CurrentViewEntity() || !smart_cast<CActor*>(Level().CurrentViewEntity())) 
	{
		UIPickUpItemIcon.Show(false);
		return;
	};


	shared_str sect_name	= m_pPickUpItem->object().cNameSect();

	//properties used by inventory menu
	int m_iGridWidth	= pSettings->r_u32(sect_name, "inv_grid_width");
	int m_iGridHeight	= pSettings->r_u32(sect_name, "inv_grid_height");

	int m_iXPos			= pSettings->r_u32(sect_name, "inv_grid_x");
	int m_iYPos			= pSettings->r_u32(sect_name, "inv_grid_y");

	float scale_x = m_iPickUpItemIconWidth/
		float(m_iGridWidth*INV_GRID_WIDTH);

	float scale_y = m_iPickUpItemIconHeight/
		float(m_iGridHeight*INV_GRID_HEIGHT);

	scale_x = (scale_x>1) ? 1.0f : scale_x;
	scale_y = (scale_y>1) ? 1.0f : scale_y;

	float scale = scale_x<scale_y?scale_x:scale_y;

	UIPickUpItemIcon.GetUIStaticItem().SetOriginalRect(
		float(m_iXPos * INV_GRID_WIDTH),
		float(m_iYPos * INV_GRID_HEIGHT),
		float(m_iGridWidth * INV_GRID_WIDTH),
		float(m_iGridHeight * INV_GRID_HEIGHT));

	UIPickUpItemIcon.SetStretchTexture(true);

	UIPickUpItemIcon.SetWidth(m_iGridWidth*INV_GRID_WIDTH*scale);
	UIPickUpItemIcon.SetHeight(m_iGridHeight*INV_GRID_HEIGHT*scale);

	UIPickUpItemIcon.SetWndPos(Fvector2().set(	m_iPickUpItemIconX+(m_iPickUpItemIconWidth-UIPickUpItemIcon.GetWidth())/2.0f,
												m_iPickUpItemIconY+(m_iPickUpItemIconHeight-UIPickUpItemIcon.GetHeight())/2.0f) );

	UIPickUpItemIcon.SetColor(color_rgba(255,255,255,192));
	UIPickUpItemIcon.Show(true);
};

void CUIMainIngameWnd::OnConnected()
{
	UIZoneMap->SetupCurrentMap();
}

void CUIMainIngameWnd::OnSectorChanged(int sector)
{
	UIZoneMap->OnSectorChanged(sector);
}

void CUIMainIngameWnd::reset_ui()
{
	m_pGrenade						= NULL;
	m_pItem							= NULL;
	m_pPickUpItem					= NULL;
}


#include "../../xrEngine/xr_input.h"
#include "../GamePersistent.h"

void hud_adjust_mode_keyb(int dik);
void hud_draw_adjust_mode();

#ifdef DEBUG
	void attach_adjust_mode_keyb(int dik);
	void attach_draw_adjust_mode();
#endif

struct TS{
	ref_sound test_sound;
};
TS* pTS = NULL;
void test_key(int dik)
{
	hud_adjust_mode_keyb	(dik);
#ifdef DEBUG
	attach_adjust_mode_keyb	(dik);
#endif
}

void test_draw()
{
	hud_draw_adjust_mode();
#ifdef DEBUG
	attach_draw_adjust_mode();
#endif
}


#include "stdafx.h"
#include "UIHudStatesWnd.h"

#include "../Actor.h"
#include "../ActorCondition.h"
#include "../EntityCondition.h"
#include "../CustomOutfit.h"
#include "../inventory.h"

#include "UIStatic.h"
#include "UIProgressBar.h"
#include "UIXmlInit.h"
#include "UIHelper.h"
#include "UIInventoryUtilities.h"
#include "../HUDManager.h"

CUIHudStatesWnd::CUIHudStatesWnd()
{
}

CUIHudStatesWnd::~CUIHudStatesWnd()
{
}

void CUIHudStatesWnd::InitFromXml( CUIXml& xml, LPCSTR path )
{
	CUIXmlInit::InitWindow( xml, path, 0, this );
	XML_NODE* stored_root = xml.GetLocalRoot();
	
	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );

	m_back            = UIHelper::CreateStatic( xml, "back", this );
	m_back_v          = UIHelper::CreateStatic( xml, "back_v", this );

	m_ui_weapon_sign_ammo = UIHelper::CreateStatic( xml, "static_ammo", this );
	//m_ui_weapon_sign_ammo->SetEllipsis( CUIStatic::eepEnd, 2 );
	
	m_ui_weapon_icon = UIHelper::CreateStatic( xml, "static_wpn_icon", this );
	m_ui_weapon_icon->SetShader( InventoryUtilities::GetEquipmentIconsShader() );
	m_ui_weapon_icon->Enable( false );
	m_ui_weapon_icon_rect = m_ui_weapon_icon->GetWndRect();

	m_fire_mode = UIHelper::CreateStatic( xml, "static_fire_mode", this );
	
	m_ui_health_bar   = UIHelper::CreateProgressBar( xml, "progress_bar_health", this );
	m_ui_stamina_bar  = UIHelper::CreateProgressBar( xml, "progress_bar_stamina", this );


	m_ind_start_line	= UIHelper::CreateStatic( xml, "indicator_start_line", this);
	m_ind_bleeding		= UIHelper::CreateStatic( xml, "indicator_bleeding", this);
	m_ind_radiation		= UIHelper::CreateStatic( xml, "indicator_radiation", this);
	m_ind_starvation	= UIHelper::CreateStatic( xml, "indicator_starvation", this);
	m_ind_weapon_broken	= UIHelper::CreateStatic( xml, "indicator_weapon_broken", this);
	m_ind_psyhealth		= UIHelper::CreateStatic( xml, "indicator_psy", this);
	m_ind_overweight	= UIHelper::CreateStatic( xml, "indicator_overweight", this);
	
	xml.SetLocalRoot( stored_root );
}

void CUIHudStatesWnd::Update()
{
	CActor* actor = smart_cast<CActor*>( Level().CurrentViewEntity() );
	if ( !actor )
	{
		return;
	}
	/*if ( Device.dwTimeGlobal - m_last_time > 50 )
	{
		m_last_time = Device.dwTimeGlobal;
	}
	*/
	UpdateHealth( actor );
	UpdateActiveItemInfo( actor );
	UpdateIndicators( actor );
	

	inherited::Update();
}

void CUIHudStatesWnd::UpdateIndicators( CActor* actor )
{
	float hin = 0.0f;
	float xin = 0.0f;
	Fvector2 pos;
	hin = m_ind_start_line->GetWndPos().y;
	xin = m_ind_start_line->GetWndPos().x;

	// BLEEDING ICON
	float bleeding = actor->conditions().BleedingSpeed();
	u32 a_bleeding = (u32)( (1 - pow((1-bleeding),3) ) * 255);
	u32 a_reverse_bleeding = (u32)( (1 - pow(bleeding,3) ) * 255);
	m_ind_bleeding->SetTextureColor(color_rgba(a_bleeding,a_reverse_bleeding,0,255)); 
	if(bleeding < 0.05f)
		m_ind_bleeding->Show(false);
	else
	{
		m_ind_bleeding->Show(true);

        	pos.set(m_ind_bleeding->GetWndPos());
        	pos.y = hin;
       		pos.x = xin;
        	m_ind_bleeding->SetWndPos(pos);
        	hin -= m_ind_bleeding->GetWndSize().y;
	}

	// RADIATION ICON
	float radiation = actor->conditions().GetRadiation();
	u32 a_radiation = (u32)( (1 - pow((1-radiation),3) ) * 255);
	u32 a_reverse_radiation = (u32)( (1 - pow(radiation,3) ) * 255);
	m_ind_radiation->SetTextureColor(color_rgba(a_radiation,a_reverse_radiation,0,255)); 
	if(radiation < 0.1f)
		m_ind_radiation->Show(false);
	else
	{
		m_ind_radiation->Show(true);

        	pos.set(m_ind_radiation->GetWndPos());
        	pos.y = hin;
       		pos.x = xin;
        	m_ind_radiation->SetWndPos(pos);
        	hin -= m_ind_radiation->GetWndSize().y;
	}

	// STARVATION ICON
 	float satiety = actor->conditions().GetSatiety();
	u32 a_satiety = (u32)( (1 - pow(satiety,3) ) * 255);
	u32 a_reverse_satiety = (u32)( (1 - pow((1-satiety),3) ) * 255);
	m_ind_starvation->SetTextureColor(color_rgba(a_satiety,a_reverse_satiety,0,255)); 
	if(satiety > 0.8f)
		m_ind_starvation->Show(false);
	else
	{
		m_ind_starvation->Show(true);
		
        	pos.set(m_ind_starvation->GetWndPos());
        	pos.y = hin;
       		pos.x = xin;
        	m_ind_starvation->SetWndPos(pos);
        	hin -= m_ind_starvation->GetWndSize().y;
	}

	// WEAPON BROKEN ICON
	u32 slot = actor->inventory().GetActiveSlot();
	m_ind_weapon_broken->Show(false);
	if(slot==PISTOL_SLOT || slot==RIFLE_SLOT)
	{
		CWeapon* weapon = smart_cast<CWeapon*>(actor->inventory().ItemFromSlot(slot));
		if(weapon)
		{
			float condition = weapon->GetCondition();
			if(condition < 0.8f)
			{
				m_ind_weapon_broken->Show(true);
				u32 a_condition = (u32)( (1 - pow(condition,3) ) * 255);
				u32 a_reverse_condition = (u32)( (1 - pow((1-condition),3) ) * 255);
				m_ind_weapon_broken->SetTextureColor(color_rgba(a_condition,a_reverse_condition,0,255));

				pos.set(m_ind_weapon_broken->GetWndPos());
        			pos.y = hin;
       				pos.x = xin;
        			m_ind_weapon_broken->SetWndPos(pos);
        			hin -= m_ind_weapon_broken->GetWndSize().y;
			}
		}
	}

	// PSYHEALTH ICON
	float psyhealth = actor->conditions().GetPsyHealth();
	u32 a_psy = (u32)( (1 - pow(psyhealth,3) ) * 255);
	u32 a_reverse_psy = (u32)( (1 - pow((1-psyhealth),3) ) * 255);
	m_ind_psyhealth->SetTextureColor(color_rgba(a_psy,a_reverse_psy,0,255)); 
	if(psyhealth > 0.8f)
		m_ind_psyhealth->Show(false);
	else
	{
		m_ind_psyhealth->Show(true);
		
        	pos.set(m_ind_psyhealth->GetWndPos());
        	pos.y = hin;
       		pos.x = xin;
        	m_ind_psyhealth->SetWndPos(pos);
        	hin -= m_ind_psyhealth->GetWndSize().y;
	}

	// OVERWEIGHT ICON
	bool b_God = GodMode();
	float cur_weight = actor->inventory().TotalWeight();
	float max_weight = actor->MaxWalkWeight();
	float dif_weight = (cur_weight/max_weight);
	u32 a_weight = (u32)( (1 - pow((1-dif_weight),3) ) * 255);
	u32 a_reverse_weight = (u32)( (1 - pow(dif_weight,3) ) * 255);
	if (cur_weight>=max_weight && !b_God)
	{
		a_weight = 255;
		a_reverse_weight = 0;
	}
	m_ind_overweight->SetTextureColor(color_rgba(a_weight,a_reverse_weight,0,255)); 
	m_ind_overweight->Show(false);
	if(cur_weight>=max_weight-15.0f && !b_God)
	{
		m_ind_overweight->Show(true);

       	       	pos.set(m_ind_overweight->GetWndPos());
        	pos.y = hin;
       		pos.x = xin;
        	m_ind_overweight->SetWndPos(pos);
        	hin -= m_ind_overweight->GetWndSize().y;
	}
}

void CUIHudStatesWnd::UpdateHealth( CActor* actor )
{
	m_ui_health_bar->SetProgressPos( actor->GetfHealth() * 100.0f );
	m_ui_stamina_bar->SetProgressPos( actor->conditions().GetPower()*100.0f );
}

void CUIHudStatesWnd::UpdateActiveItemInfo( CActor* actor )
{
	PIItem item = actor->inventory().ActiveItem();
	if ( item ) 
	{
		xr_string	str_name;
		xr_string	icon_sect_name;
		xr_string	str_count;
		string16	str_fire_mode;
		strcpy_s					( str_fire_mode, sizeof(str_fire_mode), "" );
		item->GetBriefInfo			( str_name, icon_sect_name, str_count, str_fire_mode );

		m_ui_weapon_sign_ammo->Show	( true );
//		UIWeaponBack.SetText		( str_name.c_str() );
		m_fire_mode->Show			( true );
		m_fire_mode->SetText		( str_fire_mode );
		SetAmmoIcon					( icon_sect_name.c_str() );
		m_ui_weapon_sign_ammo->SetText( str_count.c_str() );
		
		// hack ^ begin

		CGameFont* pFont32 = UI()->Font()->pFontGraffiti32Russian;
		CGameFont* pFont22 = UI()->Font()->pFontGraffiti22Russian;
		CGameFont* pFont   = pFont32;

		if ( UI()->is_16_9_mode() )
		{
			pFont = pFont22;
		}
		else
		{
			if ( str_count.size() > 5 )
			{
				pFont = pFont22;
			}
		}
		m_ui_weapon_sign_ammo->SetFont( pFont );
	}
	else
	{
		m_ui_weapon_icon->Show		( false );
		m_ui_weapon_sign_ammo->Show	( false );
		m_fire_mode->Show			( false );
	}
}

void CUIHudStatesWnd::SetAmmoIcon( const shared_str& sect_name )
{
	if ( !sect_name.size() )
	{
		m_ui_weapon_icon->Show( false );
		return;
	}

	m_ui_weapon_icon->Show( true );

	
	if ( pSettings->line_exist( sect_name, "inv_icon" ) ) //temp
	{
		LPCSTR icon_name = pSettings->r_string( sect_name, "inv_icon" );
		m_ui_weapon_icon->InitTexture( icon_name );
	}
	else
	{
		//properties used by inventory menu
		float gridWidth  = pSettings->r_float( sect_name, "inv_grid_width"  );
		float gridHeight = pSettings->r_float( sect_name, "inv_grid_height" );

		float xPos = pSettings->r_float(sect_name, "inv_grid_x");
		float yPos = pSettings->r_float(sect_name, "inv_grid_y");

		m_ui_weapon_icon->GetUIStaticItem().SetOriginalRect(
			( xPos      * INV_GRID_WIDTH ), ( yPos       * INV_GRID_HEIGHT ),
			( gridWidth * INV_GRID_WIDTH ), ( gridHeight * INV_GRID_HEIGHT ) );
		m_ui_weapon_icon->SetStretchTexture( true );

		// now perform only width scale for ammo, which (W)size >2
		// all others ammo (1x1, 1x2) will be not scaled (original picture)
		float h = gridHeight * INV_GRID_HEIGHT * 0.65f;
		float w = gridWidth  * INV_GRID_WIDTH  * 0.65f;
		if ( gridWidth > 2.01f )
		{
			w = INV_GRID_WIDTH * 1.5f;
		}

		bool is_16x10 = UI()->is_16_9_mode();
		if ( gridWidth < 1.01f )
		{
			m_ui_weapon_icon->SetTextureOffset( (is_16x10)? 8.33f : 10.0f, 0.0f );
		}
		else
		{
			m_ui_weapon_icon->SetTextureOffset( 0.0f, 0.0f );
		}


		m_ui_weapon_icon->SetWidth( (is_16x10)? w*0.833f : w );
		m_ui_weapon_icon->SetHeight( h );
	}

}

#include "stdafx.h"
#include "Weapon.h"
#include "ParticlesObject.h"
#include "HUDManager.h"
#include "entity_alive.h"
#include "inventory_item_impl.h"
#include "inventory.h"
#include "xrserver_objects_alife_items.h"
#include "actor.h"
#include "actoreffector.h"
#include "level.h"
#include "xr_level_controller.h"
#include "game_cl_base.h"
#include "../Include/xrRender/Kinematics.h"
#include "ai_object_location.h"
#include "mathutils.h"
#include "object_broker.h"
#include "player_hud.h"
#include "gamepersistent.h"
#include "effectorFall.h"
#include "debug_renderer.h"
#include "static_cast_checked.hpp"
#include "clsid_game.h"
#include "ui/UIWindow.h"
#include "WeaponMagazinedWGrenade.h"

#define WEAPON_REMOVE_TIME		60000
#define ROTATION_TIME			0.25f

BOOL	b_toggle_weapon_aim		= FALSE;

CWeapon::CWeapon()
{
	SetState				(eHidden);
	SetNextState			(eHidden);
	m_sub_state				= eSubstateReloadBegin;
	m_bTriStateReload		= false;
	SetDefaults				();

	m_Offset.identity		();
	m_StrapOffset.identity	();

	iAmmoCurrent			= -1;
	m_dwAmmoCurrentCalcFrame= 0;

	iAmmoElapsed			= -1;
	iMagazineSize			= -1;
	m_ammoType				= 0;
	m_ammoName				= NULL;

	eHandDependence			= hdNone;

	m_zoom_params.m_fCurrentZoomFactor			= g_fov;
	m_zoom_params.m_fZoomRotationFactor			= 0.f;

	m_pAmmo					= nullptr;


	m_pFlameParticles2		= nullptr;
	m_sFlameParticles2		= nullptr;


	m_fCurrentCartirdgeDisp = 1.f;

	m_strap_bone0			= nullptr;
	m_strap_bone1			= nullptr;
	m_StrapOffset.identity	();
	m_strapped_mode			= false;
	m_can_be_strapped		= false;
	m_ef_main_weapon_type	= u32(-1);
	m_ef_weapon_type		= u32(-1);
	m_UIScope				= nullptr;
	m_set_next_ammoType_on_reload = u32(-1);
	m_crosshair_inertion	= 0.f;
	m_activation_speed_is_overriden	=	false;
	bScopeIsHasTexture = false;
	m_nearwall_last_hud_fov = psHUD_FOV_def;
}

CWeapon::~CWeapon		()
{
	xr_delete	(m_UIScope);
}

void CWeapon::Hit					(SHit* pHDS)
{
	inherited::Hit(pHDS);
}



void CWeapon::UpdateXForm	()
{
	if (Device.dwFrame == dwXF_Frame)
		return;

	dwXF_Frame				= Device.dwFrame;

	if (!H_Parent())
		return;

	// Get access to entity and its visual
	CEntityAlive*			E = smart_cast<CEntityAlive*>(H_Parent());
	
    if (!E)
       return;

	const CInventoryOwner	*parent = smart_cast<const CInventoryOwner*>(E);
	if (!parent || parent && parent->use_simplified_visual())
		return;

	if (parent->attached(this))
		return;

	IKinematics*			V = smart_cast<IKinematics*>	(E->Visual());
	VERIFY					(V);

	// Get matrices
	int						boneL = -1, boneR = -1, boneR2 = -1;

	// this ugly case is possible in case of a CustomMonster, not a Stalker, nor an Actor
	E->g_WeaponBones		(boneL,boneR,boneR2);

	if (boneR == -1)		return;

	if ((HandDependence() == hd1Hand) || (GetState() == eReload) || (!E->g_Alive()))
		boneL				= boneR2;

	V->CalculateBones_Invalidate		();
	V->CalculateBones			(true);
	Fmatrix& mL				= V->LL_GetTransform(u16(boneL));
	Fmatrix& mR				= V->LL_GetTransform(u16(boneR));
	// Calculate
	Fmatrix					mRes;
	Fvector					R,D,N;
	D.sub					(mL.c,mR.c);	

	if(fis_zero(D.magnitude())) {
		mRes.set			(E->XFORM());
		mRes.c.set			(mR.c);
	}
	else {		
		D.normalize			();
		R.crossproduct		(mR.j,D);

		N.crossproduct		(D,R);			
		N.normalize			();

		mRes.set			(R,N,D,mR.c);
		mRes.mulA_43		(E->XFORM());
	}

	UpdatePosition			(mRes);
}

void CWeapon::UpdateFireDependencies_internal()
{
	if (Device.dwFrame!=dwFP_Frame) 
	{
		dwFP_Frame			= Device.dwFrame;

		UpdateXForm			();

		if ( GetHUDmode() )
		{
			HudItemData()->setup_firedeps		(m_current_firedeps);
			VERIFY(_valid(m_current_firedeps.m_FireParticlesXForm));
		} else 
		{
			// 3rd person or no parent
			Fmatrix& parent			= XFORM();
			Fvector& fp				= vLoadedFirePoint;
			Fvector& fp2			= vLoadedFirePoint2;
			Fvector& sp				= vLoadedShellPoint;

			parent.transform_tiny	(m_current_firedeps.vLastFP,fp);
			parent.transform_tiny	(m_current_firedeps.vLastFP2,fp2);
			parent.transform_tiny	(m_current_firedeps.vLastSP,sp);
			
			m_current_firedeps.vLastFD.set	(0.f,0.f,1.f);
			parent.transform_dir	(m_current_firedeps.vLastFD);

			m_current_firedeps.m_FireParticlesXForm.set(parent);
			VERIFY(_valid(m_current_firedeps.m_FireParticlesXForm));
		}
	}
}

void CWeapon::ForceUpdateFireParticles()
{
	if ( !GetHUDmode() )
	{//update particlesXFORM real bullet direction

		if (!H_Parent())
			return;

		Fvector					p, d; 
		smart_cast<CEntity*>(H_Parent())->g_fireParams	(this, p,d);

		Fmatrix						_pxf;
		_pxf.k						= d;
		_pxf.i.crossproduct			(Fvector().set(0.0f,1.0f,0.0f),	_pxf.k);
		_pxf.j.crossproduct			(_pxf.k,		_pxf.i);
		_pxf.c						= XFORM().c;
		
		m_current_firedeps.m_FireParticlesXForm.set	(_pxf);
	}
}

constexpr const char* wpn_scope_def_bone = "wpn_scope";
constexpr const char* wpn_silencer_def_bone = "wpn_silencer";
constexpr const char* wpn_launcher_def_bone_shoc = "wpn_launcher";
constexpr const char* wpn_launcher_def_bone_cop = "wpn_grenade_launcher";

void CWeapon::Load		(LPCSTR section)
{
	inherited::Load					(section);
	CShootingObject::Load			(section);

	
	if(pSettings->line_exist(section, "flame_particles_2"))
		m_sFlameParticles2 = pSettings->r_string(section, "flame_particles_2");

	// load ammo classes
	m_ammoTypes.clear	(); 
	LPCSTR				S = pSettings->r_string(section,"ammo_class");
	if (S && S[0]) 
	{
		string128		_ammoItem;
		int				count		= _GetItemCount	(S);
		for (int it=0; it<count; ++it)	
		{
			_GetItem				(S,it,_ammoItem);
			m_ammoTypes.push_back	(_ammoItem);
		}
		m_ammoName = pSettings->r_string(*m_ammoTypes[0],"inv_name_short");
	}
	else
		m_ammoName = 0;

	iAmmoElapsed		= pSettings->r_s32		(section,"ammo_elapsed"		);
	iMagazineSize		= pSettings->r_s32		(section,"ammo_mag_size"	);
	
	////////////////////////////////////////////////////
	// ��������� ��������

	//������������� ������ �� ����� ������
	u8 rm = READ_IF_EXISTS( pSettings, r_u8, section, "cam_return", 1 );
	cam_recoil.ReturnMode = (rm == 1);
	
	rm = READ_IF_EXISTS( pSettings, r_u8, section, "cam_return_stop", 0 );
	cam_recoil.StopReturn = (rm == 1);

	float temp_f = 0.0f;
	temp_f					= pSettings->r_float( section,"cam_relax_speed" );
	cam_recoil.RelaxSpeed	= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.RelaxSpeed) );
	if ( fis_zero(cam_recoil.RelaxSpeed) )
	{
		cam_recoil.RelaxSpeed = EPS_L;
	}

	cam_recoil.RelaxSpeed_AI = cam_recoil.RelaxSpeed;
	if ( pSettings->line_exist( section, "cam_relax_speed_ai" ) )
	{
		temp_f						= pSettings->r_float( section, "cam_relax_speed_ai" );
		cam_recoil.RelaxSpeed_AI	= _abs( deg2rad( temp_f ) );
		VERIFY( !fis_zero(cam_recoil.RelaxSpeed_AI) );
		if ( fis_zero(cam_recoil.RelaxSpeed_AI) )
		{
			cam_recoil.RelaxSpeed_AI = EPS_L;
		}
	}
	temp_f						= pSettings->r_float( section, "cam_max_angle" );
	cam_recoil.MaxAngleVert		= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.MaxAngleVert) );
	if ( fis_zero(cam_recoil.MaxAngleVert) )
	{
		cam_recoil.MaxAngleVert = EPS;
	}
	
	temp_f						= pSettings->r_float( section, "cam_max_angle_horz" );
	cam_recoil.MaxAngleHorz		= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.MaxAngleHorz) );
	if ( fis_zero(cam_recoil.MaxAngleHorz) )
	{
		cam_recoil.MaxAngleHorz = EPS;
	}
	
	temp_f						= pSettings->r_float( section, "cam_step_angle_horz" );
	cam_recoil.StepAngleHorz	= deg2rad( temp_f );
	
	cam_recoil.DispersionFrac	= _abs( READ_IF_EXISTS( pSettings, r_float, section, "cam_dispersion_frac", 0.7f ) );

	//������������� ������ �� ����� ������ � ������ zoom ==> ironsight or scope
	//zoom_cam_recoil.Clone( cam_recoil ); ==== ������ !!!!!!!!!!
	zoom_cam_recoil.RelaxSpeed		= cam_recoil.RelaxSpeed;
	zoom_cam_recoil.RelaxSpeed_AI	= cam_recoil.RelaxSpeed_AI;
	zoom_cam_recoil.DispersionFrac	= cam_recoil.DispersionFrac;
	zoom_cam_recoil.MaxAngleVert	= cam_recoil.MaxAngleVert;
	zoom_cam_recoil.MaxAngleHorz	= cam_recoil.MaxAngleHorz;
	zoom_cam_recoil.StepAngleHorz	= cam_recoil.StepAngleHorz;

	zoom_cam_recoil.ReturnMode		= cam_recoil.ReturnMode;
	zoom_cam_recoil.StopReturn		= cam_recoil.StopReturn;

	
	if ( pSettings->line_exist( section, "zoom_cam_relax_speed" ) )
	{
		zoom_cam_recoil.RelaxSpeed		= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_relax_speed" ) ) );
		VERIFY( !fis_zero(zoom_cam_recoil.RelaxSpeed) );
		if ( fis_zero(zoom_cam_recoil.RelaxSpeed) )
		{
			zoom_cam_recoil.RelaxSpeed = EPS_L;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_relax_speed_ai" ) )
	{
		zoom_cam_recoil.RelaxSpeed_AI	= _abs( deg2rad( pSettings->r_float( section,"zoom_cam_relax_speed_ai" ) ) ); 
		VERIFY( !fis_zero(zoom_cam_recoil.RelaxSpeed_AI) );
		if ( fis_zero(zoom_cam_recoil.RelaxSpeed_AI) )
		{
			zoom_cam_recoil.RelaxSpeed_AI = EPS_L;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_max_angle" ) )
	{
		zoom_cam_recoil.MaxAngleVert	= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_max_angle" ) ) );
		VERIFY( !fis_zero(zoom_cam_recoil.MaxAngleVert) );
		if ( fis_zero(zoom_cam_recoil.MaxAngleVert) )
		{
			zoom_cam_recoil.MaxAngleVert = EPS;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_max_angle_horz" ) )
	{
		zoom_cam_recoil.MaxAngleHorz	= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_max_angle_horz" ) ) ); 
		VERIFY( !fis_zero(zoom_cam_recoil.MaxAngleHorz) );
		if ( fis_zero(zoom_cam_recoil.MaxAngleHorz) )
		{
			zoom_cam_recoil.MaxAngleHorz = EPS;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_step_angle_horz" ) )	{
		zoom_cam_recoil.StepAngleHorz	= deg2rad( pSettings->r_float( section, "zoom_cam_step_angle_horz" ) ); 
	}
	if ( pSettings->line_exist( section, "zoom_cam_dispersion_frac" ) )	{
		zoom_cam_recoil.DispersionFrac	= _abs( pSettings->r_float( section, "zoom_cam_dispersion_frac" ) );
	}

	m_pdm.m_fPDM_disp_base			= pSettings->r_float( section, "PDM_disp_base"			);
	m_pdm.m_fPDM_disp_vel_factor	= pSettings->r_float( section, "PDM_disp_vel_factor"	);
	m_pdm.m_fPDM_disp_accel_factor	= pSettings->r_float( section, "PDM_disp_accel_factor"	);
	m_pdm.m_fPDM_disp_crouch		= pSettings->r_float( section, "PDM_disp_crouch"		);
	m_pdm.m_fPDM_disp_crouch_no_acc	= pSettings->r_float( section, "PDM_disp_crouch_no_acc" );
	m_crosshair_inertion			= READ_IF_EXISTS(pSettings, r_float, section, "crosshair_inertion",	5.91f);
	m_first_bullet_controller.load	(section);

	fireDispersionConditionFactor = pSettings->r_float(section,"fire_dispersion_condition_factor"); 
	misfireProbability			  = pSettings->r_float(section,"misfire_probability"); 
	misfireConditionK			  = READ_IF_EXISTS(pSettings, r_float, section, "misfire_condition_k",	1.0f);
	conditionDecreasePerShot	  = pSettings->r_float(section,"condition_shot_dec"); 
		
	vLoadedFirePoint	= pSettings->r_fvector3		(section,"fire_point"		);
	
	if(pSettings->line_exist(section,"fire_point2")) 
		vLoadedFirePoint2= pSettings->r_fvector3	(section,"fire_point2");
	else 
		vLoadedFirePoint2= vLoadedFirePoint;

	// hands
	eHandDependence		= EHandDependence(pSettings->r_s32(section,"hand_dependence"));
	m_bIsSingleHanded	= true;
	if (pSettings->line_exist(section, "single_handed"))
		m_bIsSingleHanded	= !!pSettings->r_bool(section, "single_handed");
	// 
	m_fMinRadius		= pSettings->r_float		(section,"min_radius");
	m_fMaxRadius		= pSettings->r_float		(section,"max_radius");


	// ���������� � ��������� ��������� � �� ������������ � ���������
	m_eScopeStatus			 = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"scope_status");
	m_eSilencerStatus		 = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"silencer_status");
	m_eGrenadeLauncherStatus = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"grenade_launcher_status");

	m_zoom_params.m_bZoomEnabled		= !!pSettings->r_bool(section,"zoom_enabled");
	m_zoom_params.m_fZoomRotateTime		= pSettings->r_float(section,"zoom_rotate_time");

	m_allScopeNames.clear();
	//m_highlightAddons.clear();
	if ( m_eScopeStatus == ALife::eAddonAttachable )
	{
		m_sScopeName = pSettings->r_string(section,"scope_name");
		m_iScopeX = pSettings->r_s32(section,"scope_x");
		m_iScopeY = pSettings->r_s32(section,"scope_y");

                m_allScopeNames.push_back( m_sScopeName );
                if ( pSettings->line_exist( section, "scope_names" ) ) {
                  LPCSTR S = pSettings->r_string( section, "scope_names" );
                  if ( S && S[ 0 ] ) {
                    string128 _scopeItem;
                    int count = _GetItemCount( S );
                    for ( int it = 0; it < count; ++it ) {
                      _GetItem( S, it, _scopeItem );
                      m_allScopeNames.push_back( _scopeItem );
                      //m_highlightAddons.push_back( _scopeItem );
                    }
                  }
                }
	}

	if(m_eSilencerStatus == ALife::eAddonAttachable)
	{
		m_sSilencerName = pSettings->r_string(section,"silencer_name");
		m_iSilencerX = pSettings->r_s32(section,"silencer_x");
		m_iSilencerY = pSettings->r_s32(section,"silencer_y");
	}

    
	if ( m_eGrenadeLauncherStatus == ALife::eAddonAttachable )
	{
		m_sGrenadeLauncherName = pSettings->r_string(section,"grenade_launcher_name");
		m_iGrenadeLauncherX = pSettings->r_s32(section,"grenade_launcher_x");
		m_iGrenadeLauncherY = pSettings->r_s32(section,"grenade_launcher_y");
	}


	// ����� ������� ������ ������
	if (pSettings->line_exist(section, "scope_bone"))
	{
		const char* S = pSettings->r_string(section, "scope_bone");
		if (S && strlen(S))
		{
			const int count = _GetItemCount(S);
			string128 _scope_bone{};
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _scope_bone);
				m_sWpn_scope_bones.push_back(_scope_bone);
			}
		}
		else
			m_sWpn_scope_bones.push_back(wpn_scope_def_bone);
	}
	else
		m_sWpn_scope_bones.push_back(wpn_scope_def_bone);
	m_sWpn_silencer_bone = READ_IF_EXISTS(pSettings, r_string, section, "silencer_bone", wpn_silencer_def_bone);
	m_sWpn_launcher_bone = READ_IF_EXISTS(pSettings, r_string, section, "launcher_bone", wpn_launcher_def_bone_shoc);

	if (pSettings->line_exist(section, "hidden_bones"))
	{
		const char* S = pSettings->r_string(section, "hidden_bones");
		if (S && strlen(S))
		{
			const int count = _GetItemCount(S);
			string128 _hidden_bone{};
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _hidden_bone);
				hidden_bones.push_back(_hidden_bone);
			}
		}
	}

	// ����� ������� ������ ������ - ���� �� ���������, ������������ ����� �� ������� ������� ������.
	if (pSettings->line_exist(hud_sect, "scope_bone"))
	{
		const char* S = pSettings->r_string(hud_sect, "scope_bone");
		if (S && strlen(S))
		{
			const int count = _GetItemCount(S);
			string128 _scope_bone{};
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _scope_bone);
				m_sHud_wpn_scope_bones.push_back(_scope_bone);
			}
		}
		else
			m_sHud_wpn_scope_bones = m_sWpn_scope_bones;
	}
	else
		m_sHud_wpn_scope_bones = m_sWpn_scope_bones;
	m_sHud_wpn_silencer_bone = READ_IF_EXISTS(pSettings, r_string, hud_sect, "silencer_bone", m_sWpn_silencer_bone);
	m_sHud_wpn_launcher_bone = READ_IF_EXISTS(pSettings, r_string, hud_sect, "launcher_bone", m_sWpn_launcher_bone);

	if (pSettings->line_exist(hud_sect, "hidden_bones"))
	{
		const char* S = pSettings->r_string(hud_sect, "hidden_bones");
		if (S && strlen(S))
		{
			const int count = _GetItemCount(S);
			string128 _hidden_bone{};
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _hidden_bone);
				hud_hidden_bones.push_back(_hidden_bone);
			}
		}
	}
	else
		hud_hidden_bones = hidden_bones;


	InitAddons();
	if(pSettings->line_exist(section,"weapon_remove_time"))
		m_dwWeaponRemoveTime = pSettings->r_u32(section,"weapon_remove_time");
	else
		m_dwWeaponRemoveTime = WEAPON_REMOVE_TIME;

	if(pSettings->line_exist(section,"auto_spawn_ammo"))
		m_bAutoSpawnAmmo = pSettings->r_bool(section,"auto_spawn_ammo");
	else
		m_bAutoSpawnAmmo = TRUE;



	m_zoom_params.m_bHideCrosshairInZoom		= true;

	if(pSettings->line_exist(hud_sect, "zoom_hide_crosshair"))
		m_zoom_params.m_bHideCrosshairInZoom = !!pSettings->r_bool(hud_sect, "zoom_hide_crosshair");	

	Fvector			def_dof;
	def_dof.set		(-1,-1,-1);
	m_zoom_params.m_ZoomDof		= READ_IF_EXISTS(pSettings, r_fvector3, section, "zoom_dof", Fvector().set(-1,-1,-1));
	m_zoom_params.m_bZoomDofEnabled	= !def_dof.similar(m_zoom_params.m_ZoomDof);

	m_zoom_params.m_ReloadDof	= READ_IF_EXISTS(pSettings, r_fvector4, section, "reload_dof", Fvector4().set(-1,-1,-1,-1));

    //Swartz: empty reload
    m_zoom_params.m_ReloadEmptyDof = READ_IF_EXISTS(pSettings, r_fvector4, section, "reload_empty_dof", Fvector4().set(-1, -1, -1, -1));
    //-Swartz

	m_bHasTracers			= READ_IF_EXISTS(pSettings, r_bool, section, "tracers", true);
	m_u8TracerColorID		= READ_IF_EXISTS(pSettings, r_u8, section, "tracers_color_ID", u8(-1));

	string256						temp;
	for (int i=egdNovice; i<egdCount; ++i) 
	{
		strconcat					(sizeof(temp),temp,"hit_probability_",get_token_name(difficulty_type_token,i));
		m_hit_probability[i]		= READ_IF_EXISTS(pSettings,r_float,section,temp,1.f);
	}


	// mmccxvii: FWR code
	//*
	//sndExplosion.create(READ_IF_EXISTS(pSettings, r_string, section, "snd_explosion", "weapon\\weapon_explosion"), st_Effect, sg_SourceType);
	//ppeExplosion = READ_IF_EXISTS(pSettings, r_string, section, "ppe_explosion", "ppe\\weapon_explosion.ppe");
	//*
	
	
	m_nearwall_on = READ_IF_EXISTS(pSettings, r_bool, section, "nearwall_on", READ_IF_EXISTS(pSettings, r_bool, "features", "default_nearwall_on", true));
	if (m_nearwall_on)
	{
		// ��������� ��������� HUD FOV ����� ����� ����� �������� � �����
		m_nearwall_target_hud_fov = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_target_hud_fov", 0.27f);
		m_nearwall_dist_min = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_dist_min", 0.5f);
		m_nearwall_dist_max = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_dist_max", 1.f);
		m_nearwall_speed_mod = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_speed_mod", 10.f);
	}

	////////////////////////////////////////////
	//--#SM+# Begin--
	m_strafe_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_hud_offset_pos", (Fvector{ 0.015f, 0.f, 0.f }));
	m_strafe_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_hud_offset_rot", (Fvector{ 0.f, 0.f, 4.5f }));

	m_strafe_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_aim_hud_offset_pos", (Fvector{ 0.f, 0.f, 0.f }));
	m_strafe_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_aim_hud_offset_rot", (Fvector{ 0.f, 0.f, 2.5f }));

	m_strafe_offset[2][0].set(READ_IF_EXISTS(pSettings, r_bool, section, "strafe_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "strafe_transition_time", 0.25f), 0.f); // normal
	m_strafe_offset[2][1].set(READ_IF_EXISTS(pSettings, r_bool, section, "strafe_aim_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "strafe_aim_transition_time", 0.15f), 0.f); // aim-GL
	//--#SM+# End--
	////////////////////////////////////////////

	////////////////////////////////////////////
	m_lookout_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_hud_offset_pos", (Fvector{ 0.045f, 0.f, 0.f }));
	m_lookout_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_hud_offset_rot", (Fvector{ 0.f, 0.f, 10.f }));

	m_lookout_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_aim_hud_offset_pos", (Fvector{ 0.f, 0.f, 0.f }));
	m_lookout_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_aim_hud_offset_rot", (Fvector{ 0.f, 0.f, 15.f }));

	m_lookout_offset[2][0].set(READ_IF_EXISTS(pSettings, r_bool, section, "lookout_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "lookout_transition_time", 0.25f), 0.f); // normal
	m_lookout_offset[2][1].set(READ_IF_EXISTS(pSettings, r_bool, section, "lookout_aim_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "lookout_aim_transition_time", 0.15f), 0.f); // aim-GL
	////////////////////////////////////////////

}

void CWeapon::LoadFireParams		(LPCSTR section)
{
	cam_recoil.Dispersion = deg2rad( pSettings->r_float( section,"cam_dispersion" ) ); 
	cam_recoil.DispersionInc = 0.0f;

	if ( pSettings->line_exist( section, "cam_dispersion_inc" ) )	{
		cam_recoil.DispersionInc = deg2rad( pSettings->r_float( section, "cam_dispersion_inc" ) ); 
	}
	
	zoom_cam_recoil.Dispersion		= cam_recoil.Dispersion;
	zoom_cam_recoil.DispersionInc	= cam_recoil.DispersionInc;

	if ( pSettings->line_exist( section, "zoom_cam_dispersion" ) )	{
		zoom_cam_recoil.Dispersion		= deg2rad( pSettings->r_float( section, "zoom_cam_dispersion" ) ); 
	}
	if ( pSettings->line_exist( section, "zoom_cam_dispersion_inc" ) )	{
		zoom_cam_recoil.DispersionInc	= deg2rad( pSettings->r_float( section, "zoom_cam_dispersion_inc" ) ); 
	}

	CShootingObject::LoadFireParams(section);
};



BOOL CWeapon::net_Spawn		(CSE_Abstract* DC)
{
	BOOL bResult					= inherited::net_Spawn(DC);
	CSE_Abstract					*e	= (CSE_Abstract*)(DC);
	CSE_ALifeItemWeapon			    *E	= smart_cast<CSE_ALifeItemWeapon*>(e);

	//iAmmoCurrent					= E->a_current;
	iAmmoElapsed					= E->a_elapsed;
	m_flagsAddOnState				= E->m_addon_flags.get();
	m_ammoType						= E->ammo_type;
	SetState						(E->wpn_state);
	SetNextState					(E->wpn_state);
	
	m_DefaultCartridge.Load(*m_ammoTypes[m_ammoType], u8(m_ammoType));	
	if(iAmmoElapsed) 
	{
		m_fCurrentCartirdgeDisp = m_DefaultCartridge.param_s.kDisp;
		for(int i = 0; i < iAmmoElapsed; ++i) 
			m_magazine.push_back(m_DefaultCartridge);
	}

	UpdateAddonsVisibility();
	InitAddons();

	m_dwWeaponIndependencyTime = 0;

	VERIFY((u32)iAmmoElapsed == m_magazine.size());
	m_bAmmoWasSpawned		= false;

	return bResult;
}

void CWeapon::net_Destroy	()
{
	inherited::net_Destroy	();

	//������� ������� ���������
	StopFlameParticles	();
	StopFlameParticles2	();
	StopLight			();
	Light_Destroy		();

	while (m_magazine.size()) m_magazine.pop_back();
}

BOOL CWeapon::IsUpdating()
{	
	bool bIsActiveItem = m_pInventory && m_pInventory->ActiveItem()==this;
	return bIsActiveItem || bWorking;// || IsPending() || getVisible();
}

void CWeapon::net_Export(NET_Packet& P)
{
	inherited::net_Export	(P);

	P.w_float_q8			(GetCondition(),0.0f,1.0f);


	u8 need_upd				= IsUpdating() ? 1 : 0;
	P.w_u8					(need_upd);
	P.w_u16					(u16(iAmmoElapsed));
	P.w_u8					(m_flagsAddOnState);
	P.w_u8					((u8)m_ammoType);
	P.w_u8					((u8)GetState());
	P.w_u8					((u8)IsZoomed());
}

void CWeapon::net_Import(NET_Packet& P)
{
	inherited::net_Import (P);
	
	float _cond;
	P.r_float_q8			(_cond,0.0f,1.0f);
	SetCondition			(_cond);

	u8 flags				= 0;
	P.r_u8					(flags);

	u16 ammo_elapsed = 0;
	P.r_u16					(ammo_elapsed);

	u8						NewAddonState;
	P.r_u8					(NewAddonState);

	m_flagsAddOnState		= NewAddonState;
	UpdateAddonsVisibility	();

	u8 ammoType, wstate;
	P.r_u8					(ammoType);
	P.r_u8					(wstate);

	u8 Zoom;
	P.r_u8					((u8)Zoom);

	if (H_Parent() && H_Parent()->Remote())
	{
		if (Zoom) OnZoomIn();
		else OnZoomOut();
	};
	switch (wstate)
	{	
	case eFire:
	case eFire2:
	case eSwitch:
	case eReload:
		{
		}break;	
	default:
		{
			if (ammoType >= m_ammoTypes.size())
				Msg("!! Weapon [%d], State - [%d]", ID(), wstate);
			else
			{
				m_ammoType = ammoType;
				SetAmmoElapsed((ammo_elapsed));
			}
		}break;
	}
	
	VERIFY((u32)iAmmoElapsed == m_magazine.size());
}

void CWeapon::save(NET_Packet &output_packet)
{
	inherited::save	(output_packet);
	save_data		(iAmmoElapsed,					output_packet);
	save_data		(m_flagsAddOnState, 			output_packet);
	save_data		(m_ammoType,					output_packet);
	save_data		(m_zoom_params.m_bIsZoomModeNow,output_packet);
}

void CWeapon::load(IReader &input_packet)
{
	inherited::load	(input_packet);
	load_data		(iAmmoElapsed,					input_packet);
	load_data		(m_flagsAddOnState,				input_packet);
	UpdateAddonsVisibility			();
	load_data		(m_ammoType,					input_packet);
	load_data		(m_zoom_params.m_bIsZoomModeNow,input_packet);

	if (m_zoom_params.m_bIsZoomModeNow)	
			OnZoomIn();
		else			
			OnZoomOut();
}


void CWeapon::OnEvent(NET_Packet& P, u16 type) 
{
	switch (type)
	{
	case GE_ADDON_CHANGE:
		{
			P.r_u8					(m_flagsAddOnState);
			InitAddons();
			UpdateAddonsVisibility();
		}break;

	case GE_WPN_STATE_CHANGE:
		{
			u8				state;
			P.r_u8			(state);
			P.r_u8			(m_sub_state);		
//			u8 NewAmmoType = 
				P.r_u8();
			u8 AmmoElapsed = P.r_u8();
			u8 NextAmmo = P.r_u8();
			if (NextAmmo == u8(-1))
				m_set_next_ammoType_on_reload = u32(-1);
			else
				m_set_next_ammoType_on_reload = u8(NextAmmo);

			if (OnClient()) SetAmmoElapsed(int(AmmoElapsed));			
			OnStateSwitch	(u32(state));
		}
		break;
	default:
		{
			inherited::OnEvent(P,type);
		}break;
	}
};

void CWeapon::shedule_Update	(u32 dT)
{
	// Queue shrink
//	u32	dwTimeCL		= Level().timeServer()-NET_Latency;
//	while ((NET.size()>2) && (NET[1].dwTimeStamp<dwTimeCL)) NET.pop_front();	

	// Inherited
	inherited::shedule_Update	(dT);
}

void CWeapon::OnH_B_Independent	(bool just_before_destroy)
{
	RemoveShotEffector			();

	inherited::OnH_B_Independent(just_before_destroy);

	FireEnd						();
	SetPending					(FALSE);
	SwitchState					(eHidden);

	m_strapped_mode				= false;
	m_zoom_params.m_bIsZoomModeNow	= false;
	UpdateXForm					();

	m_nearwall_last_hud_fov 	= psHUD_FOV_def;
}

void CWeapon::OnH_A_Independent	()
{
	m_dwWeaponIndependencyTime = Level().timeServer();
	inherited::OnH_A_Independent();
	Light_Destroy				();
	//UpdateAddonsVisibility		();
};

void CWeapon::OnH_A_Chield		()
{
	inherited::OnH_A_Chield		();
	UpdateAddonsVisibility		();
};

void CWeapon::OnActiveItem ()
{
	//. from Activate
	//UpdateAddonsVisibility();
	m_dwAmmoCurrentCalcFrame = 0;

//. Show
	SwitchState					(eShowing);
//-

	inherited::OnActiveItem		();
	//���� �� ����������� � ������ ���� � �����
//.	SetState					(eIdle);
//.	SetNextState				(eIdle);
}

void CWeapon::OnHiddenItem ()
{
	m_dwAmmoCurrentCalcFrame = 0;

	SwitchState(eHiding);

	OnZoomOut();
	inherited::OnHiddenItem		();

	m_set_next_ammoType_on_reload = u32(-1);
}

void CWeapon::SendHiddenItem()
{
	if (!CHudItem::object().getDestroy() && m_pInventory)
	{
		// !!! Just single entry for given state !!!
		NET_Packet		P;
		CHudItem::object().u_EventGen		(P,GE_WPN_STATE_CHANGE,CHudItem::object().ID());
		P.w_u8			(u8(eHiding));
		P.w_u8			(u8(m_sub_state));
		P.w_u8			(u8(m_ammoType& 0xff));
		P.w_u8			(u8(iAmmoElapsed & 0xff));
		P.w_u8			(u8(m_set_next_ammoType_on_reload & 0xff));
		CHudItem::object().u_EventSend		(P, net_flags(TRUE, TRUE, FALSE, TRUE));
		SetPending		(TRUE);
	}
}


void CWeapon::OnH_B_Chield		()
{
	m_dwWeaponIndependencyTime = 0;
	inherited::OnH_B_Chield		();

	OnZoomOut					();
	m_set_next_ammoType_on_reload	= u32(-1);
	
	m_nearwall_last_hud_fov 	= psHUD_FOV_def;
}

extern u32 hud_adj_mode;

void CWeapon::UpdateCL		()
{
	inherited::UpdateCL		();
	UpdateHUDAddonsVisibility();
	//��������� �� ��������
	UpdateLight				();

	//���������� ��������
	UpdateFlameParticles	();
	UpdateFlameParticles2	();

    if ((GetNextState() == GetState()) && H_Parent() == Level().CurrentEntity())
	{
		CActor* pActor	= smart_cast<CActor*>(H_Parent());
		if(pActor && !pActor->AnyMove() && this==pActor->inventory().ActiveItem())
		{
			if (hud_adj_mode==0 && 
				GetState()==eIdle && 
				(Device.dwTimeGlobal-m_dw_curr_substate_time>20000) && 
				!IsZoomed()&&
				g_player_hud->attached_item(1)==NULL)
			{
				SwitchState			(eBore);
				ResetSubStateTime	();
			}
		}
	}
}

bool  CWeapon::need_renderable()
{
	return !( IsZoomed() && ZoomTexture() && !IsRotatingToZoom() );
}

void CWeapon::renderable_Render		()
{
	UpdateXForm				();

	//���������� ���������

	RenderLight				();	

	//���� �� � ������ ���������, �� ��� HUD �������� �� ����
	if(IsZoomed() && !IsRotatingToZoom() && ZoomTexture())
		RenderHud		(FALSE);
	else
		RenderHud		(TRUE);

	inherited::renderable_Render	();
}

void CWeapon::signal_HideComplete()
{
	if(H_Parent()) 
		setVisible			(FALSE);
	SetPending				(FALSE);
}

void CWeapon::SetDefaults()
{
	SetPending			(FALSE);

	m_flags.set			(FUsingCondition, TRUE);
	bMisfire			= false;
	m_flagsAddOnState	= 0;
	m_zoom_params.m_bIsZoomModeNow	= false;
}

void CWeapon::UpdatePosition(const Fmatrix& trans)
{
	Position().set		(trans.c);
	XFORM().mul			(trans,m_strapped_mode ? m_StrapOffset : m_Offset);
	VERIFY				(!fis_zero(DET(renderable.xform)));
}


bool CWeapon::Action(s32 cmd, u32 flags) 
{
	if(inherited::Action(cmd, flags)) return true;

	
	switch(cmd) 
	{
		case kWPN_FIRE: 
			{
				//���� ������ ���-�� ������, �� ������ �� ������
				{				
					if(flags&CMD_START) 
					{
						if(IsPending())		return false;
						FireStart			();
					}else 
						FireEnd();
				};
			} 
			return true;
		case kWPN_NEXT: 
			{
				return SwitchAmmoType(flags);
			} 

		case kWPN_ZOOM:
			if(IsZoomEnabled())
			{
				if(b_toggle_weapon_aim)
				{
					if(flags&CMD_START)
					{
						if(!IsZoomed())
						{
							if(!IsPending())
							{
								if(GetState()!=eIdle)
									SwitchState(eIdle);
								OnZoomIn	();
							}
						}else
							OnZoomOut	();
					}
				}else
				{
					if(flags&CMD_START)
					{
						if(!IsZoomed() && !IsPending())
						{
							if(GetState()!=eIdle)
								SwitchState(eIdle);
							OnZoomIn	();
						}
					}else 
						if(IsZoomed())
							OnZoomOut	();
				}
				return true;
			}else 
				return false;

		case kWPN_ZOOM_INC:
		case kWPN_ZOOM_DEC:
    		if (IsZoomEnabled() && IsZoomed() && (flags&CMD_START) )
			{
				if(cmd==kWPN_ZOOM_INC)  ZoomInc();
				else					ZoomDec();
				return true;
			}else
				return false;
	}
	return false;
}

bool CWeapon::SwitchAmmoType( u32 flags ) 
{
	if ( IsPending() || OnClient() )
	{
		return false;
	}
	if ( !(flags & CMD_START) )
	{
		return false;
	}

	u32 l_newType = m_ammoType;
	bool b1, b2;
	do 
	{
		l_newType = (l_newType+1) % m_ammoTypes.size();
		b1 = (l_newType != m_ammoType);
		b2 = unlimited_ammo() ? false : ( !m_pInventory->GetAny( *m_ammoTypes[l_newType] ) );						
	} while( b1 && b2 );

	if ( l_newType != m_ammoType )
	{
		m_set_next_ammoType_on_reload = l_newType;					
		if ( OnServer() )
		{
			Reload();
		}
	}
	return true;
}

void CWeapon::SpawnAmmo(u32 boxCurr, LPCSTR ammoSect, u32 ParentID) 
{
	if(!m_ammoTypes.size())			return;
	if (OnClient())					return;
	m_bAmmoWasSpawned				= true;
	
	int l_type						= 0;
	l_type							%= m_ammoTypes.size();

	if(!ammoSect) ammoSect			= *m_ammoTypes[l_type]; 
	
	++l_type; 
	l_type							%= m_ammoTypes.size();

	CSE_Abstract *D					= F_entity_Create(ammoSect);

	if (D->m_tClassID==CLSID_OBJECT_AMMO	||
		D->m_tClassID==CLSID_OBJECT_A_M209	||
		D->m_tClassID==CLSID_OBJECT_A_VOG25	||
		D->m_tClassID==CLSID_OBJECT_A_OG7B)
	{	
		CSE_ALifeItemAmmo *l_pA		= smart_cast<CSE_ALifeItemAmmo*>(D);
		R_ASSERT					(l_pA);
		l_pA->m_boxSize				= (u16)pSettings->r_s32(ammoSect, "box_size");
		D->s_name					= ammoSect;
		D->set_name_replace			("");
//.		D->s_gameid					= u8(GameID());
		D->s_RP						= 0xff;
		D->ID						= 0xffff;
		if (ParentID == 0xffffffff)	
			D->ID_Parent			= (u16)H_Parent()->ID();
		else
			D->ID_Parent			= (u16)ParentID;

		D->ID_Phantom				= 0xffff;
		D->s_flags.assign			(M_SPAWN_OBJECT_LOCAL);
		D->RespawnTime				= 0;
		l_pA->m_tNodeID				= ai_location().level_vertex_id();

		if(boxCurr == 0xffffffff) 	
			boxCurr					= l_pA->m_boxSize;

		while(boxCurr) 
		{
			l_pA->a_elapsed			= (u16)(boxCurr > l_pA->m_boxSize ? l_pA->m_boxSize : boxCurr);
			NET_Packet				P;
			D->Spawn_Write			(P, TRUE);
			Level().Send			(P,net_flags(TRUE));

			if(boxCurr > l_pA->m_boxSize) 
				boxCurr				-= l_pA->m_boxSize;
			else 
				boxCurr				= 0;
		}
	};
	F_entity_Destroy				(D);
}

int CWeapon::GetSuitableAmmoTotal(bool use_item_to_spawn) const
{
	int l_count = iAmmoElapsed;
	if(!m_pInventory) return l_count;

	//���� �� ������ ������ ����������
	if(m_pInventory->ModifyFrame()<=m_dwAmmoCurrentCalcFrame)
		return l_count + iAmmoCurrent;

 	m_dwAmmoCurrentCalcFrame = Device.dwFrame;
	iAmmoCurrent = 0;

	for(int i = 0; i < (int)m_ammoTypes.size(); ++i) 
	{
		LPCSTR l_ammoType = *m_ammoTypes[i];

		for(TIItemContainer::iterator l_it = m_pInventory->m_belt.begin(); m_pInventory->m_belt.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = smart_cast<CWeaponAmmo*>(*l_it);

			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		for(TIItemContainer::iterator l_it = m_pInventory->m_ruck.begin(); m_pInventory->m_ruck.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = smart_cast<CWeaponAmmo*>(*l_it);
			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		if (!use_item_to_spawn)
			continue;

		if (!inventory_owner().item_to_spawn())
			continue;

		iAmmoCurrent += inventory_owner().ammo_in_box_to_spawn();
	}
	return l_count + iAmmoCurrent;
}

int CWeapon::GetCurrentTypeAmmoTotal() const
{
	int l_count = iAmmoElapsed;
	if ( !m_pInventory )
	{
		return l_count;
	}

	//���� �� ������ ������ ����������
	if ( m_pInventory->ModifyFrame() <= m_dwAmmoCurrentCalcFrame )
	{
		return l_count + iAmmoCurrent;
	}

	m_dwAmmoCurrentCalcFrame = Device.dwFrame;
	iAmmoCurrent = 0;

	VERIFY( 0 <= m_ammoType && m_ammoType < m_ammoTypes.size() );
	{
		LPCSTR l_ammoType = m_ammoTypes[m_ammoType].c_str();

		for(TIItemContainer::iterator l_it = m_pInventory->m_belt.begin(); m_pInventory->m_belt.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = smart_cast<CWeaponAmmo*>(*l_it);

			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		for(TIItemContainer::iterator l_it = m_pInventory->m_ruck.begin(); m_pInventory->m_ruck.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = smart_cast<CWeaponAmmo*>(*l_it);
			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}
	}
	return l_count + iAmmoCurrent;
}

float CWeapon::GetConditionMisfireProbability() const
{
	if( GetCondition()>0.95f ) return 0.0f;

	float mis = misfireProbability+powf(1.f-GetCondition(), 3.f)*misfireConditionK;
	clamp(mis,0.0f,0.99f);
	return mis;
}

BOOL CWeapon::CheckForMisfire	()
{
	if (OnClient()) return FALSE;

	float rnd = ::Random.randF(0.f,1.f);
	float mp = GetConditionMisfireProbability();
	if(rnd < mp)
	{
		FireEnd();

		bMisfire = true;
		SwitchState(eMisfire);		
		
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CWeapon::IsMisfire() const
{	
	return bMisfire;
}
void CWeapon::Reload()
{
	OnZoomOut();
}


bool CWeapon::IsGrenadeLauncherAttached() const
{
	return (ALife::eAddonAttachable == m_eGrenadeLauncherStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher)) || 
			ALife::eAddonPermanent == m_eGrenadeLauncherStatus;
}

bool CWeapon::IsScopeAttached() const
{
	return (ALife::eAddonAttachable == m_eScopeStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope)) || 
			ALife::eAddonPermanent == m_eScopeStatus;

}

bool CWeapon::IsSilencerAttached() const
{
	return (ALife::eAddonAttachable == m_eSilencerStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer)) || 
			ALife::eAddonPermanent == m_eSilencerStatus;
}

bool CWeapon::GrenadeLauncherAttachable() const
{
	return (ALife::eAddonAttachable == m_eGrenadeLauncherStatus);
}
bool CWeapon::ScopeAttachable() const
{
	return (ALife::eAddonAttachable == m_eScopeStatus);
}
bool CWeapon::SilencerAttachable() const
{
	return (ALife::eAddonAttachable == m_eSilencerStatus);
}

void CWeapon::UpdateHUDAddonsVisibility()
{
	if (!GetHUDmode())
		return;

	if (ScopeAttachable())
		HudItemData()->set_bone_visible(m_sHud_wpn_scope_bones, IsScopeAttached());

	if (m_eScopeStatus == ALife::eAddonDisabled)
		HudItemData()->set_bone_visible(m_sHud_wpn_scope_bones, FALSE, TRUE);
	else if (m_eScopeStatus == ALife::eAddonPermanent)
		HudItemData()->set_bone_visible(m_sHud_wpn_scope_bones, TRUE, TRUE);

	if (SilencerAttachable())
		HudItemData()->set_bone_visible(m_sHud_wpn_silencer_bone, IsSilencerAttached());

	if (m_eSilencerStatus == ALife::eAddonDisabled)
		HudItemData()->set_bone_visible(m_sHud_wpn_silencer_bone, FALSE, TRUE);
	else if (m_eSilencerStatus == ALife::eAddonPermanent)
		HudItemData()->set_bone_visible(m_sHud_wpn_silencer_bone, TRUE, TRUE);

	if (!HudItemData()->has_bone(m_sHud_wpn_launcher_bone) && HudItemData()->has_bone(wpn_launcher_def_bone_cop))
		m_sHud_wpn_launcher_bone = wpn_launcher_def_bone_cop;

	if (GrenadeLauncherAttachable())
		HudItemData()->set_bone_visible(m_sHud_wpn_launcher_bone, IsGrenadeLauncherAttached());
	
	if (m_eGrenadeLauncherStatus == ALife::eAddonDisabled)
		HudItemData()->set_bone_visible(m_sHud_wpn_launcher_bone, FALSE, TRUE);
	else if (m_eGrenadeLauncherStatus == ALife::eAddonPermanent)
		HudItemData()->set_bone_visible(m_sHud_wpn_launcher_bone, TRUE, TRUE);

	for (const shared_str& bone_name : hud_hidden_bones)
		HudItemData()->set_bone_visible(bone_name, FALSE, TRUE);
}

void CWeapon::UpdateAddonsVisibility()
{
	auto pWeaponVisual = smart_cast<IKinematics*>(Visual());
	VERIFY(pWeaponVisual);

	UpdateHUDAddonsVisibility();

	///////////////////////////////////////////////////////////////////
	u16 bone_id{};

	for (const auto& sbone : m_sWpn_scope_bones)
	{
		bone_id = pWeaponVisual->LL_BoneID(sbone);

		if (ScopeAttachable())
		{
			if (IsScopeAttached())
			{
				if (!pWeaponVisual->LL_GetBoneVisible(bone_id))
					pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
			}
			else
			{
				if (pWeaponVisual->LL_GetBoneVisible(bone_id))
					pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
			}
		}

		if (m_eScopeStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		else if (m_eScopeStatus == ALife::eAddonPermanent && bone_id != BI_NONE && !pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
	}
	///////////////////////////////////////////////////////////////////

	bone_id = pWeaponVisual->LL_BoneID(m_sWpn_silencer_bone);

	if (SilencerAttachable())
	{
		if (IsSilencerAttached())
		{
			if (!pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
		else
		{
			if (pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		}
	}

	if (m_eSilencerStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
		pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	else if (m_eSilencerStatus == ALife::eAddonPermanent && bone_id != BI_NONE && !pWeaponVisual->LL_GetBoneVisible(bone_id))
		pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);

	///////////////////////////////////////////////////////////////////

	bone_id = pWeaponVisual->LL_BoneID(m_sWpn_launcher_bone);

	if (GrenadeLauncherAttachable())
	{
		if (IsGrenadeLauncherAttached())
		{
			if (!pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
		else
		{
			if (pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		}
	}

	if (m_eGrenadeLauncherStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
		pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	else if (m_eGrenadeLauncherStatus == ALife::eAddonPermanent && bone_id != BI_NONE && !pWeaponVisual->LL_GetBoneVisible(bone_id))
		pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);

	///////////////////////////////////////////////////////////////////

	for (const auto& bone_name : hidden_bones)
	{
		bone_id = pWeaponVisual->LL_BoneID(bone_name);
		if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	}

	///////////////////////////////////////////////////////////////////

	pWeaponVisual->CalculateBones_Invalidate				();
	pWeaponVisual->CalculateBones							();
}


void CWeapon::InitAddons()
{
}

float CWeapon::CurrentZoomFactor()
{
	return IsScopeAttached() ? m_zoom_params.m_fScopeZoomFactor : m_zoom_params.m_fIronSightZoomFactor;
};

float CWeapon::GetHudFov()
{
	// ������������ HUD FOV �� ����� (� ������ �������� � �����)
	if (m_nearwall_on && ParentIsActor() && Level().CurrentViewEntity() == H_Parent())
	{
		// �������� ���������� �� ������ �� ����� � �������
		collide::rq_result& RQ = HUD().GetCurrentRayQuery();
		float dist = RQ.range;

		// ������������� ���������� � �������� �� 0 (min) �� 1 (max)
		clamp(dist, m_nearwall_dist_min, m_nearwall_dist_max);
		float fDistanceMod = ((dist - m_nearwall_dist_min) / (m_nearwall_dist_max - m_nearwall_dist_min)); // 0.f ... 1.f

		 // ������������ ������� HUD FOV �� �����
        float fBaseFov = psHUD_FOV_def + m_hud_fov_add_mod;
		clamp(fBaseFov, 0.0f, FLT_MAX);

		// ������ ����������� �������� FOV �� �����
		float src = m_nearwall_speed_mod * Device.fTimeDelta;
		clamp(src, 0.f, 1.f);

		float fTrgFov = m_nearwall_target_hud_fov + fDistanceMod * (fBaseFov - m_nearwall_target_hud_fov);
		m_nearwall_last_hud_fov = m_nearwall_last_hud_fov * (1 - src) + fTrgFov * src;
	}

    if (m_zoom_params.m_fZoomRotationFactor > 0.0f)
    {
		return m_nearwall_last_hud_fov;
    }
    else
    {
        return m_nearwall_last_hud_fov;
    }
}

void CWeapon::OnZoomIn()
{
	m_zoom_params.m_bIsZoomModeNow		= true;
	m_zoom_params.m_fCurrentZoomFactor	= CurrentZoomFactor();
	//EnableHudInertion					(FALSE);

	
	if(m_zoom_params.m_bZoomDofEnabled && !IsScopeAttached())
		GamePersistent().SetEffectorDOF	(m_zoom_params.m_ZoomDof);
}

void CWeapon::OnZoomOut()
{
	m_zoom_params.m_bIsZoomModeNow		= false;
	m_zoom_params.m_fCurrentZoomFactor	= g_fov;
	//EnableHudInertion					(TRUE);

 	GamePersistent().RestoreEffectorDOF	();
	ResetSubStateTime					();
}

bool CWeapon::UseScopeTexture() {
	return m_UIScope; // ������ ���� ���� �������� ������� - ��� �������� �������� ������������
}

CUIWindow* CWeapon::ZoomTexture()
{
	if (UseScopeTexture())
		return m_UIScope;
	else
		return NULL;
}

void CWeapon::SwitchState(u32 S)
{
	if (OnClient()) return;

#ifndef MASTER_GOLD
	if ( bDebug )
	{
		Msg("---Server is going to send GE_WPN_STATE_CHANGE to [%d], weapon_section[%s], parent[%s]",
			S, cNameSect().c_str(), H_Parent() ? H_Parent()->cName().c_str() : "NULL Parent");
	}
#endif // #ifndef MASTER_GOLD

	SetNextState		( S );
	if (CHudItem::object().Local() && !CHudItem::object().getDestroy() && m_pInventory && OnServer())	
	{
		// !!! Just single entry for given state !!!
		NET_Packet		P;
		CHudItem::object().u_EventGen		(P,GE_WPN_STATE_CHANGE,CHudItem::object().ID());
		P.w_u8			(u8(S));
		P.w_u8			(u8(m_sub_state));
		P.w_u8			(u8(m_ammoType& 0xff));
		P.w_u8			(u8(iAmmoElapsed & 0xff));
		P.w_u8			(u8(m_set_next_ammoType_on_reload & 0xff));
		CHudItem::object().u_EventSend		(P, net_flags(TRUE, TRUE, FALSE, TRUE));
	}
}

void CWeapon::OnMagazineEmpty	()
{
	VERIFY((u32)iAmmoElapsed == m_magazine.size());
}


void CWeapon::reinit			()
{
	CShootingObject::reinit		();
	CHudItemObject::reinit			();
}

void CWeapon::reload			(LPCSTR section)
{
	CShootingObject::reload		(section);
	CHudItemObject::reload			(section);
	
	m_can_be_strapped			= true;
	m_strapped_mode				= false;
	
	if (pSettings->line_exist(section,"strap_bone0"))
		m_strap_bone0			= pSettings->r_string(section,"strap_bone0");
	else
		m_can_be_strapped		= false;
	
	if (pSettings->line_exist(section,"strap_bone1"))
		m_strap_bone1			= pSettings->r_string(section,"strap_bone1");
	else
		m_can_be_strapped		= false;

	if (m_eScopeStatus == ALife::eAddonAttachable) {
		m_addon_holder_range_modifier	= READ_IF_EXISTS(pSettings,r_float,m_sScopeName,"holder_range_modifier",m_holder_range_modifier);
		m_addon_holder_fov_modifier		= READ_IF_EXISTS(pSettings,r_float,m_sScopeName,"holder_fov_modifier",m_holder_fov_modifier);
	}
	else {
		m_addon_holder_range_modifier	= m_holder_range_modifier;
		m_addon_holder_fov_modifier		= m_holder_fov_modifier;
	}


	{
		Fvector				pos,ypr;
		pos					= pSettings->r_fvector3		(section,"position");
		ypr					= pSettings->r_fvector3		(section,"orientation");
		ypr.mul				(PI/180.f);

		m_Offset.setHPB			(ypr.x,ypr.y,ypr.z);
		m_Offset.translate_over	(pos);
	}

	m_StrapOffset			= m_Offset;
	if (pSettings->line_exist(section,"strap_position") && pSettings->line_exist(section,"strap_orientation")) {
		Fvector				pos,ypr;
		pos					= pSettings->r_fvector3		(section,"strap_position");
		ypr					= pSettings->r_fvector3		(section,"strap_orientation");
		ypr.mul				(PI/180.f);

		m_StrapOffset.setHPB			(ypr.x,ypr.y,ypr.z);
		m_StrapOffset.translate_over	(pos);
	}
	else
		m_can_be_strapped	= false;

	m_ef_main_weapon_type	= READ_IF_EXISTS(pSettings,r_u32,section,"ef_main_weapon_type",u32(-1));
	m_ef_weapon_type		= READ_IF_EXISTS(pSettings,r_u32,section,"ef_weapon_type",u32(-1));
}

void CWeapon::create_physic_shell()
{
	CPhysicsShellHolder::create_physic_shell();
}

bool CWeapon::ActivationSpeedOverriden (Fvector& dest, bool clear_override)
{
	if ( m_activation_speed_is_overriden )
	{
		if ( clear_override )
		{
			m_activation_speed_is_overriden	=	false;
		}

		dest						=	m_overriden_activation_speed;
		return							true;
	}
	
	return								false;
}

void CWeapon::SetActivationSpeedOverride	(Fvector const& speed)
{
	m_overriden_activation_speed	=	speed;
	m_activation_speed_is_overriden	=	true;
}

void CWeapon::activate_physic_shell()
{
	UpdateXForm();
	CPhysicsShellHolder::activate_physic_shell();
}

void CWeapon::setup_physic_shell()
{
	CPhysicsShellHolder::setup_physic_shell();
}

int		g_iWeaponRemove = 1;

bool CWeapon::NeedToDestroyObject()	const
{
    return false;
}

ALife::_TIME_ID	 CWeapon::TimePassedAfterIndependant()	const
{
	if(!H_Parent() && m_dwWeaponIndependencyTime != 0)
		return Level().timeServer() - m_dwWeaponIndependencyTime;
	else
		return 0;
}

bool CWeapon::can_kill	() const
{
	if (GetSuitableAmmoTotal(true) || m_ammoTypes.empty())
		return				(true);

	return					(false);
}

CInventoryItem *CWeapon::can_kill	(CInventory *inventory) const
{
	if (GetAmmoElapsed() || m_ammoTypes.empty())
		return				(const_cast<CWeapon*>(this));

	TIItemContainer::iterator I = inventory->m_all.begin();
	TIItemContainer::iterator E = inventory->m_all.end();
	for ( ; I != E; ++I) {
		CInventoryItem	*inventory_item = smart_cast<CInventoryItem*>(*I);
		if (!inventory_item)
			continue;
		
		xr_vector<shared_str>::const_iterator	i = std::find(m_ammoTypes.begin(),m_ammoTypes.end(),inventory_item->object().cNameSect());
		if (i != m_ammoTypes.end())
			return			(inventory_item);
	}

	return					(0);
}

const CInventoryItem *CWeapon::can_kill	(const xr_vector<const CGameObject*> &items) const
{
	if (m_ammoTypes.empty())
		return				(this);

	xr_vector<const CGameObject*>::const_iterator I = items.begin();
	xr_vector<const CGameObject*>::const_iterator E = items.end();
	for ( ; I != E; ++I) {
		const CInventoryItem	*inventory_item = smart_cast<const CInventoryItem*>(*I);
		if (!inventory_item)
			continue;

		xr_vector<shared_str>::const_iterator	i = std::find(m_ammoTypes.begin(),m_ammoTypes.end(),inventory_item->object().cNameSect());
		if (i != m_ammoTypes.end())
			return			(inventory_item);
	}

	return					(0);
}

bool CWeapon::ready_to_kill	() const
{
	//Alundaio
	const CInventoryOwner* io = smart_cast<const CInventoryOwner*>(H_Parent());
	if (!io)
		return false;

	if (io->inventory().ActiveItem() == NULL || io->inventory().ActiveItem()->object().ID() != ID())
		return false; 
	//-Alundaio
	return					(
		!IsMisfire() && 
		((GetState() == eIdle) || (GetState() == eFire) || (GetState() == eFire2)) && 
		GetAmmoElapsed()
	);
}

// �������� ������ ������� ��������� ����
u8 CWeapon::GetCurrentHudOffsetIdx()
{
	const bool b_aiming = ((IsZoomed() && m_zoom_params.m_fZoomRotationFactor <= 1.f) || (!IsZoomed() && m_zoom_params.m_fZoomRotationFactor > 0.f));

	if (b_aiming)
	{
		const bool has_gl = GrenadeLauncherAttachable() && IsGrenadeLauncherAttached();
		const bool has_scope = ScopeAttachable() && IsScopeAttached();

		if (IsGrenadeMode())
		{
			return hud_item_measures::m_hands_offset_type_gl;
		}
		else if (has_gl)
		{
			return hud_item_measures::m_hands_offset_type_aim_gl_normal;
		}
		else
		{
			return hud_item_measures::m_hands_offset_type_aim;
		}
	}

	return hud_item_measures::m_hands_offset_type_normal;
}


// ���������� ��������� �������� ����
void CWeapon::UpdateHudAdditonal		(Fmatrix& trans)
{
	Fvector summary_offset{}, summary_rotate{};

	attachable_hud_item* hi = HudItemData();
	u8 idx = GetCurrentHudOffsetIdx();
	const bool b_aiming = idx != hud_item_measures::m_hands_offset_type_normal;
	Fvector zr_offs = hi->m_measures.m_hands_offset[hud_item_measures::m_hands_offset_pos][idx];
	Fvector zr_rot = hi->m_measures.m_hands_offset[hud_item_measures::m_hands_offset_rot][idx];

	//============ ������� ������ �� ����� ���� ===========//
	if(b_aiming)
	{
		if(IsZoomed())
			m_zoom_params.m_fZoomRotationFactor += Device.fTimeDelta/m_zoom_params.m_fZoomRotateTime;
		else
			m_zoom_params.m_fZoomRotationFactor -= Device.fTimeDelta/m_zoom_params.m_fZoomRotateTime;

		clamp(m_zoom_params.m_fZoomRotationFactor, 0.f, 1.f);

		zr_offs.mul(m_zoom_params.m_fZoomRotationFactor);
		zr_rot.mul(m_zoom_params.m_fZoomRotationFactor);

		summary_offset.add(zr_offs);
	}
	//====================================================//

	auto pActor = smart_cast<const CActor*>(H_Parent());
	const u32 iMovingState = pActor->MovingState();
	idx = b_aiming ? 1ui8 : 0ui8;

	//============= ������� ������ � ������� =============//
	{
		const bool bEnabled = m_strafe_offset[2][idx].x;
		if (!bEnabled)
			goto LOOKOUT_EFFECT;

		// ������������ ������ ������� ������
		float fStrafeMaxTime = m_strafe_offset[2][idx].y; // ����. ����� � ��������, �� ������� �� ���������� �� ������������ ���������
		if (fStrafeMaxTime <= EPS)
			fStrafeMaxTime = 0.01f;

		const float fStepPerUpd = Device.fTimeDelta / fStrafeMaxTime; // �������� ��������� ������� ��������

		if (iMovingState & mcLStrafe)
		{ // �������� �����
			float fVal = (m_fLR_MovingFactor > 0.f ? fStepPerUpd * 3 : fStepPerUpd);
			m_fLR_MovingFactor -= fVal;
		}
		else if (iMovingState & mcRStrafe)
		{ // �������� ������
			float fVal = (m_fLR_MovingFactor < 0.f ? fStepPerUpd * 3 : fStepPerUpd);
			m_fLR_MovingFactor += fVal;
		}
		else
		{ // ��������� � ����� ������ �����������
			if (m_fLR_MovingFactor < 0.0f)
			{
				m_fLR_MovingFactor += fStepPerUpd;
				clamp(m_fLR_MovingFactor, -1.0f, 0.0f);
			}
			else
			{
				m_fLR_MovingFactor -= fStepPerUpd;
				clamp(m_fLR_MovingFactor, 0.0f, 1.0f);
			}
		}

		clamp(m_fLR_MovingFactor, -1.0f, 1.0f); // ������ ������� ������ �� ������ ��������� ��� ������

		// �������� ������� ���� � �������
		Fvector moving_offs = m_strafe_offset[0][idx]; //pos
		moving_offs.mul(m_fLR_MovingFactor); // �������� �� ������ �������

		// ������� ���� � �������
		Fvector moving_rot = m_strafe_offset[1][idx]; //rot
		moving_rot.mul(-PI / 180.f); // ����������� ���� � �������
		moving_rot.mul(m_fLR_MovingFactor); // �������� �� ������ �������

		if (idx == 0)
		{ // �� �����
			moving_offs.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
			moving_rot.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
		}
		else
		{ // �� ����� ����
			moving_offs.mulm_zoom_params.(m_fZoomRotationFactor);
			moving_rot.mul(m_zoom_params.m_fZoomRotationFactor);
		}

		summary_offset.add(moving_offs);
		summary_rotate.add(moving_rot);
	}
	//====================================================//

LOOKOUT_EFFECT:
	//=============== ������� ������������ ===============//
	{
		const bool bEnabled = m_lookout_offset[2][idx].x;
		if (!bEnabled)
			goto APPLY_EFFECTS;

		float fLookoutMaxTime = m_lookout_offset[2][idx].y; // ����. ����� � ��������, �� ������� �� ���������� �� ������������ ���������
		if (fLookoutMaxTime <= EPS)
			fLookoutMaxTime = 0.01f;

		const float fStepPerUpdL = Device.fTimeDelta / fLookoutMaxTime; // �������� ��������� ������� ��������

		if ((iMovingState & mcLLookout) && !(iMovingState & mcRLookout))
		{ // ����������� �����
			float fVal = (m_fLookout_MovingFactor > 0.f ? fStepPerUpdL * 3 : fStepPerUpdL);
			m_fLookout_MovingFactor -= fVal;
		}
		else if ((iMovingState & mcRLookout) && !(iMovingState & mcLLookout))
		{ // ����������� ������
			float fVal = (m_fLookout_MovingFactor < 0.f ? fStepPerUpdL * 3 : fStepPerUpdL);
			m_fLookout_MovingFactor += fVal;
		}
		else
		{ // ��������� � ����� ������ �����������
			if (m_fLookout_MovingFactor < 0.0f)
			{
				m_fLookout_MovingFactor += fStepPerUpdL;
				clamp(m_fLookout_MovingFactor, -1.0f, 0.0f);
			}
			else
			{
				m_fLookout_MovingFactor -= fStepPerUpdL;
				clamp(m_fLookout_MovingFactor, 0.0f, 1.0f);
			}
		}

		clamp(m_fLookout_MovingFactor, -1.0f, 1.0f); // �� ������ ��������� ��� ������

		float koef{ 1.f };
		if ((iMovingState & mcCrouch) && (iMovingState & mcAccel))
			koef = 0.5; // �� ������� ��� ������ ��������� ��� ������ �������
		else if (iMovingState & mcCrouch)
			koef = 0.75; // �� ������� ��� ������ ��������� ��� �������

		// �������� ������� ����
		Fvector lookout_offs = m_lookout_offset[0][idx]; //pos
		lookout_offs.mul(koef);
		lookout_offs.mul(m_fLookout_MovingFactor); // �������� �� ������ �������

		// ������� ����
		Fvector lookout_rot = m_lookout_offset[1][idx]; //rot
		lookout_rot.mul(koef);
		lookout_rot.mul(-PI / 180.f); // ����������� ���� � �������
		lookout_rot.mul(m_fLookout_MovingFactor); // �������� �� ������ �������

		if (idx == 0)
		{ // �� �����
			lookout_offs.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
			lookout_rot.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
		}
		else
		{ // �� ����� ����
			lookout_offs.mul(m_zoom_params.m_fZoomRotationFactor);
			lookout_rot.mul(m_zoom_params.m_fZoomRotationFactor);
		}

		summary_offset.add(lookout_offs);
		summary_rotate.add(lookout_rot);
	}
	//====================================================//

APPLY_EFFECTS:
	//================ ���������� �������� ===============//
	{
		// ������� � ����������� �������� by Zander
		Fvector _angle{}, _pos{ trans.c };
		trans.getHPB(_angle);
		_angle.add(-summary_rotate);
		//Msg("##[%s] summary_rotate: [%f,%f,%f]", __FUNCTION__, summary_rotate.x, summary_rotate.y, summary_rotate.z);
		trans.setHPB(_angle.x, _angle.y, _angle.z);
		trans.c = _pos;

		Fmatrix hud_rotation;
		hud_rotation.identity();

		if (b_aiming)
		{
			hud_rotation.rotateX(zr_rot.x);

			Fmatrix hud_rotation_y;
			hud_rotation_y.identity();
			hud_rotation_y.rotateY(zr_rot.y);
			hud_rotation.mulA_43(hud_rotation_y);

			hud_rotation_y.identity();
			hud_rotation_y.rotateZ(zr_rot.z);
			hud_rotation.mulA_43(hud_rotation_y);
			//Msg("~~[%s] zr_rot: [%f,%f,%f]", __FUNCTION__, zr_rot.x, zr_rot.y, zr_rot.z);
		}
		//Msg("--[%s] summary_offset: [%f,%f,%f]", __FUNCTION__, summary_offset.x, summary_offset.y, summary_offset.z);
		hud_rotation.translate_over(summary_offset);
		trans.mulB_43(hud_rotation);
	}
	//====================================================//
}

void CWeapon::SetAmmoElapsed(int ammo_count)
{
	iAmmoElapsed				= ammo_count;

	u32 uAmmo					= u32(iAmmoElapsed);

	if (uAmmo != m_magazine.size())
	{
		if (uAmmo > m_magazine.size())
		{
			CCartridge			l_cartridge; 
			l_cartridge.Load	(*m_ammoTypes[m_ammoType], u8(m_ammoType));
			while (uAmmo > m_magazine.size())
				m_magazine.push_back(l_cartridge);
		}
		else
		{
			while (uAmmo < m_magazine.size())
				m_magazine.pop_back();
		};
	};
}

u32	CWeapon::ef_main_weapon_type	() const
{
	VERIFY	(m_ef_main_weapon_type != u32(-1));
	return	(m_ef_main_weapon_type);
}

u32	CWeapon::ef_weapon_type	() const
{
	VERIFY	(m_ef_weapon_type != u32(-1));
	return	(m_ef_weapon_type);
}

bool CWeapon::IsNecessaryItem	    (const shared_str& item_sect)
{
	return (std::find(m_ammoTypes.begin(), m_ammoTypes.end(), item_sect) != m_ammoTypes.end() );
}

void CWeapon::modify_holder_params		(float &range, float &fov) const
{
	if (!IsScopeAttached()) {
		inherited::modify_holder_params	(range,fov);
		return;
	}
	range	*= m_addon_holder_range_modifier;
	fov		*= m_addon_holder_fov_modifier;
}

bool CWeapon::render_item_ui_query()
{
	bool b_is_active_item = (m_pInventory->ActiveItem()==this);
	bool res = b_is_active_item && IsZoomed() && ZoomHideCrosshair() && ZoomTexture() && !IsRotatingToZoom();
	return res;
}

void CWeapon::render_item_ui()
{
	ZoomTexture()->Update	();
	ZoomTexture()->Draw		();
}

bool CWeapon::unlimited_ammo() 
{ 
	if (IsGameTypeSingle())
		return psActorFlags.test(AF_UNLIMITEDAMMO) && 
				m_DefaultCartridge.m_flags.test(CCartridge::cfCanBeUnlimited); 

			
};

LPCSTR	CWeapon::GetCurrentAmmo_ShortName	()
{
	if (m_magazine.empty()) return ("");
	CCartridge &l_cartridge = m_magazine.back();
	return *(l_cartridge.m_InvShortName);
}

float CWeapon::Weight()
{
	float res = CInventoryItemObject::Weight();
	if(IsGrenadeLauncherAttached()&&GetGrenadeLauncherName().size()){
		res += pSettings->r_float(GetGrenadeLauncherName(),"inv_weight");
	}
	if(IsScopeAttached()&&GetScopeName().size()){
		res += pSettings->r_float(GetScopeName(),"inv_weight");
	}
	if(IsSilencerAttached()&&GetSilencerName().size()){
		res += pSettings->r_float(GetSilencerName(),"inv_weight");
	}
	
	if(iAmmoElapsed)
	{
		float w		= pSettings->r_float(*m_ammoTypes[m_ammoType],"inv_weight");
		float bs	= pSettings->r_float(*m_ammoTypes[m_ammoType],"box_size");

		res			+= w*(iAmmoElapsed/bs);
	}
	return res;
}

bool CWeapon::show_crosshair()
{
	return !IsPending() && ( !IsZoomed() || !ZoomHideCrosshair() );
}

bool CWeapon::show_indicators()
{
	return ! ( IsZoomed() && ZoomTexture() );
}

float CWeapon::GetConditionToShow	() const
{
	return	(GetCondition());//powf(GetCondition(),4.0f));
}

BOOL CWeapon::ParentMayHaveAimBullet	()
{
	CObject* O=H_Parent();
	CEntityAlive* EA=smart_cast<CEntityAlive*>(O);
	return EA->cast_actor()!=0;
}

BOOL CWeapon::ParentIsActor	()
{
	CObject* O			= H_Parent();
	if (!O)
		return FALSE;

	CEntityAlive* EA	= smart_cast<CEntityAlive*>(O);
	if (!EA)
		return FALSE;

	return EA->cast_actor()!=0;
}

extern u32 hud_adj_mode;

void CWeapon::debug_draw_firedeps()
{
#ifdef DEBUG
	if(hud_adj_mode==5||hud_adj_mode==6||hud_adj_mode==7)
	{
		CDebugRenderer			&render = Level().debug_renderer();

		if(hud_adj_mode==5)
			render.draw_aabb(get_LastFP(),	0.005f,0.005f,0.005f,D3DCOLOR_XRGB(255,0,0));

		if(hud_adj_mode==6)
			render.draw_aabb(get_LastFP2(),	0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,0,255));

		if(hud_adj_mode==7)
			render.draw_aabb(get_LastSP(),		0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,255,0));
	}
#endif // DEBUG
}

const float &CWeapon::hit_probability	() const
{
	VERIFY					((g_SingleGameDifficulty >= egdNovice) && (g_SingleGameDifficulty <= egdMaster)); 
	return					(m_hit_probability[g_SingleGameDifficulty]);
}

void CWeapon::OnStateSwitch	(u32 S)
{
	inherited::OnStateSwitch(S);
	m_dwAmmoCurrentCalcFrame = 0;

    if (GetState() == eReload)
    {
        if (iAmmoElapsed == 0) //Swartz: re-written to use reload empty DOF
        {
            if (H_Parent() == Level().CurrentEntity() && !fsimilar(m_zoom_params.m_ReloadEmptyDof.w, -1.0f))
            {
                CActor* current_actor = smart_cast<CActor*>(H_Parent());
                if (current_actor)
                    current_actor->Cameras().AddCamEffector(xr_new<CEffectorDOF>(m_zoom_params.m_ReloadEmptyDof));
            }
        }
        else
        {
            if (H_Parent() == Level().CurrentEntity() && !fsimilar(m_zoom_params.m_ReloadDof.w, -1.0f))
            {
                CActor* current_actor = smart_cast<CActor*>(H_Parent());
                if (current_actor)
                    current_actor->Cameras().AddCamEffector(xr_new<CEffectorDOF>(m_zoom_params.m_ReloadDof));
            }
        }
    }
}

void CWeapon::OnAnimationEnd(u32 state) 
{
	inherited::OnAnimationEnd(state);
}

void CWeapon::render_hud_mode()
{
	RenderLight();
}

bool CWeapon::MovingAnimAllowedNow()
{
	return !IsZoomed();
}

bool CWeapon::IsHudModeNow()
{
	return (HudItemData()!=NULL);
}

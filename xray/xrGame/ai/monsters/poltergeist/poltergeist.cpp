#include "stdafx.h"
#include "poltergeist.h"
#include "poltergeist_state_manager.h"
#include "../../../characterphysicssupport.h"
#include "../../../PHMovementControl.h"
#include "../../../PhysicsShellHolder.h"
#include "../../../ai_debug.h"
#include "poltergeist_movement.h"
#include "../../../detail_path_manager.h"
#include "../monster_velocity_space.h"
#include "../../../level.h"
#include "../../../level_debug.h"
#include "../control_animation_base.h"
#include "../control_movement_base.h"
#include "../control_path_builder_base.h"
#include "../../../PhysicsShell.h"
#include "../../../GamePersistent.h"
#include "../../../../Include/xrRender/KinematicsAnimated.h"

#include "sound_player.h"


CPoltergeist::CPoltergeist()
{
	StateMan					= xr_new<CStateManagerPoltergeist>(this);
	m_sound_player				= xr_new<CSoundPlayer>(this);
	
	invisible_vel.set			(0.1f, 0.1f);
	
	m_flame						= 0;
	m_tele						= 0;

	m_scare_delay.min			= 0;
	m_scare_delay.normal		= 0;
	m_scare_delay.aggressive	= 0;

}

CPoltergeist::~CPoltergeist()
{
	xr_delete		(StateMan);
	xr_delete		(m_sound_player);
	xr_delete		(m_flame);
	xr_delete		(m_tele);
}

void CPoltergeist::Load(LPCSTR section)
{
	inherited::Load	(section);

	anim().accel_load			(section);
	anim().accel_chain_add		(eAnimWalkFwd,		eAnimRun);

	invisible_vel.set(pSettings->r_float(section,"Velocity_Invisible_Linear"),pSettings->r_float(section,"Velocity_Invisible_Angular"));
	movement().detail().add_velocity(MonsterMovement::eVelocityParameterInvisible,CDetailPathManager::STravelParams(invisible_vel.linear, invisible_vel.angular));

	anim().AddReplacedAnim(&m_bDamaged, eAnimWalkFwd, eAnimWalkDamaged);
	anim().AddReplacedAnim(&m_bDamaged, eAnimRun,	 eAnimRunDamaged);
	
	SVelocityParam &velocity_none		= move().get_velocity(MonsterMovement::eVelocityParameterIdle);	
	SVelocityParam &velocity_turn		= move().get_velocity(MonsterMovement::eVelocityParameterStand);
	SVelocityParam &velocity_walk		= move().get_velocity(MonsterMovement::eVelocityParameterWalkNormal);
	SVelocityParam &velocity_run		= move().get_velocity(MonsterMovement::eVelocityParameterRunNormal);
	SVelocityParam &velocity_walk_dmg	= move().get_velocity(MonsterMovement::eVelocityParameterWalkDamaged);
	SVelocityParam &velocity_run_dmg	= move().get_velocity(MonsterMovement::eVelocityParameterRunDamaged);
	//SVelocityParam &velocity_steal		= move().get_velocity(MonsterMovement::eVelocityParameterSteal);
	//SVelocityParam &velocity_drag		= move().get_velocity(MonsterMovement::eVelocityParameterDrag);


	anim().AddAnim(eAnimStandIdle,		"stand_idle_",			-1, &velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimStandTurnLeft,	"stand_turn_ls_",		-1, &velocity_turn,		PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimStandTurnRight,	"stand_turn_rs_",		-1, &velocity_turn,		PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimWalkFwd,		"stand_walk_fwd_",		-1, &velocity_walk,	PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimRun,			"stand_run_fwd_",		-1,	&velocity_run,		PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimAttack,			"stand_attack_",		-1, &velocity_turn,		PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimDie,			"stand_idle_",			 0, &velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimMiscAction_00,	"fall_down_",			-1, &velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimMiscAction_01,	"fly_",					-1, &velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimCheckCorpse,	"stand_check_corpse_",	-1,	&velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimEat,			"stand_eat_",			-1, &velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
	anim().AddAnim(eAnimLookAround,		"stand_look_around_",	-1,	&velocity_none,				PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");	
	anim().AddAnim(eAnimWalkDamaged,	"stand_walk_dmg_",		-1, &velocity_walk_dmg,	PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");	
	anim().AddAnim(eAnimRunDamaged,		"stand_walk_dmg_",		-1, &velocity_run_dmg,	PS_STAND,	"fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");	

	anim().LinkAction(ACT_STAND_IDLE,	eAnimStandIdle);
	anim().LinkAction(ACT_SIT_IDLE,		eAnimStandIdle);
	anim().LinkAction(ACT_LIE_IDLE,		eAnimStandIdle);
	anim().LinkAction(ACT_WALK_FWD,		eAnimWalkFwd);
	anim().LinkAction(ACT_WALK_BKWD,	eAnimWalkFwd);
	anim().LinkAction(ACT_RUN,			eAnimRun);
	anim().LinkAction(ACT_EAT,			eAnimEat);
	anim().LinkAction(ACT_SLEEP,		eAnimStandIdle);
	anim().LinkAction(ACT_REST,			eAnimStandIdle);
	anim().LinkAction(ACT_DRAG,			eAnimStandIdle);
	anim().LinkAction(ACT_ATTACK,		eAnimAttack);
	anim().LinkAction(ACT_STEAL,		eAnimWalkFwd);
	anim().LinkAction(ACT_LOOK_AROUND,	eAnimLookAround);

#ifdef DEBUG	
	anim().accel_chain_test		();
#endif

	READ_IF_EXISTS(pSettings,r_u32,section,"PsyAura_Fake_Delay", 8000);
	READ_IF_EXISTS(pSettings,r_float,section,"PsyAura_Fake_MaxAddDist", 90.f);

	m_height_change_velocity = READ_IF_EXISTS(pSettings,r_float,section,"Height_Change_Velocity", 0.5f);
	m_height_change_min_time = READ_IF_EXISTS(pSettings,r_u32,section,"Height_Change_Min_Time", 3000);
	m_height_change_max_time = READ_IF_EXISTS(pSettings,r_u32,section,"Height_Change_Max_Time", 10000);
	m_height_min			 = READ_IF_EXISTS(pSettings,r_float,section,"Height_Min", 0.4f);
	m_height_max			 = READ_IF_EXISTS(pSettings,r_float,section,"Height_Max", 2.f);

	LPCSTR polter_type = pSettings->r_string(section,"type");
	
	if (xr_strcmp(polter_type,"flamer") == 0) {
		m_flame			= xr_new<CPolterFlame>(this);
		m_flame->load	(section);
	} else {
		m_tele			= xr_new<CPolterTele>(this);
		m_tele->load	(section);
	}
	
	// ZergO: ?????????? ????? ? ??????????? ?? ?????????
	const int count = DEFAULT_SAMPLE_COUNT;
	LPCSTR bone = "bip01_head";

	m_sound_player->add("Sound_Death",		  count, SOUND_TYPE_MONSTER_DYING,		MonsterSound::eCriticalPriority,	u32(MonsterSound::eCaptureAllChannels),	EPolterSounds::eSndDeath, bone);
	m_sound_player->add("Sound_Hidden_Death", count, SOUND_TYPE_MONSTER_DYING,		MonsterSound::eCriticalPriority,	u32(MonsterSound::eCaptureAllChannels),	EPolterSounds::eSndDeathHidden, bone);
	m_sound_player->add("Sound_Hit",		  count, SOUND_TYPE_MONSTER_INJURING,	MonsterSound::eHighPriority,		u32(MonsterSound::eCaptureAllChannels),	EPolterSounds::eSndHit, bone);
	m_sound_player->add("Sound_Hidden_Hit",	  count, SOUND_TYPE_MONSTER_INJURING,	MonsterSound::eHighPriority,		u32(MonsterSound::eCaptureAllChannels),	EPolterSounds::eSndHitHidden, bone);

	particle_fire_shield			= pSettings->r_string(section,"Particle_Shield");

	// ????????? ????? ? ?????? ??????????? ??????
	m_scare_delay.min		= READ_IF_EXISTS(pSettings, r_u32, section, "Delay_Scare_Min", 15000);
	m_scare_delay.normal	= READ_IF_EXISTS(pSettings, r_u32, section, "Delay_Scare_Normal", 40000);
	m_scare_delay.aggressive= READ_IF_EXISTS(pSettings, r_u32, section, "Delay_Scare_Aggressive", 25000);

}

void CPoltergeist::reload(LPCSTR section)
{
	inherited::reload(section);
	Energy::reload(section,"Invisible_");
}

void CPoltergeist::reinit()
{
	inherited::reinit();
	Energy::reinit();

	m_current_position = Position();

	target_height		= 0.3f;
	time_height_updated = 0;

	Energy::set_auto_activate();
	Energy::set_auto_deactivate();
	Energy::enable();

	// start hidden
	state_invisible						= true;	
	setVisible							(false);
	
	m_current_position = Position		();
	character_physics_support()->movement()->DestroyCharacter();
	
	m_height							= 0.3f;
	time_height_updated					= 0;
	
	EnableHide							();
}

void CPoltergeist::Hide()
{
	if (state_invisible) return;
	
	state_invisible		= true;	
	setVisible			(false);
	
	m_current_position	= Position		();
	character_physics_support()->movement()->DestroyCharacter();

	ability()->on_hide	();

	if ( pSettings->line_exist( cNameSect().c_str(), "visible_immunities_sect" ) )
	  conditions().LoadImmunities( pSettings->r_string( cNameSect().c_str(), "immunities_sect" ), pSettings );
}

void CPoltergeist::Show()
{
	if (!state_invisible) return;

	state_invisible = false;
	
	setVisible(TRUE);

	com_man().seq_run(anim().get_motion_id(eAnimMiscAction_00));

	Position() = m_current_position;
	character_physics_support()->movement()->SetPosition(Position());
	character_physics_support()->movement()->CreateCharacter();
	
	ability()->on_show	();

	if ( pSettings->line_exist( cNameSect().c_str(), "visible_immunities_sect" ) )
	  conditions().LoadImmunities( pSettings->r_string( cNameSect().c_str(), "visible_immunities_sect" ), pSettings );
}

void CPoltergeist::UpdateCL()
{
	inherited::UpdateCL();

	def_lerp(m_height, target_height, m_height_change_velocity, client_update_fdelta());
	
	ability()->update_frame	();
}

void CPoltergeist::ForceFinalAnimation()
{
	if (state_invisible) 
		anim().SetCurAnim(eAnimMiscAction_01);
}


void CPoltergeist::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);
	CTelekinesis::schedule_update();
	Energy::schedule_update();

	UpdateHeight();

	if (state_invisible)
		ability()->update_schedule();
}


BOOL CPoltergeist::net_Spawn (CSE_Abstract* DC) 
{
	if (!inherited::net_Spawn(DC)) 
		return(FALSE);
	VERIFY(character_physics_support());
	VERIFY(character_physics_support()->movement());
	character_physics_support()->movement()->DestroyCharacter();
	// ????????? ?????????
	setVisible		(false);
	ability()->on_hide();
	
	return			(TRUE);
}

void CPoltergeist::net_Destroy()
{
	inherited::net_Destroy();
	Energy::disable();

	ability()->on_destroy();
}

void CPoltergeist::Die(CObject* who)
{
	if (state_invisible)
	{
		if (m_tele) 
		{
			setVisible(true);

			if (PPhysicsShell()) 
			{
				Fmatrix M;
				M.set							(XFORM());
				M.translate_over				(m_current_position);
				PPhysicsShell()->SetTransform	(M);
			} 
			else 
				Position() = m_current_position;
		}
		//else if (m_flame)
		//{
			//CExplosive::GenExplodeEvent(Position(),Fvector().set(0.f,1.f,0.f));
		//}
	}
	else
		m_sound_player->play(EPolterSounds::eSndDeath);

	inherited::Die				(who);
	Energy::disable				();

	if (state_invisible)
		ability()->on_die		();
}

void CPoltergeist::Hit(SHit* pHDS)
{
	if (state_invisible)
	{
		ability()->on_hit(pHDS);
		if ( m_tele && (pHDS->hit_type == ALife::eHitTypeFireWound) && (Device.dwFrame != last_hit_frame) )
		{
			// ????????? ??????? ? ?????????????? ????????
			Fmatrix pos; 
			//CParticlesPlayer::MakeXFORM(this,element,Fvector().set(0.f,0.f,1.f),p_in_object_space,pos);
			CParticlesPlayer::MakeXFORM(this,pHDS->bone(),pHDS->dir,pHDS->p_in_bone_space,pos);

			// ?????????? particles
			CParticlesObject* ps = CParticlesObject::Create(particle_fire_shield,TRUE);
			
			ps->UpdateParent(pos,Fvector().set(0.f,0.f,0.f));
			GamePersistent().ps_needtoplay.push_back(ps);
		}
		
		last_hit_frame = Device.dwFrame;
		inherited::Hit(pHDS);
	}
	else
	{
		m_sound_player->play(EPolterSounds::eSndHit);
		inherited::Hit(pHDS);
	}
}


void CPoltergeist::UpdateHeight()
{
	if (!state_invisible) return;
	
	u32 cur_time = Device.dwTimeGlobal;
	
	if (time_height_updated < cur_time)	{
		time_height_updated = cur_time + Random.randI(m_height_change_min_time,m_height_change_max_time);
		target_height		= Random.randF(m_height_min, m_height_max);		
	}
}

void CPoltergeist::on_activate()
{
	if (m_disable_hide) return;

	Hide();
	
	m_height			= 0.3f;
	time_height_updated = 0;
}

void CPoltergeist::on_deactivate()
{
	if (m_disable_hide) return;

	Show();
}

CMovementManager *CPoltergeist::create_movement_manager	()
{
	m_movement_manager				= xr_new<CPoltergeisMovementManager>(this);

	control().add					(m_movement_manager, ControlCom::eControlPath);
	control().install_path_manager	(m_movement_manager);
	control().set_base_controller	(m_path_base, ControlCom::eControlPath);

	return							(m_movement_manager);
}


void CPoltergeist::net_Relcase(CObject *O)
{
	inherited::net_Relcase		(O);
	CTelekinesis::remove_links	(O);
}


#ifdef DEBUG
CBaseMonster::SDebugInfo CPoltergeist::show_debug_info()
{
	CBaseMonster::SDebugInfo info = inherited::show_debug_info();
	if (!info.active) return CBaseMonster::SDebugInfo();

	string128 text;
	sprintf_s(text, "Invisibility Value = [%f]", Energy::get_value());
	DBG().text(this).add_item(text, info.x, info.y+=info.delta_y, info.color);
	DBG().text(this).add_item("---------------------------------------", info.x, info.y+=info.delta_y, info.delimiter_color);

	return CBaseMonster::SDebugInfo();
}
#endif


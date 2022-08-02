#include "stdafx.h"
#include "ActorHelmet.h"
#include "Actor.h"
#include "Inventory.h"
#include "Torch.h"
#include "BoneProtections.h"
#include "../Include/xrRender/Kinematics.h"
//#include "CustomOutfit.h"

CHelmet::CHelmet()
{
	m_flags.set(FUsingCondition, TRUE);
	m_HitTypeProtection.resize(ALife::eHitTypeMax);
	for(int i=0; i<ALife::eHitTypeMax; i++)
		m_HitTypeProtection[i] = 1.0f;

	m_boneProtection = xr_new<SBoneProtections>();
}

CHelmet::~CHelmet()
{
	xr_delete(m_boneProtection);
}

BOOL CHelmet::net_Spawn(CSE_Abstract* DC)
{
	CActor* pActor = smart_cast<CActor*>( Level().CurrentViewEntity() );
	ReloadBonesProtection( pActor );

	BOOL res = inherited::net_Spawn(DC);
	return					(res);
}

void CHelmet::net_Export(NET_Packet& P)
{
	inherited::net_Export(P);
	P.w_float_q8(GetCondition(),0.0f,1.0f);
}

void CHelmet::net_Import(NET_Packet& P)
{
	inherited::net_Import(P);
	float _cond;
	P.r_float_q8(_cond,0.0f,1.0f);
	SetCondition(_cond);
}

/*void CCustomOutfit::OnH_A_Chield()
{
	inherited::OnH_A_Chield();
//	ReloadBonesProtection();
}*/


void CHelmet::Load(LPCSTR section) 
{
	inherited::Load(section);

	m_HitTypeProtection[ALife::eHitTypeBurn]		= pSettings->r_float(section,"burn_protection");
	m_HitTypeProtection[ALife::eHitTypeStrike]		= pSettings->r_float(section,"strike_protection");
	m_HitTypeProtection[ALife::eHitTypeShock]		= pSettings->r_float(section,"shock_protection");
	m_HitTypeProtection[ALife::eHitTypeWound]		= pSettings->r_float(section,"wound_protection");
	m_HitTypeProtection[ALife::eHitTypeRadiation]	= pSettings->r_float(section,"radiation_protection");
	m_HitTypeProtection[ALife::eHitTypeTelepatic]	= pSettings->r_float(section,"telepatic_protection");
	m_HitTypeProtection[ALife::eHitTypeChemicalBurn]= pSettings->r_float(section,"chemical_burn_protection");
	m_HitTypeProtection[ALife::eHitTypeExplosion]	= pSettings->r_float(section,"explosion_protection");
	m_HitTypeProtection[ALife::eHitTypeFireWound]	= pSettings->r_float(section,"fire_wound_protection");
	m_HitTypeProtection[ALife::eHitTypePhysicStrike]= READ_IF_EXISTS(pSettings, r_float, section, "physic_strike_protection", 0.0f);

	m_boneProtection->m_fHitFracActor				= pSettings->r_float(section, "hit_fraction_actor");

	m_fPowerLoss					= READ_IF_EXISTS(pSettings, r_float, section, "power_loss",    1.0f );
	clamp							( m_fPowerLoss, 0.0f, 1.0f );

	m_fHealthRestoreSpeed			= READ_IF_EXISTS(pSettings, r_float, section, "health_restore_speed",    0.0f );
	m_fRadiationRestoreSpeed		= READ_IF_EXISTS(pSettings, r_float, section, "radiation_restore_speed", 0.0f );
	m_fSatietyRestoreSpeed			= READ_IF_EXISTS(pSettings, r_float, section, "satiety_restore_speed",   0.0f );
	m_fPowerRestoreSpeed			= READ_IF_EXISTS(pSettings, r_float, section, "power_restore_speed",     0.0f );
	m_fBleedingRestoreSpeed			= READ_IF_EXISTS(pSettings, r_float, section, "bleeding_restore_speed",  0.0f );

	if (pSettings->line_exist(section, "nightvision_sect"))
		m_NightVisionSect = pSettings->r_string(section, "nightvision_sect");
	else
		m_NightVisionSect = NULL;

	m_BonesProtectionSect			= READ_IF_EXISTS(pSettings, r_string, section, "bones_koeff_protection",  "" );
	//m_fShowNearestEnemiesDistance	= READ_IF_EXISTS(pSettings, r_float, section, "nearest_enemies_show_dist",  0.0f );

}

void CHelmet::ReloadBonesProtection( CActor* pActor )
{
	if ( !pActor || !pActor->Visual() || m_BonesProtectionSect.size() == 0 )
	{
		return;
	}
	m_boneProtection->reload( m_BonesProtectionSect, smart_cast<IKinematics*>( pActor->Visual() ) );
}

void CHelmet::Hit(float hit_power, ALife::EHitType hit_type)
{
	hit_power *= m_HitTypeK[hit_type];
	ChangeCondition(-hit_power);
}

float CHelmet::GetDefHitTypeProtection(ALife::EHitType hit_type)
{
	return m_HitTypeProtection[hit_type]*GetCondition();
}

float CHelmet::GetHitTypeProtection(ALife::EHitType hit_type, s16 element)
{
	float fBase = m_HitTypeProtection[hit_type]*GetCondition();
	float bone = m_boneProtection->getBoneProtection(element);
	return fBase*bone;
}

float CHelmet::GetBoneArmor(s16 element)
{
	return m_boneProtection->getBoneArmor(element);
}

float CHelmet::HitThroughArmor(float hit_power, s16 element, float ap, bool& add_wound)
{
	float NewHitPower = hit_power;
	float BoneArmor = m_boneProtection->getBoneArmor(element)*GetCondition();

	if( ap > EPS && ap > BoneArmor )
	{
		//пуля пробила бронь
		float d_ap = ap - BoneArmor;
		NewHitPower *= ( d_ap / ap );

		if ( NewHitPower < m_boneProtection->m_fHitFracActor )
		{
			NewHitPower = m_boneProtection->m_fHitFracActor;
		}
		if ( !IsGameTypeSingle() )
		{
			NewHitPower *= m_boneProtection->getBoneProtection(element);
		}
		
		if ( NewHitPower < 0.0f ) { NewHitPower = 0.0f; }

		//увеличить изношенность костюма
		Hit( NewHitPower, ALife::eHitTypeFireWound );
	}
	else
	{
		//пуля НЕ пробила бронь
		NewHitPower *= m_boneProtection->m_fHitFracActor;
		add_wound = false; 	//раны нет
		Hit( NewHitPower, ALife::eHitTypeFireWound );
	}// if >=

	return NewHitPower;
}

void CHelmet::OnMoveToSlot(EItemPlace prev)
{
	//inherited::OnMoveToSlot		(prev);
	if (m_pInventory && (prev == eItemPlaceSlot))
	{
		CActor* pActor = smart_cast<CActor*>( m_pInventory->GetOwner() );
		if (pActor)
		{
			CTorch* pTorch = smart_cast<CTorch*>(pActor->inventory().ItemFromSlot(TORCH_SLOT));
			if(pTorch && pTorch->GetNightVisionStatus())
				pTorch->SwitchNightVision(true);
		}
	}
}

/*void CHelmet::OnH_B_Independent(bool just_before_destroy) 
{
	inherited::OnH_B_Independent(just_before_destroy);
	
	CActor* pActor = smart_cast<CActor*> (H_Parent());
		if (pActor)
		{
			CTorch* pTorch = smart_cast<CTorch*>(pActor->inventory().ItemFromSlot(TORCH_SLOT));
			if(pTorch)
				pTorch->SwitchNightVision(false);
		}
}*/


void CHelmet::OnMoveToRuck(EItemPlace prev)
{
	//inherited::OnMoveToRuck		(prev);
	if (m_pInventory && prev == eItemPlaceSlot)
	{
		CActor* pActor = smart_cast<CActor*> (m_pInventory->GetOwner());
		if (pActor)
		{
			CTorch* pTorch = smart_cast<CTorch*>(pActor->inventory().ItemFromSlot(TORCH_SLOT));
			if(pTorch)
				pTorch->SwitchNightVision(false);
		}
	}
}

bool CHelmet::install_upgrade_impl( LPCSTR section, bool test )
{
	bool result = CInventoryItemObject::install_upgrade_impl( section, test );

	result |= process_if_exists( section, "burn_protection",          &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeBurn]        , test );
	result |= process_if_exists( section, "shock_protection",         &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeShock]       , test );
	result |= process_if_exists( section, "strike_protection",        &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeStrike]      , test );
	result |= process_if_exists( section, "wound_protection",         &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeWound]       , test );
	result |= process_if_exists( section, "radiation_protection",     &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeRadiation]   , test );
	result |= process_if_exists( section, "telepatic_protection",     &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeTelepatic]   , test );
	result |= process_if_exists( section, "chemical_burn_protection", &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeChemicalBurn], test );
	result |= process_if_exists( section, "explosion_protection",     &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeExplosion]   , test );
	result |= process_if_exists( section, "fire_wound_protection",    &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypeFireWound]   , test );
	result |= process_if_exists( section, "physic_strike_protection", &CInifile::r_float, m_HitTypeProtection[ALife::eHitTypePhysicStrike], test );

	LPCSTR str;
	bool result2 = process_if_exists_set( section, "bones_koeff_protection", &CInifile::r_string, str, test );
	if ( result2 && !test )
	{
		m_BonesProtectionSect._set( str );
		CActor* pActor = smart_cast<CActor*>( Level().CurrentViewEntity() );
		ReloadBonesProtection( pActor );
	}
	result |= result2;
	//result |= process_if_exists( section, "nearest_enemies_show_dist",  &CInifile::r_float, m_fShowNearestEnemiesDistance,  test );

	result2 = process_if_exists_set( section, "nightvision_sect", &CInifile::r_string, str, test );
	if ( result2 && !test )
	{
		m_NightVisionSect._set( str );
	}
	result |= result2;

	result |= process_if_exists( section, "health_restore_speed",    &CInifile::r_float, m_fHealthRestoreSpeed,    test );
	result |= process_if_exists( section, "radiation_restore_speed", &CInifile::r_float, m_fRadiationRestoreSpeed, test );
	result |= process_if_exists( section, "satiety_restore_speed",   &CInifile::r_float, m_fSatietyRestoreSpeed,   test );
	result |= process_if_exists( section, "power_restore_speed",     &CInifile::r_float, m_fPowerRestoreSpeed,     test );
	result |= process_if_exists( section, "bleeding_restore_speed",  &CInifile::r_float, m_fBleedingRestoreSpeed,  test );

	result |= process_if_exists( section, "power_loss", &CInifile::r_float, m_fPowerLoss, test );
	clamp( m_fPowerLoss, 0.0f, 1.0f );

	return result;
}

#pragma once

#include "inventory_item_object.h"

struct SBoneProtections;

class CHelmet: public CInventoryItemObject {
private:
    typedef	CInventoryItemObject inherited;
public:
							CHelmet					(void);
	virtual					~CHelmet				(void);

	virtual void			Load					(LPCSTR section);
	
	virtual void			Hit						(float P, ALife::EHitType hit_type);

	float					GetHitTypeProtection	(ALife::EHitType hit_type, s16 element);
	float					GetDefHitTypeProtection	(ALife::EHitType hit_type);
	float					GetBoneArmor			(s16 element);

	float					HitThroughArmor			(float hit_power, s16 element, float ap, bool& add_wound);



	virtual void			OnMoveToSlot			(EItemPlace prev);
	virtual void			OnMoveToRuck			(EItemPlace prev);
	//virtual void			OnH_A_Chield			();
	//virtual void 			OnH_B_Independent		(bool just_before_destroy);

	
protected:
	HitImmunity::HitTypeSVec		m_HitTypeProtection;
	float							m_fPowerLoss;

	SBoneProtections*		m_boneProtection;	

public:
	float					m_fHealthRestoreSpeed;
	float 					m_fRadiationRestoreSpeed;
	float 					m_fSatietyRestoreSpeed;
	float					m_fPowerRestoreSpeed;
	float					m_fBleedingRestoreSpeed;

	shared_str				m_NightVisionSect;
	shared_str				m_BonesProtectionSect;

	//float					m_fShowNearestEnemiesDistance;
	virtual BOOL			net_Spawn				(CSE_Abstract* DC);
	virtual void			net_Export				(NET_Packet& P);
	virtual void			net_Import				(NET_Packet& P);
			void			ReloadBonesProtection	(CActor* pActor);

protected:
	virtual bool			install_upgrade_impl	( LPCSTR section, bool test );
};

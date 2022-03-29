// hit_immunity.h: ����� ��� ��� ��������, ������� ������������
//				   ������������ ���������� ��� ������ ����� �����
//////////////////////////////////////////////////////////////////////

#pragma once

#include "alife_space.h"
#include "hit_immunity_space.h"

class CHitImmunity
{
public:
	CHitImmunity();
	virtual ~CHitImmunity();

	virtual void LoadImmunities (LPCSTR section,CInifile* ini);

	virtual float AffectHit		(float power, ALife::EHitType hit_type);
	
			float GetHitImmunity (ALife::EHitType hit_type) const				{return m_HitTypeK[hit_type];}
			
protected:
	//������������ �� ������� ����������� ���
	//��� ��������������� ���� �����������
	//(��� �������� �������� � ����������� ��������)
	HitImmunity::HitTypeSVec m_HitTypeK;
};
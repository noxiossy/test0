#include "UIWindow.h"
#include "..\..\xrServerEntities\alife_space.h"
#include "..\actor_defs.h"

class CUIStatic;
class CUIProgressBar;
class CUIXml;
class CActor;

class CUIHudStatesWnd : public CUIWindow
{
private:
	typedef CUIWindow						inherited;

	CUIStatic*			m_back;
	CUIStatic*			m_back_v;

	CUIStatic*			m_ui_weapon_sign_ammo;
	CUIStatic*			m_ui_weapon_icon;
	Frect				m_ui_weapon_icon_rect;
	CUIStatic*			m_fire_mode;

	CUIProgressBar*		m_ui_health_bar;
	CUIProgressBar*		m_ui_stamina_bar;

protected:
	CUIStatic*			m_ind_start_line;
	CUIStatic*			m_ind_radiation;
	CUIStatic*			m_ind_starvation;
	CUIStatic*			m_ind_weapon_broken;
	CUIStatic*			m_ind_bleeding;
	CUIStatic*			m_ind_psyhealth;
	CUIStatic*			m_ind_overweight;


public:
					CUIHudStatesWnd		();
	virtual			~CUIHudStatesWnd	();

			void	InitFromXml			( CUIXml& xml, LPCSTR path );
			void	Load_section		();
	virtual void	Update				();

			void	UpdateHealth		( CActor* actor );
			void	SetAmmoIcon			( const shared_str& sect_name );
			void	UpdateActiveItemInfo( CActor* actor );

			void	UpdateIndicators	( CActor* actor );
}; // class CUIHudStatesWnd

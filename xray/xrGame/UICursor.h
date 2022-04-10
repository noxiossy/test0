#pragma once

#include "ui_base.h"
//.#include "../Include/xrRender/FactoryPtr.h"
//. class IUIShader;
class CUIStatic;

class CUICursor:	public pureRender,
					public pureScreenResolutionChanged
{
	bool			bVisible{};
	Fvector2		vPos{};
	Fvector2		vPrevPos{};
	bool			m_b_use_win_cursor;
	CUIStatic*		m_static;
	void			InitInternal	();
public:
					CUICursor		();
	virtual			~CUICursor		();
	void 			OnRender		() override;
	
	Fvector2		GetCursorPositionDelta() const;

	Fvector2		GetCursorPosition		() const;
	void			SetUICursorPosition		(const Fvector2& pos);
	void			UpdateCursorPosition		(const int _dx, const int _dy);
	virtual void	OnScreenResolutionChanged	();

	bool			IsVisible					() const {return bVisible;}
	void			Show						() {bVisible = true;}
	void			Hide						() {bVisible = false;}
};

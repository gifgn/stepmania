#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: ScreenCaution

 Desc: Screen that displays while resources are being loaded.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "ScreenStyleSplash.h"
#include "GameConstantsAndTypes.h"
#include "PrefsManager.h"
#include "AnnouncerManager.h"
#include "GameState.h"
#include "RageSoundManager.h"
#include "ThemeManager.h"
#include "GameState.h"
#include "GameManager.h"

#define NEXT_SCREEN				THEME->GetMetric("ScreenStyleSplash","NextScreen")


const ScreenMessage SM_DoneOpening		= ScreenMessage(SM_User-7);
const ScreenMessage SM_StartClosing		= ScreenMessage(SM_User-8);


ScreenStyleSplash::ScreenStyleSplash()
{
	SOUNDMAN->StopMusic();

	if(!PREFSMAN->m_bShowDontDie)
	{
		this->SendScreenMessage( SM_GoToNextScreen, 0.f );
		return;
	}

	CString sGameName = GAMESTATE->GetCurrentGameDef()->m_szName;	
	CString sStyleName = GAMESTATE->GetCurrentStyleDef()->m_szName;
	int iDifficulty = GAMESTATE->m_PreferredDifficulty[0];

	m_Background.LoadFromAniDir( THEME->GetPathTo("BGAnimations",ssprintf("ScreenStyleSplash-%s-%s-%d",sGameName.GetString(),sStyleName.GetString(),iDifficulty) ) );
	this->AddChild( &m_Background );
	
	m_Wipe.OpenWipingRight( SM_DoneOpening );
	this->AddChild( &m_Wipe );

//	m_FadeWipe.SetOpened();
//	this->AddChild( &m_FadeWipe );

	this->SendScreenMessage( SM_StartClosing, 2 );
}


void ScreenStyleSplash::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	if( m_Wipe.IsClosing() )
		return;

	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );
}


void ScreenStyleSplash::HandleScreenMessage( const ScreenMessage SM )
{
	switch( SM )
	{
	case SM_StartClosing:
		if( !m_Wipe.IsClosing() )
			m_Wipe.CloseWipingRight( SM_GoToNextScreen );
		break;
	case SM_DoneOpening:
	
		break;
	case SM_GoToPrevScreen:
		SCREENMAN->SetNewScreen( "ScreenTitleMenu" );
		break;
	case SM_GoToNextScreen:
		SCREENMAN->SetNewScreen( NEXT_SCREEN );
		break;
	}
}

void ScreenStyleSplash::MenuStart( PlayerNumber pn )
{
		m_Wipe.CloseWipingRight( SM_GoToNextScreen );
}

void ScreenStyleSplash::MenuBack( PlayerNumber pn )
{
	if(m_FadeWipe.IsClosing())
		return;
	this->ClearMessageQueue();
	m_FadeWipe.CloseWipingLeft( SM_GoToPrevScreen );
	SOUNDMAN->PlayOnce( THEME->GetPathTo("Sounds","menu back") );
}


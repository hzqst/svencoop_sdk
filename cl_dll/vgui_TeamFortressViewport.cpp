//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include<VGUI_Cursor.h>
#include<VGUI_Frame.h>
#include<VGUI_Label.h>
#include<VGUI_Surface.h>
#include<VGUI_BorderLayout.h>
#include<VGUI_Panel.h>
#include<VGUI_ImagePanel.h>
#include<VGUI_Button.h>
#include<VGUI_ActionSignal.h>
#include<VGUI_InputSignal.h>
#include<VGUI_MenuSeparator.h>
#include<VGUI_TextPanel.h>
#include<VGUI_LoweredBorder.h>
#include<VGUI_LineBorder.h>
#include<VGUI_Scheme.h>
#include<VGUI_Font.h>
#include<VGUI_App.h>
#include<VGUI_BuildGroup.h>

#include "hud.h"
#include "cl_util.h"
#include "SvenVersion.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "keydefs.h"
#include "demo.h"
#include "demo_api.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_CameraPanel.h"
#include "vgui_MediaSelect.h"
#include "vgui_VotePanel.h"
#include "vgui_Inventory.h"
#include "vgui_MOTDWindow.h"
#include "vgui_ScorePanel.h"
#include "vgui_SpectatorPanel.h"

#include "shake.h"
#include "screenfade.h"

extern int g_iVisibleMouse;
extern int mouse_oldbuttonstate;
class CCommandMenu;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

// Scoreboard positions
#define SBOARD_INDENT_X			XRES(104)
#define SBOARD_INDENT_Y			YRES(40)

// low-res scoreboard indents
#define SBOARD_INDENT_X_512		30
#define SBOARD_INDENT_Y_512		30

#define SBOARD_INDENT_X_400		0
#define SBOARD_INDENT_Y_400		20

void IN_ResetMouse( void );
extern float * GetClientColor( int clientIndex );

using namespace vgui;

// Team Colors
int iNumberOfTeamColors = 5;
int iTeamColors[6][3] =
{
	{ 101, 128, 152 },	// Default
	{ 31, 95, 255 },		// Blue
	{ 255, 31, 31 },		// Red
	{ 255, 255, 95 },		// Yellow
	{ 31, 255, 95 },		// Green
	{ 255, 255, 255 },	// White
};


// Used for Class specific buttons
const char *sTFClasses[] =
{
	"",
	"SCOUT",
	"SNIPER",
	"SOLDIER",
	"DEMOMAN",
	"MEDIC",
	"HWGUY",
	"PYRO",
	"SPY",
	"ENGINEER",
	"CIVILIAN",
};

const char *sLocalisedClasses[] = 
{
	"#Civilian",
	"#Scout",
	"#Sniper",
	"#Soldier",
	"#Demoman",
	"#Medic",
	"#HWGuy",
	"#Pyro",
	"#Spy",
	"#Engineer",
	"#Random",
	"#Civilian",
};

const char *sTFClassSelection[] = 
{
	"civilian",
	"scout",
	"sniper",
	"soldier",
	"demoman",
	"medic",
	"hwguy",
	"pyro",
	"spy",
	"engineer",
	"randompc",
	"civilian",
};

#ifdef _TFC
int iBuildingCosts[] =
{
	BUILD_COST_DISPENSER,
	BUILD_COST_SENTRYGUN,
	BUILD_COST_TELEPORTER
};

// This maps class numbers to the Invalid Class bit.
// This is needed for backwards compatability in maps that were finished before
// all the classes were in TF. Hence the wacky sequence.
int sTFValidClassInts[] =
{
	0,
	TF_ILL_SCOUT,
	TF_ILL_SNIPER,
	TF_ILL_SOLDIER,
	TF_ILL_DEMOMAN,
	TF_ILL_MEDIC,
	TF_ILL_HVYWEP,
	TF_ILL_PYRO,
	TF_ILL_SPY,
	TF_ILL_ENGINEER,
	TF_ILL_RANDOMPC,
};
#endif

// Get the name of TGA file, based on GameDir
char* GetVGUITGAName(const char *pszName)
{
	int i;
	char sz[256]; 
	static char gd[256]; 
	const char *gamedir;

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;
	sprintf(sz, pszName, i);

	gamedir = gEngfuncs.pfnGetGameDirectory();
	sprintf(gd, "%s/gfx/vgui/%s.tga",gamedir,sz);

	return gd;
}

//================================================================
// COMMAND MENU
//================================================================
void CCommandMenu::AddButton( CommandButton *pButton )
{
	if (m_iButtons >= MAX_BUTTONS)
		return;

	m_aButtons[m_iButtons] = pButton;
	m_iButtons++;
	pButton->setParent( this );
	pButton->setFont( Scheme::sf_primary3 );

	// give the button a default key binding
	if ( m_iButtons < 10 )
	{
		pButton->setBoundKey( m_iButtons + '0' );
	}
	else if ( m_iButtons == 10 )
	{
		pButton->setBoundKey( '0' );
	}
}

void CCommandMenu::RemoveAllButtons(void)
{
	/*
	for(int i=0;i<m_iButtons;i++)
	{
		CommandButton *pTemp = m_aButtons[i]; 
		m_aButtons[i] = NULL;
		
		pTemp
		if(pTemp)
		{
			delete(pTemp);
		}
		
	}
	*/
	removeAllChildren();
	m_iButtons=0;
	
}

//-----------------------------------------------------------------------------
// Purpose: Tries to find a button that has a key bound to the input, and
//			presses the button if found
// Input  : keyNum - the character number of the input key
// Output : Returns true if the command menu should close, false otherwise
//-----------------------------------------------------------------------------
bool CCommandMenu::KeyInput( int keyNum )
{
	// loop through all our buttons looking for one bound to keyNum
	for ( int i = 0; i < m_iButtons; i++ )
	{
		if ( !m_aButtons[i]->IsNotValid() )
		{
			if ( m_aButtons[i]->getBoundKey() == keyNum )
			{
				// hit the button
				if ( m_aButtons[i]->GetSubMenu() )
				{
					// open the sub menu
					gViewPort->SetCurrentCommandMenu( m_aButtons[i]->GetSubMenu() );
					return false;
				}
				else
				{
					// run the bound command
					m_aButtons[i]->fireActionSignal();
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: clears the current menus buttons of any armed (highlighted) 
//			state, and all their sub buttons
//-----------------------------------------------------------------------------
void CCommandMenu::ClearButtonsOfArmedState( void )
{
	for ( int i = 0; i < GetNumButtons(); i++ )
	{
		m_aButtons[i]->setArmed( false );

		if ( m_aButtons[i]->GetSubMenu() )
		{
			m_aButtons[i]->GetSubMenu()->ClearButtonsOfArmedState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSubMenu - 
// Output : CommandButton
//-----------------------------------------------------------------------------
CommandButton *CCommandMenu::FindButtonWithSubmenu( CCommandMenu *pSubMenu )
{
	for ( int i = 0; i < GetNumButtons(); i++ )
	{
		if ( m_aButtons[i]->GetSubMenu() == pSubMenu )
			return m_aButtons[i];
	}

	return NULL;
}

// Recalculate the visible buttons
bool CCommandMenu::RecalculateVisibles( int iYOffset, bool bHideAll )
{
	int iCurrentY = 0;
	bool iVisibleButtons = false;

	if (iYOffset)
		setPos(_pos[0], iYOffset);

	// Cycle through all the buttons in this menu, and see which will be visible
	for (int i = 0; i < m_iButtons; i++)
	{
		CommandButton *pButton = m_aButtons[i];
		CCommandMenu *pSubMenu = pButton->GetSubMenu();

		if (pButton->IsNotValid() || bHideAll )
		{
			pButton->setVisible( false );
			if (pSubMenu)
				pSubMenu->RecalculateVisibles(iCurrentY + _pos[1], true );
			}
		else if ( !pSubMenu || pSubMenu->RecalculateVisibles(iCurrentY + _pos[1], false ) )
				{
			int x, y;

			pButton->setVisible( true );
			pButton->getPos(x, y);
			pButton->setPos(x, iCurrentY);
			iVisibleButtons = true;
			iCurrentY += BUTTON_SIZE_Y - 1;
		}
		else
		{
			pButton->setVisible(false);
		}
	}

	// Set Size
	setSize( _size[0], iCurrentY + 1 );
	return iVisibleButtons;
}

// Make sure all submenus can fit on the screen
void CCommandMenu::RecalculatePositions( int iYOffset )
{
	int iTop = _pos[1] + iYOffset;
	int iAdjust = 0;

	// Calculate if this is going to fit onscreen, and shuffle it up if it won't
	int iBottom = iTop + _size[1];

	if ( iBottom > ScreenHeight )
	{
		// Move in increments of button sizes
		while (iAdjust < iBottom - ScreenHeight)
			iAdjust += BUTTON_SIZE_Y - 1;

		iTop -= iAdjust;

		// Make sure it doesn't move off the top of the screen (the menu's too big to fit it all)
		if ( iTop < 0 )
		{
			iAdjust += iTop;
			iTop = 0;
		}
	}

	for (int i = 0; i < m_iButtons; i++)
		m_aButtons[i]->UpdateSubMenus( iAdjust );

	setPos(_pos[0], iTop);
}


// Make this menu and all menus above it in the chain visible
void CCommandMenu::MakeVisible( CCommandMenu *pChildMenu )
{
/*
	// Push down the button leading to the child menu
	for (int i = 0; i < m_iButtons; i++)
	{
		if ( (pChildMenu != NULL) && (m_aButtons[i]->GetSubMenu() == pChildMenu) )
		{
			m_aButtons[i]->setArmed( true );
		}
		else
		{
			m_aButtons[i]->setArmed( false );
		}
	}
*/

	setVisible(true);
	if (m_pParentMenu)
		m_pParentMenu->MakeVisible( this );
}

//================================================================
// CreateSubMenu
CCommandMenu *TeamFortressViewport::CreateSubMenu( CommandButton *pButton, CCommandMenu *pParentMenu )
{
	int iXPos = 0, y = 0;

	if (pParentMenu)
	{
		iXPos = pParentMenu->GetXOffset() + CMENU_SIZE_X - 1;
		y = pParentMenu->GetYOffset() + (m_pCurrentCommandMenu->GetNumButtons() - 1) * BUTTON_SIZE_Y;
	}

	CCommandMenu *pMenu = new  CCommandMenu(pParentMenu, iXPos, y, CMENU_SIZE_X, 0);
	pMenu->setParent(this);
	pButton->AddSubMenu( pMenu );
	pButton->setFont( Scheme::sf_primary3 );

	// Create the Submenu-open signal
	InputSignal *pISignal = new CMenuHandler_PopupSubMenuInput(pButton, pMenu);
	pButton->addInputSignal(pISignal);

	// Put a > to show it's a submenu
	CImageLabel *pLabel = new CImageLabel("arrow", CMENU_SIZE_X - SUBMENU_SIZE_X, SUBMENU_SIZE_Y);
	pLabel->setParent(pButton);
	pLabel->addInputSignal(pISignal);

	// Reposition
	pLabel->setPos(CMENU_SIZE_X - pLabel->getImageWide(), (BUTTON_SIZE_Y - pLabel->getImageTall()) / 2);

	// Create the mouse off signal for the Label too
	if (!pButton->m_bNoHighlight)
		pLabel->addInputSignal( new CHandler_CommandButtonHighlight(pButton) );

	return pMenu;
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the memory allocated for TeamFortressViewport is nulled out
// Input  : stAllocateBlock - 
// Output : void *
//-----------------------------------------------------------------------------
void *TeamFortressViewport::operator new( size_t stAllocateBlock )
{
//	void *mem = Panel::operator new( stAllocateBlock );
	void *mem = ::operator new( stAllocateBlock );
	memset( mem, 0, stAllocateBlock );
	return mem;
}

//-----------------------------------------------------------------------------
// Purpose: InputSignal handler for the main viewport
//-----------------------------------------------------------------------------
class CViewPortInputHandler : public InputSignal
{
public:
	bool bPressed;

	CViewPortInputHandler()
	{
	}

	virtual void cursorMoved(int x,int y,Panel* panel) {}
	virtual void cursorEntered(Panel* panel) {}
	virtual void cursorExited(Panel* panel) {}
	virtual void mousePressed(MouseCode code,Panel* panel) 
	{
		if ( code != MOUSE_LEFT )
		{
			// send a message to close the command menu
			// this needs to be a message, since a direct call screws the timing
			gEngfuncs.pfnClientCmd( "ForceCloseCommandMenu\n" );
		}
	}
	virtual void mouseReleased(MouseCode code,Panel* panel)
	{
	}

	virtual void mouseDoublePressed(MouseCode code,Panel* panel) {}
	virtual void mouseWheeled(int delta,Panel* panel) {}
	virtual void keyPressed(KeyCode code,Panel* panel) {}
	virtual void keyTyped(KeyCode code,Panel* panel) {}
	virtual void keyReleased(KeyCode code,Panel* panel) {}
	virtual void keyFocusTicked(Panel* panel) {}
};


//================================================================
TeamFortressViewport::TeamFortressViewport(int x,int y,int wide,int tall) : Panel(x,y,wide,tall), m_SchemeManager(wide,tall)
{
	gViewPort = this;
	m_iInitialized = false;
	m_pMediaSelect = NULL;
	m_pVotePanel = NULL;
	m_pVotePopup = NULL;
	m_pMapVotePanel = NULL;
	m_pCameraPanel = NULL;
	m_pInventory = NULL;
	m_pTextWindows[0] = m_pTextWindows[1] = NULL;
	m_pScoreBoard = NULL;
	m_pSpectatorPanel = NULL;
	m_pCurrentMenu = NULL;
	m_pCurrentCommandMenu = NULL;

	gEngfuncs.pfnRegisterVariable("hud_takesshots", "0", FCVAR_ARCHIVE);

	Initialize();
	addInputSignal( new CViewPortInputHandler );

	int r, g, b, a;
	
	Scheme* pScheme = App::getInstance()->getScheme();

	// primary text color
	// Get the colors
	//!! two different types of scheme here, need to integrate
	SchemeHandle_t hPrimaryScheme = m_SchemeManager.getSchemeHandle( "Primary Button Text" );
	{
		// font
		pScheme->setFont( Scheme::sf_primary1, m_SchemeManager.getFont(hPrimaryScheme) );

		// text color
		m_SchemeManager.getFgColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor(Scheme::sc_primary1, r, g, b, a );		// sc_primary1 is non-transparent orange

		// background color (transparent black)
		m_SchemeManager.getBgColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor(Scheme::sc_primary3, r, g, b, a );

		// armed foreground color
		m_SchemeManager.getFgArmedColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor(Scheme::sc_secondary2, r, g, b, a );

		// armed background color
		m_SchemeManager.getBgArmedColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor(Scheme::sc_primary2, r, g, b, a );

		//!! need to get this color from scheme file
		// used for orange borders around buttons
		m_SchemeManager.getBorderColor( hPrimaryScheme, r, g, b, a );
		// pScheme->setColor(Scheme::sc_secondary1, r, g, b, a );
		pScheme->setColor(Scheme::sc_secondary1, 255, 255, 255, 0);
	}

	// Change the second primary font (used in the scoreboard)
	SchemeHandle_t hScoreboardScheme = m_SchemeManager.getSchemeHandle( "Scoreboard Text" );
	{
		pScheme->setFont(Scheme::sf_primary2, m_SchemeManager.getFont(hScoreboardScheme) );
	}
	
	// Change the third primary font (used in command menu)
	SchemeHandle_t hCommandMenuScheme = m_SchemeManager.getSchemeHandle( "CommandMenu Text" );
	{
		pScheme->setFont(Scheme::sf_primary3, m_SchemeManager.getFont(hCommandMenuScheme) );
	}

	App::getInstance()->setScheme(pScheme);

	CreateSpectatorMenu();
	CreateScoreBoard();
	CreateTeamMenu();
	CreateCommandMenu();
	CreateMediaSelectMenu();
	CreateVoteMenu();
	CreateCameraMenu();
	CreateInventoryMenu();


}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
void TeamFortressViewport::Initialize()
{
	CMenuPanel *panels[] =
	{
		m_pMediaSelect, m_pVotePanel,
		m_pMapVotePanel, m_pVotePopup,
		m_pCameraPanel, m_pInventory
	};

	for (int i = 0; i < 6; i++)
{
	// Force each menu to Initialize
	if (panels[i])
			panels[i]->Close();
	}
	if (m_pMapVotePanel)
		m_pMapVotePanel->Reset();

	if (m_pScoreBoard)
	{
		m_pScoreBoard->Initialize();
		HideScoreBoard();
	}
	if (m_pSpectatorPanel)
		m_pSpectatorPanel->setVisible( false );

	HideVGUIMenu();
	HideCommandMenu();

	// Clear out some data
	m_iGotAllMOTD = true;
	m_flScoreBoardLastUpdated = 0;
	g_iTeamNumber = 0;
	memset(m_szServerName, 0, sizeof(m_szServerName));
	memset(m_szServerHostname, 0, sizeof(m_szServerHostname));
	memset(m_sMapName, 0, sizeof(m_sMapName));
	sprintf(m_szServerName, "Sven Co-op " SVEN_GAME_VERSION_STRING);
	for (int i = 0; i < 5; i++)
	{
		m_iValidClasses[i] = 0;
		memset(m_sTeamNames[i], 0, sizeof(m_sTeamNames[i]));
	}

	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor(Scheme::scu_none) );
}

void TeamFortressViewport::VidInit()
{
	if (m_pCameraPanel)
		m_pCameraPanel->VidInit();
}

class CException;
//-----------------------------------------------------------------------------
// Purpose: Read the Command Menu structure from the txt file and create the menu.
//			Returns Index of menu in m_pCommandMenus
//-----------------------------------------------------------------------------
void TeamFortressViewport::CreateCommandMenu()
{
	m_pCommandMenus[0] = new CCommandMenu(NULL, 0, CMENU_TOP, CMENU_SIZE_X, 300);	// This will be resized once we know how many items are in it
	m_pCommandMenus[0]->setParent(this);
	m_pCommandMenus[0]->setVisible(false);
	m_iNumMenus = 1;
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = 0;
	char *pFile = (char*)gEngfuncs.COM_LoadFile("commandmenu.txt", 5, NULL);
	if (!pFile)
	{
		gEngfuncs.Con_DPrintf("Unable to open commandmenu.txt\n");
		SetCurrentCommandMenu( NULL );
		return;
	}

	gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For5Seconds",   m_sDetpackStrings[0], MAX_TEAMNAME_SIZE);
	gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For20Seconds",   m_sDetpackStrings[1], MAX_TEAMNAME_SIZE);
	gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For50Seconds",   m_sDetpackStrings[2], MAX_TEAMNAME_SIZE);

	// Now start parsing the menu structure
	m_pCurrentCommandMenu = m_pCommandMenus[0];

	char cCustom[1024];
	char szLastButtonText[32] = "file start";
	char *pfile = gEngfuncs.COM_ParseFile(pFile, cCustom);
	while (cCustom[0] && m_iNumMenus < MAX_MENUS )
	{
		// Keep looping until we hit the end of this menu
		while (cCustom[0] != '}' && cCustom[0])
		{
			char cText[32], boundKey[32], command[128], map[256] = "";
			bool iCustom = false, bGetExtraToken = true;
			CommandButton *pButton = NULL;

			// We should never be here without a Command Menu
			if (!m_pCurrentCommandMenu)
			{
				gEngfuncs.Con_Printf("Error in Commandmenu.txt file after '%s'.\n", szLastButtonText );
				m_iInitialized = false;
				gEngfuncs.COM_FreeFile(pFile);
				return;
			}

			if (!strcmp(cCustom, "CUSTOM") )
			{
				iCustom = true;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
			}
			// See if it's a map
			else if (!strcmp(cCustom, "MAP") )
			{
				// Get the mapname
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
				strncpy(map, cCustom, sizeof(map) - 1);
				map[sizeof(map) -1] = 0;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
			}

			strncpy(boundKey, cCustom, sizeof(boundKey) - 1);
			boundKey[sizeof(boundKey) - 1] = 0;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
			strncpy( cText, cCustom, sizeof(cText) - 1);
			cText[sizeof(cText) - 1] = 0;

			// save off the last button text we've come across (for error reporting)
			strcpy( szLastButtonText, cText );

			// Get the button command
			pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
			strncpy(command, cCustom, sizeof(command) - 1);
			command[sizeof(command) - 1] = 0;

			int iButtonY = m_pCurrentCommandMenu->GetNumButtons() * BUTTON_SIZE_Y;

			// Custom button handling
			if ( iCustom )
			{
				pButton = CreateCustomButton( cText, command);

				// Get the next token to see if we're a menu
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);

				if (cCustom[0] == '{' )
					strcpy(command, "{");
				else
					bGetExtraToken = false;
				}
			else if (map[0])
			{
				// create a map button
				pButton = new MapButton(map, cText, 0, iButtonY, CMENU_SIZE_X, BUTTON_SIZE_Y);
			}
			else
			{
				// normal button
				pButton = new CommandButton(cText, 0, iButtonY, CMENU_SIZE_X, BUTTON_SIZE_Y);
			}

			// add the button into the command menu
			if ( pButton )
			{
				m_pCurrentCommandMenu->AddButton( pButton );
				pButton->setBoundKey(boundKey[0] );
				pButton->setParentMenu( m_pCurrentCommandMenu );

				// Override font in CommandMenu
				pButton->setFont( Scheme::sf_primary3 );
			}

			// Find out if it's a submenu or a button we're dealing with
			if (command[0] == '{' )
			{
				if ( m_iNumMenus >= MAX_MENUS )
					gEngfuncs.Con_Printf("Too many menus in commandmenu.txt past '%s'\n", szLastButtonText );
				else if (pButton)
				{
					// Create the menu
					m_pCommandMenus[m_iNumMenus] = CreateSubMenu(pButton, m_pCurrentCommandMenu);
					m_pCurrentCommandMenu = m_pCommandMenus[m_iNumMenus++];
				}
			}
			else if ( !iCustom && pButton)
			{
				pButton->addActionSignal(new CMenuHandler_StringCommand(command));
				// Create an input signal that'll popup the current menu
				pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput(pButton, m_pCurrentCommandMenu) );
			}

			// Get the next token
			if ( bGetExtraToken )
				pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
			}

		if (m_pCurrentCommandMenu)
			m_pCurrentCommandMenu = m_pCurrentCommandMenu->GetParentMenu();

		pfile = gEngfuncs.COM_ParseFile(pfile, cCustom);
	}

	m_pCurrentMenu = NULL;
	SetCurrentCommandMenu( NULL );
	gEngfuncs.COM_FreeFile(pFile);

	m_iInitialized = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pButtonText - 
//			*pButtonName - 
// Output : CommandButton
//-----------------------------------------------------------------------------
CommandButton *TeamFortressViewport::CreateCustomButton( char *pButtonText, char *pButtonName )
{
	if ( !strcmp( pButtonName, "!CHANGETEAM" ) )
	{
		CommandButton *pButton = new CommandButton(pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y);
		CCommandMenu *pMenu = CreateSubMenu(pButton, m_pCurrentCommandMenu);
		m_pCommandMenus[m_iNumMenus++] = pMenu;

		// ChangeTeam buttons
		for (int i = 0; i < 4; i++)
		{
			char sz[128];
			sprintf(sz, "jointeam %d", i+1);
			m_pTeamButtons[i] = new TeamButton(i+1, "teamname", 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);
			m_pTeamButtons[i]->addActionSignal(new CMenuHandler_StringCommandWatch( sz ));
			pMenu->AddButton( m_pTeamButtons[i] );
		}

		// Auto Assign button
		m_pTeamButtons[4] = new TeamButton(5, CHudTextMessage::BufferedLocaliseTextString( "#Team_AutoAssign" ), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);
		m_pTeamButtons[4]->addActionSignal(new CMenuHandler_StringCommand( "jointeam 5" ));
		pMenu->AddButton( m_pTeamButtons[4] );

		// Spectate button
		m_pTeamButtons[5] = new SpectateButton( CHudTextMessage::BufferedLocaliseTextString( "#Menu_Spectate" ), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false);
		m_pTeamButtons[5]->addActionSignal(new CMenuHandler_StringCommand( "spectate" ));
		pMenu->AddButton( m_pTeamButtons[5] );
		return pButton;
	}

	if ( !strcmp( pButtonName, "!MAPBRIEFING" ) )
	{
		CommandButton *pButton = new CommandButton(pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y);
		pButton->addActionSignal(new CMenuHandler_TextWindow(MENU_MAPBRIEFING));
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput(pButton, m_pCurrentCommandMenu) );
		return pButton;
}

	return NULL;
}

//=======================================================================
void TeamFortressViewport::ShowCommandMenu()
{
	if (!m_iInitialized)
		return;

	//Already have a menu open.
	if ( m_pCurrentMenu )  
		return;

	// is the command menu open?
	if ( m_pCurrentCommandMenu )
	{
		HideCommandMenu();
		return;
	}

	// Not visible while in intermission
	if ( gHUD.m_iIntermission )
		return;

	// Recalculate visible menus
	UpdateCommandMenu();
	HideVGUIMenu();

	SetCurrentCommandMenu( m_pCommandMenus[0] );
	m_flMenuOpenTime = gHUD.m_flTime;
	UpdateCursorState();

	// get command menu parameters
	for ( int i = 2; i < gEngfuncs.Cmd_Argc(); i++ )
	{
		const char *param = gEngfuncs.Cmd_Argv( i - 1 );
		if ( param )
		{
			if ( m_pCurrentCommandMenu->KeyInput(param[0]) )
			{
				// kill the menu open time, since the key input is final
				HideCommandMenu();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key input of "-commandmenu"
// Input  : 
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputSignalHideCommandMenu()
{
	if (!m_iInitialized)
		return;

	// if they've just tapped the command menu key, leave it open
	if ( (m_flMenuOpenTime + 0.3) > gHUD.m_flTime )
		return;

	HideCommandMenu();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the command menu
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideCommandMenu()
{
	if (!m_iInitialized)
		return;

	if ( m_pCommandMenus[0])
		m_pCommandMenus[0]->ClearButtonsOfArmedState();

	m_flMenuOpenTime = 0;
	SetCurrentCommandMenu( NULL );
	UpdateCursorState();
}

//-----------------------------------------------------------------------------
// Purpose: Bring up the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::ShowScoreBoard( void )
{
	if (m_pScoreBoard)
		{
			m_pScoreBoard->Open();
			UpdateCursorState();
		}
	}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the scoreboard is up
//-----------------------------------------------------------------------------
bool TeamFortressViewport::IsScoreBoardVisible( void )
{
	if (m_pScoreBoard)
		return m_pScoreBoard->isVisible();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Hide the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideScoreBoard( void )
{
	// Prevent removal of scoreboard during intermission
	if ( gHUD.m_iIntermission || !m_pScoreBoard || !m_pScoreBoard->isVisible())
		return;

	m_pScoreBoard->setVisible(false);

		GetClientVoiceMgr()->StopSquelchMode();

		UpdateCursorState();

	for (int i = 0; i < 5; i++)
	{
		if (mouse_oldbuttonstate & (1 << i))
			gEngfuncs.Key_Event(K_MOUSE1 + i, 0);
	}

	mouse_oldbuttonstate = 0;
}


// Set the submenu of the Command Menu
void TeamFortressViewport::SetCurrentCommandMenu( CCommandMenu *pNewMenu )
{
	for (int i = 0; i < m_iNumMenus; i++)
		m_pCommandMenus[i]->setVisible(false);

	m_pCurrentCommandMenu = pNewMenu;

	if (m_pCurrentCommandMenu)
	{
		m_pCurrentCommandMenu->setParent(this);
		m_pCurrentCommandMenu->MakeVisible( NULL );
}
}

void TeamFortressViewport::UpdateCommandMenu()
	{
		m_pCommandMenus[0]->RecalculateVisibles( 0, false );
	m_pCommandMenus[0]->RecalculatePositions( 0 );

}

void COM_FileBase ( const char *in, char *out);

void TeamFortressViewport::UpdateSpectatorMenu()
{
	if (m_pSpectatorPanel)
		m_pSpectatorPanel->Update();
}

//======================================================================
void TeamFortressViewport::CreateScoreBoard()
{
	int xdent = XRES(80), y = YRES(40);

	m_pScoreBoard = new ScorePanel(xdent, y, ScreenWidth - xdent * 2, ScreenHeight - y * 2);
	m_pScoreBoard->setParent(this);
	m_pScoreBoard->setVisible(false);
}

//======================================================================
// Set the VGUI Menu
void TeamFortressViewport::SetCurrentMenu( CMenuPanel *pMenu )
{
	m_pCurrentMenu = pMenu;
	if ( m_pCurrentMenu && !gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		m_pCurrentMenu->Open();
		m_pCurrentMenu->setVisible(true);
	}
}

//================================================================
// Text Window
CMenuPanel* TeamFortressViewport::CreateTextWindow( int iTextToShow )
{
	if (iTextToShow < 0 || iTextToShow > 1)
		return NULL;

	char cTitle[64];
	char *cText = NULL;

	if ( iTextToShow == 1)
	{
		strncpy( cTitle, m_szServerName[0] ? m_szServerName : "Sven Co-op " SVEN_GAME_VERSION_STRING, sizeof(cTitle) - 1);
		cTitle[sizeof(cTitle)-1] = 0;
		cText = m_szMOTD;
	}
	else
	{
		char map[64];
		char paths[2][260];

		strncpy(map, m_sMapName, sizeof(map) - 1);
		map[sizeof(map) - 1] = 0;
		if (!map[0])
			COM_FileBase(gEngfuncs.pfnGetLevelName(), map);
			if (!map[0])
				return NULL;

		_snprintf(paths[0], sizeof(paths[0]), "maps/%s_motd.txt", map);
		_snprintf(paths[1], sizeof(paths[1]), "maps/%s.txt", map);

		for (int i = 0; i < 2; i++)
		{
			char *pFile = (char*)gEngfuncs.COM_LoadFile(paths[i], 5, NULL );

		if (!pFile)
				continue;

			size_t length = strlen(pFile);

			if (length)
			{
				cText = (char *)malloc(length + 1);
				if (cText)
					memcpy(cText, pFile, length + 1);
			}

			gEngfuncs.COM_FreeFile(pFile);
			if (cText)
				break;
		}

		if (!cText)
			return NULL;

		strncpy( cTitle, CHudTextMessage::BufferedLocaliseTextString("#Textwindow_Mission_Briefing"), sizeof(cTitle) - 1);
		cTitle[sizeof(cTitle) -1] = 0;
	}

	if (!m_pTextWindows[iTextToShow])
		m_pTextWindows[iTextToShow] = new CMessageWindowPanel(false, 0, 0, ScreenWidth, ScreenHeight, iTextToShow);

	CMessageWindowPanel *pMOTDPanel = m_pTextWindows[iTextToShow];
	pMOTDPanel->Initialize(cText, cTitle, true);
	pMOTDPanel->setParent( this );

	if (iTextToShow == 0)
		free(cText);

	return pMOTDPanel;
}

//================================================================
// VGUI Menus
void TeamFortressViewport::ShowVGUIMenu( int iMenu )
{
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return;

	// Don't open any menus except the MOTD during intermission
	// MOTD needs to be accepted because it's sent down to the client
	// after map change, before intermission's turned off
	if ( gHUD.m_iIntermission && iMenu != MENU_INTRO )
		return;

	for (CMenuPanel *pMenu = m_pCurrentMenu; pMenu; pMenu = pMenu->GetNextMenu())
		{
			if (pMenu->GetMenuID() == iMenu)
				return;
	}

	CMenuPanel *pNewMenu = NULL;

	switch ( iMenu )
	{
	case MENU_TEAM:
		pNewMenu = ShowTeamMenu();
		break;

	// Map Briefing removed now that it appears in the team menu
	case MENU_MAPBRIEFING:
		pNewMenu = CreateTextWindow(0);
		break;

	case MENU_INTRO:
		pNewMenu = CreateTextWindow(1);
		break;

	case MENU_SWITCH_TO_MAPBRIEFING:
		HideTopMenu();
		iMenu = MENU_MAPBRIEFING;
		pNewMenu = CreateTextWindow(0);
		break;

	case MENU_SWITCH_TO_INTRO:
		HideTopMenu();
		iMenu = MENU_INTRO;
		pNewMenu = CreateTextWindow(1);
		break;
	case MENU_VOTE:
		pNewMenu = ShowVoteMenu();
		break;
	case MENU_MAPVOTE:
		pNewMenu = ShowMapVoteMenu();
		break;
	case MENU_VOTEPOPUP:
		pNewMenu = ShowVotePopup();
		break;
	case MENU_CAMERA:
		pNewMenu = ShowCameraMenu();
		break;
	case MENU_INVENTORY:
		pNewMenu = ShowInventoryMenu();
		break;
	case MENU_MEDIASELECT:
		pNewMenu = ShowMediaSelectMenu();
		break;

	default:
		return;
	}

	if (!pNewMenu)
		return;

	if (iMenu != MENU_MAPBRIEFING && iMenu != MENU_INTRO)
	{
		for (int i = 0; i < 2; i++)
		{
			if (m_pTextWindows[i])
				RemoveMenu(m_pTextWindows[i]);
		}
	}

	HideCommandMenu();

	pNewMenu->SetMenuID( iMenu );
	pNewMenu->SetActive( true );
	pNewMenu->setParent(this);

	// See if another menu is visible, and if so, cache this one for display once the other one's finished
	if (!m_pCurrentMenu || m_pCurrentMenu == pNewMenu)
	{
		m_pCurrentMenu = pNewMenu;
		pNewMenu->Open();
	}
	else
	{
		RemoveMenu(pNewMenu);

		CMenuPanel *temp = m_pCurrentMenu;

		temp->Close();
			m_pCurrentMenu = pNewMenu;
		pNewMenu->SetNextMenu( temp );
		pNewMenu->Open();
	}

	UpdateCursorState();
		}

CMenuPanel* TeamFortressViewport::ShowTeamMenu()
{
	return NULL;
}

CMenuPanel *TeamFortressViewport::ShowVoteMenu()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback() || !m_pVotePanel)
		return NULL;

	m_pVotePanel->setVisible(true);
	return m_pVotePanel;
}

CMenuPanel *TeamFortressViewport::ShowMapVoteMenu()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback() || !m_pMapVotePanel)
		return NULL;

	m_pMapVotePanel->setVisible(true);
	return m_pMapVotePanel;
}

CMenuPanel *TeamFortressViewport::ShowVotePopup()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback() || !m_pVotePopup)
		return NULL;

	m_pVotePopup->setVisible(true);
	return m_pVotePopup;
}

CMenuPanel *TeamFortressViewport::ShowCameraMenu()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback() || !m_pCameraPanel)
		return NULL;

	m_pCameraPanel->setVisible(true);
	return m_pCameraPanel;
}

CMenuPanel *TeamFortressViewport::ShowInventoryMenu()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback() || !m_pInventory)
		return NULL;

	m_pInventory->setVisible(true);
	return m_pInventory;
}

CMenuPanel *TeamFortressViewport::ShowMediaSelectMenu()
{
	if (gEngfuncs.pDemoAPI->IsPlayingback())
		return NULL;

	CMenuPanel *pMenu = (CMenuPanel *)m_pMediaSelect;

	pMenu->Reset();
	return pMenu;
}

void TeamFortressViewport::SetEndOfTime( float flTime )
{
	gHUD.m_flTimeEnd = flTime;
}

// Removes all VGUI Menu's onscreen
void TeamFortressViewport::HideVGUIMenu()
{
	while (m_pCurrentMenu)
	{
		CMenuPanel *pMenu = m_pCurrentMenu;

		pMenu->Close();
		SetCurrentMenu(pMenu->GetNextMenu());
		pMenu->m_pNextMenu = NULL;
		UpdateCursorState();
	}
}

// Remove the top VGUI menu, and bring up the next one
void TeamFortressViewport::HideTopMenu()
{
	if (m_pCurrentMenu)
	{
		CMenuPanel *pMenu = m_pCurrentMenu;

		pMenu->Close();
		SetCurrentMenu(pMenu->GetNextMenu() );
		pMenu->m_pNextMenu = NULL;
	}

	UpdateCursorState();
}

void TeamFortressViewport::RemoveMenu( CMenuPanel *pMenu )
{
	if (!pMenu)
		return;

	CMenuPanel *pPrevious = NULL;

	for (CMenuPanel *pCurrent = m_pCurrentMenu; pCurrent; pCurrent = pCurrent->GetNextMenu())
	{
		if (pCurrent == pMenu)
		{
			if (pPrevious)
				pPrevious->SetNextMenu(pMenu->GetNextMenu());
			else
			{
				SetCurrentMenu(pMenu->GetNextMenu());
				pMenu->m_pNextMenu = NULL;
			}

			pMenu->Close();
			break;
		}

		pPrevious = pCurrent;
	}

	UpdateCursorState();
}

// Return TRUE if the HUD's allowed to print text messages
bool TeamFortressViewport::AllowedToPrintText()
{
	// Prevent text messages when fullscreen menus are up
	if (!m_pCurrentMenu)
		return true;

	int iId = m_pCurrentMenu->GetMenuID();

	return iId != MENU_TEAM && iId != MENU_INTRO;
}

//======================================================================================
//======================================================================================
// SPECTATOR MENU
//======================================================================================
// Spectator "Menu" explaining the Spectator buttons
void TeamFortressViewport::CreateSpectatorMenu()
{
	// Create the Panel
	m_pSpectatorPanel = new SpectatorPanel(0, 0, ScreenWidth, ScreenHeight);
	m_pSpectatorPanel->setParent(this);
	m_pSpectatorPanel->setVisible(false);
	m_pSpectatorPanel->Initialize();
}

void TeamFortressViewport::CreateTeamMenu()
{
}

void TeamFortressViewport::CreateCameraMenu()
{
	m_pCameraPanel = new CCameraPanel(255, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pCameraPanel->setParent(this);
	m_pCameraPanel->setVisible(false);
}

void TeamFortressViewport::CreateMediaSelectMenu()
{
	m_pMediaSelect = new CMediaSelect(100, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pMediaSelect->setParent(this);
	m_pMediaSelect->setVisible(false);
}

void TeamFortressViewport::CreateVoteMenu()
{
	m_pVotePanel = new CVotePanel(210, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pVotePanel->setParent(this);
	m_pVotePanel->setVisible(false);
	m_pMapVotePanel = new CMapVotePanel(210, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pMapVotePanel->setParent(this);
	m_pMapVotePanel->setVisible(false);
	m_pVotePopup = new CVotePopup(235, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pVotePopup->setParent(this);
	m_pVotePopup->setVisible(false);
}

void TeamFortressViewport::CreateInventoryMenu()
{
	m_pInventory = new CInventoryPanel(210, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pInventory->setParent(this);
	m_pInventory->setVisible(false);
}

//======================================================================================
// UPDATE HUD SECTIONS
//======================================================================================
// We've got an update on player info
// Recalculate any menus that use it.
void TeamFortressViewport::UpdateOnPlayerInfo()
{
	if (m_pScoreBoard)
		m_pScoreBoard->Update();
}

void TeamFortressViewport::UpdateCursorState()
{
	// Need cursor if any VGUI window is up
	if (m_pCurrentMenu == m_pCameraPanel && m_pCameraPanel && m_pCameraPanel->isVisible())
	{
		g_iVisibleMouse = 1;

		Scheme::SchemeCursor cursor;

		if ( m_pCameraPanel->HasOverrideCursor() )
			cursor = Scheme::scu_none;
		else
			cursor = Scheme::scu_arrow;

		App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(cursor));
		return;
	}

	if (m_pCurrentMenu
		|| (m_pMediaSelect && ((CMenuPanel *)m_pMediaSelect)->isVisible())
		|| (m_pVotePanel && m_pVotePanel->isVisible()) || GetClientVoiceMgr()->IsInSquelchMode() )
	{
		g_iVisibleMouse = 1;
		App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow) );
		return;
	}

	if ( m_pCurrentCommandMenu )
	{
		g_iVisibleMouse = gHUD.m_pCvarStealMouse->value != 0;
			App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow) );
			return;
		}

	if ( !gEngfuncs.pDemoAPI->IsPlayingback() )
		IN_ResetMouse();

	g_iVisibleMouse = 0;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor(Scheme::scu_none) );
}

void TeamFortressViewport::UpdateHighlights()
{
	if (m_pCurrentCommandMenu)
		m_pCurrentCommandMenu->MakeVisible( NULL );
}

void TeamFortressViewport::GetAllPlayersInfo( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		gEngfuncs.pfnGetPlayerInfo( i, &g_PlayerInfoList[i] );

		if ( g_PlayerInfoList[i].thisplayer )
			m_pScoreBoard->m_iPlayerNum = i;  // !!!HACK: this should be initialized elsewhere... maybe gotten from the engine
	}
}

void TeamFortressViewport::paintBackground()
{
	if (m_pScoreBoard)
	{
		int x, y;
		getApp()->getCursorPos(x, y);
		m_pScoreBoard->cursorMoved(x, y, m_pScoreBoard);
	}

	// See if the command menu is visible and needs recalculating due to some external change
	if (m_iCurrentTeamNumber != g_iTeamNumber)
	{
		UpdateCommandMenu();
		m_iCurrentTeamNumber = g_iTeamNumber;
	}

	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	UpdateSpectatorMenu();

	if ( m_pScoreBoard->isVisible() && gHUD.m_flTime > m_flScoreBoardLastUpdated)
	{
		m_pScoreBoard->Update();
		m_flScoreBoardLastUpdated = gHUD.m_flTime + 0.5;
	}

	int extents[4];
	getAbsExtents(extents[0],extents[1],extents[2],extents[3]);
	VGui_ViewportPaintBackground(extents);
}

//================================================================
// Input Handler for Drag N Drop panels
void CDragNDropHandler::cursorMoved(int x,int y,Panel* panel)
{
	if(m_bDragging)
	{
		App::getInstance()->getCursorPos(x,y);			
		m_pPanel->setPos(m_iaDragOrgPos[0]+(x-m_iaDragStart[0]),m_iaDragOrgPos[1]+(y-m_iaDragStart[1]));
		
		if(m_pPanel->getParent()!=null)
		{			
			m_pPanel->getParent()->repaint();
		}
	}
}

void CDragNDropHandler::mousePressed(MouseCode code,Panel* panel)
{
	int x,y;
	App::getInstance()->getCursorPos(x,y);
	m_bDragging=true;
	m_iaDragStart[0]=x;
	m_iaDragStart[1]=y;
	m_pPanel->getPos(m_iaDragOrgPos[0],m_iaDragOrgPos[1]);
	App::getInstance()->setMouseCapture(panel);

	m_pPanel->setDragged(m_bDragging);
	m_pPanel->requestFocus();
} 

void CDragNDropHandler::mouseReleased(MouseCode code,Panel* panel)
{
	m_bDragging=false;
	m_pPanel->setDragged(m_bDragging);
	App::getInstance()->setMouseCapture(null);
}

//================================================================
// Number Key Input
bool TeamFortressViewport::SlotInput( int iSlot )
{
	// If there's a menu up, give it the input
	if ( m_pCurrentMenu )
		return m_pCurrentMenu->SlotInput( iSlot );

	return FALSE;
}

// Direct Key Input
int	TeamFortressViewport::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	// Enter gets out of Spectator Mode by bringing up the Team Menu
	if (m_pCurrentMenu && !gEngfuncs.Con_IsVisible())
	{
		int iMenuID = m_pCurrentMenu->GetMenuID();

		// Get number keys as Input for Team/Class menus
		if (iMenuID == MENU_TEAM)
		{
			// Escape gets you out of Team/Class menus if the Cancel button is visible
			if ( keynum == K_ESCAPE && g_iTeamNumber)
				{
					HideTopMenu();
					return 0;
				}

			if ( down && keynum >= '0' && keynum <= '9')
			{
				m_pCurrentMenu->SlotInput(keynum - '0' );
					return 0;
				}
			}

		if (iMenuID == MENU_CAMERA)
		{
			m_pCameraPanel->KeyInput(down, keynum, pszCurrentBinding);

			if (keynum == K_MWHEELUP || keynum == K_MWHEELDOWN)
				return 0;
		}

		// Grab enter keys to close TextWindows
		if ( down && (keynum == K_ENTER || keynum == K_KP_ENTER || keynum == K_SPACE || keynum == K_ESCAPE)
			&& ( iMenuID == MENU_MAPBRIEFING || iMenuID == MENU_INTRO))
			{
				HideTopMenu();
				return 0;
			}

		if (down && pszCurrentBinding && !strcmp(pszCurrentBinding, "+jump") && iMenuID == MENU_TEAM)
			return 0;
			}

	if (!down)
		return 1;

	if (m_pCurrentCommandMenu )
	{
		// Escape hides the command menu
		if ( keynum == K_ESCAPE )
		{
			HideCommandMenu();
			return 0;
		}

		// only trap the number keys
		if ( keynum >= '0' && keynum <= '9' )
		{
			if ( m_pCurrentCommandMenu->KeyInput(keynum) )
				HideCommandMenu();

			return 0;
		}
	}

	return 1;
}

//================================================================
// Message Handlers
int TeamFortressViewport::MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = READ_SHORT();

	// Force the menu to update
	UpdateCommandMenu();

	return 1;
}

int TeamFortressViewport::MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int count = READ_BYTE();
	m_iNumberOfTeams = V_min(count, 5);

	for (int teamNum = 0; teamNum < count; teamNum++)
	{
		const char *name = READ_STRING();

		if (teamNum < m_iNumberOfTeams)
		{
			char *teamName = GetTeamName(teamNum + 1);

		gHUD.m_TextMessage.LocaliseTextString(name, teamName, MAX_TEAMNAME_SIZE );
			teamName[MAX_TEAMNAME_SIZE - 1] = 0;

		// Set the team name buttons
		if (m_pTeamButtons[teamNum])
			m_pTeamButtons[teamNum]->setText(teamName);
		}

		float color[3];

		color[0] = READ_COORD();
		color[1] = READ_COORD();
		color[2] = READ_COORD();

		// range check this value...m_pDisguiseButtons[5];
		if ( teamNum < m_iNumberOfTeams)
		{
			m_iTeamColors[teamNum][0] = (int)color[0];
			m_iTeamColors[teamNum][1] = (int)color[1];
			m_iTeamColors[teamNum][2] = (int)color[2];
		}
	}

	return 1;
}

int TeamFortressViewport::MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iMenu = READ_BYTE();

	// Map briefing includes the name of the map (because it's sent down before the client knows what map it is)
	if (iMenu == MENU_MAPBRIEFING)
	{
		strncpy( m_sMapName, READ_STRING(), sizeof(m_sMapName) );
		m_sMapName[ sizeof(m_sMapName) - 1 ] = '\0';
	}

	// Bring up the menu6
	ShowVGUIMenu( iMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
	if (m_iGotAllMOTD)
		m_szMOTD[0] = 0;

	BEGIN_READ( pbuf, iSize );

	m_iGotAllMOTD = READ_BYTE() != 0;

	const char *text = READ_STRING();

	strncat(m_szMOTD, text, sizeof(m_szMOTD) - strlen(m_szMOTD) - 1);
	m_szMOTD[ sizeof(m_szMOTD)-1 ] = 0;

	// don't show MOTD for HLTV spectators
	if (!m_szMOTD[0])
	{
		strcpy(m_szMOTD, "No server MOTD available.");
		ShowVGUIMenu(MENU_MAPBRIEFING);
	}
	else if (m_iGotAllMOTD)
	{
		ShowVGUIMenu( MENU_INTRO );
	}

	return 1;
}

int TeamFortressViewport::MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szServerName, READ_STRING(), sizeof(m_szServerName) - 1);
	m_szServerName[sizeof(m_szServerName) - 1] = 0;
	strcpy(m_szServerHostname, m_szServerName);

	if (!m_szServerName[0] || !stricmp(m_szServerName, "Half-Life") || !stricmp(m_szServerName, "Sven Co-op"))
		sprintf(m_szServerName, "Sven Co-op " SVEN_GAME_VERSION_STRING);

	return 1;
}

int TeamFortressViewport::MsgFunc_NextMap( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ(pbuf, iSize);
	strncpy(m_sNextMapName, READ_STRING(), sizeof(m_sNextMapName) - 1);
	m_sNextMapName[sizeof(m_sNextMapName) - 1] = 0;
	return 1;
}

int TeamFortressViewport::MsgFunc_VoteMenu(const char *pszName, int iSize, void *pbuf)
{
	if (!m_pVotePopup)
		return 0;

	int result = m_pVotePopup->MsgFunc_VoteMenu(pszName, iSize, pbuf);

	ShowVGUIMenu(MENU_VOTEPOPUP);
	return result;
}

int TeamFortressViewport::MsgFuncs_EndVote(const char *pszName, int iSize, void *pbuf)
{
	if (m_pVotePopup)
		RemoveMenu(m_pVotePopup);

	return 1;
}

int TeamFortressViewport::MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int cl = READ_BYTE();
	float frags = READ_FLOAT();
	int deaths = READ_LONG();
	float health = READ_FLOAT();
	float armor = READ_FLOAT();
	short playerclass = READ_BYTE();
	bool showextras = READ_BYTE() > 0;
	short adminlevel = READ_BYTE();
	short teamnumber = 0;

	if (playerclass >= 16 && playerclass <= 19)
		teamnumber = playerclass - 15;

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		extra_player_info_t &info = g_PlayerExtraInfo[cl];

		info.frags = frags;
		info.deaths = deaths;
		info.health = health;
		info.armor = armor;
		info.playerclass = playerclass;
		info.teamnumber = teamnumber;
		info.showextras = showextras;
		info.adminlevel = adminlevel;

		if (m_pScoreBoard)
			m_pScoreBoard->Update();
	}

	return 1;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths 
// if this message is never received, then scores will simply be the combined totals of the players.
int TeamFortressViewport::MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	char *TeamName = READ_STRING();

	// find the team matching the name
	int i;
	for ( i = 1; i <= m_pScoreBoard->m_iNumTeams; i++ )
	{
		if ( !stricmp( TeamName, g_TeamInfo[i].name ) )
			break;
	}

	if ( i > m_pScoreBoard->m_iNumTeams )
		return 1;

	// use this new score data instead of combined player scoresw
	g_TeamInfo[i].scores_overriden = TRUE;
	g_TeamInfo[i].frags = READ_SHORT();
	g_TeamInfo[i].deaths = READ_SHORT();

	return 1;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
int TeamFortressViewport::MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf )
{
	if (!m_pScoreBoard)
		return 1;

	BEGIN_READ( pbuf, iSize );
	short cl = READ_BYTE();
	
	if ( cl > 0 && cl <= MAX_PLAYERS )
	{  
		// set the players team
		strncpy( g_PlayerExtraInfo[cl].teamname, READ_STRING(), MAX_TEAM_NAME - 1);
		g_PlayerExtraInfo[cl].teamname[MAX_TEAM_NAME - 1] = 0;
	}

	// rebuild the list of teams
	m_pScoreBoard->RebuildTeams();

	return 1;
}

void TeamFortressViewport::DeathMsg( int killer, int victim )
{
	m_pScoreBoard->DeathMsg(killer,victim);
}

int TeamFortressViewport::MsgFunc_Spectator( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	short cl = READ_BYTE();
	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_IsSpectator[cl] = READ_BYTE() != 0;
	}

	return 1;
}

int TeamFortressViewport::MsgFunc_AllowSpec( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iAllowSpectators = READ_BYTE() != 0;
	UpdateCommandMenu();

	return 1;
}

#if defined( _TFC )
const Vector& GetTeamColor( int team_no );
extern globalvars_t *gpGlobals;
#endif 


// used to fade a player's screen out/in when they're spectating someone who is teleported

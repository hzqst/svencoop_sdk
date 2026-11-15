/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud.cpp
//
// implementation of CHud class
//

#include "hud.h"
#include "cl_util.h"
#include "SvenVersion.h"
#include <string.h>

extern int __MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );
#include <stdio.h>
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

#include "demo.h"
#include "demo_api.h"
#include "vgui_ScorePanel.h"
#include "clientsprite.h"
#include "client_sporetrail.h"
#include "client_effects.h"
#include "vgui_Inventory.h"
#include "client_messagebox.h"
#include "vgui_CameraPanel.h"
#include "portal_manager.h"
#include "client_sound.h"
#include "client_surface.h"
#include "client_media.h"
#include "vgui_MediaSelect.h"
#include "vgui_VotePanel.h"
#include "client_electro.h"
#include "Discord.h"
#include "client_weather.h"
#include "te_custom.h"
#include "IClient.h"
#include "pm_defs.h"
#include "event_api.h"
#include "r_efx.h"

hud_player_info_t	 g_PlayerInfoList[MAX_PLAYERS+1];	   // player info from the engine
extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];   // additional player info sent directly to the client dll

#define VOICE_ACK_ICON_LINE	3

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 255;

		if( entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo)/sizeof(g_PlayerExtraInfo[0]) )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	virtual void UpdateCursorState()
	{
		gViewPort->UpdateCursorState();
	}

	// Above the third HUD line from the bottom, inside the right border
	virtual int	GetAckIconYOffset()
	{
		return gHUD.GetLineY( VOICE_ACK_ICON_LINE ) - gHUD.GetLineSeparatorHeight();
	}

	virtual int	GetAckIconXOffset()
	{
		return ScreenWidth - gHUD.GetBorderSize();
	}

	virtual bool			CanShowSpeakerLabels()
	{
		if( gViewPort && gViewPort->m_pScoreBoard )
			return !gViewPort->m_pScoreBoard->isVisible();
		else
			return false;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iCount);

extern cvar_t *sensitivity;
cvar_t *cl_lw = NULL;
cvar_t *cl_rollangle;
cvar_t *cl_rollspeed;

extern int __MsgFunc_Flamethwr(const char *pszName, int iSize, void *pbuf);
extern int __MsgFunc_OnTank(const char *pszName, int iSize, void *pbuf);
extern int __MsgFunc_TracerDecal(const char *pszName, int iSize, void *pbuf);
extern int EV_Message_Gib(const char *pszName, int iSize, void *pbuf);

void ShutdownInput (void);

int __MsgFunc_Gib(const char *pszName, int iSize, void *pbuf)
{
	return EV_Message_Gib(pszName, iSize, pbuf);
}

int __MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf );
}

int __MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_InitHUD( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_CdAudio(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_CdAudio(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ViewMode( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetFOV( pszName, iSize, pbuf );
}

int __MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Concuss( pszName, iSize, pbuf );
}

int __MsgFunc_VModelPos(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_VModelPos(pszName, iSize, pbuf);
}

int __MsgFunc_ClassicMode(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ClassicMode(pszName, iSize, pbuf);
}

int __MsgFunc_TE_CUSTOM(const char *pszName, int iSize, void *pbuf)
{
	TE_CUSTOM_Message(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_Fog(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Fog(pszName, iSize, pbuf);
}

int __MsgFunc_ClServerInfo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ClServerInfo(pszName, iSize, pbuf);
}

int __MsgFunc_ClExtrasInfo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ClExtrasInfo(pszName, iSize, pbuf);
}

int __MsgFunc_ServerVer(const char *pszName, int iSize, void *pbuf)
{
	char version[33] = {};
	char clientVersion[33] = SVEN_GAME_VERSION_STRING;
	char serverVersion[33] = {};

	BEGIN_READ(pbuf, iSize);
	strncpy(version, READ_STRING(), sizeof(version) - 1);
	strcpy(serverVersion, version);

	char *suffix = strchr(clientVersion, ' ');

	if (suffix)
		*suffix = 0;

	suffix = strchr(serverVersion, ' ');
	if (suffix)
		*suffix = 0;

	if (stricmp(clientVersion, serverVersion))
	{
		gEngfuncs.pfnClientCmd("disconnect");

		char message[257] = {};

		snprintf(message, sizeof(message),
			"Server is running a different version to you.\t\tClient: %s\t\tServer: %s\n"
			"Please join a server running the same version as you, or visit www.svencoop.com to check for updates.\n",
			clientVersion, serverVersion);

		if (CVAR_GET_FLOAT("developer") >= 1)
			gEngfuncs.Con_DPrintf("%s", message);

		message[strlen(message) - 1] = 0;
		UTIL_MessageBox(message, NULL, MSGBOX_ICONERROR, MSGBOX_OK);
	}

	strcpy(gHUD.m_szServerVersion, version);
	return 0;
}

int __MsgFunc_ServerBuild(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	strncpy(gHUD.m_szServerBuild, READ_STRING(), sizeof(gHUD.m_szServerBuild) - 1);
	gHUD.m_szServerBuild[sizeof(gHUD.m_szServerBuild) - 1] = 0;
	return 0;
}

int __MsgFunc_NextMap(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_NextMap(pszName, iSize, pbuf);

	return 0;
}

int __MsgFunc_MapList(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort && gViewPort->m_pMapVotePanel)
		gViewPort->m_pMapVotePanel->MsgFunc_MapList(pszName, iSize, pbuf);

	return 1;
}

int __MsgFunc_VoteMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		gViewPort->MsgFunc_VoteMenu(pszName, iSize, pbuf);

	return 1;
}

int __MsgFunc_EndVote(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		gViewPort->MsgFuncs_EndVote(pszName, iSize, pbuf);

	return 1;
}

void __CmdFunc_VoteMenu(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_VOTE);
}

void __CmdFunc_VoteMap(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_MAPVOTE);
}

void __CmdFunc_CloseMenus(void)
{
	if (gViewPort)
		gViewPort->HideVGUIMenu();
}

int __MsgFunc_PrtlUpdt(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_PrtlUpdt(pszName, iSize, pbuf);
	return 1;
}

void CHud::MsgFunc_PrtlUpdt(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int entityIndex = READ_LONG();

	if (!READ_BYTE())
	{
		P_DisablePortal(entityIndex);
		return;
	}

	P_EnablePortal(entityIndex);

	Vector origin, angles;
	Vector sourceOrigin(0, 0, 0), sourceAngles(0, 0, 0);
	int sourceEntityIndex = 0;
	unsigned int width = 512;
	unsigned int height = 512;
	bool noPlanes = false;
	bool shootThrough = true;

	for (int i = 0; i < 3; i++)
		origin[i] = READ_LONG() / 8.0f;

	for (int i = 0; i < 3; i++)
		angles[i] = READ_LONG() / 8.0f;

	unsigned char mode = READ_BYTE();
	unsigned char textureMode = READ_BYTE();
	float zoom = READ_FLOAT();
	unsigned char fps = READ_BYTE();
	int minDistance = READ_LONG();
	int maxDistance = READ_LONG();
	bool halfFrame = READ_BYTE() != 0;

	if (mode)
	{
		if (READ_BYTE())
		{
			sourceEntityIndex = READ_LONG();

			for (int i = 0; i < 3; i++)
				sourceOrigin[i] = READ_LONG() / 8.0f;

			for (int i = 0; i < 3; i++)
				sourceAngles[i] = READ_CHAR() * (360.0f / 256);
		}

		if (mode == 1)
		{
			if (READ_BYTE())
			{
				width = READ_LONG();
				height = READ_LONG();
			}
		}
		else if (mode == 2)
		{
			noPlanes = READ_BYTE() != 0;
			shootThrough = READ_BYTE() != 0;
		}
	}

	char textureName[33] = {};

	if (textureMode == 2)
		UTIL_CopyString(textureName, READ_STRING(), sizeof(textureName));

	P_UpdatePortal(entityIndex, sourceEntityIndex, origin, angles, mode, textureMode, textureName,
		sourceOrigin, sourceAngles, width, height, zoom, fps, halfFrame, noPlanes, shootThrough,
		minDistance, maxDistance);
}

int __MsgFunc_Playlist(const char *pszName, int iSize, void *pbuf)
{
	MediaPlayer.PlayList(pszName, iSize, pbuf);
	return 0;
}

void __CmdFunc_MediaPlayer(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_MEDIASELECT);
}

void __CmdFunc_PlayMedia(void)
{
	MediaPlayer.PerformAction(MEDIA_PLAY);
}

void __CmdFunc_MediaHud(void)
{
	MediaPlayer.ToggleHUD();
}

void __CmdFunc_Time(void)
{
	MediaPlayer.ToggleTimeDisplayMode();
}

void __CmdFunc_NextSong(void)
{
	MediaPlayer.PerformAction(MEDIA_NEXT);
}

void __CmdFunc_StopSong(void)
{
	MediaPlayer.PerformAction(MEDIA_STOP);
}

void __CmdFunc_RandomSong(void)
{
	MediaPlayer.ToggleRandomMode();
}

void __CmdFunc_LastSong(void)
{
	MediaPlayer.PerformAction(MEDIA_PREVIOUS);
}

void __CmdFunc_PauseSong(void)
{
	MediaPlayer.PerformAction(MEDIA_PAUSE);
}

void __CmdFunc_IncVol(void)
{
	MediaPlayer.ChangeVolume(true);
}

void __CmdFunc_DecVol(void)
{
	MediaPlayer.ChangeVolume(false);
}

void __CmdFunc_RelPls(void)
{
	gViewPort->m_pMediaSelect->Rebuild();
}

void __CmdFunc_RandomFog(void)
{
}

int g_fAllowGilded = 1;

void __CmdFunc_ToggleGilded(void)
{
	g_fAllowGilded = !g_fAllowGilded;

	if (g_fAllowGilded)
		gEngfuncs.Con_Printf("Gilded skin allowed.\n");
	else
		gEngfuncs.Con_Printf("Gilded skin removed.\n");
}

void __CmdFunc_npc_moveto(void)
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	if (!player)
		return;

	Vector forward(0, 0, 0);
	Vector right(0, 0, 0);
	Vector up(0, 0, 0);
	Vector angles(0, 0, 0);

	gEngfuncs.GetViewAngles(angles);
	AngleVectors(angles, forward, right, up);

	Vector end = forward * 2048 + player->origin;
	Vector viewOfs(0, 0, 0);

	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOfs);

	Vector start = viewOfs + player->origin;
	pmtrace_t trace = *gEngfuncs.PM_TraceLine(start, end, PM_TRACELINE_PHYSENTSONLY, 2, -1);

	if (trace.fraction != 1.0)
		gEngfuncs.pEfxAPI->R_SparkShower(trace.endpos);

	gEngfuncs.pfnServerCmdUnreliable("npc_moveto");
}

int __MsgFunc_CameraMouse(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int mode = READ_BYTE();

	if (!gViewPort)
		return 0;

	const char *cursor = NULL;

	if (!mode)
	{
		if (gViewPort->m_pCurrentMenu == gViewPort->m_pCameraPanel)
			gViewPort->HideVGUIMenu();
	}
	else
	{
		gViewPort->ShowVGUIMenu(MENU_CAMERA);

		if (mode == 2)
			cursor = READ_STRING();
	}

	if (gViewPort->m_pCameraPanel)
	{
		gViewPort->m_pCameraPanel->SetOverrideCursor(cursor);
		gViewPort->UpdateCursorState();
	}

	return 0;
}

int __MsgFunc_WeatherFX(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iWeatherID = READ_SHORT();
	Vector vecMins, vecMaxs, vecAngles;

	vecMins.x = READ_COORD();
	vecMins.y = READ_COORD();
	vecMins.z = READ_COORD();
	vecMaxs.x = READ_COORD();
	vecMaxs.y = READ_COORD();
	vecMaxs.z = READ_COORD();
	vecAngles.x = READ_ANGLE();
	vecAngles.y = READ_ANGLE();
	vecAngles.z = READ_ANGLE();

	int iModel = READ_SHORT();
	float flScale = READ_FLOAT();
	int iVPType = READ_BYTE();
	int iSplashModel = READ_SHORT();
	float flSplashScale = READ_FLOAT();
	int iSplashVPType = READ_BYTE();
	int iRenderMode = READ_BYTE();
	float flRenderAmt = READ_FLOAT();
	Vector vecRenderColor;

	vecRenderColor.x = READ_BYTE();
	vecRenderColor.y = READ_BYTE();
	vecRenderColor.z = READ_BYTE();

	int iRenderFx = READ_BYTE();
	float flFrequency = READ_FLOAT();
	float flSpeed = READ_FLOAT();
	float flAngleVariance = READ_FLOAT();
	float flSpeedVariance = READ_FLOAT();

	TE_CreateWeatherEffect(iWeatherID, vecMins, vecMaxs, vecAngles, iModel, flScale, iVPType,
		iSplashModel, flSplashScale, iSplashVPType, iRenderMode, flRenderAmt, vecRenderColor,
		iRenderFx, flFrequency, flSpeed, flAngleVariance, flSpeedVariance);
	return 0;
}

int __MsgFunc_ChangeSky(const char *pszName, int iSize, void *pbuf)
{
	char szSkyName[32];

	memset(szSkyName, 0, sizeof(szSkyName));

	BEGIN_READ(pbuf, iSize);
	UTIL_CopyString(szSkyName, READ_STRING(), sizeof(szSkyName));

	if (strlen(szSkyName))
		g_pEngineClient->ChangeSkymap(szSkyName);

	Vector vecColor(-1, -1, -1);

	READ_VECTOR(vecColor);

	if (vecColor.x >= 0 && vecColor.y >= 0 && vecColor.z >= 0)
		g_pEngineClient->ChangeSkycolor(vecColor.x, vecColor.y, vecColor.z);

	return 0;
}

int __MsgFunc_TimeEnd(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort && gViewPort->GetScoreBoard())
	{
		BEGIN_READ(pbuf, iSize);
		gViewPort->SetEndOfTime(READ_LONG() + gHUD.m_flTime);
	}

	return 1;
}

void __CmdFunc_MOTD(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_INTRO);
}

void __CmdFunc_MapBriefing(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_MAPBRIEFING);
}

void __CmdFunc_TeamMenu(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_TEAM);
}

int __MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GameMode( pszName, iSize, pbuf );
}

// TFFree Command Menu
void __CmdFunc_OpenCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->ShowCommandMenu();
	}
}

void __CmdFunc_CloseCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

void __CmdFunc_ForceCloseCommandMenu( void )
{
	if ( gViewPort )
	{
		gViewPort->HideCommandMenu();
	}
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ValClass( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamNames( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_MOTD( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	Discord::GetDiscord()->SetServername(pszName, iSize, pbuf);

	if (gViewPort)
		return gViewPort->MsgFunc_ServerName( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Spectator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_AllowSpec( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_InvAdd(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort && gViewPort->GetInventoryPanel())
		gViewPort->GetInventoryPanel()->MsgFunc_InvAdd(pszName, iSize, pbuf);

	return 1;
}

int __MsgFunc_InvRemove(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort && gViewPort->GetInventoryPanel())
		gViewPort->GetInventoryPanel()->MsgFunc_InvRemove(pszName, iSize, pbuf);

	return 1;
}

void __CmdFunc_InvMenu(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_INVENTORY);
}

void __CmdFunc_InvMenuShow(void)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(MENU_INVENTORY);
}

void __CmdFunc_InvMenuHide(void)
{
	if (gViewPort)
		gViewPort->HideVGUIMenu();
}

// This is called every time the DLL is loaded
void CHud :: Init( void )
{
	CVAR_CREATE("cl_outline_sounds", "0", FCVAR_CLIENTDLL);
	CVAR_CREATE("cl_spectrum_debug", "0", FCVAR_CLIENTDLL);
#ifdef _WIN32
	CVAR_CREATE("s_force_dsound", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
#endif
	CVAR_CREATE("s_mute_unfocused", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	CClient_SoundEngine::GetSoundEngine()->Initialize();

	HOOK_MESSAGE(PrtlUpdt);
	HOOK_MESSAGE(Playlist);
	HOOK_MESSAGE(StartSound);
	HOOK_MESSAGE(Speaksent);
	HOOK_MESSAGE(CbElec);
	HOOK_MESSAGE(ShkFlash);
	HOOK_MESSAGE(TE_CUSTOM);
	HOOK_MESSAGE(MapList);
	HOOK_MESSAGE(VoteMenu);
	HOOK_MESSAGE(EndVote);
	HOOK_COMMAND("votemenu", VoteMenu);
	HOOK_COMMAND("vgui_votemap", VoteMap);
	HOOK_COMMAND("PauseSong", PauseSong);
	HOOK_COMMAND("MEDIAHUD", MediaHud);
	HOOK_COMMAND("MediaTime", Time);
	HOOK_COMMAND("NextSong", NextSong);
	HOOK_COMMAND("LastSong", LastSong);
	HOOK_COMMAND("RandomSong", RandomSong);
	HOOK_COMMAND("StopSong", StopSong);
	HOOK_COMMAND("PlayMedia", PlayMedia);
	HOOK_COMMAND("MediaPlayer", MediaPlayer);
	HOOK_COMMAND("Fog", RandomFog);
	HOOK_COMMAND("IncVol", IncVol);
	HOOK_COMMAND("DecVol", DecVol);
	HOOK_COMMAND("RelPls", RelPls);
	HOOK_COMMAND("vgui_closeall", CloseMenus);
	HOOK_COMMAND("uzi_gilded", ToggleGilded);
	HOOK_COMMAND("servermotd", MOTD);
	HOOK_COMMAND("missionbriefing", MapBriefing);
	HOOK_COMMAND("npc_moveto", npc_moveto);
	HOOK_COMMAND("inventory", InvMenu);
	HOOK_COMMAND("+inventory", InvMenuShow);
	HOOK_COMMAND("-inventory", InvMenuHide);
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE(CdAudio);
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( VModelPos );
	HOOK_MESSAGE( ClassicMode );
	HOOK_MESSAGE( OnTank );
	HOOK_MESSAGE(RampSprite);
	HOOK_MESSAGE(ShieldRic);
	HOOK_MESSAGE(ToxicCloud);
	HOOK_MESSAGE(SRDetonate);
	HOOK_MESSAGE(SRPrimed);
	HOOK_MESSAGE(SRPrimedOff);
	HOOK_MESSAGE(CreateBlood);
	HOOK_MESSAGE(InvAdd);
	HOOK_MESSAGE(InvRemove);
	HOOK_MESSAGE(TracerDecal);
	HOOK_MESSAGE(SporeTrail);
	HOOK_MESSAGE(GargSplash);
	HOOK_MESSAGE( Fog );

	// TFFree CommandMenu
	HOOK_COMMAND( "+commandmenu", OpenCommandMenu );
	HOOK_COMMAND( "-commandmenu", CloseCommandMenu );
	HOOK_COMMAND( "ForceCloseCommandMenu", ForceCloseCommandMenu );
	HOOK_COMMAND("teammenu", TeamMenu);

	HOOK_MESSAGE( ValClass );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE(ServerVer);
	HOOK_MESSAGE(ServerBuild);
	HOOK_MESSAGE(NextMap);
	HOOK_MESSAGE(TimeEnd);
	HOOK_MESSAGE(WeatherFX);
	HOOK_MESSAGE(CameraMouse);
	HOOK_MESSAGE(Flamethwr);
	HOOK_MESSAGE(ChangeSky);
	HOOK_MESSAGE(ClServerInfo);
	HOOK_MESSAGE(ClExtrasInfo);
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );

	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );

	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE(DeathMsg);
	HOOK_MESSAGE(Gib);		// controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );		// controls whether or not to automatically take screenshots at the end of a round


	m_iClientIndex = 0;
	m_iClientUserID = 0;
	memset(m_szClientAuthID, 0, sizeof(m_szClientAuthID));
	m_bViewModelPos = false;
	m_vecViewModelPos = Vector(0, 0, 0);
	m_bClassicMode = false;
	m_flTimeEnd = 0;
	m_iFOV = 0;

	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", 0 );
	CVAR_CREATE("cl_gibcount", "10", 0);
	CVAR_CREATE("psys_rendermode", "0", FCVAR_CLIENTDLL);
	CVAR_CREATE("psys_spritename", "0", FCVAR_CLIENTDLL);
	default_fov = CVAR_CREATE( "default_fov", "70", FCVAR_ARCHIVE );
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_CLIENTDLL );
	m_pCvarDebug = CVAR_CREATE( "hud_debug", "0", FCVAR_CLIENTDLL );
	m_pCvarHideCustom = CVAR_CREATE( "hud_hidecustom", "0", FCVAR_CLIENTDLL );
	m_pCvarBorderless = CVAR_CREATE( "r_borderless", "0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
	m_pCvarBorderSize = CVAR_CREATE( "hud_bordersize", "16", FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
	m_pCvarDefaultAlpha = CVAR_CREATE( "hud_alpha_default", "160", FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
	m_pCvarMaxAlpha = CVAR_CREATE( "hud_alpha_max", "255", FCVAR_ARCHIVE | FCVAR_CLIENTDLL );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );
	cl_rollangle = CVAR_CREATE("cl_rollangle", "2.0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
	cl_rollspeed = CVAR_CREATE("cl_rollspeed", "200", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);

	m_pSpriteList = NULL;

	// Clear any old HUD list
	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Health.Init();
	m_Battery.Init();
	m_Ammo.Init();
	m_Train.Init();
	m_Flash.Init();
	m_Crosshair.Init();
	m_TextMessage.Init();
	m_Inventory.Init();
	m_SayText.Init();
	m_NotifyText.Init();
	m_Menu.Init();
	m_Geiger.Init();
	m_Custom.Init();
	m_Message.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (vgui::Panel**)&gViewPort);

	MsgFunc_ResetHUD(0, 0, NULL );
	strcpy(m_szServerVersion, "Unknown");
	strcpy(m_szServerBuild, "Unknown");
	GetCryptKey(0);
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
	Shutdown();
}

void CHud::Shutdown(void)
{
	m_Custom.Shutdown();
	Discord::GetDiscord()->DiscordExit();

	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;
	m_rghSprites = NULL;
	m_rgrcRects = NULL;
	m_rgszSpriteNames = NULL;

	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	for (int i = 0; i < 2; ++i)
	{
		if (m_pCryptKeys[i])
		{
			delete[] m_pCryptKeys[i];
			m_pCryptKeys[i] = NULL;
		}
	}
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ) == 0 )
			return i;
	}

	return -1; // invalid sprite
}

void CHud :: VidInit( void )
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");
	
	m_hsprCursor = 0;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == 640 )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == 640 )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == 640 )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_HUD_minus = GetSpriteIndex( "minus" );
	m_HUD_plus = GetSpriteIndex( "plus" );

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Crosshair.VidInit();
	m_Message.VidInit();
	m_SayText.VidInit();
	m_NotifyText.VidInit();
	m_Menu.VidInit();
	m_TextMessage.VidInit();
	m_Inventory.VidInit();
	m_Custom.VidInit();
	GetClientVoiceMgr()->VidInit();

	if (gViewPort)
		gViewPort->VidInit();

	m_fFinishedNovodex = false;
	strcpy(m_szServerVersion, "Unknown");
	strcpy(m_szServerBuild, "Unknown");
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else 
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else 
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV( void )
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		int i = 0;
		unsigned char buf[ 100 ];

		// Active
		*( float * )&buf[ i ] = g_lastFOV;
		i += sizeof( float );

		Demo_WriteBuffer( TYPE_ZOOM, i, buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT( "default_fov" );

	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = def_fov;
		m_flMouseSensitivity = 0;
		return 1;
	}

	m_iFOV = newfov;

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == def_fov )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = newfov / V_max( def_fov, 0.1 ) * sensitivity->value * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}

// Client side weapons zoom through the same path as the server's SetFOV message
void SetGlobalFOV( int fov )
{
	gHUD.MsgFunc_SetFOV( "SetFOV", 1, &fov );
}


void CHud::AddHudElem(CHudBase *phudelem)
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST *)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity( void )
{
	return m_flMouseSensitivity;
}



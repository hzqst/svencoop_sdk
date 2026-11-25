#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "gamerules.h"
#include "server_support.h"
#include "AdminLevel.h"
#include "ASClientSayParameters.h"
#include "CASServerManager.h"
#include "CASHook.h"
#include "user_messages.h"
#include "client_commands.h"
#include "CServer.h"
#include "ConCommandDefs.h"
#include "donid.h"
#include "game.h"
#include "iengineserver.h"
#include "item_inventory.h"
#include "ModelUtils.h"
#include "nodes.h"
#include "observer.h"
#include "player_hud.h"
#include "schedule.h"
#include "shake.h"
#include "soundent.h"
#include "squadmonster.h"
#include "weapons.h"
#include <ctype.h>

#define COMMAND_TEXT_LENGTH      128
#define MAX_PLAYER_NAME_LENGTH   32
#define CALL_SPRITE_OFFSET       51
#define CALL_SPRITE_LIFETIME     2.5f

void Host_Say( edict_t *sender, int teamOnly, int argumentCount,
               const char **arguments, const char *command )
{
	const char *commandName = arguments[0];
	bool console = !ENTINDEX( sender ) && IS_DEDICATED_SERVER();
	char rawText[256];
	char *message = (char *) command;

	if ( !argumentCount )
		return;

	if ( stricmp( commandName, "say" ) && stricmp( commandName, "say_team" ) )
	{
		if ( argumentCount >= 2 )
			UTIL_FormatString( rawText, sizeof( rawText ), "%s %s", commandName, CMD_ARGS() );
		else
			UTIL_FormatString( rawText, sizeof( rawText ), "%s", commandName );

		message = rawText;
	}
	else if ( argumentCount < 2 )
	{
		return;
	}

	if ( *message == '"' )
	{
		message++;
		message[strlen( message ) - 1] = 0;
	}

	if ( CASServerManager::GetInstance()->IsScriptLifecycleEntered() )
	{
		CBasePlayer *player = (CBasePlayer *) CBaseEntity::Instance( sender );
		CASSayParameters *parameters = new CASSayParameters( player, teamOnly, message );

		g_ClientSayHook.Call( player, 0, parameters );

		bool hide = parameters->ShouldHide();

		parameters->Release();

		if ( hide )
			return;
	}

	char *printable = message;

	while ( printable )
	{
		if ( !*printable )
			return;

		if ( isprint( (unsigned char) *printable ) && !isspace( (unsigned char) *printable ) )
			break;

		printable++;
	}

	char name[MAX_PLAYER_NAME_LENGTH + sizeof( "(TEAM) " )];

	if ( console )
		UTIL_CopyString( name, "Server Console", sizeof( name ) );
	else if ( teamOnly )
		UTIL_FormatString( name, sizeof( name ), "(TEAM) %s", STRING( sender->v.netname ) );
	else
		UTIL_CopyString( name, STRING( sender->v.netname ), MAX_PLAYER_NAME_LENGTH );

	char text[256];

	if ( !strnicmp( message, "/me ", 4 ) )
	{
		message += 3;
		UTIL_FormatString( text, sizeof( text ), "%c* %s", 3, name );
	}
	else if ( !strnicmp( message, "/shrug", 6 ) )
	{
		message += 6;
		UTIL_FormatString( text, sizeof( text ), "%c%s: \xC2\xAF\\_(\xE3\x83\x84)_/\xC2\xAF", 2, name );
	}
	else
	{
		UTIL_FormatString( text, sizeof( text ), "%c%s: ", 2, name );
	}

	int available = sizeof( text ) - 2 - strlen( text ); // -2 for the newline and terminator

	if ( (int) strlen( message ) > available )
		message[available] = 0;

	UTIL_AppendString( text, message, sizeof( text ) );
	UTIL_AppendString( text, "\n", sizeof( text ) );

	CBaseEntity *client = NULL;

	while ( ( client = UTIL_FindEntityByClassname( client, "player" ) ) != NULL )
	{
		if ( !client->pev )
			break;

		edict_t *recipient = ENT( client->pev );

		if ( !recipient || recipient->free || !OFFSET( recipient ) )
			break;

		if ( recipient == sender || !client->IsNetClient() )
			continue;

		if ( teamOnly && g_pGameRules->PlayerRelationship( client, CBaseEntity::Instance( sender ) ) != GR_TEAMMATE )
			continue;

		( *g_engfuncs.pfnMessageBegin )( MSG_ONE, gmsgSayText, NULL, recipient );
		WRITE_BYTE( ENTINDEX( sender ) );
		WRITE_STRING( text );
		MESSAGE_END();
	}

	( *g_engfuncs.pfnMessageBegin )( MSG_ONE, gmsgSayText, NULL, ENT( &sender->v ) );
	WRITE_BYTE( ENTINDEX( sender ) );
	WRITE_STRING( text );
	MESSAGE_END();

	const char *logFormat;

	if ( console )
		logFormat = "\"%s\" console \"%s\"\n";
	else if ( teamOnly )
		logFormat = "\"%s\" say_team \"%s\"\n";
	else
		logFormat = "\"%s\" say \"%s\"\n";

	UTIL_LogPrintf( logFormat, PlayerLog( sender ), message );
	g_engfuncs.pfnServerPrint( text );
}

void Host_Say( edict_t *pEntity, int teamonly )
{
	int argumentCount = CMD_ARGC();

	if ( !argumentCount )
		return;
	const char **arguments = new const char *[argumentCount];

	for ( int index = 0; index < argumentCount; index++ )
		arguments[index] = CMD_ARGV( index );

	Host_Say( pEntity, teamonly, argumentCount, arguments, CMD_ARGS() );
	delete[] arguments;
}

bool PerformDebugCommands( CBasePlayer *player )
{
	char text[COMMAND_TEXT_LENGTH + 1] = { 0 };

	if ( !player || !player->IsPlayer() )
		return false;

	if ( stricmp( CMD_ARGV( 0 ), "show_active_players" ) )
		return false;

	ClientPrint( player->pev, HUD_PRINTCONSOLE, "Active Players are: \n" );

	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
	{
		CBasePlayer *active = UTIL_GetActivePlayer( index );

		if ( !active )
			continue;

		UTIL_FormatString( text, sizeof( text ), "  \"%s\" (%2f)\n",
		                   STRING( active->pev->netname ), active->pev->health );
		ClientPrint( player->pev, HUD_PRINTCONSOLE, text );
	}

	return true;
}

CBaseEntity *ClientCreateEntityByClassname( CBasePlayer *player, const char *className, bool ally, float scale )
{
	if ( !player )
		return NULL;

	if ( !className || !*className )
	{
		ALERT( at_console, "Invalid classname\n" );
		return NULL;
	}

	UTIL_MakeVectors( player->pev->v_angle );

	TraceResult trace;
	Vector end = player->pev->origin + gpGlobals->v_forward * 256.0f;

	trace.vecEndPos = Vector( 0, 0, 0 );
	trace.vecPlaneNormal = Vector( 0, 0, 0 );
	UTIL_TraceLine( player->pev->origin, end, ignore_monsters, player->edict(), &trace );

	edict_t *created = CREATE_NAMED_ENTITY( MAKE_STRING( className ) );
	CBaseEntity *entity = created ? CBaseEntity::Instance( created ) : NULL;

	if ( !entity )
	{
		ALERT( at_console, "Could not create \"%s\"\n", className );
		return NULL;
	}

	entity->pev->origin = trace.vecEndPos;
	entity->pev->angles = Vector( 0, player->pev->angles.y, 0 );
	entity->pev->classname = AllocStringLocal( className );

	if ( !stricmp( className, "rpg_rocket" ) )
		entity->pev->angles.x = -player->pev->v_angle.x;

	if ( ally )
	{
		CBaseMonster *monster = dynamic_cast<CBaseMonster *>( entity );

		if ( monster )
			monster->SetPlayerAlly( true );
	}

	if ( scale != 0.0f )
	{
		float lower = 0.0001f;

		if ( scale >= lower )
			lower = scale;

		float clamped = 10000.0f;

		if ( scale <= clamped )
			clamped = lower;

		entity->pev->scale = clamped;
	}

	ALERT( at_console, "Creates \"%s\"\n", className );
	return entity;
}

#define PLAYER_COMMAND_TRACE_RANGE 8192
#define PLAYER_COMMAND_FOLLOWERS   256
#define PLAYER_COMMAND_WAIT        4.0f
#define MEDIC_CALL_RANGE           512
#define STUCK_SEARCH_LIMIT         48
#define STUCK_SEARCH_TRIES         64
#define NODE_PATH_CAPS             832
#define GIB_NOPENALTY              3
#define INVENTORY_WEIGHT_LIMIT     100.0f

extern bool g_iActiveIdle;
extern cvar_t timeleft;
extern cvar_t allow_spectators;

// The player's hull is tested for starting solid, so where it ends hardly matters
void TracePlayerHullAt( edict_t *entity, const Vector &origin, TraceResult *trace )
{
	bool ducking = ( entity->v.flags & FL_DUCKING ) != 0;
	Vector end( 16, 16, ducking ? 36 : 72 );

	UTIL_TraceHull( origin, end, dont_ignore_monsters, ducking ? head_hull : human_hull, entity, trace );
}

bool FixPlayerStuck( edict_t *entity )
{
	if ( !entity || entity->free )
		return false;

	if ( strcmp( STRING( entity->v.classname ), "player" ) )
		return false;

	int step = (int) ( entity->v.maxs.x > 0 ? entity->v.maxs.x : 8.0f );
	Vector center = entity->v.origin;

	for ( int range = step; range <= STUCK_SEARCH_LIMIT; range += step )
	{
		for ( int attempt = 0; attempt < STUCK_SEARCH_TRIES; attempt++ )
		{
			TraceResult trace;
			Vector origin;

			origin.x = RANDOM_FLOAT( center.x - range, center.x + range );
			origin.y = RANDOM_FLOAT( center.y - range, center.y + range );
			origin.z = RANDOM_FLOAT( center.z - range, center.z + range );

			TracePlayerHullAt( entity, origin, &trace );

			if ( !trace.fStartSolid )
			{
				UTIL_SetOrigin( &entity->v, origin );
				return true;
			}
		}
	}

	return false;
}

int XtraLevel( CBasePlayer *player, bool force )
{
	return player ? player->GetPlayerExtras( force ) : EXTRAS_INIT;
}

void PrintInfoFloats( CBaseEntity *viewer, const char *text, const char *format, float x, float y, float z )
{
	char first[32];
	char second[32];
	char third[32];

	UTIL_FormatString( first, sizeof( first ), format, x );
	UTIL_FormatString( second, sizeof( second ), format, y );
	UTIL_FormatString( third, sizeof( third ), format, z );

	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, text, first, second, third );
}

void PrintInfoFloat( CBaseEntity *viewer, const char *text, const char *format, float value )
{
	char buffer[32];

	UTIL_FormatString( buffer, sizeof( buffer ), format, value );
	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, text, buffer );
}

void PrintInfoInt( CBaseEntity *viewer, const char *text, int value )
{
	char buffer[32];

	UTIL_FormatString( buffer, sizeof( buffer ), "%d", value );
	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, text, buffer );
}

void PrintInfoNamed( CBaseEntity *viewer, const char *text, int value, const char *const *names, int nameCount )
{
	char buffer[32];

	UTIL_FormatString( buffer, sizeof( buffer ), "%d", value );
	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, text, buffer,
	             value >= 0 && value < nameCount && names[value] ? names[value] : "Unknown" );
}

const char *PlayerModelChoice( char *infoBuffer )
{
	const char *model = g_engfuncs.pfnInfoKeyValue( infoBuffer, "cl_model" );

	return *model ? model : g_engfuncs.pfnInfoKeyValue( infoBuffer, "model" );
}

static const char *const g_RenderModeNames[] = { "Normal", "Color", "Texture", "Glow", "Solid", "Additive" };

static const char *const g_RenderFxNames[] = {
	"Normal", "Slow Pulse", "Fast Pulse", "Slow Wide Pulse", "Fast Wide Pulse", "Slow Fade Away", "Fast Fade Away",
	"Slow Become Solid", "Fast Become Solid", "Slow Strobe", "Fast Strobe", "Faster Strobe", "Slow Flicker",
	"Fast Flicker", "Constant Glow", "Distort", "Hologram", "Dead Player", "Explode", "Glow Shell",
	"Clamp Min Scale (Sprites)", "Light Multiplier (iuser2)"
};

static const char *const g_TakeDamageNames[] = { "No", "Yes", "Yes, can auto-aim at" };

static const char *const g_MoveTypeNames[] = {
	"None", NULL, NULL, "Walk", "Step", "Fly", "Toss", "Push", "No-clip", "Fly missile", "Bounce", "Bounce missile",
	"Follow", "Push step"
};

static const char *const g_SolidNames[] = { "Not solid", "Trigger", "Bounding box", "Slide box", "BSP" };
static const char *const g_WaterLevelNames[] = { "Dry", "Feet wet", "Waist wet", "Head wet" };

void PrintPlayerCheats( CBaseEntity *viewer, CBasePlayer *player )
{
	static const char *const names[] = { "God mode", "No clip", "No target", "Non-solid", "Invisibility" };
	static const char *const lowerNames[] = { "god mode", "no clip", "no target", "non-solid", "invisibility" };

	if ( player->m_iCheats <= 0 )
	{
		ClientPrint( viewer->pev, HUD_PRINTCONSOLE, "Cheats       : 0 (None)\n" );
		return;
	}

	char list[COMMAND_TEXT_LENGTH + 1] = { 0 };
	char count[32];
	int active = 0;

	for ( int index = 0; index < ARRAYSIZE( names ); index++ )
	{
		if ( !( player->m_iCheats & ( 1 << index ) ) )
			continue;

		if ( active++ )
		{
			UTIL_AppendString( list, ", ", sizeof( list ) );
			UTIL_AppendString( list, lowerNames[index], sizeof( list ) );
		}
		else
			UTIL_CopyString( list, names[index], sizeof( list ) );
	}

	UTIL_FormatString( count, sizeof( count ), "%d", active );
	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, "Cheats       : %s (%s)\n", count, active ? list : "None" );
}

void PrintPlayerExtras( CBaseEntity *viewer, CBasePlayer *player )
{
	const extrasinfo_s *extras = CServer::GetInstance()->GetPlayerExtras( player );
	char text[COMMAND_TEXT_LENGTH + 1];

	UTIL_FormatString( text, sizeof( text ), "%d", extras ? extras->iLevel : EXTRAS_NONE );

	if ( extras && *extras->szName )
	{
		UTIL_AppendString( text, " (", sizeof( text ) );
		UTIL_AppendString( text, extras->szName, sizeof( text ) );

		if ( *extras->szDescription )
		{
			UTIL_AppendString( text, ": ", sizeof( text ) );
			UTIL_AppendString( text, extras->szDescription, sizeof( text ) );
		}

		UTIL_AppendString( text, ")", sizeof( text ) );
	}

	ClientPrint( viewer->pev, HUD_PRINTCONSOLE, "Extras level : %s\n", text );
}

void PrintEntityInfo( CBaseEntity *viewer, CBaseEntity *entity )
{
	if ( !viewer || !entity )
		return;

	entvars_t *ev = entity->pev;
	CBaseToggle *toggle = dynamic_cast<CBaseToggle *>( entity );
	CBaseMonster *monster = dynamic_cast<CBaseMonster *>( entity );
	CBasePlayer *player = dynamic_cast<CBasePlayer *>( entity );
	entvars_t *to = viewer->pev;

	PrintInfoInt( viewer, "Information for entity #%s:\n", entity->entindex() );
	ClientPrint( to, HUD_PRINTCONSOLE, "-------------\n" );
	ClientPrint( to, HUD_PRINTCONSOLE, "Entity class : %s\n", STRING( ev->classname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Entity name  : %s\n", STRING( ev->targetname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Global name  : %s\n", STRING( ev->globalname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Net name     : %s\n", STRING( ev->netname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Target       : %s\n", STRING( ev->target ) );

	if ( toggle )
		ClientPrint( to, HUD_PRINTCONSOLE, "Master       : %s\n", STRING( toggle->m_sMaster ) );

	PrintInfoFloats( viewer, "Origin (XYZ) : %s %s %s\n", "%.3f", ev->origin.x, ev->origin.y, ev->origin.z );

	PrintInfoFloats( viewer, "Angles (PYR) : %s %s %s\n", "%.3f", ev->angles.x * 3.0f, ev->angles.y, ev->angles.z );
	PrintInfoFloats( viewer, "Min size     : %s %s %s\n", "%.3f", ev->mins.x, ev->mins.y, ev->mins.z );
	PrintInfoFloats( viewer, "Max size     : %s %s %s\n", "%.3f", ev->maxs.x, ev->maxs.y, ev->maxs.z );
	PrintInfoFloats( viewer, "Min abs size : %s %s %s\n", "%.3f", ev->absmin.x, ev->absmin.y, ev->absmin.z );
	PrintInfoFloats( viewer, "Max abs size : %s %s %s\n", "%.3f", ev->absmax.x, ev->absmax.y, ev->absmax.z );
	PrintInfoFloats( viewer, "Velocity     : %s %s %s\n", "%.3f", ev->velocity.x, ev->velocity.y, ev->velocity.z );
	PrintInfoFloats( viewer, "Ang. velocity: %s %s %s\n", "%.3f", ev->avelocity.x, ev->avelocity.y, ev->avelocity.z );
	PrintInfoInt( viewer, "Sequence     : %s\n", ev->sequence );
	PrintInfoInt( viewer, "Gait sequence: %s\n", ev->gaitsequence );
	PrintInfoFloat( viewer, "Frame        : %s\n", "%.3f", ev->frame );
	PrintInfoFloat( viewer, "Anim. time   : %s\n", "%.3f", ev->animtime );
	PrintInfoFloat( viewer, "Frame rate   : %s\n", "%.3f", ev->framerate );
	PrintInfoFloat( viewer, "Scale        : %s\n", "%.3f", ev->scale );

	const char *model = STRING( ev->model );

	if ( *model == '*' )
		ClientPrint( to, HUD_PRINTCONSOLE, "Model number : %s\n", model + 1 );
	else
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Model file   : %s\n", model );
		PrintInfoInt( viewer, "Model skin   : %s\n", ev->skin );
		PrintInfoInt( viewer, "Model body   : %s\n", ev->body );
	}

	if ( player )
	{
		char *infoBuffer = g_engfuncs.pfnGetInfoKeyBuffer( player->edict() );

		ClientPrint( to, HUD_PRINTCONSOLE, "Model choice : %s\n", PlayerModelChoice( infoBuffer ) );
		ClientPrint( to, HUD_PRINTCONSOLE, "Model now    : %s\n", g_engfuncs.pfnInfoKeyValue( infoBuffer, "model" ) );
	}

	PrintInfoNamed( viewer, "Render mode  : %s (%s)\n", ev->rendermode, g_RenderModeNames, ARRAYSIZE( g_RenderModeNames ) );
	PrintInfoFloat( viewer, "Render amount: %s\n", "%.3f", ev->renderamt );
	PrintInfoFloats( viewer, "Render color : %s %s %s\n", "%.3f", ev->rendercolor.x, ev->rendercolor.y, ev->rendercolor.z );
	PrintInfoNamed( viewer, "Render effect: %s (%s)\n", ev->renderfx, g_RenderFxNames, ARRAYSIZE( g_RenderFxNames ) );

	if ( monster )
	{
		const char *state = player && player->IsObserver() ? "Observer" : entity->IsAlive() ? "Alive" : "Dead";

		ClientPrint( to, HUD_PRINTCONSOLE, "State        : %s\n", state );
	}

	char first[32];
	char second[32];
	char third[32];

	UTIL_FormatString( first, sizeof( first ), "%.0f", ev->health );
	UTIL_FormatString( second, sizeof( second ), "%.0f", ev->max_health );
	ClientPrint( to, HUD_PRINTCONSOLE, "Health       : %s (%s max)\n", first, second );

	UTIL_FormatString( first, sizeof( first ), "%.0f", ev->armorvalue );
	UTIL_FormatString( second, sizeof( second ), "%.0f", ev->armortype );
	ClientPrint( to, HUD_PRINTCONSOLE, "Armor        : %s (%s max)\n", first, second );

	PrintInfoNamed( viewer, "Take damage  : %s (%s)\n", (int) ev->takedamage, g_TakeDamageNames, ARRAYSIZE( g_TakeDamageNames ) );
	PrintInfoFloat( viewer, "Frags        : %s\n", "%.0f", ev->frags );
	PrintInfoInt( viewer, "Deaths       : %s\n", player ? player->m_iDeaths : 0 );
	PrintInfoFloat( viewer, "Gravity mod  : %sx\n", "%.2f", ev->gravity );
	PrintInfoFloat( viewer, "Friction mod : %sx\n", "%.2f", ev->friction );

	UTIL_FormatString( first, sizeof( first ), "%.2f", ev->speed );
	UTIL_FormatString( second, sizeof( second ), "%.2f", ev->maxspeed );
	UTIL_FormatString( third, sizeof( third ), "%.2f", CVAR_GET_FLOAT( "sv_maxvelocity" ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Speed        : %s (%s max, %s absolute max)\n", first, second, third );

	PrintInfoNamed( viewer, "Move type    : %s (%s)\n", ev->movetype, g_MoveTypeNames, ARRAYSIZE( g_MoveTypeNames ) );
	PrintInfoNamed( viewer, "Solid        : %s (%s)\n", ev->solid, g_SolidNames, ARRAYSIZE( g_SolidNames ) );
	PrintInfoNamed( viewer, "Water level  : %s (%s)\n", ev->waterlevel, g_WaterLevelNames, ARRAYSIZE( g_WaterLevelNames ) );
	PrintInfoInt( viewer, "Spawn flags  : %s\n", ev->spawnflags );
	PrintInfoInt( viewer, "Main flags   : %s\n", ev->flags );
	PrintInfoInt( viewer, "Effect flags : %s\n", ev->effects );
	PrintInfoInt( viewer, "Weapon flags : %s\n", ev->weapons );

	UTIL_FormatString( first, sizeof( first ), "%d", entity->Classify() );
	ClientPrint( to, HUD_PRINTCONSOLE, "Classify     : %s (%s) <%s>\n", first,
	             entity->GetClassificationName(), entity->GetClassificationTag() );

	if ( monster )
	{
		PrintInfoFloats( viewer, "FX glow (RGB): %s %s %s\n", "%.2f", monster->m_vecEffectGlowColor.x,
		                 monster->m_vecEffectGlowColor.y, monster->m_vecEffectGlowColor.z );
		PrintInfoInt( viewer, "FX no weapons: %s\n", monster->m_iEffectBlockWeapons );
		PrintInfoInt( viewer, "FX invulnerb.: %s\n", monster->m_iEffectInvulnerable );
		PrintInfoInt( viewer, "FX invisible : %s\n", monster->m_iEffectInvisible );
		PrintInfoInt( viewer, "FX non-solid : %s\n", monster->m_iEffectNonSolid );
		PrintInfoFloat( viewer, "FX pre-drown : %s\n", "%.2f", monster->m_flEffectRespiration );
		PrintInfoFloat( viewer, "FX gravity   : %s\n", "%.2f", monster->m_flEffectGravity );
		PrintInfoFloat( viewer, "FX friction  : %s\n", "%.2f", monster->m_flEffectFriction );
		PrintInfoFloat( viewer, "FX speed     : %s\n", "%.2f", monster->m_flEffectSpeed );
		PrintInfoFloat( viewer, "FX damage    : %s\n", "%.2f", monster->m_flEffectDamage );
	}

	if ( player )
	{
		static const char *const adminNames[] = { "Player", "Administrator" };

		PrintInfoInt( viewer, "Player class : %s\n", ev->playerclass );
		ClientPrint( to, HUD_PRINTCONSOLE, "Auth ID      : %s\n", GETPLAYERAUTHID( player->edict() ) );
		PrintInfoInt( viewer, "User ID      : %s\n", GETPLAYERUSERID( player->edict() ) );
		ClientPrint( to, HUD_PRINTCONSOLE, "Non-colliding: %s\n", ev->iuser4 > 0 ? "Yes" : "No" );
		ClientPrint( to, HUD_PRINTCONSOLE, "Jumping block: %s\n", ev->fuser4 > 0 ? "Yes" : "No" );
		PrintPlayerCheats( viewer, player );

		UTIL_FormatString( first, sizeof( first ), "%d", player->m_iAdminLevel );

		const char *admin = "Unknown";

		if ( player->m_iAdminLevel == ADMIN_OWNER )
			admin = IS_DEDICATED_SERVER() ? "Owner" : "Host";
		else if ( player->m_iAdminLevel >= 0 && player->m_iAdminLevel < ARRAYSIZE( adminNames ) )
			admin = adminNames[player->m_iAdminLevel];

		ClientPrint( to, HUD_PRINTCONSOLE, "Admin level  : %s (%s)\n", first, admin );
		PrintPlayerExtras( viewer, player );
	}

	ClientPrint( to, HUD_PRINTCONSOLE, "-------------\n" );
}

void PrintPlayerInfo( CBaseEntity *viewer, CBasePlayer *player )
{
	if ( !viewer || !player )
		return;

	char *infoBuffer = g_engfuncs.pfnGetInfoKeyBuffer( player->edict() );
	entvars_t *to = viewer->pev;
	char first[32];
	char second[32];

	PrintInfoInt( viewer, "Information for player #%s:\n", player->entindex() );
	ClientPrint( to, HUD_PRINTCONSOLE, "-------------\n" );
	ClientPrint( to, HUD_PRINTCONSOLE, "Name         : %s\n", STRING( player->pev->netname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Unique ID    : %s\n", GETPLAYERAUTHID( player->edict() ) );
	PrintInfoInt( viewer, "Player ID    : #%s\n", GETPLAYERUSERID( player->edict() ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Model choice : %s\n", PlayerModelChoice( infoBuffer ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Model now    : %s\n", g_engfuncs.pfnInfoKeyValue( infoBuffer, "model" ) );

	UTIL_FormatString( first, sizeof( first ), "%.0f", player->pev->health );
	UTIL_FormatString( second, sizeof( second ), "%.0f", player->pev->max_health );
	ClientPrint( to, HUD_PRINTCONSOLE, "Health       : %s (%s max)\n", first, second );

	UTIL_FormatString( first, sizeof( first ), "%.0f", player->pev->armorvalue );
	UTIL_FormatString( second, sizeof( second ), "%.0f", player->pev->armortype );
	ClientPrint( to, HUD_PRINTCONSOLE, "Armor        : %s (%s max)\n", first, second );
}

void CommandInventoryList( CBasePlayer *player )
{
	entvars_t *to = player->pev;

	if ( !player->IsAlive() )
	{
		ClientPrint( to, HUD_PRINTCENTER, "Can't check inventory while dead.\n" );
		return;
	}

	if ( !player->m_pInventory || !Inventory_Count( player ) )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "You're not holding any inventory.\n" );
		return;
	}

	float weight = 0;
	char text[COMMAND_TEXT_LENGTH + 1];

	ClientPrint( to, HUD_PRINTCONSOLE, "\n>>>>>>>> INVENTORY LIST <<<<<<<<\n\n" );

	for ( CInventoryItemNode *node = player->m_pInventory; node; node = node->m_pNext )
	{
		CItemInventory *item = (CItemInventory *) (CBaseEntity *) node->m_hItem;

		if ( !item )
			continue;

		weight += item->m_flWeight;

		const char *name = item->m_szItemName ? STRING( item->m_szItemName ) : "<nameless item>";

		UTIL_FormatString( text, sizeof( text ), "#%d: %s (%.2f kG)", item->entindex(), name, item->m_flWeight );

		if ( item->m_szDisplayName )
		{
			UTIL_AppendString( text, " -- ", sizeof( text ) );
			UTIL_AppendString( text, STRING( item->m_szDisplayName ), sizeof( text ) );
		}

		ClientPrint( to, HUD_PRINTCONSOLE, "%s\n", text );
	}

	UTIL_FormatString( text, sizeof( text ), "\nTotal weight: %.2f kG (of %.2f allowed)\n", weight,
	                   INVENTORY_WEIGHT_LIMIT );
	ClientPrint( to, HUD_PRINTCONSOLE, text );
	ClientPrint( to, HUD_PRINTCONSOLE, "Use the 'dropitem' command to throw away an item.\n" );
	ClientPrint( to, HUD_PRINTCONSOLE, ">>>>>>>> INVENTORY END <<<<<<<<\n\n" );
}

bool InventoryItemMatches( CItemInventory *item, const char *name )
{
	if ( *name == '#' )
		return item->entindex() == atoi( name + 1 );

	return !stricmp( name, STRING( item->m_szItemName ) );
}

void PrintInventoryAction( CBasePlayer *player, const char *action, CItemInventory *item )
{
	char text[COMMAND_TEXT_LENGTH + 1];

	UTIL_FormatString( text, sizeof( text ), "%s (#%d)", action, item->entindex() );

	if ( item->m_szItemName )
	{
		UTIL_AppendString( text, ": ", sizeof( text ) );
		UTIL_AppendString( text, STRING( item->m_szItemName ), sizeof( text ) );
	}

	ClientPrint( player->pev, HUD_PRINTCONSOLE, "%s\n", text );
}

void CommandActivateItem( CBasePlayer *player )
{
	entvars_t *to = player->pev;

	if ( !player->IsAlive() )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Can't activate inventory while dead.\n" );
		return;
	}

	if ( player->pev->flags & FL_FROZEN )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Can't activate inventory while frozen.\n" );
		return;
	}

	if ( !player->m_pInventory || !Inventory_Count( player ) )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "You're not holding any inventory.\n" );
		return;
	}

	if ( CMD_ARGC() < 2 )
	{
		ClientPrint( to, HUD_PRINTCONSOLE,
		             "You need to specify an item name or item index (i.e. #100).\n"
		             "Command \"inventorylist\" shows your inventory contents.\n" );
		return;
	}

	const char *name = CMD_ARGV( 1 );

	for ( CInventoryItemNode *node = player->m_pInventory; node; node = node->m_pNext )
	{
		CItemInventory *item = (CItemInventory *) (CBaseEntity *) node->m_hItem;

		if ( item && InventoryItemMatches( item, name ) )
		{
			PrintInventoryAction( player, "Activating item", item );
			item->Activate( false );
			return;
		}
	}
}

void CommandDropItem( CBasePlayer *player )
{
	entvars_t *to = player->pev;

	if ( !player->IsAlive() )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Can't drop inventory while dead.\n" );
		return;
	}

	if ( player->pev->flags & FL_FROZEN )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Can't drop inventory while frozen.\n" );
		return;
	}

	if ( !player->m_pInventory || !Inventory_Count( player ) )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "You're not holding any inventory.\n" );
		return;
	}

	if ( CMD_ARGC() < 2 )
	{
		ClientPrint( to, HUD_PRINTCONSOLE,
		             "You need to specify an item name, item index (i.e. #100), or use * to throw away everything.\n"
		             "Command \"inventorylist\" shows your inventory contents.\n" );
		return;
	}

	const char *name = CMD_ARGV( 1 );
	CInventoryItemNode *node = player->m_pInventory;

	// Dropping frees the node, so step past it first
	while ( node )
	{
		CInventoryItemNode *next = node->m_pNext;
		CItemInventory *item = (CItemInventory *) (CBaseEntity *) node->m_hItem;

		if ( item && ( !strcmp( name, "*" ) || InventoryItemMatches( item, name ) ) )
		{
			PrintInventoryAction( player, "Dropping item", item );
			item->Drop( false, player, false );
		}

		node = next;
	}
}

static Task_t tlPlayerMoveAndHold[] = {
	{ TASK_RUN_PATH,          (float) 9999 },
	{ TASK_WAIT_FOR_MOVEMENT, (float) 0    },
	{ TASK_WAIT_INDEFINITE,   (float) 0    },
};

static Schedule_t slPlayerMoveAndHold[] = {
	{ tlPlayerMoveAndHold,
	  ARRAYSIZE( tlPlayerMoveAndHold ),
	  bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_CAN_MELEE_ATTACK1_MOVING | bits_COND_CAN_RANGE_ATTACK2_MOVING,
	  bits_SOUND_COMBAT | bits_SOUND_DANGER,
	  "Idle GuardPoint" },
};

static Task_t tlPlayerMoveAndGuard[] = {
	{ TASK_RUN_PATH,          (float) 9999             },
	{ TASK_WAIT_FOR_MOVEMENT, (float) 0                },
	{ TASK_SET_SCHEDULE,      (float) SCHED_GUARD_POINT },
};

static Schedule_t slPlayerMoveAndGuard[] = {
	{ tlPlayerMoveAndGuard, ARRAYSIZE( tlPlayerMoveAndGuard ), 0, 0, "Idle GuardPoint" },
};

bool IsPlayerFollower( CBaseEntity *entity, CBasePlayer *player )
{
	CBaseMonster *monster = entity->MyMonsterPointer();

	return monster && monster->IRelationship( player ) <= R_NO && (CBaseEntity *) monster->m_hTargetEnt == player;
}

void CommandNpcOrder( CBasePlayer *player, const char *order )
{
	CBaseEntity *monsters[PLAYER_COMMAND_FOLLOWERS];
	int count = player->FindMonstersInWorld( monsters, PLAYER_COMMAND_FOLLOWERS, FL_MONSTER );

	if ( !stricmp( order, "npc_return" ) )
	{
		for ( int index = 0; index < count; index++ )
		{
			if ( !IsPlayerFollower( monsters[index], player ) )
				continue;

			CBaseMonster *monster = monsters[index]->MyMonsterPointer();

			monster->ChangeSchedule( monster->GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE ) );
			monster->UseSound();
		}
	}
	else if ( !stricmp( order, "npc_findcover" ) )
	{
		for ( int index = 0; index < count; index++ )
		{
			if ( !IsPlayerFollower( monsters[index], player ) )
				continue;

			CBaseMonster *monster = monsters[index]->MyMonsterPointer();

			monster->ChangeSchedule( monster->GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY ) );
			monster->PainSound();
		}
	}
	else if ( !stricmp( order, "npc_attackmytarget" ) )
	{
		CBaseEntity *target = FindEntityForward( player, PLAYER_COMMAND_TRACE_RANGE );

		if ( target && ( target->pev->flags & FL_MONSTER ) && target->MyMonsterPointer()
		     && target->MyMonsterPointer()->IRelationship( player ) >= R_NO )
		{
			for ( int index = 0; index < count; index++ )
			{
				CBaseMonster *monster = monsters[index]->MyMonsterPointer();

				if ( !monster || monster->IRelationship( player ) > R_NO || target->Classify() == CLASS_PLAYER_ALLY
				     || (CBaseEntity *) monster->m_hTargetEnt != player )
					continue;

				monster->m_hEnemy = target;
				monster->m_vecEnemyLKP = target->pev->origin;
				monster->SetConditions( bits_COND_NEW_ENEMY );
				monster->AlertSound();
			}
		}
	}
	else
	{
		TraceResult trace;
		Vector eyes = player->pev->origin + player->pev->view_ofs;

		UTIL_MakeVectors( player->pev->v_angle );
		UTIL_TraceLine( eyes, eyes + gpGlobals->v_forward * 2048, dont_ignore_monsters, player->edict(), &trace );

		for ( int index = 0; index < count; index++ )
		{
			if ( !IsPlayerFollower( monsters[index], player ) )
				continue;

			CBaseMonster *monster = monsters[index]->MyMonsterPointer();

			if ( !monster->MoveToLocation( ACT_RUN, 2, trace.vecEndPos, 64 ) )
			{
				monster->UnUseSound();
				continue;
			}

			monster->UseSound();

			bool engaged = monster->m_hEnemy != NULL || monster->HasConditions( bits_COND_SEE_ENEMY );

			monster->ChangeSchedule( engaged ? slPlayerMoveAndGuard : slPlayerMoveAndHold );
		}
	}

	player->m_flNextNpcCommandTime = gpGlobals->time + 1.0f;
}

void CommandHostInfo( CBasePlayer *player )
{
	entvars_t *to = player->pev;
	char text[COMMAND_TEXT_LENGTH + 1];

	ClientPrint( to, HUD_PRINTCONSOLE, "Server name              : %s\n", CVAR_GET_STRING( "hostname" ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Server is private?       : %s\n", *CVAR_GET_STRING( "sv_password" ) ? "Yes" : "No" );
	ClientPrint( to, HUD_PRINTCONSOLE, "Server is dedicated?     : %s\n", IS_DEDICATED_SERVER() ? "Yes" : "No" );

	if ( !IS_DEDICATED_SERVER() )
	{
		ClientPrint( to, HUD_PRINTCONSOLE, "Hosting player           : " );

		CBaseEntity *host = UTIL_FindEntityByIndex( 1, false );

		if ( host && host->IsPlayer() )
		{
			UTIL_FormatString( text, sizeof( text ), "%s [%s]", STRING( host->pev->netname ), GETPLAYERAUTHID( host->edict() ) );
			ClientPrint( to, HUD_PRINTCONSOLE, "%s\n", text );
		}
		else
			ClientPrint( to, HUD_PRINTCONSOLE, "Unknown\n" );
	}

	ClientPrint( to, HUD_PRINTCONSOLE, "Administrative contact   : %s\n", CVAR_GET_STRING( "sv_contact" ) );

	UTIL_FormatString( text, sizeof( text ), "%d", gpGlobals->maxClients );
	ClientPrint( to, HUD_PRINTCONSOLE, "Max players              : %s\n", text );
	ClientPrint( to, HUD_PRINTCONSOLE, "Current map              : %s\n", STRING( gpGlobals->mapname ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Next map                 : %s\n", CVAR_GET_STRING( "mp_nextmap_cycle" ) );
	ClientPrint( to, HUD_PRINTCONSOLE, "Cheats mode              : " );

	const char *cheats = "Unknown\n";

	switch ( (int) CVAR_GET_FLOAT( "sv_cheats" ) )
	{
		case 0:
			cheats = "Off\n";
			break;
		case 1:
			cheats = "On\n";
			break;
		case 2:
			cheats = "Restricted (administrators only)\n";
			break;
		case 3:
			cheats = "Restricted (owners/host only)\n";
			break;
		case 255:
			cheats = "On with engine debugging\n";
			break;
	}

	ClientPrint( to, HUD_PRINTCONSOLE, cheats );

	UTIL_FormatString( text, sizeof( text ), "%d (limit is %d)", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities );
	ClientPrint( to, HUD_PRINTCONSOLE, "Edict count              : %s\n", text );

	const int modelLimit = gpGlobals->maxModels;
	UTIL_FormatString( text, sizeof( text ), "%d (limit is %d)", g_pEngineServer->NumberOfPrecachedModels(), modelLimit );
	ClientPrint( to, HUD_PRINTCONSOLE, "Loaded models            : %s\n", text );
}

void CommandMapInfo( CBasePlayer *player )
{
	static const char *const whole[] = {
		"mp_allowmonsterinfo", "mp_allowplayerinfo", "mp_ammo_droprules", "mp_ammo_respawndelay", "mp_banana",
		"mp_barnacle_paralyze", "mp_disable_autoclimb", "mp_disable_medkit_points", "mp_disable_pcbalancing",
		"mp_disable_player_rappel", "mp_disablegaussjump", "mp_falldamage", "mp_flashlight", "mp_forcerespawn",
		"mp_fraglimit", "mp_grapple_mode", "mp_hevsuit_voice", "mp_item_respawndelay", "#mp_modelselection",
		"mp_no_akimbo_uzis", "mp_noblastgibs", "mp_npckill", "mp_observer_cyclic", "mp_observer_mode",
		"#mp_pcbalancing_factorlist", "mp_respawndelay", "mp_suitpower", "mp_survival_nextmap",
		"mp_survival_startdelay", "mp_survival_starton", "mp_survival_supported", "mp_timelimit",
		"mp_weapon_droprules", "mp_weapon_respawndelay", "mp_weaponfadedelay", "mp_weaponstay"
	};
	static const char *const fractional[] = {
		"sv_accelerate", "sv_airaccelerate", "sv_friction", "sv_gravity", "sv_maxspeed", "sv_maxvelocity",
		"sv_wateraccelerate", "sv_waterfriction", "sv_zmax"
	};

	char label[64];
	char text[COMMAND_TEXT_LENGTH + 1];

	for ( int index = 0; index < ARRAYSIZE( whole ); index++ )
	{
		const char *name = whole[index];
		bool isString = *name == '#';

		if ( isString )
			name++;

		UTIL_FormatString( label, sizeof( label ), "%-25s: %%s\n", name );

		if ( isString )
			ClientPrint( player->pev, HUD_PRINTCONSOLE, label, CVAR_GET_STRING( name ) );
		else
		{
			UTIL_FormatString( text, sizeof( text ), "%d", (int) CVAR_GET_FLOAT( name ) );
			ClientPrint( player->pev, HUD_PRINTCONSOLE, label, text );
		}
	}

	for ( int index = 0; index < ARRAYSIZE( fractional ); index++ )
	{
		UTIL_FormatString( label, sizeof( label ), "%-25s: %%s\n", fractional[index] );
		UTIL_FormatString( text, sizeof( text ), "%.2f", CVAR_GET_FLOAT( fractional[index] ) );
		ClientPrint( player->pev, HUD_PRINTCONSOLE, label, text );
	}
}

void PrintCommandWait( CBasePlayer *player, float nextTime, const char *format )
{
	CLIENT_PRINTF( player->edict(), print_center, UTIL_VarArgs( format, nextTime - gpGlobals->time ) );
}

void CommandMedic( CBasePlayer *player )
{
	if ( player->m_flNextMiscCommandTime > gpGlobals->time )
		return;

	if ( !player->IsAlive() && ( player->pev->effects & EF_NODRAW ) && !player->IsObserver() )
		return;

	if ( player->pev->flags & FL_FROZEN )
		return;

	CBaseEntity *caller = player;

	if ( player->IsObserver() )
	{
		caller = player->GetObserver()->GetDeadBody();

		if ( !caller )
			return;
	}

	player->ShowOverheadSprite( "sprites/saveme.spr", CALL_SPRITE_OFFSET, CALL_SPRITE_LIFETIME );

	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityInSphere( entity, caller->pev->origin, MEDIC_CALL_RANGE ) ) != NULL )
	{
		if ( strcmp( STRING( entity->pev->classname ), "monster_human_medic_ally" ) )
			continue;

		CSquadMonster *medic = entity->MySquadMonsterPointer();

		if ( medic && medic->HealMePlayer( player ) )
			break;
	}

	const char *sound = ( player->m_afPhysicsFlags & PFLAG_ONBARNACLE ) ? "scientist/sci_dragoff.wav" : "speech/saveme1.wav";

	EMIT_SOUND( caller->edict(), CHAN_VOICE, sound, 1, ATTN_NORM );
	player->m_flNextMiscCommandTime = gpGlobals->time + PLAYER_COMMAND_WAIT;
}

void CommandGrenade( CBasePlayer *player )
{
	if ( player->m_flNextMiscCommandTime > gpGlobals->time || !player->IsAlive() )
		return;

	if ( ( player->pev->flags & FL_FROZEN ) || ( player->m_afPhysicsFlags & PFLAG_ONBARNACLE ) )
		return;

	player->ShowOverheadSprite( "sprites/grenade.spr", CALL_SPRITE_OFFSET, CALL_SPRITE_LIFETIME );
	CLIENT_COMMAND( player->edict(), "npc_findcover\n" );
	EMIT_SOUND( player->edict(), CHAN_VOICE, "speech/grenade1.wav", 1, ATTN_NORM );
	player->m_flNextMiscCommandTime = gpGlobals->time + PLAYER_COMMAND_WAIT;
}

void CommandStuck( CBasePlayer *player )
{
	if ( !player->IsAlive() || ( player->pev->flags & FL_FROZEN ) || player->pev->movetype != MOVETYPE_WALK )
		return;

	if ( player->m_afPhysicsFlags & PFLAG_ONBARNACLE )
		return;

	if ( player->m_flNextMiscCommandTime > gpGlobals->time )
	{
		PrintCommandWait( player, player->m_flNextMiscCommandTime, "You must wait %.1f seconds before\nyou can unstick yourself.\n" );
		return;
	}

	TraceResult trace;

	TracePlayerHullAt( player->edict(), player->pev->origin, &trace );

	if ( !trace.fStartSolid )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, "You don't appear to be stuck.\nIf you really are, suicide instead.\n" );
		return;
	}

	if ( !FixPlayerStuck( player->edict() ) )
		ClientPrint( player->pev, HUD_PRINTCENTER, "No open space close enough.\nYou must suicide instead.\n" );

	player->m_flNextMiscCommandTime = gpGlobals->time + PLAYER_COMMAND_WAIT;
}

void CommandEntityInfo( CBasePlayer *player )
{
	if ( CMD_ARGC() < 2 )
	{
		ClientPrint( player->pev, HUD_PRINTCONSOLE, "Entity info: Entity not specified.\n" );
		return;
	}

	const char *name = CMD_ARGV( 1 );

	if ( *name == '#' && name[1] )
	{
		edict_t *edict = INDEXENT( atoi( name + 1 ) );
		CBaseEntity *entity = edict && !edict->free ? CBaseEntity::Instance( edict ) : NULL;

		if ( entity && ( !strcmp( name + 1, "0" ) || entity->entindex() ) )
		{
			PrintEntityInfo( player, entity );
			return;
		}
	}
	else
	{
		CBaseEntity *entity = UTIL_FindEntityByTargetname( NULL, name );

		if ( entity )
		{
			for ( ; entity; entity = UTIL_FindEntityByTargetname( entity, name ) )
				PrintEntityInfo( player, entity );

			return;
		}
	}

	ClientPrint( player->pev, HUD_PRINTCONSOLE, "Entity info: Entity \"%s\" not found.\n", name );
}

void CommandThatInfo( CBasePlayer *player )
{
	CBaseEntity *entity = FindEntityForward( player, PLAYER_COMMAND_TRACE_RANGE );

	if ( !CheatsAllowed( player, "That info", false, true ) && entity && entity->IsPlayer() )
	{
		PrintPlayerInfo( player, (CBasePlayer *) entity );
		return;
	}

	if ( !CheatsAllowed( player, "That info", false, false ) )
		return;

	if ( entity )
		PrintEntityInfo( player, entity );
	else
		CLIENT_PRINTF( player->edict(), print_center, "That info: No entity found\n" );
}

void CommandPlayerInfo( CBasePlayer *player )
{
	CBaseEntity *target;

	if ( CMD_ARGC() < 2 )
		target = FindEntityForward( player, PLAYER_COMMAND_TRACE_RANGE );
	else if ( atoi( CMD_ARGV( 1 ) ) < 1 )
		target = UTIL_PlayerBySteamId( CMD_ARGV( 1 ) );
	else
		target = UTIL_GetActivePlayer( atoi( CMD_ARGV( 1 ) ) );

	if ( !target )
	{
		CLIENT_PRINTF( player->edict(), print_center, "Player info: No player found\n" );
		return;
	}

	if ( !target->IsPlayer() )
	{
		CLIENT_PRINTF( player->edict(), print_center, "Player info: Not a player\n" );
		return;
	}

	PrintPlayerInfo( player, (CBasePlayer *) target );
}

void CommandTimeLeft( CBasePlayer *player )
{
	char text[COMMAND_TEXT_LENGTH + 1];

	if ( CVAR_GET_FLOAT( "mp_notimelimit" ) != 0 )
		UTIL_CopyString( text, "Map time limit has been disabled.\n", sizeof( text ) );
	else if ( timelimit.value <= 0 )
		UTIL_CopyString( text, "No time limit for this map.\n", sizeof( text ) );
	else
	{
		int seconds = (int) timeleft.value;

		UTIL_FormatString( text, sizeof( text ), "Time left until next map: %d:%02d.\n", seconds / 60, seconds % 60 );
	}

	ClientPrint( player->pev, HUD_PRINTCONSOLE, text );
}

void CommandTrigger( CBasePlayer *player )
{
	if ( CMD_ARGC() < 2 )
	{
		ClientPrint( player->pev, HUD_PRINTCONSOLE, "No target specified.\n" );
		return;
	}

	static const char *const names[] = { "off", "on", "set", "toggle", "kill" };

	USE_TYPE useType = USE_TOGGLE;
	int valueArgument = 3;
	const char *type = CMD_ARGV( 2 );

	for ( int index = 0; index < ARRAYSIZE( names ); index++ )
	{
		if ( !stricmp( type, names[index] ) )
		{
			useType = (USE_TYPE) index;
			valueArgument = 2;
			break;
		}
	}

	FireTargets( CMD_ARGV( 1 ), player, player, useType, (float) atof( CMD_ARGV( valueArgument ) ) );
}

void CommandGhost( CBasePlayer *player )
{
	CObserver *observer = player->GetObserver();

	if ( atoi( CMD_ARGV( 1 ) ) < 1 )
	{
		observer->StopObserver( true );
		return;
	}

	if ( !allow_spectators.value )
		return;

	CBaseEntity *start = observer->FindObserverStartPoint();

	observer->StartObserver( start->pev->origin, start->pev->angles, true );
	UTIL_ScreenFade( player, Vector( 128, 0, 0 ), 1, 15, 0, FFADE_OUT | FFADE_STAYOUT );
}

void SetPlayerCheatFromArgument( CBasePlayer *player, int cheat, const char *on, const char *off )
{
	bool enable = atoi( CMD_ARGV( 1 ) ) >= 1;

	if ( enable )
		player->m_iCheats |= cheat;
	else
		player->m_iCheats &= ~cheat;

	ClientPrint( player->pev, HUD_PRINTCENTER, enable ? on : off );
	player->ApplyEffects();
}

void CommandTeleport( CBasePlayer *player )
{
	if ( CMD_ARGC() != 4 )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, "Teleport: Needs 3 co-ordinates\n" );
		return;
	}

	Vector origin;

	for ( int index = 0; index < 3; index++ )
	{
		const char *argument = CMD_ARGV( index + 1 );

		origin[index] = (float) atof( argument );

		if ( origin[index] == 0 && strcmp( argument, "0" ) )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "Teleport: Invalid co-ordinates\n" );
			return;
		}
	}

	player->Teleport( origin, player->pev->v_angle );
	ClientPrint( player->pev, HUD_PRINTCENTER, "Whoosh!\n" );
	EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, "misc/r_tele3.wav", 1, ATTN_NONE, SND_CHANGE_PITCH, PITCH_NORM );
}

void CommandToggleLongJump( CBasePlayer *player )
{
	if ( !player->m_fLongJump )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, "Long jump module not present\n" );
		return;
	}

	bool disabled = atoi( g_engfuncs.pfnGetPhysicsKeyValue( player->edict(), "nolj" ) ) != 0;

	g_engfuncs.pfnSetPhysicsKeyValue( player->edict(), "nolj", disabled ? "0" : "1" );
	ClientPrint( player->pev, HUD_PRINTCENTER, disabled ? "Long jump module ON\n" : "Long jump module OFF\n" );
}

void CommandSubmitId( CBasePlayer *player )
{
	if ( !CanHaveXtra( XTRA_SUBMIT_ID, player ) )
	{
		ClientPrint( player->pev, HUD_PRINTCONSOLE, "We don't know you sorry, or you're playing in a LAN game.\n" );
		return;
	}

	if ( player->m_flNextMiscCommandTime > gpGlobals->time )
	{
		PrintCommandWait( player, player->m_flNextMiscCommandTime, "You must wait %.1f seconds before\nyou can submit your ID again.\n" );
		return;
	}

	const extrasinfo_s *extras = CServer::GetInstance()->GetPlayerExtras( player );

	if ( !extras || !*extras->szName || !*extras->szDescription )
		return;

	hudtextparms_t params = { 0 };
	char text[COMMAND_TEXT_LENGTH + 1];

	params.x = -1;
	params.y = 0.2f;
	params.r1 = 255;
	params.r2 = 255;
	params.b2 = 255;
	params.fadeinTime = 1;
	params.fadeoutTime = 1;
	params.holdTime = 3;
	params.fxTime = 2;
	params.channel = 1;

	UTIL_FormatString( text, sizeof( text ), "%s (%s) is on the server using the name \"%s\"", extras->szName,
	                   extras->szDescription, STRING( player->pev->netname ) );
	UTIL_HudMessageAll( params, text );

	player->m_flNextMiscCommandTime = gpGlobals->time + 5.0f;
}

void CommandNodePath( void )
{
	int path[MAX_PATH_SIZE];
	int start = atoi( CMD_ARGV( 1 ) );
	int end = atoi( CMD_ARGV( 2 ) );
	bool found = WorldGraph.FindShortestPath( path, start, end, NODE_HUMAN_HULL, NODE_PATH_CAPS ) != 0;

	ALERT( at_console, found ? "Success: start %d, end %d\n" : "Failure: start %d, end %d\n", start, end );
}

void CommandRemove( void )
{
	if ( CMD_ARGC() < 2 )
	{
		ALERT( at_console, "No target specified.\n" );
		return;
	}

	const char *name = CMD_ARGV( 1 );

	if ( !*name )
	{
		ALERT( at_console, "Invalid targetname\n" );
		return;
	}

	CBaseEntity *entity = NULL;
	int removed = 0;

	while ( ( entity = UTIL_FindEntityByTargetname( entity, name ) ) != NULL )
	{
		UTIL_Remove( entity );
		removed++;
	}

	if ( removed > 0 )
		ALERT( at_console, "Removed %d %s\n", removed, removed == 1 ? "entity" : "entities" );
	else
		ALERT( at_console, "'%s' not found\n", name );
}

void CommandShowExtras( CBasePlayer *player )
{
	const extrasinfo_s *extras = CServer::GetInstance()->GetPlayerExtras( player );
	entvars_t *to = player->pev;

	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "[Extras] Currently applied to '%s':\n", GETPLAYERAUTHID( player->edict() ) ) );

	if ( !extras )
		return;

	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "AuthID: %s\n", extras->szAuthId ) );
	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "Source: %s (%d)\n", ExtrasSourceToString( extras->iSource ), extras->iSource ) );
	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "Name: %s\n", extras->szName ) );
	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "Title: %s\n", extras->szDescription ) );
	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "Level: %d - %s\n", extras->iLevel, ExtrasLevelToString( extras->iLevel ) ) );
	ClientPrint( to, HUD_PRINTCONSOLE, UTIL_VarArgs( "Last queried: %.3f (time now is %.3f)\n", extras->flLastQuery, gpGlobals->time ) );
}

void CommandGetExtras( CBasePlayer *player )
{
	if ( player->m_flNextMiscCommandTime > gpGlobals->time )
	{
		PrintCommandWait( player, player->m_flNextMiscCommandTime, "You must wait %.1f seconds before\nyou can use 'getextras' again.\n" );
		return;
	}

	int level = XtraLevel( player, false );

	player->m_flNextMiscCommandTime = gpGlobals->time + PLAYER_COMMAND_WAIT;
	ClientPrint( player->pev, HUD_PRINTCONSOLE,
	             "[Extras] Your extras will be searched for shortly. Use command 'showextras' in a moment. "
	             "(Wait until the query finishes.)\n" );

	if ( level != EXTRAS_INIT && level != EXTRAS_ERROR )
		ClientPrint( player->pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "[Extras] Current level is %d, but this may change in a moment.\n", level ) );
}

void CommandHudReset( CBasePlayer *player )
{
	if ( player->m_flNextMiscCommandTime > gpGlobals->time )
	{
		PrintCommandWait( player, player->m_flNextMiscCommandTime, "You must wait %.1f seconds before\nyou can use 'hud_reset' again.\n" );
		return;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, player->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	player->m_iClientHealth = -1;
	player->m_iClientBattery = -1;
	player->m_iClientFOV = -1;

	MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, player->pev );
	MESSAGE_END();

	g_pGameRules->InitHUD( player );
}

// Returns false when the command isn't one of these. A command refused for
// cheats clears logCommand, as it never took effect
bool PerformPlayerCommands( CBasePlayer *player, const char *command, bool &logCommand )
{
#define PLAYER_CHEAT_COMMAND( label, requireAlive ) \
	if ( !CheatsAllowed( player, label, requireAlive, false ) ) \
	{ \
		logCommand = false; \
		return true; \
	}

	entvars_t *pev = player->pev;

	if ( !stricmp( command, "medic" ) || !stricmp( command, "heal" ) )
		CommandMedic( player );
	else if ( !stricmp( command, "grenade" ) || !stricmp( command, "takecover" ) )
		CommandGrenade( player );
	else if ( !stricmp( command, "gibme" ) )
	{
		if ( player->IsAlive() )
			player->TakeDamage( pev, pev, 5000, DMG_ALWAYSGIB );
	}
	else if ( !stricmp( command, "stuck" ) || !stricmp( command, "unstuck" ) || !stricmp( command, "unstick" ) )
		CommandStuck( player );
	else if ( !stricmp( command, "inventorylist" ) )
		CommandInventoryList( player );
	else if ( !stricmp( command, "activateitem" ) )
		CommandActivateItem( player );
	else if ( !stricmp( command, "dropitem" ) )
		CommandDropItem( player );
	else if ( !stricmp( command, "npc_return" ) || !stricmp( command, "npc_findcover" )
	          || !stricmp( command, "npc_attackmytarget" ) || !stricmp( command, "npc_moveto" ) )
	{
		if ( !player->IsAlive() )
			return true;

		if ( gpGlobals->time >= player->m_flNextNpcCommandTime )
			CommandNpcOrder( player, command );
	}
	else if ( !stricmp( command, "hostinfo" ) )
		CommandHostInfo( player );
	else if ( !stricmp( command, "mapinfo" ) )
		CommandMapInfo( player );
	else if ( !stricmp( command, "mapname" ) )
		ClientPrint( pev, HUD_PRINTCONSOLE, STRING( gpGlobals->mapname ) );
	else if ( !stricmp( command, "nextmap" ) )
		ClientPrint( pev, HUD_PRINTCONSOLE, CVAR_GET_STRING( "mp_nextmap_cycle" ) );
	else if ( !stricmp( command, "cl_entitycount" ) || !stricmp( command, "cl_precachecount" ) )
	{
		char text[COMMAND_TEXT_LENGTH + 1];

		if ( !stricmp( command, "cl_entitycount" ) )
			UTIL_FormatString( text, sizeof( text ), "CBaseEntity count: %d of %d\n", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities );
		else
		{
			const int modelLimit = gpGlobals->maxModels;
			UTIL_FormatString( text, sizeof( text ), "Pre-cached model count: %d of %d\n", g_pEngineServer->NumberOfPrecachedModels(), modelLimit );
		}

		ClientPrint( pev, HUD_PRINTCONSOLE, text );
		CLIENT_PRINTF( player->edict(), print_center, text );
	}
	else if ( !stricmp( command, "myinfo" ) )
	{
		if ( CheatsAllowed( player, "My info", false, true ) )
			PrintEntityInfo( player, player );
		else
			PrintPlayerInfo( player, player );
	}
	else if ( !stricmp( command, "thatinfo" ) )
		CommandThatInfo( player );
	else if ( !stricmp( command, "entityinfo" ) )
	{
		if ( CheatsAllowed( player, "Entity info", false, false ) )
			CommandEntityInfo( player );
	}
	else if ( !stricmp( command, "playerinfo" ) )
		CommandPlayerInfo( player );
	else if ( !stricmp( command, "playlist" ) )
	{
		( *g_engfuncs.pfnMessageBegin )( MSG_ONE, gmsgPlaylist, NULL, ENT( pev ) );
			WRITE_STRING( CMD_ARGV( 1 ) );
		MESSAGE_END();
	}
	else if ( !stricmp( command, "time" ) || !stricmp( command, "timeleft" ) )
		CommandTimeLeft( player );
	else if ( !stricmp( command, "monster_status" ) )
	{
		PLAYER_CHEAT_COMMAND( "Monster status", false );
		player->CheatImpulseCommands( 103 );
	}
	else if ( !stricmp( command, "giveall" ) )
	{
		PLAYER_CHEAT_COMMAND( "Give all", true );
		player->CheatImpulseCommands( 101 );
	}
	else if ( !stricmp( command, "frametime" ) )
		ALERT( at_console, "Frametime is: %f\n", gpGlobals->frametime );
	else if ( !stricmp( command, "give" ) )
	{
		PLAYER_CHEAT_COMMAND( "Give", true );

		if ( !strstr( CMD_ARGV( 1 ), "monster_" ) )
			player->GiveNamedItem( STRING( ALLOC_STRING( CMD_ARGV( 1 ) ) ), SF_GIVENITEM, 0 );
	}
	else if ( !stricmp( command, "givepoints" ) )
	{
		PLAYER_CHEAT_COMMAND( "Give points", false );

		if ( CMD_ARGC() < 2 )
		{
			ClientPrint( pev, HUD_PRINTCONSOLE, "Empty points value.\n" );
			return true;
		}

		float points = (float) atof( CMD_ARGV( 1 ) );

		if ( points == 0 )
		{
			ClientPrint( pev, HUD_PRINTCONSOLE, "Invalid points value '%s'.\n", CMD_ARGV( 1 ) );
			return true;
		}

		pev->frags += points;
		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Score adjusted by %f points. New score is %f points.\n", points, pev->frags ) );
	}
	else if ( !stricmp( command, "trigger" ) )
	{
		PLAYER_CHEAT_COMMAND( "Trigger", false );
		CommandTrigger( player );
	}
	else if ( !stricmp( command, "ent_trigger_ahead" ) )
	{
		PLAYER_CHEAT_COMMAND( "Entity trigger ahead", false );

		CBaseEntity *entity = FindEntityForward( player, PLAYER_COMMAND_TRACE_RANGE );

		if ( entity )
			entity->Use( player, player, USE_TOGGLE, 0 );
		else
			CLIENT_PRINTF( player->edict(), print_center, "Entity trigger ahead: No entity found\n" );
	}
	else if ( !stricmp( command, "ent_name_ahead" ) )
	{
		PLAYER_CHEAT_COMMAND( "Entity name ahead", false );

		CBaseEntity *entity = FindEntityForward( player, PLAYER_COMMAND_TRACE_RANGE );

		if ( !entity )
		{
			CLIENT_PRINTF( player->edict(), print_center, "Entity name ahead: No entity found\n" );
			return true;
		}

		const char *text = UTIL_VarArgs( "Entity class name is: %s\nTarget name is: %s\nModel is: %s\n", STRING( entity->pev->classname ),
		                                 STRING( entity->pev->targetname ), STRING( entity->pev->model ) );

		CLIENT_PRINTF( player->edict(), print_center, text );
		CLIENT_PRINTF( player->edict(), print_console, text );
	}
	else if ( !stricmp( command, "ghost" ) )
	{
		PLAYER_CHEAT_COMMAND( "Ghost", false );
		CommandGhost( player );
	}
	else if ( !stricmp( command, "godmode" ) )
	{
		PLAYER_CHEAT_COMMAND( "God mode", true );
		SetPlayerCheatFromArgument( player, PLAYER_CHEAT_GODMODE, "God mode ON\n", "God mode OFF\n" );
	}
	else if ( !stricmp( command, "cl_noclip" ) )
	{
		PLAYER_CHEAT_COMMAND( "No clip", true );
		SetPlayerCheatFromArgument( player, PLAYER_CHEAT_NOCLIP, "No clip ON\n", "No clip OFF\n" );
	}
	else if ( !stricmp( command, "cl_notarget" ) )
	{
		PLAYER_CHEAT_COMMAND( "No target", true );
		SetPlayerCheatFromArgument( player, PLAYER_CHEAT_NOTARGET, "No target ON\n", "No target OFF\n" );
	}
	else if ( !stricmp( command, "cl_nonsolid" ) )
	{
		PLAYER_CHEAT_COMMAND( "Non-solid", true );
		SetPlayerCheatFromArgument( player, PLAYER_CHEAT_NONSOLID, "Non-solid ON\n", "Non-solid OFF\n" );
	}
	else if ( !stricmp( command, "cl_invisible" ) )
	{
		PLAYER_CHEAT_COMMAND( "Invisibility", true );
		SetPlayerCheatFromArgument( player, PLAYER_CHEAT_INVISIBLE, "Invisibility ON\n", "Invisibility OFF\n" );
	}
	else if ( !stricmp( command, "teleport" ) )
	{
		PLAYER_CHEAT_COMMAND( "Teleport", false );
		CommandTeleport( player );
	}
	else if ( !stricmp( command, "reviveme" ) )
	{
		PLAYER_CHEAT_COMMAND( "Self revive", false );

		if ( player->IsAlive() )
			ClientPrint( pev, HUD_PRINTCENTER, "Self revive N/A: Already alive" );
		else
			player->Revive();
	}
	else if ( !stricmp( command, "healme" ) )
	{
		PLAYER_CHEAT_COMMAND( "Heal", true );
		pev->health = pev->max_health;
	}
	else if ( !stricmp( command, "chargeme" ) )
	{
		PLAYER_CHEAT_COMMAND( "HEV charge", true );
		pev->armorvalue = pev->armortype;
	}
	else if ( !stricmp( command, "mygravity" ) )
	{
		PLAYER_CHEAT_COMMAND( "Gravity mod", true );

		bool reset = CMD_ARGC() < 2;

		player->m_flOriginalGravity = reset ? 1.0f : (float) atof( CMD_ARGV( 1 ) );
		ClientPrint( pev, HUD_PRINTCENTER, reset ? "Gravity reset\n" : "Gravity set\n" );
		player->ApplyEffects();
	}
	else if ( !stricmp( command, "myfriction" ) )
	{
		PLAYER_CHEAT_COMMAND( "Friction mod", true );

		bool reset = CMD_ARGC() < 2;

		player->m_flOriginalFriction = reset ? 1.0f : V_min( V_max( (float) atof( CMD_ARGV( 1 ) ), 0.0f ), 1.0f );
		ClientPrint( pev, HUD_PRINTCENTER, reset ? "Friction reset\n" : "Friction set\n" );
		player->ApplyEffects();
	}
	else if ( !stricmp( command, "activeidle" ) )
		ClientPrint( pev, HUD_PRINTCENTER, g_iActiveIdle ? "Active idle ON\n" : "Active idle OFF\n" );
	else if ( !stricmp( command, "toggleextras" ) )
	{
		player->m_fDonorExtras = !player->m_fDonorExtras;
		ClientPrint( pev, HUD_PRINTCENTER, player->m_fDonorExtras ? "Donor extras ON\n" : "Donor extras OFF\n" );
	}
	else if ( !stricmp( command, "togglelongjump" ) )
		CommandToggleLongJump( player );
	else if ( !stricmp( command, "fov" ) )
	{
		if ( CheatsAllowed( player, "fov", false, false ) && CMD_ARGC() > 1 )
			player->m_iFOV = atoi( CMD_ARGV( 1 ) );
		else
			CLIENT_PRINTF( player->edict(), print_console, UTIL_VarArgs( "\"FOV\" is locked at \"%d\"\n", player->m_iFOV ) );
	}
	else if ( !stricmp( command, "lastinv" ) )
	{
		if ( player->HasWeapons() )
			player->SelectLastItem();
	}
	else if ( !stricmp( command, "nav_generate" ) )
	{
		// Navigation map generation is not available
	}
	else if ( !stricmp( command, "submitid" ) )
		CommandSubmitId( player );
	else if ( !stricmp( command, "stuck_kill" ) )
	{
		if ( !player->IsAlive() || player->m_flNextStuckKillTime > gpGlobals->time )
			return true;

		player->m_flNextStuckKillTime = gpGlobals->time + 1.0f;
		pev->health = 0;
		player->Killed( pev, GIB_NOPENALTY );
	}
	else if ( !stricmp( command, "nodepath" ) )
		CommandNodePath();
	else if ( !stricmp( command, "remove" ) )
	{
		PLAYER_CHEAT_COMMAND( "Remove", false );
		CommandRemove();
	}
	else if ( !stricmp( command, "concuss" ) )
	{
		if ( !CheatsAllowed( player, "Concuss", false, false ) )
			return true;

		if ( CMD_ARGC() >= 2 && !strcmp( CMD_ARGV( 1 ), "?" ) )
		{
			ALERT( at_console, "Usage: concuss [amplitude (50.0 - default)] [frequency (1.0 - default)] [fadeTime (1.0 - default)]\n" );
			return true;
		}

		float amplitude = CMD_ARGC() > 1 ? (float) atof( CMD_ARGV( 1 ) ) : 50.0f;
		float frequency = CMD_ARGC() > 2 ? (float) atof( CMD_ARGV( 2 ) ) : 1.0f;
		float fadeTime = CMD_ARGC() > 3 ? (float) atof( CMD_ARGV( 3 ) ) : 1.0f;

		UTIL_ConcussionEffect( player, amplitude, frequency, fadeTime );
	}
	else if ( !stricmp( command, "showextras" ) )
		CommandShowExtras( player );
	else if ( !stricmp( command, "getextras" ) )
		CommandGetExtras( player );
	else if ( !stricmp( command, "hud_reset" ) )
		CommandHudReset( player );
	else
		return false;

#undef PLAYER_CHEAT_COMMAND

	return true;
}

void LogClientCommand( CBasePlayer *player, const char *command )
{
	cvar_t *logging = CVAR_GET_POINTER( "sv_log_client_commands" );

	if ( !logging || logging->value < 1 || !*command )
		return;

	char text[COMMAND_TEXT_LENGTH + 1] = { 0 };
	int count = CMD_ARGC();

	for ( int index = 0; index < count; index++ )
	{
		UTIL_AppendString( text, CMD_ARGV( index ), sizeof( text ) );

		if ( index < count - 1 )
			UTIL_AppendString( text, " ", sizeof( text ) );
	}

	const char *name = STRING( player->pev->netname );

	if ( *name )
		UTIL_LogPrintf( "Command from %s (%s): %s\n", GETPLAYERAUTHID( player->edict() ), name, text );
	else
		UTIL_LogPrintf( "Command from %s: %s\n", GETPLAYERAUTHID( player->edict() ), text );
}

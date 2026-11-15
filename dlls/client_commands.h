#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

class CBaseEntity;
class CBasePlayer;
typedef struct edict_s edict_t;

void Host_Say( edict_s *sender, int teamOnly );
void Host_Say( edict_s *sender, int teamOnly, int argumentCount,
               const char **arguments, const char *command );
bool PerformDebugCommands( CBasePlayer *player );
bool PerformPlayerCommands( CBasePlayer *player, const char *command, bool &logCommand );
void LogClientCommand( CBasePlayer *player, const char *command );
bool FixPlayerStuck( edict_t *entity );
int XtraLevel( CBasePlayer *player, bool force );
void PrintEntityInfo( CBaseEntity *viewer, CBaseEntity *entity );
void PrintPlayerInfo( CBaseEntity *viewer, CBasePlayer *player );
CBaseEntity *ClientCreateEntityByClassname( CBasePlayer *player, const char *className, bool ally, float scale );

#endif

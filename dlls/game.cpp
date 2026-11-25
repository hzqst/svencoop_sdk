/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#include "extdll.h"
#include "CASLog.h"
#include "CASServerManager.h"
#include "CConCommandSystem.h"
#include "CHashStringPool.h"
#include "MapCycle.h"
#include "server_support.h"
#include "server_entvars.h"
#include "CServer.h"
#include "client.h"
#include "CASPluginManager.h"
#include "ASFileSystem.h"
#include "CAdminList.h"
#include "eiface.h"
#include "util.h"
#include "game.h"

cvar_t	displaysoundlist = {"displaysoundlist","0"};

cvar_t mp_weaponfadedelay = { "mp_weaponfadedelay", "60", FCVAR_PROTECTED };
cvar_t mp_classic_mode = { "mp_classic_mode", "-1", FCVAR_PROTECTED };
cvar_t weaponmode_eagle = { "weaponmode_eagle", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_displacer = { "weaponmode_displacer", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_shotgun = { "weaponmode_shotgun", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_9mmhandgun = { "weaponmode_9mmhandgun", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_mp5 = { "weaponmode_mp5", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_crossbow = { "weaponmode_crossbow", "0.0", FCVAR_PROTECTED };
cvar_t weaponmode_rpg = { "weaponmode_rpg", "0.0", FCVAR_PROTECTED };
cvar_t mp_disablegaussjump = { "mp_disablegaussjump", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_banana = { "mp_banana", "1.0", FCVAR_SERVER };
cvar_t mp_dropweapons = { "mp_dropweapons", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_modelselection = { "mp_modelselection", "1", FCVAR_PROTECTED };
cvar_t mp_allowmonsterinfo = { "mp_allowmonsterinfo", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_allowplayerinfo = { "mp_allowplayerinfo", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_timelimit_empty = { "mp_timelimit_empty", "10", FCVAR_PROTECTED };
cvar_t timeleft_empty = { "mp_timeleft_empty", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nomedkit = { "mp_nomedkit", "0.0", FCVAR_PROTECTED };
cvar_t mp_disable_pcbalancing = { "mp_disable_pcbalancing", "0", FCVAR_PROTECTED };
cvar_t mp_disable_medkit_points = { "mp_disable_medkit_points", "0", FCVAR_PROTECTED };
cvar_t mp_pcbalancing_factorlist = { "mp_pcbalancing_factorlist", "", FCVAR_PROTECTED };
cvar_t sv_notransition = { "sv_notransition", "", FCVAR_PROTECTED };
cvar_t mp_barnacle_paralyze = { "mp_barnacle_paralyze", "0", FCVAR_PROTECTED };
cvar_t mp_disable_autoclimb = { "mp_disable_autoclimb", "0", FCVAR_PROTECTED };
cvar_t mp_disable_player_rappel = { "mp_disable_player_rappel", "0", FCVAR_PROTECTED };
cvar_t mp_grapple_mode = { "mp_grapple_mode", "0", FCVAR_PROTECTED };
cvar_t mp_no_akimbo_uzis = { "mp_no_akimbo_uzis", "0", FCVAR_PROTECTED };
cvar_t mp_noblastgibs = { "mp_noblastgibs", "0", FCVAR_PROTECTED };
cvar_t mp_telefrag = { "mp_telefrag", "0", FCVAR_PROTECTED };
cvar_t mp_item_respawndelay = { "mp_item_respawndelay", "0", FCVAR_PROTECTED };
cvar_t mp_startoffset = { "mp_startoffset", "0", FCVAR_PROTECTED };
cvar_t mp_respawndelay = { "mp_respawndelay", "6", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_suitpower = { "mp_suitpower", "1", FCVAR_PROTECTED };
cvar_t mp_weapon_droprules = { "mp_weapon_droprules", "1", FCVAR_PROTECTED };
cvar_t mp_ammo_droprules = { "mp_ammo_droprules", "1", FCVAR_PROTECTED };
cvar_t mp_monsterpoints = { "mp_monsterpoints", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };

cvar_t mp_nextmap = { "mp_nextmap", "", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_ignore_mapcfg = { "mp_nextmap_ignore_mapcfg", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_parted_mapcfg_limit = { "mp_nextmap_parted_mapcfg_limit", "-1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_cycle = { "mp_nextmap_cycle", "", FCVAR_SERVER | FCVAR_UNLOGGED };
cvar_t mp_nextmap_cycle_minp = { "mp_nextmap_cycle_minp", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_cycle_maxp = { "mp_nextmap_cycle_maxp", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_cycle_cmds = { "mp_nextmap_cycle_cmds", "", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_nextmap_cycle_rules = { "mp_nextmap_cycle_rules", "", FCVAR_PROTECTED | FCVAR_UNLOGGED };

cvar_t mp_survival_mode = { "mp_survival_mode", "1", FCVAR_PROTECTED };
cvar_t mp_survival_supported = { "mp_survival_supported", "0", FCVAR_PROTECTED };
cvar_t mp_survival_minplayers = { "mp_survival_minplayers", "1", FCVAR_PROTECTED };
cvar_t mp_survival_voteallow = { "mp_survival_voteallow", "1", FCVAR_PROTECTED };
cvar_t mp_survival_retries = { "mp_survival_retries", "3", FCVAR_PROTECTED };
cvar_t mp_survival_nextmap = { "mp_survival_nextmap", "", FCVAR_PROTECTED };
cvar_t mp_survival_starton = { "mp_survival_starton", "1", FCVAR_PROTECTED };
cvar_t mp_survival_startdelay = { "mp_survival_startdelay", "30", FCVAR_PROTECTED };

cvar_t mp_voteallow = { "mp_voteallow", "1", FCVAR_PROTECTED };
cvar_t mp_votetimebetween = { "mp_votetimebetween", "40", FCVAR_PROTECTED };
cvar_t mp_votetimecheck = { "mp_votetimecheck", "16", FCVAR_PROTECTED };
cvar_t mp_votemaprequired = { "mp_votemaprequired", "67", FCVAR_PROTECTED };
cvar_t mp_votekillrequired = { "mp_votekillrequired", "51", FCVAR_PROTECTED };
cvar_t mp_votebanrequired = { "mp_votebanrequired", "-1", FCVAR_PROTECTED };
cvar_t mp_votekickrequired = { "mp_votekickrequired", "80", FCVAR_PROTECTED };
cvar_t mp_voteclassicmoderequired = { "mp_voteclassicmoderequired", "51", FCVAR_PROTECTED };
cvar_t mp_votesurvivalmoderequired = { "mp_votesurvivalmoderequired", "51", FCVAR_PROTECTED };
cvar_t mapvotecfgfile = { "mapvotecfgfile", "mapvote.cfg", FCVAR_SERVER | FCVAR_UNLOGGED };
cvar_t mp_votekill_respawndelay = { "mp_votekill_respawndelay", "15.0", FCVAR_PROTECTED };
cvar_t mp_playervotedelay = { "mp_playervotedelay", "120.0", FCVAR_PROTECTED };
cvar_t mp_bantime = { "mp_bantime", "1440", FCVAR_PROTECTED };
cvar_t mp_playerinactive = { "mp_playerinactive", "30.0", FCVAR_PROTECTED };
cvar_t mp_kickbantime = { "mp_kickbantime", "25", FCVAR_PROTECTED };

cvar_t mp_observer_mode = { "mp_observer_mode", "0", FCVAR_PROTECTED };
cvar_t mp_observer_cyclic = { "mp_observer_cyclic", "0", FCVAR_PROTECTED };

cvar_t mp_multiplespawn = { "mp_multiplespawn", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t mp_hevsuit_voice = { "mp_hevsuit_voice", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };

cvar_t mp_weapon_respawndelay = { "mp_weapon_respawndelay", "-2", FCVAR_PROTECTED };
cvar_t mp_ammo_respawndelay = { "mp_ammo_respawndelay", "-2", FCVAR_PROTECTED };

cvar_t skillcfgfile = { "skillcfgfile", "skill.cfg", FCVAR_SERVER | FCVAR_UNLOGGED };
cvar_t sv_disable_online_api = { "sv_disable_online_api", "0", FCVAR_SERVER };

// multiplayer server rules
cvar_t	fragsleft	= {"mp_fragsleft","0", FCVAR_SERVER | FCVAR_UNLOGGED };	  // Don't spam console/log files/users with this changing
cvar_t	timeleft	= {"mp_timeleft","0" , FCVAR_SERVER | FCVAR_UNLOGGED };	  // "      "

// multiplayer server rules
cvar_t	teamplay	= {"mp_teamplay","0", FCVAR_SERVER };
cvar_t	fraglimit	= {"mp_fraglimit","0", FCVAR_SERVER };
cvar_t	timelimit	= {"mp_timelimit","0", FCVAR_SERVER };
cvar_t	friendlyfire= {"mp_friendlyfire","0", FCVAR_SERVER };
cvar_t	npckilling = {"mp_npckill", "1", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t npc_dropweapons = { "npc_dropweapons", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t sv_ai_enemy_detection_mode = { "sv_ai_enemy_detection_mode", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t sv_report_unknown_entities = { "sv_report_unknown_entities", "0", FCVAR_PROTECTED | FCVAR_UNLOGGED };
cvar_t sv_log_monster_damage = { "sv_log_monster_damage", "1.0", FCVAR_PROTECTED };
cvar_t sv_log_monster_monster_kills = { "sv_log_monster_monster_kills", "0.0", FCVAR_PROTECTED };
cvar_t sv_log_monster_deaths = { "sv_log_monster_deaths", "0.0", FCVAR_PROTECTED };
cvar_t sv_log_player_deaths = { "sv_log_player_deaths", "1.0", FCVAR_PROTECTED };
cvar_t	falldamage	= {"mp_falldamage","0", FCVAR_SERVER };
cvar_t	weaponstay	= {"mp_weaponstay","0", FCVAR_SERVER };
cvar_t	forcerespawn= {"mp_forcerespawn","1", FCVAR_SERVER };
cvar_t	flashlight	= {"mp_flashlight","0", FCVAR_SERVER };
cvar_t	aimcrosshair= {"mp_autocrosshair","1", FCVAR_SERVER };
cvar_t	decalfrequency = {"decalfrequency","30", FCVAR_SERVER };
cvar_t	teamlist = {"mp_teamlist","hgrunt;scientist", FCVAR_SERVER };
cvar_t	teamoverride = {"mp_teamoverride","1" };
cvar_t	defaultteam = {"mp_defaultteam","0" };
cvar_t	allowmonsters={"mp_allowmonsters","0", FCVAR_SERVER };

cvar_t  allow_spectators = { "allow_spectators", "0.0", FCVAR_SERVER };		// 0 prevents players from being spectators

cvar_t  mp_chattime = {"mp_chattime","10", FCVAR_SERVER };
cvar_t	mp_keep_globalstate = {"mp_keep_globalstate", "0", FCVAR_PROTECTED};

// Engine Cvars
cvar_t 	*g_psv_gravity = NULL;
cvar_t	*g_psv_aim = NULL;
cvar_t	*g_footsteps = NULL;

//CVARS FOR SKILL LEVEL SETTINGS
cvar_t sk_agrunt_health = {"sk_agrunt_health", "0", FCVAR_PROTECTED};
cvar_t sk_agrunt_dmg_punch = {"sk_agrunt_dmg_punch", "0", FCVAR_PROTECTED};
cvar_t sk_agrunt_melee_engage_distance = {"sk_agrunt_melee_engage_distance", "0", FCVAR_PROTECTED};
cvar_t sk_agrunt_berserker_dmg_punch = {"sk_agrunt_berserker_dmg_punch", "0", FCVAR_PROTECTED};

cvar_t sk_apache_health = {"sk_apache_health", "0", FCVAR_PROTECTED};
cvar_t sk_apache_enable_rockets = {"sk_apache_enable_rockets", "1", FCVAR_PROTECTED};

cvar_t sk_barney_health = {"sk_barney_health", "0", FCVAR_PROTECTED};

cvar_t sk_barnacle_health = {"sk_barnacle_health", "0", FCVAR_PROTECTED};
cvar_t sk_barnacle_bite = {"sk_barnacle_bite", "0", FCVAR_PROTECTED};

cvar_t sk_bullsquid_health = {"sk_bullsquid_health", "0", FCVAR_PROTECTED};
cvar_t sk_bullsquid_dmg_bite = {"sk_bullsquid_dmg_bite", "0", FCVAR_PROTECTED};
cvar_t sk_bullsquid_dmg_whip = {"sk_bullsquid_dmg_whip", "0", FCVAR_PROTECTED};
cvar_t sk_bullsquid_dmg_spit = {"sk_bullsquid_dmg_spit", "0", FCVAR_PROTECTED};

cvar_t sk_bigmomma_health_factor = {"sk_bigmomma_health_factor", "1.0", FCVAR_PROTECTED};
cvar_t sk_bigmomma_dmg_slash = {"sk_bigmomma_dmg_slash", "50", FCVAR_PROTECTED};
cvar_t sk_bigmomma_dmg_blast = {"sk_bigmomma_dmg_blast", "100", FCVAR_PROTECTED};
cvar_t sk_bigmomma_radius_blast = {"sk_bigmomma_radius_blast", "250", FCVAR_PROTECTED};

cvar_t sk_gargantua_health = {"sk_gargantua_health", "0", FCVAR_PROTECTED};
cvar_t sk_gargantua_dmg_slash = {"sk_gargantua_dmg_slash", "0", FCVAR_PROTECTED};
cvar_t sk_gargantua_dmg_fire = {"sk_gargantua_dmg_fire", "0", FCVAR_PROTECTED};
cvar_t sk_gargantua_dmg_stomp = {"sk_gargantua_dmg_stomp", "0", FCVAR_PROTECTED};

cvar_t sk_babygargantua_health = {"sk_babygargantua_health", "0", FCVAR_PROTECTED};
cvar_t sk_babygargantua_dmg_slash = {"sk_babygargantua_dmg_slash", "0", FCVAR_PROTECTED};
cvar_t sk_babygargantua_dmg_fire = {"sk_babygargantua_dmg_fire", "0", FCVAR_PROTECTED};
cvar_t sk_babygargantua_dmg_stomp = {"sk_babygargantua_dmg_stomp", "0", FCVAR_PROTECTED};

cvar_t sk_hassassin_health = {"sk_hassassin_health", "0", FCVAR_PROTECTED};
cvar_t sk_hassassin_enable_cloaking = {"sk_hassassin_enable_cloaking", "0", FCVAR_PROTECTED};

cvar_t sk_headcrab_health = {"sk_headcrab_health", "0", FCVAR_PROTECTED};
cvar_t sk_headcrab_dmg_bite = {"sk_headcrab_dmg_bite", "0", FCVAR_PROTECTED};

cvar_t sk_hgrunt_health = {"sk_hgrunt_health", "0", FCVAR_PROTECTED};
cvar_t sk_hgrunt_kick = {"sk_hgrunt_kick", "0", FCVAR_PROTECTED};
cvar_t sk_hgrunt_pellets = {"sk_hgrunt_pellets", "0", FCVAR_PROTECTED};
cvar_t sk_hgrunt_gspeed = {"sk_hgrunt_gspeed", "0", FCVAR_PROTECTED};

cvar_t sk_houndeye_health = {"sk_houndeye_health", "0", FCVAR_PROTECTED};
cvar_t sk_houndeye_dmg_blast = {"sk_houndeye_dmg_blast", "0", FCVAR_PROTECTED};

cvar_t sk_islave_health = {"sk_islave_health", "0", FCVAR_PROTECTED};
cvar_t sk_islave_dmg_claw = {"sk_islave_dmg_claw", "0", FCVAR_PROTECTED};
cvar_t sk_islave_dmg_clawrake = {"sk_islave_dmg_clawrake", "0", FCVAR_PROTECTED};
cvar_t sk_islave_dmg_zap = {"sk_islave_dmg_zap", "0", FCVAR_PROTECTED};
cvar_t sk_islave_speed_zap = {"sk_islave_speed_zap", "1", FCVAR_PROTECTED};

cvar_t sk_ichthyosaur_health = {"sk_ichthyosaur_health", "0", FCVAR_PROTECTED};
cvar_t sk_ichthyosaur_shake = {"sk_ichthyosaur_shake", "0", FCVAR_PROTECTED};

cvar_t sk_leech_health = {"sk_leech_health", "0", FCVAR_PROTECTED};
cvar_t sk_leech_dmg_bite = {"sk_leech_dmg_bite", "0", FCVAR_PROTECTED};

cvar_t sk_controller_health = {"sk_controller_health", "0", FCVAR_PROTECTED};
cvar_t sk_controller_dmgzap = {"sk_controller_dmgzap", "0", FCVAR_PROTECTED};
cvar_t sk_controller_speedball = {"sk_controller_speedball", "0", FCVAR_PROTECTED};
cvar_t sk_controller_dmgball = {"sk_controller_dmgball", "0", FCVAR_PROTECTED};

cvar_t sk_nihilanth_health = {"sk_nihilanth_health", "0", FCVAR_PROTECTED};
cvar_t sk_nihilanth_zap = {"sk_nihilanth_zap", "0", FCVAR_PROTECTED};

cvar_t sk_scientist_health = {"sk_scientist_health", "0", FCVAR_PROTECTED};

cvar_t sk_snark_health = {"sk_snark_health", "0", FCVAR_PROTECTED};
cvar_t sk_snark_dmg_bite = {"sk_snark_dmg_bite", "0", FCVAR_PROTECTED};
cvar_t sk_snark_dmg_pop = {"sk_snark_dmg_pop", "0", FCVAR_PROTECTED};

cvar_t sk_zombie_health = {"sk_zombie_health", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_dmg_one_slash = {"sk_zombie_dmg_one_slash", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_dmg_both_slash = {"sk_zombie_dmg_both_slash", "0", FCVAR_PROTECTED};

cvar_t sk_tor_health = {"sk_tor_health", "0", FCVAR_PROTECTED};
cvar_t sk_tor_punch = {"sk_tor_punch", "0", FCVAR_PROTECTED};
cvar_t sk_tor_energybeam = {"sk_tor_energybeam", "0", FCVAR_PROTECTED};
cvar_t sk_tor_sonicblast = {"sk_tor_sonicblast", "0", FCVAR_PROTECTED};

cvar_t sk_turret_health = {"sk_turret_health", "0", FCVAR_PROTECTED};

cvar_t sk_miniturret_health = {"sk_miniturret_health", "0", FCVAR_PROTECTED};

cvar_t sk_sentry_health = {"sk_sentry_health", "0", FCVAR_PROTECTED};

cvar_t sk_hwgrunt_health = {"sk_hwgrunt_health", "0", FCVAR_PROTECTED};
cvar_t sk_hwgrunt_minipellets = {"sk_hwgrunt_minipellets", "0", FCVAR_PROTECTED};

cvar_t sk_rgrunt_explode = {"sk_rgrunt_explode", "0", FCVAR_PROTECTED};

cvar_t sk_massassin_sniper = {"sk_massassin_sniper", "0", FCVAR_PROTECTED};

cvar_t sk_grunt_buckshot = {"sk_grunt_buckshot", "0", FCVAR_PROTECTED};
cvar_t sk_grunt_handgrenades = {"sk_grunt_handgrenades", "1", FCVAR_PROTECTED};

cvar_t sk_otis_health = {"sk_otis_health", "0", FCVAR_PROTECTED};
cvar_t sk_otis_bullet = {"sk_otis_bullet", "0", FCVAR_PROTECTED};

cvar_t sk_zombie_barney_health = {"sk_zombie_barney_health", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_barney_dmg_one_slash = {"sk_zombie_barney_dmg_one_slash", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_barney_dmg_both_slash = {"sk_zombie_barney_dmg_both_slash", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_soldier_health = {"sk_zombie_soldier_health", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_soldier_dmg_one_slash = {"sk_zombie_soldier_dmg_one_slash", "0", FCVAR_PROTECTED};
cvar_t sk_zombie_soldier_dmg_both_slash = {"sk_zombie_soldier_dmg_both_slash", "0", FCVAR_PROTECTED};

cvar_t sk_gonome_health = {"sk_gonome_health", "0", FCVAR_PROTECTED};
cvar_t sk_gonome_dmg_one_slash = {"sk_gonome_dmg_one_slash", "0", FCVAR_PROTECTED};
cvar_t sk_gonome_dmg_guts = {"sk_gonome_dmg_guts", "0", FCVAR_PROTECTED};
cvar_t sk_gonome_dmg_one_bite = {"sk_gonome_dmg_one_bite", "0", FCVAR_PROTECTED};

cvar_t sk_shocktrooper_health = {"sk_shocktrooper_health", "0", FCVAR_PROTECTED};
cvar_t sk_shocktrooper_kick = {"sk_shocktrooper_kick", "0", FCVAR_PROTECTED};
cvar_t sk_shocktrooper_maxcharge = {"sk_shocktrooper_maxcharge", "0", FCVAR_PROTECTED};
cvar_t sk_shocktrooper_enable_spores = {"sk_shocktrooper_enable_spores", "1", FCVAR_PROTECTED};

cvar_t sk_pitdrone_health = {"sk_pitdrone_health", "0", FCVAR_PROTECTED};
cvar_t sk_pitdrone_dmg_whip = {"sk_pitdrone_dmg_whip", "0", FCVAR_PROTECTED};
cvar_t sk_pitdrone_dmg_bite = {"sk_pitdrone_dmg_bite", "0", FCVAR_PROTECTED};
cvar_t sk_pitdrone_dmg_spit = {"sk_pitdrone_dmg_spit", "0", FCVAR_PROTECTED};

cvar_t sk_voltigore_health = {"sk_voltigore_health", "0", FCVAR_PROTECTED};
cvar_t sk_voltigore_dmg_punch = {"sk_voltigore_dmg_punch", "0", FCVAR_PROTECTED};
cvar_t sk_voltigore_dmg_beam = {"sk_voltigore_dmg_beam", "0", FCVAR_PROTECTED};
cvar_t sk_voltigore_dmg_explode = {"sk_voltigore_dmg_explode", "0", FCVAR_PROTECTED};

cvar_t sk_sqknest_health = {"sk_sqknest_health", "0", FCVAR_PROTECTED};

cvar_t sk_shockroach_dmg_xpl_touch = {"sk_shockroach_dmg_xpl_touch", "0", FCVAR_PROTECTED};
cvar_t sk_shockroach_dmg_xpl_splash = {"sk_shockroach_dmg_xpl_splash", "0", FCVAR_PROTECTED};

cvar_t sk_kingpin_health = {"sk_kingpin_health", "0", FCVAR_PROTECTED};
cvar_t sk_kingpin_lightning = {"sk_kingpin_lightning", "0", FCVAR_PROTECTED};
cvar_t sk_kingpin_tele_blast = {"sk_kingpin_tele_blast", "0", FCVAR_PROTECTED};
cvar_t sk_kingpin_plasma_blast = {"sk_kingpin_plasma_blast", "0", FCVAR_PROTECTED};
cvar_t sk_kingpin_melee = {"sk_kingpin_melee", "0", FCVAR_PROTECTED};
cvar_t sk_kingpin_telefrag = {"sk_kingpin_telefrag", "0", FCVAR_PROTECTED};

cvar_t sk_mortar = {"sk_mortar", "200", FCVAR_PROTECTED};

cvar_t sk_plr_HpMedic = {"sk_plr_HpMedic", "0", FCVAR_PROTECTED};
cvar_t sk_plr_crowbar = {"sk_plr_crowbar", "0", FCVAR_PROTECTED};
cvar_t sk_plr_wrench = {"sk_plr_wrench", "0", FCVAR_PROTECTED};
cvar_t sk_plr_grapple = {"sk_plr_grapple", "0", FCVAR_PROTECTED};
cvar_t sk_plr_762_bullet = {"sk_plr_762_bullet", "0", FCVAR_PROTECTED};
cvar_t sk_plr_9mm_bullet = {"sk_plr_9mm_bullet", "0", FCVAR_PROTECTED};
cvar_t sk_plr_357_bullet = {"sk_plr_357_bullet", "0", FCVAR_PROTECTED};
cvar_t sk_plr_9mmAR_bullet = {"sk_plr_9mmAR_bullet", "0", FCVAR_PROTECTED};
cvar_t sk_plr_9mmAR_grenade = {"sk_plr_9mmAR_grenade", "0", FCVAR_PROTECTED};
cvar_t sk_plr_buckshot = {"sk_plr_buckshot", "0", FCVAR_PROTECTED};
cvar_t sk_plr_xbow_bolt_client = {"sk_plr_xbow_bolt_client", "0", FCVAR_PROTECTED};
cvar_t sk_plr_xbow_bolt_monster = {"sk_plr_xbow_bolt_monster", "0", FCVAR_PROTECTED};
cvar_t sk_plr_rpg = {"sk_plr_rpg", "0", FCVAR_PROTECTED};
cvar_t sk_plr_gauss = {"sk_plr_gauss", "0", FCVAR_PROTECTED};
cvar_t sk_plr_egon_narrow = {"sk_plr_egon_narrow", "0", FCVAR_PROTECTED};
cvar_t sk_plr_egon_wide = {"sk_plr_egon_wide", "0", FCVAR_PROTECTED};
cvar_t sk_plr_hand_grenade = {"sk_plr_hand_grenade", "0", FCVAR_PROTECTED};
cvar_t sk_plr_satchel = {"sk_plr_satchel", "0", FCVAR_PROTECTED};
cvar_t sk_plr_tripmine = {"sk_plr_tripmine", "0", FCVAR_PROTECTED};
cvar_t sk_plr_spore = {"sk_plr_spore", "0", FCVAR_PROTECTED};
cvar_t sk_plr_uzi = {"sk_plr_uzi", "0", FCVAR_PROTECTED};
cvar_t sk_plr_secondarygauss = {"sk_plr_secondarygauss", "0", FCVAR_PROTECTED};
cvar_t sk_plr_shockrifle = {"sk_plr_shockrifle", "0", FCVAR_PROTECTED};
cvar_t sk_plr_shockrifle_beam = {"sk_plr_shockrifle_beam", "0", FCVAR_PROTECTED};
cvar_t sk_plr_shockrifle_recharge = {"sk_plr_shockrifle_recharge", "6.667", FCVAR_PROTECTED};
cvar_t sk_plr_displacer_other = {"sk_plr_displacer_other", "0", FCVAR_PROTECTED};
cvar_t sk_plr_displacer_radius = {"sk_plr_displacer_radius", "0", FCVAR_PROTECTED};

cvar_t sk_12mm_bullet = {"sk_12mm_bullet", "0", FCVAR_PROTECTED};

cvar_t sk_9mmAR_bullet = {"sk_9mmAR_bullet", "0", FCVAR_PROTECTED};

cvar_t sk_9mm_bullet = {"sk_9mm_bullet", "0", FCVAR_PROTECTED};

cvar_t sk_556_bullet = {"sk_556_bullet", "0", FCVAR_PROTECTED};

cvar_t sk_hornet_dmg = {"sk_hornet_dmg", "0", FCVAR_PROTECTED};
cvar_t sk_hornet_pdmg = {"sk_hornet_pdmg", "0", FCVAR_PROTECTED};

cvar_t sk_tentacle = {"sk_tentacle", "0", FCVAR_PROTECTED};

cvar_t sk_blkopsosprey = {"sk_blkopsosprey", "0", FCVAR_PROTECTED};

cvar_t sk_osprey = {"sk_osprey", "0", FCVAR_PROTECTED};

cvar_t sk_stukabat = {"sk_stukabat", "0", FCVAR_PROTECTED};
cvar_t sk_stukabat_dmg_bite = {"sk_stukabat_dmg_bite", "0", FCVAR_PROTECTED};

cvar_t sk_suitcharger = {"sk_suitcharger", "0", FCVAR_PROTECTED};

cvar_t sk_battery = {"sk_battery", "0", FCVAR_PROTECTED};

cvar_t sk_healthcharger = {"sk_healthcharger", "0", FCVAR_PROTECTED};

cvar_t sk_healthkit = {"sk_healthkit", "0", FCVAR_PROTECTED};

cvar_t sk_scientist_heal = {"sk_scientist_heal", "0", FCVAR_PROTECTED};

cvar_t sk_monster_head = {"sk_monster_head", "2", FCVAR_PROTECTED};
cvar_t sk_monster_chest = {"sk_monster_chest", "1", FCVAR_PROTECTED};
cvar_t sk_monster_stomach = {"sk_monster_stomach", "1", FCVAR_PROTECTED};
cvar_t sk_monster_arm = {"sk_monster_arm", "1", FCVAR_PROTECTED};
cvar_t sk_monster_leg = {"sk_monster_leg", "1", FCVAR_PROTECTED};

cvar_t sk_player_head = {"sk_player_head", "2", FCVAR_PROTECTED};
cvar_t sk_player_chest = {"sk_player_chest", "1", FCVAR_PROTECTED};
cvar_t sk_player_stomach = {"sk_player_stomach", "1", FCVAR_PROTECTED};
cvar_t sk_player_arm = {"sk_player_arm", "1", FCVAR_PROTECTED};
cvar_t sk_player_leg = {"sk_player_leg", "1", FCVAR_PROTECTED};

// END Cvars for Skill Level settings

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit( void )
{
	// Register cvars here:

	g_psv_gravity = CVAR_GET_POINTER( "sv_gravity" );
	g_psv_aim = CVAR_GET_POINTER( "sv_aim" );
	g_footsteps = CVAR_GET_POINTER( "mp_footsteps" );

	CVAR_REGISTER (&displaysoundlist);
	CVAR_REGISTER( &mp_weaponfadedelay );
	CVAR_REGISTER( &mp_classic_mode );
	CVAR_REGISTER( &weaponmode_eagle );
	CVAR_REGISTER( &weaponmode_displacer );
	CVAR_REGISTER( &weaponmode_shotgun );
	CVAR_REGISTER( &weaponmode_9mmhandgun );
	CVAR_REGISTER( &weaponmode_mp5 );
	CVAR_REGISTER( &weaponmode_crossbow );
	CVAR_REGISTER( &weaponmode_rpg );
	CVAR_REGISTER( &mp_disablegaussjump );
	CVAR_REGISTER( &mp_banana );
	CVAR_REGISTER( &mp_dropweapons );
	CVAR_REGISTER( &mp_modelselection );
	CVAR_REGISTER( &mp_allowmonsterinfo );
	CVAR_REGISTER( &mp_allowplayerinfo );
	CVAR_REGISTER( &mp_timelimit_empty );
	CVAR_REGISTER( &timeleft_empty );
	CVAR_REGISTER( &mp_nomedkit );
	CVAR_REGISTER( &mp_disable_pcbalancing );
	CVAR_REGISTER( &mp_disable_medkit_points );
	CVAR_REGISTER( &mp_pcbalancing_factorlist );
	CVAR_REGISTER( &sv_notransition );
	CVAR_REGISTER( &mp_barnacle_paralyze );
	CVAR_REGISTER( &mp_disable_autoclimb );
	CVAR_REGISTER( &mp_disable_player_rappel );
	CVAR_REGISTER( &mp_grapple_mode );
	CVAR_REGISTER( &mp_no_akimbo_uzis );
	CVAR_REGISTER( &mp_noblastgibs );
	CVAR_REGISTER( &mp_telefrag );
	CVAR_REGISTER( &mp_item_respawndelay );
	CVAR_REGISTER( &mp_startoffset );
	CVAR_REGISTER( &mp_respawndelay );
	CVAR_REGISTER( &mp_suitpower );
	CVAR_REGISTER( &mp_weapon_droprules );
	CVAR_REGISTER( &mp_ammo_droprules );
	CVAR_REGISTER( &mp_monsterpoints );

	CVAR_REGISTER( &mp_nextmap );
	CVAR_REGISTER( &mp_nextmap_ignore_mapcfg );
	CVAR_REGISTER( &mp_nextmap_parted_mapcfg_limit );
	CVAR_REGISTER( &mp_nextmap_cycle );
	CVAR_REGISTER( &mp_nextmap_cycle_minp );
	CVAR_REGISTER( &mp_nextmap_cycle_maxp );
	CVAR_REGISTER( &mp_nextmap_cycle_cmds );
	CVAR_REGISTER( &mp_nextmap_cycle_rules );

	CVAR_REGISTER( &mp_survival_mode );
	CVAR_REGISTER( &mp_survival_supported );
	CVAR_REGISTER( &mp_survival_minplayers );
	CVAR_REGISTER( &mp_survival_voteallow );
	CVAR_REGISTER( &mp_survival_retries );
	CVAR_REGISTER( &mp_survival_nextmap );
	CVAR_REGISTER( &mp_survival_starton );
	CVAR_REGISTER( &mp_survival_startdelay );

	CVAR_REGISTER( &mp_voteallow );
	CVAR_REGISTER( &mp_votetimebetween );
	CVAR_REGISTER( &mp_votetimecheck );
	CVAR_REGISTER( &mp_votemaprequired );
	CVAR_REGISTER( &mp_votekillrequired );
	CVAR_REGISTER( &mp_votebanrequired );
	CVAR_REGISTER( &mp_votekickrequired );
	CVAR_REGISTER( &mp_voteclassicmoderequired );
	CVAR_REGISTER( &mp_votesurvivalmoderequired );
	CVAR_REGISTER( &mapvotecfgfile );
	CVAR_REGISTER( &mp_votekill_respawndelay );
	CVAR_REGISTER( &mp_playervotedelay );
	CVAR_REGISTER( &mp_bantime );
	CVAR_REGISTER( &mp_playerinactive );
	CVAR_REGISTER( &mp_kickbantime );

	CVAR_REGISTER( &mp_observer_mode );
	CVAR_REGISTER( &mp_observer_cyclic );

	CVAR_REGISTER( &mp_multiplespawn );
	CVAR_REGISTER( &mp_hevsuit_voice );

	CVAR_REGISTER( &mp_weapon_respawndelay );
	CVAR_REGISTER( &mp_ammo_respawndelay );
	CVAR_REGISTER( &allow_spectators );
	CVAR_REGISTER(&plugin_list_file);
	CVAR_REGISTER(&as_log_level);
	CVAR_REGISTER(&as_script_log_max);
	CVAR_REGISTER(&as_file_size_quota);
	CVAR_REGISTER(&as_compiler_treat_warnings_as_errors);

	CVAR_REGISTER( &skillcfgfile );
	CVAR_REGISTER( &adminsfile );
	CVAR_REGISTER( &adminsicon );

	CVAR_REGISTER (&teamplay);
	CVAR_REGISTER (&fraglimit);
	CVAR_REGISTER (&timelimit);

	CVAR_REGISTER (&fragsleft);
	CVAR_REGISTER (&timeleft);

	CVAR_REGISTER (&friendlyfire);
	CVAR_REGISTER (&npckilling);
	CVAR_REGISTER( &npc_dropweapons );
	CVAR_REGISTER( &sv_ai_enemy_detection_mode );
	CVAR_REGISTER( &sv_report_unknown_entities );
	CVAR_REGISTER( &sv_log_monster_damage );
	CVAR_REGISTER( &sv_log_monster_monster_kills );
	CVAR_REGISTER( &sv_log_monster_deaths );
	CVAR_REGISTER( &sv_log_player_deaths );
	CVAR_REGISTER (&falldamage);
	CVAR_REGISTER (&weaponstay);
	CVAR_REGISTER (&forcerespawn);
	CVAR_REGISTER (&flashlight);
	CVAR_REGISTER (&aimcrosshair);
	CVAR_REGISTER (&decalfrequency);
	CVAR_REGISTER (&teamlist);
	CVAR_REGISTER (&teamoverride);
	CVAR_REGISTER (&defaultteam);
	CVAR_REGISTER (&allowmonsters);

	CVAR_REGISTER (&mp_chattime);

// REGISTER CVARS FOR SKILL LEVEL STUFF
	// Agrunt
	CVAR_REGISTER ( &mp_keep_globalstate);// {"sk_agrunt_health1","0"};
	CVAR_REGISTER ( &sk_plr_HpMedic);// {"sk_agrunt_health2","0"};
	CVAR_REGISTER ( &sk_agrunt_health);// {"sk_agrunt_health3","0"};

	CVAR_REGISTER ( &sk_agrunt_dmg_punch);// {"sk_agrunt_dmg_punch1","0"};
	CVAR_REGISTER ( &sk_agrunt_melee_engage_distance);// {"sk_agrunt_dmg_punch2","0"};
	CVAR_REGISTER ( &sk_agrunt_berserker_dmg_punch);// {"sk_agrunt_dmg_punch3","0"};

	// Apache
	CVAR_REGISTER ( &sk_apache_health);// {"sk_apache_health1","0"};
	CVAR_REGISTER ( &sk_apache_enable_rockets);// {"sk_apache_health2","0"};
	CVAR_REGISTER ( &sk_barney_health);// {"sk_apache_health3","0"};

	// Barney
	CVAR_REGISTER ( &sk_barnacle_health);// {"sk_barney_health1","0"};
	CVAR_REGISTER ( &sk_barnacle_bite);// {"sk_barney_health2","0"};
	CVAR_REGISTER ( &sk_bullsquid_health);// {"sk_barney_health3","0"};

	// Bullsquid
	CVAR_REGISTER ( &sk_bullsquid_dmg_bite);// {"sk_bullsquid_health1","0"};
	CVAR_REGISTER ( &sk_bullsquid_dmg_whip);// {"sk_bullsquid_health2","0"};
	CVAR_REGISTER ( &sk_bullsquid_dmg_spit);// {"sk_bullsquid_health3","0"};
	 
	CVAR_REGISTER ( &sk_bigmomma_health_factor);// {"sk_bullsquid_dmg_bite1","0"};
	CVAR_REGISTER ( &sk_bigmomma_dmg_slash);// {"sk_bullsquid_dmg_bite2","0"};
	CVAR_REGISTER ( &sk_bigmomma_dmg_blast);// {"sk_bullsquid_dmg_bite3","0"};

	CVAR_REGISTER ( &sk_bigmomma_radius_blast);// {"sk_bullsquid_dmg_whip1","0"};
	CVAR_REGISTER ( &sk_gargantua_health);// {"sk_bullsquid_dmg_whip2","0"};
	CVAR_REGISTER ( &sk_gargantua_dmg_slash);// {"sk_bullsquid_dmg_whip3","0"};

	CVAR_REGISTER ( &sk_gargantua_dmg_fire);// {"sk_bullsquid_dmg_spit1","0"};
	CVAR_REGISTER ( &sk_gargantua_dmg_stomp);// {"sk_bullsquid_dmg_spit2","0"};
	CVAR_REGISTER ( &sk_babygargantua_health);// {"sk_bullsquid_dmg_spit3","0"};


	CVAR_REGISTER ( &sk_babygargantua_dmg_slash);// {"sk_bigmomma_health_factor1","1.0"};
	CVAR_REGISTER ( &sk_babygargantua_dmg_fire);// {"sk_bigmomma_health_factor2","1.0"};
	CVAR_REGISTER ( &sk_babygargantua_dmg_stomp);// {"sk_bigmomma_health_factor3","1.0"};

	CVAR_REGISTER ( &sk_hassassin_health);// {"sk_bigmomma_dmg_slash1","50"};
	CVAR_REGISTER ( &sk_hassassin_enable_cloaking);// {"sk_bigmomma_dmg_slash2","50"};
	CVAR_REGISTER ( &sk_headcrab_health);// {"sk_bigmomma_dmg_slash3","50"};

	CVAR_REGISTER ( &sk_headcrab_dmg_bite);// {"sk_bigmomma_dmg_blast1","100"};
	CVAR_REGISTER ( &sk_hgrunt_health);// {"sk_bigmomma_dmg_blast2","100"};
	CVAR_REGISTER ( &sk_hgrunt_kick);// {"sk_bigmomma_dmg_blast3","100"};

	CVAR_REGISTER ( &sk_hgrunt_pellets);// {"sk_bigmomma_radius_blast1","250"};
	CVAR_REGISTER ( &sk_hgrunt_gspeed);// {"sk_bigmomma_radius_blast2","250"};
	CVAR_REGISTER ( &sk_hwgrunt_health);// {"sk_bigmomma_radius_blast3","250"};

	// Gargantua
	CVAR_REGISTER ( &sk_hwgrunt_minipellets);// {"sk_gargantua_health1","0"};
	CVAR_REGISTER ( &sk_rgrunt_explode);// {"sk_gargantua_health2","0"};
	CVAR_REGISTER ( &sk_houndeye_health);// {"sk_gargantua_health3","0"};

	CVAR_REGISTER ( &sk_houndeye_dmg_blast);// {"sk_gargantua_dmg_slash1","0"};
	CVAR_REGISTER ( &sk_islave_health);// {"sk_gargantua_dmg_slash2","0"};
	CVAR_REGISTER ( &sk_islave_dmg_claw);// {"sk_gargantua_dmg_slash3","0"};

	CVAR_REGISTER ( &sk_islave_dmg_clawrake);// {"sk_gargantua_dmg_fire1","0"};
	CVAR_REGISTER ( &sk_islave_dmg_zap);// {"sk_gargantua_dmg_fire2","0"};
	CVAR_REGISTER ( &sk_islave_speed_zap);// {"sk_gargantua_dmg_fire3","0"};

	CVAR_REGISTER ( &sk_ichthyosaur_health);// {"sk_gargantua_dmg_stomp1","0"};
	CVAR_REGISTER ( &sk_ichthyosaur_shake);// {"sk_gargantua_dmg_stomp2","0"};
	CVAR_REGISTER ( &sk_leech_health);// {"sk_gargantua_dmg_stomp3","0"};


	// Hassassin
	CVAR_REGISTER ( &sk_leech_dmg_bite);// {"sk_hassassin_health1","0"};
	CVAR_REGISTER ( &sk_controller_health);// {"sk_hassassin_health2","0"};
	CVAR_REGISTER ( &sk_controller_dmgzap);// {"sk_hassassin_health3","0"};


	// Headcrab
	CVAR_REGISTER ( &sk_controller_speedball);// {"sk_headcrab_health1","0"};
	CVAR_REGISTER ( &sk_controller_dmgball);// {"sk_headcrab_health2","0"};
	CVAR_REGISTER ( &sk_nihilanth_health);// {"sk_headcrab_health3","0"};

	CVAR_REGISTER ( &sk_nihilanth_zap);// {"sk_headcrab_dmg_bite1","0"};
	CVAR_REGISTER ( &sk_scientist_health);// {"sk_headcrab_dmg_bite2","0"};
	CVAR_REGISTER ( &sk_snark_health);// {"sk_headcrab_dmg_bite3","0"};


	// Hgrunt
	CVAR_REGISTER ( &sk_snark_dmg_bite);// {"sk_hgrunt_health1","0"};
	CVAR_REGISTER ( &sk_snark_dmg_pop);// {"sk_hgrunt_health2","0"};
	CVAR_REGISTER ( &sk_zombie_health);// {"sk_hgrunt_health3","0"};

	CVAR_REGISTER ( &sk_zombie_dmg_one_slash);// {"sk_hgrunt_kick1","0"};
	CVAR_REGISTER ( &sk_zombie_dmg_both_slash);// {"sk_hgrunt_kick2","0"};
	CVAR_REGISTER ( &sk_tor_health);// {"sk_hgrunt_kick3","0"};

	CVAR_REGISTER ( &sk_tor_punch);
	CVAR_REGISTER ( &sk_tor_energybeam);
	CVAR_REGISTER ( &sk_tor_sonicblast);

	CVAR_REGISTER ( &sk_turret_health);
	CVAR_REGISTER ( &sk_miniturret_health);
	CVAR_REGISTER ( &sk_sentry_health);

	// Houndeye
	CVAR_REGISTER ( &sk_otis_bullet);// {"sk_houndeye_health1","0"};
	CVAR_REGISTER ( &sk_otis_health);// {"sk_houndeye_health2","0"};
	CVAR_REGISTER ( &sk_massassin_sniper);// {"sk_houndeye_health3","0"};

	CVAR_REGISTER ( &sk_grunt_buckshot);// {"sk_houndeye_dmg_blast1","0"};
	CVAR_REGISTER ( &sk_grunt_handgrenades);// {"sk_houndeye_dmg_blast2","0"};
	CVAR_REGISTER ( &sk_zombie_barney_health);// {"sk_houndeye_dmg_blast3","0"};


	// ISlave
	CVAR_REGISTER ( &sk_zombie_barney_dmg_one_slash);// {"sk_islave_health1","0"};
	CVAR_REGISTER ( &sk_zombie_barney_dmg_both_slash);// {"sk_islave_health2","0"};
	CVAR_REGISTER ( &sk_zombie_soldier_health);// {"sk_islave_health3","0"};

	CVAR_REGISTER ( &sk_zombie_soldier_dmg_one_slash);// {"sk_islave_dmg_claw1","0"};
	CVAR_REGISTER ( &sk_zombie_soldier_dmg_both_slash);// {"sk_islave_dmg_claw2","0"};
	CVAR_REGISTER ( &sk_gonome_health);// {"sk_islave_dmg_claw3","0"};

	CVAR_REGISTER ( &sk_gonome_dmg_one_slash);// {"sk_islave_dmg_clawrake1","0"};
	CVAR_REGISTER ( &sk_gonome_dmg_guts);// {"sk_islave_dmg_clawrake2","0"};
	CVAR_REGISTER ( &sk_gonome_dmg_one_bite);// {"sk_islave_dmg_clawrake3","0"};

	CVAR_REGISTER ( &sk_shocktrooper_health);// {"sk_islave_dmg_zap1","0"};
	CVAR_REGISTER ( &sk_shocktrooper_kick);// {"sk_islave_dmg_zap2","0"};
	CVAR_REGISTER ( &sk_shocktrooper_maxcharge);// {"sk_islave_dmg_zap3","0"};


	// Icthyosaur
	CVAR_REGISTER ( &sk_shocktrooper_enable_spores);// {"sk_ichthyosaur_health1","0"};
	CVAR_REGISTER ( &sk_pitdrone_health);// {"sk_ichthyosaur_health2","0"};
	CVAR_REGISTER ( &sk_pitdrone_dmg_whip);// {"sk_ichthyosaur_health3","0"};

	CVAR_REGISTER ( &sk_pitdrone_dmg_spit);// {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER ( &sk_pitdrone_dmg_bite);// {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER ( &sk_voltigore_health);// {"sk_ichthyosaur_health3","0"};



	// Leech
	CVAR_REGISTER ( &sk_voltigore_dmg_beam);// {"sk_leech_health1","0"};
	CVAR_REGISTER ( &sk_voltigore_dmg_punch);// {"sk_leech_health2","0"};
	CVAR_REGISTER ( &sk_voltigore_dmg_explode);// {"sk_leech_health3","0"};

	CVAR_REGISTER ( &sk_sqknest_health);// {"sk_leech_dmg_bite1","0"};
	CVAR_REGISTER ( &sk_shockroach_dmg_xpl_touch);// {"sk_leech_dmg_bite2","0"};
	CVAR_REGISTER ( &sk_shockroach_dmg_xpl_splash);// {"sk_leech_dmg_bite3","0"};


	// Controller
	CVAR_REGISTER ( &sk_plr_shockrifle);
	CVAR_REGISTER ( &sk_plr_shockrifle_beam);
	CVAR_REGISTER ( &sk_plr_shockrifle_recharge);

	CVAR_REGISTER ( &sk_kingpin_health);
	CVAR_REGISTER ( &sk_kingpin_lightning);
	CVAR_REGISTER ( &sk_kingpin_tele_blast);

	CVAR_REGISTER ( &sk_kingpin_plasma_blast);
	CVAR_REGISTER ( &sk_kingpin_melee);
	CVAR_REGISTER ( &sk_kingpin_telefrag);

	CVAR_REGISTER ( &sk_mortar);
	CVAR_REGISTER ( &sk_plr_crowbar);
	CVAR_REGISTER ( &sk_plr_wrench);

	// Nihilanth
	CVAR_REGISTER ( &sk_plr_grapple);// {"sk_nihilanth_health1","0"};
	CVAR_REGISTER ( &sk_plr_9mm_bullet);// {"sk_nihilanth_health2","0"};
	CVAR_REGISTER ( &sk_plr_762_bullet);// {"sk_nihilanth_health3","0"};

	CVAR_REGISTER ( &sk_plr_357_bullet);
	CVAR_REGISTER ( &sk_plr_9mmAR_bullet);
	CVAR_REGISTER ( &sk_plr_9mmAR_grenade);

	// Scientist
	CVAR_REGISTER ( &sk_plr_buckshot);// {"sk_scientist_health1","0"};
	CVAR_REGISTER ( &sk_plr_xbow_bolt_monster);// {"sk_scientist_health2","0"};
	CVAR_REGISTER ( &sk_plr_xbow_bolt_client);// {"sk_scientist_health3","0"};


	// Snark
	CVAR_REGISTER ( &sk_plr_rpg);// {"sk_snark_health1","0"};
	CVAR_REGISTER ( &sk_plr_gauss);// {"sk_snark_health2","0"};
	CVAR_REGISTER ( &sk_plr_secondarygauss);// {"sk_snark_health3","0"};

	CVAR_REGISTER ( &sk_plr_egon_narrow);// {"sk_snark_dmg_bite1","0"};
	CVAR_REGISTER ( &sk_plr_egon_wide);// {"sk_snark_dmg_bite2","0"};
	CVAR_REGISTER ( &sk_plr_hand_grenade);// {"sk_snark_dmg_bite3","0"};

	CVAR_REGISTER ( &sk_plr_satchel);// {"sk_snark_dmg_pop1","0"};
	CVAR_REGISTER ( &sk_plr_tripmine);// {"sk_snark_dmg_pop2","0"};
	CVAR_REGISTER ( &sk_plr_spore);// {"sk_snark_dmg_pop3","0"};



	// Zombie
	CVAR_REGISTER ( &sk_plr_uzi);// {"sk_zombie_health1","0"};
	CVAR_REGISTER ( &sk_plr_displacer_other);// {"sk_zombie_health3","0"};
	CVAR_REGISTER ( &sk_plr_displacer_radius);// {"sk_zombie_health3","0"};

	CVAR_REGISTER ( &sk_12mm_bullet);// {"sk_zombie_dmg_one_slash1","0"};
	CVAR_REGISTER ( &sk_9mmAR_bullet);// {"sk_zombie_dmg_one_slash2","0"};
	CVAR_REGISTER ( &sk_9mm_bullet);// {"sk_zombie_dmg_one_slash3","0"};

	CVAR_REGISTER ( &sk_556_bullet);// {"sk_zombie_dmg_both_slash1","0"};
	CVAR_REGISTER ( &sk_hornet_dmg);// {"sk_zombie_dmg_both_slash2","0"};
	CVAR_REGISTER ( &sk_suitcharger);// {"sk_zombie_dmg_both_slash3","0"};


	//Turret
	CVAR_REGISTER ( &sk_battery);// {"sk_turret_health1","0"};
	CVAR_REGISTER ( &sk_healthcharger);// {"sk_turret_health2","0"};
	CVAR_REGISTER ( &sk_healthkit);// {"sk_turret_health3","0"};


	// MiniTurret
	CVAR_REGISTER ( &sk_scientist_heal);// {"sk_miniturret_health1","0"};
	CVAR_REGISTER ( &sk_monster_head);// {"sk_miniturret_health2","0"};
	CVAR_REGISTER ( &sk_monster_chest);// {"sk_miniturret_health3","0"};


	// Sentry Turret
	CVAR_REGISTER ( &sk_monster_stomach);// {"sk_sentry_health1","0"};
	CVAR_REGISTER ( &sk_monster_arm);// {"sk_sentry_health2","0"};
	CVAR_REGISTER ( &sk_monster_leg);// {"sk_sentry_health3","0"};


	// PLAYER WEAPONS

	// Crowbar whack
	CVAR_REGISTER ( &sk_player_head);// {"sk_plr_crowbar1","0"};
	CVAR_REGISTER ( &sk_player_chest);// {"sk_plr_crowbar2","0"};
	CVAR_REGISTER ( &sk_player_stomach);// {"sk_plr_crowbar3","0"};

	// Glock Round
	CVAR_REGISTER ( &sk_player_arm);// {"sk_plr_9mm_bullet1","0"};
	CVAR_REGISTER ( &sk_player_leg);// {"sk_plr_9mm_bullet2","0"};
	CVAR_REGISTER ( &sk_hornet_pdmg);// {"sk_plr_9mm_bullet3","0"};

	// 357 Round
	CVAR_REGISTER ( &sk_tentacle);// {"sk_plr_357_bullet1","0"};
	CVAR_REGISTER ( &sk_blkopsosprey);// {"sk_plr_357_bullet2","0"};
	CVAR_REGISTER ( &sk_osprey);// {"sk_plr_357_bullet3","0"};

	// MP5 Round
	CVAR_REGISTER ( &sk_stukabat);// {"sk_plr_9mmAR_bullet1","0"};
	CVAR_REGISTER ( &sk_stukabat_dmg_bite);
// END REGISTER CVARS FOR SKILL LEVEL STUFF

	SERVER_COMMAND( "exec skill.cfg\n" );

	LinkUserMessages();

	if ( !CVAR_GET_POINTER( "sv_disable_online_api" ) )
		CVAR_REGISTER( &sv_disable_online_api );

	CServer::GetInstance();
	CConCommandSystem::GetInstance()->Initialize();
	CASServerManager::GetInstance()->Startup();
}

void GameDLLUninit( void )
{
	CASServerManager::GetInstance()->Shutdown();
	CConCommandSystem::GetInstance()->Shutdown();
	CServer::DestroyInstance();
	g_StringPool.Clear();
	DestroyMapCycle( &g_mapcycle );
	RemoveGlobalEntvarsList();
}

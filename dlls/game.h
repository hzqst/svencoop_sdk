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

#ifndef GAME_H
#define GAME_H

extern void GameDLLInit( void );
extern void GameDLLUninit( void );


extern cvar_t	displaysoundlist;

extern cvar_t mp_weaponfadedelay;
extern cvar_t mp_classic_mode;
extern cvar_t weaponmode_eagle;
extern cvar_t weaponmode_displacer;
extern cvar_t weaponmode_shotgun;
extern cvar_t weaponmode_9mmhandgun;
extern cvar_t weaponmode_mp5;
extern cvar_t weaponmode_crossbow;
extern cvar_t weaponmode_rpg;
extern cvar_t mp_disablegaussjump;
extern cvar_t mp_banana;
extern cvar_t mp_dropweapons;
extern cvar_t mp_modelselection;
extern cvar_t mp_allowmonsterinfo;
extern cvar_t mp_allowplayerinfo;
extern cvar_t mp_timelimit_empty;
extern cvar_t timeleft_empty;
extern cvar_t mp_nomedkit;
extern cvar_t mp_disable_pcbalancing;
extern cvar_t mp_disable_medkit_points;
extern cvar_t mp_pcbalancing_factorlist;
extern cvar_t sv_notransition;
extern cvar_t mp_barnacle_paralyze;
extern cvar_t mp_disable_autoclimb;
extern cvar_t mp_disable_player_rappel;
extern cvar_t mp_grapple_mode;
extern cvar_t mp_no_akimbo_uzis;
extern cvar_t mp_noblastgibs;
extern cvar_t mp_telefrag;
extern cvar_t mp_item_respawndelay;
extern cvar_t mp_startoffset;
extern cvar_t mp_respawndelay;
extern cvar_t mp_suitpower;
extern cvar_t mp_weapon_droprules;
extern cvar_t mp_ammo_droprules;
extern cvar_t mp_monsterpoints;

extern cvar_t mp_nextmap;
extern cvar_t mp_nextmap_ignore_mapcfg;
extern cvar_t mp_nextmap_parted_mapcfg_limit;
extern cvar_t mp_nextmap_cycle;
extern cvar_t mp_nextmap_cycle_minp;
extern cvar_t mp_nextmap_cycle_maxp;
extern cvar_t mp_nextmap_cycle_cmds;
extern cvar_t mp_nextmap_cycle_rules;

extern cvar_t mp_survival_mode;
extern cvar_t mp_survival_supported;
extern cvar_t mp_survival_minplayers;
extern cvar_t mp_survival_voteallow;
extern cvar_t mp_survival_retries;
extern cvar_t mp_survival_nextmap;
extern cvar_t mp_survival_starton;
extern cvar_t mp_survival_startdelay;

extern cvar_t mp_voteallow;
extern cvar_t mp_votetimebetween;
extern cvar_t mp_votetimecheck;
extern cvar_t mp_votemaprequired;
extern cvar_t mp_votekillrequired;
extern cvar_t mp_votebanrequired;
extern cvar_t mp_votekickrequired;
extern cvar_t mp_voteclassicmoderequired;
extern cvar_t mp_votesurvivalmoderequired;
extern cvar_t mapvotecfgfile;
extern cvar_t mp_votekill_respawndelay;
extern cvar_t mp_playervotedelay;
extern cvar_t mp_bantime;
extern cvar_t mp_playerinactive;
extern cvar_t mp_kickbantime;

extern cvar_t mp_observer_mode;
extern cvar_t mp_observer_cyclic;

extern cvar_t mp_multiplespawn;
extern cvar_t mp_hevsuit_voice;

extern cvar_t mp_weapon_respawndelay;
extern cvar_t mp_ammo_respawndelay;

extern cvar_t	sk_tor_health;
extern cvar_t	sk_tor_punch;
extern cvar_t	sk_tor_energybeam;
extern cvar_t	sk_tor_sonicblast;

// multiplayer server rules
extern cvar_t	teamplay;
extern cvar_t	fraglimit;
extern cvar_t	timelimit;
extern cvar_t	friendlyfire;
extern cvar_t	npckilling;
extern cvar_t npc_dropweapons;
extern cvar_t sv_ai_enemy_detection_mode;
extern cvar_t sv_report_unknown_entities;
extern cvar_t sv_log_monster_monster_kills;
extern cvar_t sv_log_monster_deaths;
extern cvar_t sv_log_player_deaths;
extern cvar_t sk_plr_HpMedic;
extern cvar_t sk_plr_wrench;
extern cvar_t sk_plr_grapple;
extern cvar_t sk_plr_uzi;
extern cvar_t sk_plr_displacer_other;
extern cvar_t sk_plr_displacer_radius;
extern cvar_t	sk_plr_hand_grenade;
extern cvar_t	sk_plr_9mmAR_grenade;
extern cvar_t	sk_mortar;
extern cvar_t	sk_plr_rpg;
extern cvar_t	sk_plr_satchel;
extern cvar_t	sk_plr_tripmine;
extern cvar_t sk_plr_crowbar;
extern cvar_t sk_snark_dmg_pop;
extern cvar_t	sk_plr_xbow_bolt_monster;
extern cvar_t	falldamage;
extern cvar_t	weaponstay;
extern cvar_t	forcerespawn;
extern cvar_t	flashlight;
extern cvar_t	aimcrosshair;
extern cvar_t	decalfrequency;
extern cvar_t	teamlist;
extern cvar_t	teamoverride;
extern cvar_t	defaultteam;
extern cvar_t	allowmonsters;

// Engine Cvars
extern cvar_t	*g_psv_gravity;
extern cvar_t	*g_psv_aim;
extern cvar_t	*g_footsteps;

#endif		// GAME_H

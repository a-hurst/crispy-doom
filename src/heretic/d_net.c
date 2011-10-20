// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//	DOOM Network game communication and protocol,
//	all OS independend parts.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "doomfeatures.h"

#include "m_argv.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "doomdef.h"
#include "w_checksum.h"

#include "deh_main.h"

#include "d_loop.h"

ticcmd_t *netcmds;

extern void D_DoAdvanceDemo(void);
extern void D_ProcessEvents(void);
extern void G_BuildTiccmd(ticcmd_t *cmd, int maketic);
extern boolean G_CheckDemoStatus(void);

// Called when a player leaves the game

static void PlayerQuitGame(player_t *player)
{
    static char exitmsg[80];
    unsigned int player_num;

    player_num = player - players;

    // Note:
    // The Heretic source code does this, which doesn't actually work.
    // As a result, the exit message is never seen.

    strcpy(exitmsg, "PLAYER 1 LEFT THE GAME");
    exitmsg[7] += player_num;
    players[consoleplayer].message = exitmsg;

    playeringame[player_num] = false;
    players[consoleplayer].message = exitmsg;

    // TODO: check if it is sensible to do this:

    if (demorecording) 
    {
        G_CheckDemoStatus ();
    }
}

static void RunTic(ticcmd_t *cmds, boolean *ingame)
{
    extern boolean advancedemo;
    unsigned int i;

    // Check for player quits.

    for (i = 0; i < MAXPLAYERS; ++i)
    {
        if (!demoplayback && playeringame[i] && !ingame[i])
        {
            PlayerQuitGame(&players[i]);
        }
    }

    netcmds = cmds;

    // check that there are players in the game.  if not, we cannot
    // run a tic.

    if (advancedemo)
        D_DoAdvanceDemo ();

    G_Ticker ();
}

static loop_interface_t doom_loop_interface = {
    D_ProcessEvents,
    G_BuildTiccmd,
    RunTic,
    MN_Ticker
};


// Load game settings from the specified structure and 
// set global variables.

static void LoadGameSettings(net_gamesettings_t *settings,
                             net_connect_data_t *connect_data)
{
    unsigned int i;

    deathmatch = settings->deathmatch;
    ticdup = settings->ticdup;
    startepisode = settings->episode;
    startmap = settings->map;
    startskill = settings->skill;
    // TODO startloadgame = settings->loadgame;
    nomonsters = settings->nomonsters;
    respawnparm = settings->respawn_monsters;

    if (!connect_data->drone)
    {
        consoleplayer = settings->consoleplayer;
    }
    else
    {
        consoleplayer = 0;
    }
    
    for (i=0; i<MAXPLAYERS; ++i) 
    {
        playeringame[i] = i < settings->num_players;
    }
}

// Save the game settings from global variables to the specified
// game settings structure.

static void SaveGameSettings(net_gamesettings_t *settings,
                             net_connect_data_t *connect_data)
{
    // Fill in game settings structure with appropriate parameters
    // for the new game

    settings->deathmatch = deathmatch;
    settings->episode = startepisode;
    settings->map = startmap;
    settings->skill = startskill;
    // TODO settings->loadgame = startloadgame;
    settings->gameversion = exe_heretic_1_3;
    settings->nomonsters = nomonsters;
    settings->respawn_monsters = respawnparm;
    settings->timelimit = 0;
    settings->lowres_turn = false;

    connect_data->drone = false;
    connect_data->max_players = MAXPLAYERS;

    //
    // Connect data
    //

    // Game type fields:

    connect_data->gamemode = gamemode;
    connect_data->gamemission = gamemission;

    connect_data->lowres_turn = false;

    // Read checksums of our WAD directory and dehacked information

    W_Checksum(connect_data->wad_md5sum);
    DEH_Checksum(connect_data->deh_md5sum);

    connect_data->is_freedoom = 0;
}

void D_InitSinglePlayerGame(net_gamesettings_t *settings)
{
    // default values for single player

    settings->consoleplayer = 0;
    settings->num_players = 1;

    netgame = false;

    //!
    // @category net
    //
    // Start the game playing as though in a netgame with a single
    // player.  This can also be used to play back single player netgame
    // demos.
    //

    if (M_CheckParm("-solo-net") > 0)
    {
        netgame = true;
    }
}

//
// D_CheckNetGame
// Works out player numbers among the net participants
//

void D_CheckNetGame (void)
{
    net_connect_data_t connect_data;
    net_gamesettings_t settings;

    D_RegisterLoopCallbacks(&doom_loop_interface);

    // Call D_QuitNetGame on exit 

    I_AtExit(D_QuitNetGame, true);

    SaveGameSettings(&settings, &connect_data);

    if (D_InitNetGame(&connect_data, &settings))
    {
        netgame = true;
        autostart = true;
    }
    else
    {
        D_InitSinglePlayerGame(&settings);
    }

    LoadGameSettings(&settings, &connect_data);
}
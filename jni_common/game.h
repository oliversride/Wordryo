/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2011 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _GAME_H_
#define _GAME_H_

#include "model.h"
#include "board.h"
#include "comms.h"
#include "server.h"
#include "util.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct LocalPlayer {
    XP_UCHAR* name;
    XP_UCHAR* password;
    XP_UCHAR* dictName;
    XP_U16 secondsUsed;
    XP_Bool isLocal;
    XP_U8 robotIQ;              /* 0 means not a robot; 1-100 means how
                                   dumb is it with 1 meaning very smart */
} LocalPlayer;

#define LP_IS_ROBOT(lp) ((lp)->robotIQ != 0)
#define LP_IS_LOCAL(lp) ((lp)->isLocal)

#define DUMB_ROBOT 0
#define SMART_ROBOT 1

typedef struct CurGameInfo {
    XP_UCHAR* dictName;
    LocalPlayer players[MAX_NUM_PLAYERS];
    XP_U32 gameID;      /* uniquely identifies game */
    XP_U16 gameSeconds; /* for timer */
    XP_LangCode dictLang;
    XP_U8 nPlayers;
    XP_U8 boardSize;
    DeviceRole serverRole;

    XP_Bool hintsNotAllowed;
    XP_Bool zoomOnDrop;
    XP_Bool timerEnabled;
    XP_Bool allowPickTiles;
    XP_Bool allowHintRect;
    XWPhoniesChoice phoniesAction;
    XP_Bool confirmBTConnect;   /* only used for BT */
} CurGameInfo;

typedef struct _GameStateInfo {
    XP_U16 visTileCount;
    XW_TrayVisState trayVisState;
    XP_Bool canHint;
    XP_Bool canUndo;  // Tiles can go back to tray.
    XP_Bool canRedo;
    XP_Bool inTrade;
    XP_Bool tradeTilesSelected;
    XP_Bool gameIsConnected;
    XP_Bool canShuffle;
    XP_Bool curTurnSelected;
} GameStateInfo;

typedef struct XWGame {
    BoardCtxt* board;
    ModelCtxt* model;
    ServerCtxt* server;
#ifndef XWFEATURE_STANDALONE_ONLY
    CommsCtxt* comms;
#endif
} XWGame;

void game_makeNewGame( MPFORMAL XWGame* game, CurGameInfo* gi, 
                       XW_UtilCtxt* util, DrawCtx* draw, 
                       CommonPrefs* cp, const TransportProcs* procs
#ifdef SET_GAMESEED
                       ,XP_U16 gameSeed
#endif
                       );
void game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, XW_UtilCtxt* util, 
                 CommonPrefs* cp, const TransportProcs* procs );
void game_changeDict( MPFORMAL XWGame* game, CurGameInfo* gi, 
                      DictionaryCtxt* dict );

XP_Bool game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                             CurGameInfo* gi, DictionaryCtxt* dict, 
                             const PlayerDicts* dicts, XW_UtilCtxt* util, 
                             DrawCtx* draw, CommonPrefs* cp,
                             const TransportProcs* procs );

void game_saveToStream( const XWGame* game, const CurGameInfo* gi, 
                        XWStreamCtxt* stream, XP_U16 saveToken );
void game_saveSucceeded( const XWGame* game, XP_U16 saveToken );
void game_dispose( XWGame* game );

void game_getState( const XWGame* game, GameStateInfo* gsi );

void gi_initPlayerInfo( MPFORMAL CurGameInfo* gi, 
                        const XP_UCHAR* nameTemplate );
void gi_disposePlayerInfo( MPFORMAL CurGameInfo* gi );
void gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi );
void gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi );
void gi_copy( MPFORMAL CurGameInfo* destGI, const CurGameInfo* srcGi );
XP_U16 gi_countLocalPlayers( const CurGameInfo* gi, XP_Bool humanOnly );

XP_Bool player_hasPasswd( LocalPlayer* player );
XP_Bool player_passwordMatches( LocalPlayer* player, XP_U8* buf, XP_U16 len );
XP_U16 player_timePenalty( CurGameInfo* gi, XP_U16 playerNum );

#ifdef CPLUS
}
#endif

#endif

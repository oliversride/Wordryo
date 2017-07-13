/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000 - 2011 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _MODELP_H_
#define _MODELP_H_

#include "model.h"
#include "movestak.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct PendingTile {
    XP_U8 col;
    XP_U8 row;
    XP_U8 home;
    Tile tile; /* includes face and blank bit */
} PendingTile;

typedef struct PlayerCtxt {
    XP_S16 score;
    XP_S16 curMoveScore; /* negative means illegal */
    XP_Bool curMoveValid;
    TrayTileSet trayTiles;
    XP_U8 nPending;      /* still in tray but "on board" */
    XP_U8 nUndone;       /* tiles above nPending we can reuse */
    PendingTile pendingTiles[MAX_TRAY_TILES];
} PlayerCtxt;

typedef struct _RecordWordsInfo {
    XWStreamCtxt* stream;
    XP_U16 nWords;
} RecordWordsInfo;

typedef struct ModelVolatiles {
    XW_UtilCtxt* util;
    struct CurGameInfo* gi;
    DictionaryCtxt* dict;
    PlayerDicts dicts;
    BoardListener boardListenerFunc;
    StackCtxt* stack;
    void* boardListenerData;
    TrayListener trayListenerFunc;
    void* trayListenerData;
    DictListener dictListenerFunc;
    void* dictListenerData;
    RecordWordsInfo rwi;
    WordNotifierInfo wni; 
    XP_U16 nTilesOnBoard;
    CellTile* tiles;

    XP_U16 nBonuses;
    XWBonusType* bonuses;

    MPSLOT
} ModelVolatiles;

struct ModelCtxt {

    ModelVolatiles vol;

    PlayerCtxt players[MAX_NUM_PLAYERS];
    XP_U16 nPlayers;
    XP_U16 nCols;
    XP_U16 nRows;

    const ModelCtxt* loaner;    /* allows sharing bonuses */
};

#define TILES_SIZE(m,nc) ((nc) * (nc) * sizeof((m)->vol.tiles[0]))

void invalidateScore( ModelCtxt* model, XP_S16 player );
XP_Bool tilesInLine( ModelCtxt* model, XP_S16 turn, XP_Bool* isHorizontal );
void normalizeMoves( ModelCtxt* model, XP_S16 turn, 
                     XP_Bool isHorizontal, MoveInfo* moveInfo );
void adjustScoreForUndone( ModelCtxt* model, MoveInfo* mi, XP_U16 turn );
#ifdef CPLUS
}
#endif

#endif

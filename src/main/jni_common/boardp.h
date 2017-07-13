/* -*-mode: C; fill-column: 78; -*- */
/* 
 * Copyright 1997 - 2010 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _BOARDP_H_
#define _BOARDP_H_

#include "comtypes.h"
#include "model.h"
#include "board.h"
#include "engine.h"
#include "mempool.h" /* debug only */

#ifdef CPLUS
extern "C" {
#endif

typedef struct _DragObjInfo {
    BoardObjectType obj;
    union {
        struct {
            XP_U16 col;
            XP_U16 row;
        } board;
        struct {
            XP_U16 index;
        } tray;
    } u;
} DragObjInfo;

typedef enum {
    DT_NONE
    ,DT_DIVIDER
    ,DT_TILE
#ifdef XWFEATURE_SEARCHLIMIT
    ,DT_HINTRGN
#endif
    ,DT_BOARD
} DragType;


typedef struct _DragState {
    DragType dtype;
    XP_Bool didMove;            /* there was change during the drag; not a
                                   tap */
    XP_Bool cellChanged;        /* nothing dragged but movement happened */
    XP_Bool scrollTimerSet;
    XP_Bool isBlank;            /* cache rather than lookup in model */
    Tile tile;                  /* cache rather than lookup in model */
    DragObjInfo start;
    DragObjInfo cur;            /* where dragged object (not pen) is */
#ifdef XWFEATURE_RAISETILE
    XP_U16 yyAdd;
#endif
#ifdef XWFEATURE_CROSSHAIRS
    struct {
        XP_S16 col; 
        XP_S16 row;
    } crosshairs;
#endif
} DragState;

typedef struct _BoardArrow { /* gets flipped along with board */
    XP_U8 col;
    XP_U8 row;
    XP_Bool vert;
    XP_Bool visible;
} BoardArrow;

#ifdef KEYBOARD_NAV
typedef struct _BdCursorLoc {
    XP_U8 col;
    XP_U8 row;
} BdCursorLoc;
#endif

#ifdef XWFEATURE_MINIWIN
/* We only need two of these, one for the value hint and the other for the
   trading window.  There's never more than of the former since it lives only
   as long as the pen is down.  There are, in theory, as many trading windows
   as there are (local) players, but they can all use the same window. */
typedef struct _MiniWindowStuff {
    void* closure;
    const XP_UCHAR* text;
    XP_Rect rect;
} MiniWindowStuff;

enum { MINIWINDOW_VALHINT, MINIWINDOW_TRADING };
typedef XP_U16 MiniWindowType; /* one of the two above */
#endif

typedef struct _PerTurnInfo {
#ifdef KEYBOARD_NAV
    XP_Rect scoreRects;
    BdCursorLoc bdCursor;
#endif
    BoardArrow boardArrow;
    XP_U16 scoreDims;
    XP_U8 dividerLoc; /* 0 means left of 0th tile, etc. */
    TileBit traySelBits;
#ifdef XWFEATURE_SEARCHLIMIT
    BdHintLimits limits;
#endif
#ifdef KEYBOARD_NAV
    XP_U8   trayCursorLoc; /* includes divider!!! */
#endif
    XP_Bool dividerSelected; /* probably need to save this */
    XP_Bool tradeInProgress;
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool hasHintRect;
#endif
} PerTurnInfo;

typedef struct _ScrollData {
    XP_U16 scale;
    XP_U16 offset;
    XP_U16 maxOffset;
    XP_U16 lastVisible;
    XP_U16 dims[MAX_COLS];
} ScrollData;
typedef enum { SCROLL_H, SCROLL_V, N_SCROLL_DIMS } SDIndex;

struct BoardCtxt {
/*     BoardVTable* vtable; */
    ModelCtxt* model;
    ServerCtxt* server;
    DrawCtx* draw;
    XW_UtilCtxt* util;

    struct CurGameInfo* gi;
    ScrollData sd[N_SCROLL_DIMS];

    XP_U16 preHideYOffset;
    XP_U16 prevYScrollOffset; /* represents where the last draw took place;
                                 used to see if bit scrolling can be used */
    XP_U16 penDownX;
    XP_U16 penDownY;

    XP_U32 timerStoppedTime;
    XP_U16 timerSaveCount;
#ifdef DEBUG    
    XP_S16 timerStoppedTurn;
#endif

    RowFlags redrawFlags[MAX_ROWS];

    XP_Rect boardBounds;
    XP_U16 heightAsSet;
    XP_U16 maxCellSz;

    BoardObjectType penDownObject;

    XP_Bool needsDrawing;
    XP_Bool isFlipped;
    XP_Bool showGrid;
    XP_Bool gameOver;
    XP_Bool leftHanded;
    XP_Bool badWordRejected;
    XP_Bool timerPending;
    XP_Bool disableArrow;
    XP_Bool hideValsInTray;
    XP_Bool skipCommitConfirm;
    XP_Bool allowPeek;          /* Can look at non-turn player's rack */
#ifdef XWFEATURE_CROSSHAIRS
    XP_Bool hideCrosshairs;
#endif

    XP_Bool eraseTray;
    XP_Bool boardObscuresTray;
    XP_Bool boardHidesTray;
    XP_Bool scoreSplitHor;/* how to divide the scoreboard? */
    XP_Bool srcIsPen;      /* We're processing a pen event, not a key event */

    XP_U16 star_row;
    XP_U16 zoomCount;

    /* Unless KEYBOARD_NAV is defined, this does not change */
    BoardObjectType focussed;

#ifdef KEYBOARD_NAV
    XP_Bool focusHasDived;
    XP_Bool hideFocus;          /* not saved */
    XP_Bool trayHiddenPreFocus; /* not saved */
    XP_Rect remRect;            /* on scoreboard */
#endif

    /* scoreboard state */
    XP_Rect scoreBdBounds;
    XP_Rect timerBounds;
    XP_U8 selPlayer; /* which player is selected (!= turn) */

    PerTurnInfo pti[MAX_NUM_PLAYERS];
    PerTurnInfo* selInfo;

    /* tray state */
    /* +W */
    /* XP_U8 trayScaleH; */
    /* XP_U8 trayScaleV; */
    XP_U16 trayScaleH;
    XP_U16 trayScaleV;
    XP_Rect trayBounds;
    XP_U16 remDim;      /* width (or ht) of the "rem:" string in scoreboard */
    XP_U8 dividerWidth; /* 0 would mean invisible */
    XP_U16 traySteal;   /* +W */
    XP_Bool dividerInvalid;

    XP_Bool scoreBoardInvalid;
    DragState dragState;

#ifdef XWFEATURE_MINIWIN
    MiniWindowStuff miniWindowStuff[2];
    XP_Bool tradingMiniWindowInvalid;
#endif

    TileBit trayInvalBits;
#ifdef KEYBOARD_NAV
    XP_U8   scoreCursorLoc;
#endif

    XW_TrayVisState trayVisState;
    XP_Bool penTimerFired;
    XP_Bool showCellValues;
    XP_Bool showColors;

    MPSLOT
};

#define CURSOR_LOC_REM 0
#ifdef XWFEATURE_MINIWIN
# define valHintMiniWindowActive( board ) \
     ((XP_Bool)((board)->miniWindowStuff[MINIWINDOW_VALHINT].text != NULL))
#endif
#define MY_TURN(b) ((b)->selPlayer == server_getCurrentTurn( (b)->server ))
#define TRADE_IN_PROGRESS(b) ((b)->selInfo->tradeInProgress==XP_TRUE)

/* tray-related functions */
XP_Bool handlePenUpTray( BoardCtxt* board, XP_U16 x, XP_U16 y );
void drawTray( BoardCtxt* board );
XP_Bool moveTileToArrowLoc( BoardCtxt* board, XP_U8 index );
XP_U16 indexForBits( XP_U8 bits );
XP_Bool rectContainsPt( const XP_Rect* rect1, XP_S16 x, XP_S16 y );
XP_Bool checkRevealTray( BoardCtxt* board );
void figureTrayTileRect( BoardCtxt* board, XP_U16 index, XP_Rect* rect );
XP_Bool rectsIntersect( const XP_Rect* rect1, const XP_Rect* rect2 );
XP_S16 pointToTileIndex( BoardCtxt* board, XP_U16 x, XP_U16 y, 
                         XP_Bool* onDividerP );
//XP_S16 pointToTileIndexW( BoardCtxt* board, XP_U16 x, XP_U16 y,
//                         XP_Bool* onDividerP );
void board_selectPlayer( BoardCtxt* board, XP_U16 newPlayer, XP_Bool canPeek );
void flipIf( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
             XP_U16* fCol, XP_U16* fRow );
XP_Bool pointOnSomething( BoardCtxt* board, XP_U16 x, XP_U16 y, 
                          BoardObjectType* wp );
XP_Bool coordToCell( const BoardCtxt* board, XP_S16 xx, XP_S16 yy, 
                     XP_U16* colP, XP_U16* rowP );
XP_Bool cellOccupied( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                      XP_Bool inclPending );
XP_Bool holdsPendingTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow );

XP_Bool moveTileToBoard( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                         XP_U16 tileIndex, Tile blankFace );
XP_Bool board_replaceNTiles( BoardCtxt* board, XP_U16 nTiles );

void invalTilesUnderRect( BoardCtxt* board, const XP_Rect* rect );
void invalCellRegion( BoardCtxt* board, XP_U16 colA, XP_U16 rowA, XP_U16 colB, 
                      XP_U16 rowB );
void invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row );
void invalDragObj( BoardCtxt* board, const DragObjInfo* di );
#ifdef XWFEATURE_CROSSHAIRS
void invalCol( BoardCtxt* board, XP_U16 col );
void invalRow( BoardCtxt* board, XP_U16 row );
#endif

void invalTrayTilesAbove( BoardCtxt* board, XP_U16 tileIndex );
void invalTrayTilesBetween( BoardCtxt* board, XP_U16 tileIndex1, 
                            XP_U16 tileIndex2 );
#ifdef XWFEATURE_MINIWIN
void makeMiniWindowForText( BoardCtxt* board, const XP_UCHAR* text, 
                            MiniWindowType winType );
void hideMiniWindow( BoardCtxt* board, XP_Bool destroy,
                     MiniWindowType winType );

void invalSelTradeWindow( BoardCtxt* board );
#else
# define invalSelTradeWindow(b)
#endif

XP_Bool getCellRect( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                     XP_Rect* rect);
void getDragCellRect( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                      XP_Rect* rectP );
void invalCellsUnderRect( BoardCtxt* board, const XP_Rect* rect );

#ifdef XWFEATURE_SEARCHLIMIT
void invalCurHintRect( BoardCtxt* board, XP_U16 player );
#endif


void moveTileInTray( BoardCtxt* board, XP_U16 moveTo, XP_U16 moveFrom );
XP_Bool handleTrayDuringTrade( BoardCtxt* board, XP_S16 index );
void getSelTiles( const BoardCtxt* board, TileBit selBits, TrayTileSet* selTiles );

const XP_UCHAR* getTileDrawInfo( const BoardCtxt* board, Tile tile, 
                                 XP_Bool isBlank, XP_Bitmaps* bitmaps, 
                                 XP_S16* value );
XP_Bool dividerMoved( BoardCtxt* board, XP_U8 newLoc );

XP_Bool scrollIntoView( BoardCtxt* board, XP_U16 col, XP_U16 row );
XP_Bool willScrollIntoViewX( BoardCtxt* board, XP_U16 col);
XP_Bool willScrollIntoViewY( BoardCtxt* board, XP_U16 row );
XP_Bool onBorderCanScroll( const BoardCtxt* board, SDIndex indx, XP_U16 row, 
                           XP_S16* change );
XP_Bool adjustXOffset( BoardCtxt* board, XP_S16 moveBy );
XP_Bool adjustYOffset( BoardCtxt* board, XP_S16 moveBy );
XP_Bool rectContainsRect( const XP_Rect* rect1, const XP_Rect* rect2 );


#ifdef KEYBOARD_NAV
XP_Bool tray_moveCursor( BoardCtxt* board, XP_Key cursorKey, 
                         XP_Bool preflightOnly, XP_Bool* up );
void adjustForDivider( const BoardCtxt* board, XP_S16* index );
XP_Bool tray_keyAction( BoardCtxt* board );
DrawFocusState dfsFor( BoardCtxt* board, BoardObjectType obj );
void shiftFocusUp( BoardCtxt* board, XP_Key key );
void getFocussedTileCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp );
void getRectCenter( const XP_Rect* rect, XP_U16* xp, XP_U16* yp );
#else
# define dfsFor( board, obj ) DFS_NONE
#endif

// +W new in tray.c
int getDrawIndex(ModelCtxt* model, XP_S16 turn, XP_U16 index);
int getRealIndex(ModelCtxt* model, XP_S16 turn, XP_U16 index);
int compare (const void * a, const void * b);

#ifdef CPLUS
}
#endif

#endif

/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997 - 2011 by Eric House (xwords@eehouse.org).  All rights
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

#include "boardp.h"
#include "dragdrpp.h"
#include "strutils.h"
#include "modelp.h"

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static void drawPendingScore(BoardCtxt *board, XP_S16 turnScore,
                             XP_Bool hasCursor);

static XP_S16 figurePendingScore(const BoardCtxt *board);

static XP_U16 countTilesToShow(BoardCtxt *board);

static void figureDividerRect(BoardCtxt *board, XP_Rect *rect);

static XP_S16
trayLocToIndex(BoardCtxt *board, XP_U16 loc) {
    if (loc >= model_getNumTilesInTray(board->model,
                                       board->selPlayer)) {
        loc *= -1;
        /* (0 * -1) is still 0, so reduce by 1.  Will need to adjust
           below.  NOTE: this is something of a hack.*/
        --loc;
    }
    return loc;
} /* trayLocToIndex */

XP_S16
pointToTileIndex(BoardCtxt *board, XP_U16 x, XP_U16 y, XP_Bool *onDividerP) {
    XP_S16 result = -1; /* not on a tile */
    XP_Rect divider;
    XP_Rect biggerRect;
    XP_Bool onDivider;

    figureDividerRect(board, &divider);

    /* The divider rect is narrower and kinda hard to tap on.  Let's expand
       it just for this test */
    biggerRect = divider;
    if (board->srcIsPen) {
        biggerRect.left -= 2;   /* should be in proportion to tile dims */
        biggerRect.width += 4;
    }
    onDivider = rectContainsPt(&biggerRect, x, y);

    if (!onDivider) {
        if (x > divider.left) {
            XP_ASSERT(divider.width == board->dividerWidth);
            x -= divider.width;
        }

        XP_ASSERT(x >= board->trayBounds.left);
        x -= board->trayBounds.left;
        result = x / board->trayScaleH;
        result = trayLocToIndex(board, result);
    }

    if (onDividerP != NULL) {
        *onDividerP = onDivider;
    }

    return result;
} /* pointToTileIndex */

void
figureTrayTileRect(BoardCtxt *board, XP_U16 index, XP_Rect *rect) {
    rect->left = board->trayBounds.left + (index * board->trayScaleH);
    rect->top = board->trayBounds.top/*  + 1 */;

    rect->width = board->trayScaleH;
    rect->height = board->trayScaleV;

    if (board->selInfo->dividerLoc <= index) {
        rect->left += board->dividerWidth;
    }
} /* figureTileRect */

/* When drawing tray mid-drag:
 *
 * Rule is not to touch the model.  
 *
 * Cases: Tile's been dragged into tray (but not yet dropped.); tile's been
 * dragged out of tray (but not yet dropped); and tile's been dragged within
 * tray.  More's the point, there's an added tile and a removed one.  We draw
 * the added tile extra, and skip the removed one.
 *
 * We're walking two arrays at once, backwards.  The first is the tile rects
 * themselves.  If the dirty bit is set, something must get drawn.  The second
 * is the model's view of tiles augmented by drag-and-drop.  D-n-d may have
 * removed a tile from the tray (for drawing purposes only), have added one,
 * or both (drag-within-tray case).  Since a drag lasts only until pen-up,
 * there's never more than one tile involved.  Adjustment is never by more
 * than one.
 *
 * So while one counter (i) walks the array of rects, we can't use it
 * unmodified to fetch from the model.  Instead we increment or decrement it
 * based on the drag state.
 */

void
drawTray(BoardCtxt *board) {
    XP_Rect tileRect;

    if ((board->trayInvalBits != 0) || board->dividerInvalid) {
        const XP_S16 turn = board->selPlayer;
        PerTurnInfo *pti = board->selInfo;

        XP_S16 turnScore = figurePendingScore(board);

        if (draw_trayBegin(board->draw, &board->trayBounds, turn,
                           turnScore, dfsFor(board, OBJ_TRAY))) {
            DictionaryCtxt *dictionary = model_getDictionary(board->model);
            XP_S16 cursorBits = 0;
            XP_Bool cursorOnDivider = XP_FALSE;
#ifdef KEYBOARD_NAV
            XP_S16 cursorTile = pti->trayCursorLoc;
            if ( (board->focussed == OBJ_TRAY) && !board->hideFocus ) {
                cursorOnDivider = pti->dividerLoc == cursorTile;
                if ( board->focusHasDived ) {
                    if ( !cursorOnDivider ) {
                        adjustForDivider( board, &cursorTile );
                        cursorBits = 1 << cursorTile;
                    }
                } else {
                    cursorBits = ALLTILES;
                    cursorOnDivider = XP_TRUE;
                }
            }
#endif

            if (dictionary != NULL) {
                XP_Bool showFaces = board->trayVisState == TRAY_REVEALED;
                Tile blank = dict_getBlankTile(dictionary);

                if (turn >= 0) {
                    XP_S16 ii; /* which tile slot are we drawing in */
                    XP_U16 ddAddedIndx, ddRmvdIndx;
                    XP_U16 numInTray = countTilesToShow(board);
                    XP_Bool isBlank;
                    XP_Bool isADrag = dragDropInProgress(board);
                    CellFlags baseFlags = board->hideValsInTray && !board->showCellValues
                                          ? CELL_VALHIDDEN : CELL_NONE;

                    dragDropGetTrayChanges(board, &ddRmvdIndx, &ddAddedIndx);

                    /* draw in reverse order so drawing happens after
                       erasing */
                    for (ii = MAX_TRAY_TILES - 1; ii >= 0; --ii) {
                        CellFlags flags = baseFlags;
                        XP_U16 mask = 1 << ii;

//                        int drawIndex = getDrawIndex(board->model, board->selPlayer, ii);
//                        XP_Bool ddOnBoard = isADrag && (ddRmvdIndx != ddAddedIndx) && (ddRmvdIndx == ii);
//                        XP_Bool ddInTray = isADrag && (ddRmvdIndx == ddAddedIndx) && (ddRmvdIndx == ii);
//                    	__android_log_print(ANDROID_LOG_INFO, "drawTray", "ii=%i isADrag=%i ddRmvdIndx=%i ddAddedIndx=%i ddOnBoard=%i ddInTray=%i", ii, isADrag, ddRmvdIndx, ddAddedIndx, ddOnBoard, ddInTray);
//                        if (ddOnBoard){
//                        	figureTrayTileRect( board, drawIndex, &tileRect );
//                        	draw_drawTile( board->draw, &tileRect, NULL, NULL, -1, flags | CELL_ISEMPTY );
//                        	continue;
//                        }
                        if ((board->trayInvalBits & mask) == 0) {
                            continue;
                        }
#ifdef KEYBOARD_NAV
                        if ( (cursorBits & mask) != 0 ) {
                            flags |= CELL_ISCURSOR;
                        }
#endif
                        figureTrayTileRect(board, ii, &tileRect);
                        if (ii >= numInTray) {
                            tileRect.top = tileRect.top + board->traySteal;
                            tileRect.height = tileRect.height - board->traySteal;
                            draw_drawTile(board->draw, &tileRect, NULL,
                                          NULL, -1, flags | CELL_ISEMPTY);
                        } else if (showFaces) {
                            XP_Bitmaps bitmaps;
                            const XP_UCHAR *textP = (XP_UCHAR *) NULL;
                            XP_U8 traySelBits = pti->traySelBits;
                            XP_S16 value;
                            Tile tile;

                            if (ddAddedIndx == ii) {
                                dragDropTileInfo(board, &tile, &isBlank);
                            } else {
                                XP_U16 modIndex = ii;
                                if (ddAddedIndx < ii) {
                                    --modIndex;
                                }
                                /* while we're right of the removal area,
                                   draw the one from the right to cover. */
                                if (ddRmvdIndx <= modIndex /*slotIndx*/ ) {
                                    ++modIndex;
                                }
                                tile = model_getPlayerTile(board->model,
                                                           turn, modIndex);
                                isBlank = tile == blank;
                            }

                            textP = getTileDrawInfo(board, tile, isBlank,
                                                    &bitmaps, &value);
                            if (isADrag) {
                                if (ddAddedIndx == ii) {
                                    flags |= CELL_HIGHLIGHT;
                                }
                            } else if ((traySelBits & (1 << ii)) != 0) {
                                flags |= CELL_HIGHLIGHT;
                            }
                            if (isBlank) {
                                flags |= CELL_ISBLANK;
                            }

                            tileRect.top = tileRect.top + board->traySteal;
                            tileRect.height = tileRect.height - board->traySteal;
                            draw_drawTile(board->draw, &tileRect, textP,
                                          bitmaps.nBitmaps > 0 ? &bitmaps : NULL,
                                          value, flags);
                        } else {
                            tileRect.top = tileRect.top + board->traySteal;
                            tileRect.height = tileRect.height - board->traySteal;
                            draw_drawTileBack(board->draw, &tileRect, flags);
                        }
                    }
                    figureTrayTileRect(board, 7 - 1, &tileRect);
                    tileRect.top = tileRect.top + board->traySteal;
                    tileRect.height = tileRect.height - board->traySteal;
                    XP_Rect tileRectA;
                    XP_Rect tileRectB;
                    XP_Rect tileRectEmpty;
                    figureTrayTileRect(board, numInTray, &tileRectA);
                    figureTrayTileRect(board, 7 - 2, &tileRectB);
                    tileRectEmpty.top = tileRect.top;
                    tileRectEmpty.height = tileRect.height;
                    tileRectEmpty.left = tileRectA.left;
                    tileRectEmpty.width = tileRectB.left - tileRectA.left + tileRectB.width;
                    // Show/hide.
//                    __android_log_print(ANDROID_LOG_INFO, "tray.c", "drawTray draw_updateTrayButtons");
//                    __android_log_print(ANDROID_LOG_INFO, "tray.c", "drawTray tileRect ltwh=%i %i %i %i", tileRect.left, tileRect.top, tileRect.width, tileRect.height);
//                    __android_log_print(ANDROID_LOG_INFO, "tray.c", "drawTray tileRectEmpty ltwh=%i %i %i %i", tileRectEmpty.left, tileRectEmpty.top, tileRectEmpty.width, tileRectEmpty.height);
//                    __android_log_print(ANDROID_LOG_INFO, "tray.c", "drawTray numInTray=%i", numInTray);
                    draw_updateTrayButtons(board->draw, &tileRect, &tileRectEmpty, numInTray);
                }

                if ((board->dividerWidth > 0) && board->dividerInvalid) {
                    CellFlags flags = cursorOnDivider ? CELL_ISCURSOR : CELL_NONE;
                    XP_Rect divider;
                    figureDividerRect(board, &divider);
                    if (pti->dividerSelected
                        || dragDropIsDividerDrag(board)) {
                        flags |= CELL_HIGHLIGHT;
                    }
                    draw_drawTrayDivider(board->draw, &divider, flags);
                    board->dividerInvalid = XP_FALSE;
                }
                drawPendingScore(board, turnScore,
                                 (cursorBits & (1 << (MAX_TRAY_TILES - 1))) != 0);
            }

            draw_objFinished(board->draw, OBJ_TRAY, &board->trayBounds,
                             dfsFor(board, OBJ_TRAY));

            board->trayInvalBits = 0;
        }
    }

} /* drawTray */



const XP_UCHAR *
getTileDrawInfo(const BoardCtxt *board, Tile tile, XP_Bool isBlank,
                XP_Bitmaps *bitmaps, XP_S16 *value) {
    const XP_UCHAR *face = NULL;
    DictionaryCtxt *dict = model_getDictionary(board->model);
    if (isBlank) {
        tile = dict_getBlankTile(dict);
    } else {
        face = dict_getTileString(dict, tile);
    }

    *value = dict_getTileValue(dict, tile);
    if (!isBlank && dict_faceIsBitmap(dict, tile)) {
        dict_getFaceBitmaps(dict, tile, bitmaps);
    } else {
        bitmaps->nBitmaps = 0;
    }

    return face;
}

static XP_U16
countTilesToShow(BoardCtxt *board) {
    XP_U16 numToShow;
    XP_S16 selPlayer = board->selPlayer;
    XP_U16 ddAddedIndx, ddRemovedIndx;

    XP_ASSERT(selPlayer >= 0);
    if (board->trayVisState == TRAY_REVEALED) {
        numToShow = model_getNumTilesInTray(board->model, selPlayer);
    } else {
        numToShow = model_getNumTilesTotal(board->model, selPlayer);
    }

    dragDropGetTrayChanges(board, &ddRemovedIndx, &ddAddedIndx);
    if (ddAddedIndx < MAX_TRAY_TILES) {
        ++numToShow;
    }
    if (ddRemovedIndx < MAX_TRAY_TILES) {
        --numToShow;
    }

    XP_ASSERT(numToShow <= MAX_TRAY_TILES);
    return numToShow;
} /* countTilesToShow */

static XP_S16
figurePendingScore(const BoardCtxt *board) {
    XP_S16 turnScore;
    (void) getCurrentMoveScoreIfLegal(board->model, board->selPlayer,
                                      (XWStreamCtxt *) NULL,
                                      (WordNotifierInfo *) NULL,
                                      &turnScore);
    return turnScore;
}

static void
drawPendingScore(BoardCtxt *board, XP_S16 turnScore, XP_Bool hasCursor) {
    /* Draw the pending score down in the last tray's rect */
    if (countTilesToShow(board) < MAX_TRAY_TILES) {
        XP_U16 selPlayer = board->selPlayer;
        XP_Rect lastTileR;

        figureTrayTileRect(board, MAX_TRAY_TILES - 1, &lastTileR);
        draw_score_pendingScore(board->draw, &lastTileR, turnScore,
                                selPlayer,
                                hasCursor ? CELL_ISCURSOR : CELL_NONE);
    }
} /* drawPendingScore */

static void
figureDividerRect(BoardCtxt *board, XP_Rect *rect) {
    figureTrayTileRect(board, board->selInfo->dividerLoc, rect);
    rect->left -= board->dividerWidth;
    rect->width = board->dividerWidth;
} /* figureDividerRect */

void
invalTilesUnderRect(BoardCtxt *board, const XP_Rect *rect) {
    /* This is an expensive way to do this -- calculating all the rects rather
       than starting with the bounds of the rect passed in -- but this
       function is called so infrequently and there are only 7 tiles, so leave
       it for now.  If it needs to be faster, invalCellsUnderRect is the model
       to use. */

    XP_U16 ii;
    XP_Rect locRect;

    for (ii = 0; ii < MAX_TRAY_TILES; ++ii) {
        figureTrayTileRect(board, ii, &locRect);
        if (rectsIntersect(rect, &locRect)) {
            board_invalTrayTiles(board, (TileBit) (1 << ii));
        }
    }

    figureDividerRect(board, &locRect);
    if (rectsIntersect(rect, &locRect)) {
        board->dividerInvalid = XP_TRUE;
    }
} /* invalTilesUnderRect */

XP_Bool
handleTrayDuringTrade(BoardCtxt *board, XP_S16 index) {
    TileBit bits;

    XP_ASSERT(index >= 0);

    bits = 1 << index;
    board->selInfo->traySelBits ^= bits;
    board_invalTrayTiles(board, bits);
    return XP_TRUE;
} /* handleTrayDuringTrade */

void
getSelTiles(const BoardCtxt *board, TileBit selBits, TrayTileSet *selTiles) {
    XP_U16 nTiles = 0;
    XP_S16 index;
    XP_S16 turn = board->selPlayer;
    const ModelCtxt *model = board->model;

    for (index = 0; selBits != 0; selBits >>= 1, ++index) {
        if (0 != (selBits & 0x01)) {
            Tile tile = model_getPlayerTile(model, turn, index);
            XP_ASSERT(nTiles < VSIZE(selTiles->tiles));
            selTiles->tiles[nTiles++] = tile;
        }
    }
    selTiles->nTiles = nTiles;
}

static XP_Bool
handleActionInTray(BoardCtxt *board, XP_S16 index, XP_Bool onDivider) {
    XP_Bool result = XP_FALSE;
    PerTurnInfo *pti = board->selInfo;

    if (onDivider) {
        /* toggle divider sel state */
        pti->dividerSelected = !pti->dividerSelected;
        board->dividerInvalid = XP_TRUE;
        pti->traySelBits = NO_TILES;
        result = XP_TRUE;
    } else if (pti->tradeInProgress) {
        if (index >= 0) {
            result = handleTrayDuringTrade(board, index);
        }
    } else if (index >= 0) {
        result = moveTileToArrowLoc(board, (XP_U8) index);
#ifndef DISABLE_TILE_SEL
        if ( !result ) {
            TileBit newBits = 1 << index;
            XP_U8 selBits = pti->traySelBits;
            /* Tap on selected tile unselects.  If we don't do this,
               then there's no way to unselect and so no way to turn
               off the placement arrow */
            if ( newBits == selBits ) {
                board_invalTrayTiles( board, selBits );
                pti->traySelBits = NO_TILES;
            } else if ( selBits != 0 ) {
                XP_U16 selIndex = indexForBits( selBits );
                model_moveTileOnTray( board->model, board->selPlayer,
                                      selIndex, index );
                pti->traySelBits = NO_TILES;
            } else {
                 board_invalTrayTiles( board, newBits );
                 pti->traySelBits = newBits;
            }
            board->dividerInvalid = 
                board->dividerInvalid || pti->dividerSelected;
            pti->dividerSelected = XP_FALSE;
            result = XP_TRUE;
        }
#endif
    } else if (index == -(MAX_TRAY_TILES)) { /* pending score tile */
        (void) board_replaceNTiles(board, 1);
        result = XP_TRUE;
// Was:
//        result = board_commitTurn( board );
//#if defined XWFEATURE_TRAYUNDO_ALL
//    } else if ( index < 0 ) { /* other empty area */
//        /* it better be true */
//        (void)board_replaceTiles( board );
//        result = XP_TRUE;
//#elif defined XWFEATURE_TRAYUNDO_ONE
//    } else if ( index < 0 ) { /* other empty area */
//        /* it better be true */
//        (void)board_replaceNTiles( board, 1 );
//        result = XP_TRUE;
//#endif
    } else if (index < 0) {
        (void) board_replaceNTiles(board, 1);
        result = XP_TRUE;
    }
    return result;
} /* handleActionInTray */

XP_Bool
handlePenUpTray(BoardCtxt *board, XP_U16 x, XP_U16 y) {
    XP_Bool onDivider;
    //XP_S16 index = pointToTileIndexW( board, x, y, &onDivider );
    XP_S16 index = pointToTileIndex(board, x, y, &onDivider);
    return handleActionInTray(board, index, onDivider);
} /* handlePenUpTray */

XP_U16
indexForBits(XP_U8 bits) {
    XP_U16 result = 0;
    XP_U16 mask = 1;

    XP_ASSERT(bits != 0); /* otherwise loops forever */

    while ((mask & bits) == 0) {
        ++result;
        mask <<= 1;
    }
    return result;
} /* indexForBits */

XP_Bool
dividerMoved(BoardCtxt *board, XP_U8 newLoc) {
    XP_U8 oldLoc = board->selInfo->dividerLoc;
    XP_Bool moved = oldLoc != newLoc;
    if (moved) {
        board->selInfo->dividerLoc = newLoc;

        /* This divider's index corresponds to the tile it's to the left of, and
           there's no need to invalidate any tiles to the left of the uppermore
           divider position. */
        if (oldLoc > newLoc) {
            --oldLoc;
        } else {
            --newLoc;
        }
        invalTrayTilesBetween(board, newLoc, oldLoc);

        board->dividerInvalid = XP_TRUE;
        /* changed number of available tiles */
        board_resetEngine(board);
    }
    return moved;
} /* dividerMoved */

void
board_invalTrayTiles(BoardCtxt *board, TileBit what) {
    board->trayInvalBits |= what;
} /* invalTrayTiles */

void
invalTrayTilesAbove(BoardCtxt *board, XP_U16 tileIndex) {
    TileBit bits = 0;
    while (tileIndex < MAX_TRAY_TILES) {
        bits |= 1 << tileIndex++;
    }
    board_invalTrayTiles(board, bits);
}

void
invalTrayTilesBetween(BoardCtxt *board, XP_U16 tileIndex1,
                      XP_U16 tileIndex2) {
    TileBit bits = 0;

    if (tileIndex1 > tileIndex2) {
        XP_U16 tmp = tileIndex1;
        tileIndex1 = tileIndex2;
        tileIndex2 = tmp;
    }

    while (tileIndex1 <= tileIndex2) {
        bits |= (1 << tileIndex1);
        ++tileIndex1;
    }
    board_invalTrayTiles(board, bits);
} /* invalTrayTilesBetween */

XP_Bool
board_juggleTray(BoardCtxt *board) {
    XP_Bool result = XP_FALSE;
    const XP_S16 turn = board->selPlayer;

    if (checkRevealTray(board)) {
        XP_S16 nTiles;
        XP_U16 dividerLoc = board->selInfo->dividerLoc;
        ModelCtxt *model = board->model;

        nTiles = model_getNumTilesInTray(model, turn) - dividerLoc;
        if (nTiles > 1) {
            XP_S16 ii;
            Tile tmpT[MAX_TRAY_TILES];
            XP_U16 newT[MAX_TRAY_TILES];

            /* loop until there'll be change */
            while (!randIntArray(newT, nTiles)) {
            }

            /* save copies of the tiles in juggled order */
            for (ii = 0; ii < nTiles; ++ii) {
                tmpT[ii] = model_getPlayerTile(model, turn,
                                               (Tile) (dividerLoc + newT[ii]));
            }

            /* delete tiles off right end; put juggled ones back on the other */
            for (ii = nTiles - 1; ii >= 0; --ii) {
                (void) model_removePlayerTile(model, turn, -1);
                model_addPlayerTile(model, turn, dividerLoc, tmpT[ii]);
            }
            board->selInfo->traySelBits = 0;
            result = XP_TRUE;
        }
    }
    return result;
} /* board_juggleTray */

#ifdef KEYBOARD_NAV
void
adjustForDivider( const BoardCtxt* board, XP_S16* index )
{
    XP_U16 dividerLoc = board->selInfo->dividerLoc;
    if ( dividerLoc <= *index ) {
        --*index;
    }
}

XP_Bool
tray_moveCursor( BoardCtxt* board, XP_Key cursorKey, XP_Bool preflightOnly,
                 XP_Bool* pUp )
{
    XP_Bool draw = XP_FALSE;
    XP_Bool up = XP_FALSE;

    if ( cursorKey == XP_CURSOR_KEY_UP || cursorKey == XP_CURSOR_KEY_DOWN ) {
        up = XP_TRUE;
    } else if ( (cursorKey == XP_CURSOR_KEY_RIGHT)
                || (cursorKey == XP_CURSOR_KEY_LEFT) ) {
        XP_Bool resetEngine = XP_FALSE;
        XP_S16 delta = cursorKey == XP_CURSOR_KEY_RIGHT ? 1 : -1;
        const XP_U16 selPlayer = board->selPlayer;
        PerTurnInfo* pti = board->selInfo;
        XP_S16 trayCursorLoc;
        XP_S16 newLoc;
        for ( ; ; ) {
            trayCursorLoc = pti->trayCursorLoc;
            newLoc = trayCursorLoc + delta;
            if ( newLoc < 0 || newLoc > MAX_TRAY_TILES ) {
                up = XP_TRUE;
            } else if ( !preflightOnly ) {
                XP_S16 tileLoc = trayCursorLoc;
                XP_U16 nTiles = board->trayVisState == TRAY_REVEALED
                    ? model_getNumTilesInTray( board->model, selPlayer ) 
                    : MAX_TRAY_TILES;
                XP_Bool cursorOnDivider = trayCursorLoc == pti->dividerLoc;
                XP_Bool cursorObjSelected;
                XP_S16 newTileLoc;

                adjustForDivider( board, &tileLoc );
                cursorObjSelected = cursorOnDivider?
                    pti->dividerSelected : pti->traySelBits == (1 << tileLoc);

                if ( !cursorObjSelected ) {
                    /* nothing to do */
                } else if ( cursorOnDivider ) {
                    /* just drag the divider */
                    pti->dividerLoc = newLoc;
                    resetEngine = XP_TRUE;
                } else if ( pti->tradeInProgress ) {
                    /* nothing to do */
                } else {
                    /* drag the tile, skipping over the divider if needed */
                    if ( (newLoc == pti->dividerLoc) && (newLoc > 0) ) {
                        newLoc += delta;
                        resetEngine = XP_TRUE;
                    }
                    newTileLoc = newLoc;
                    adjustForDivider( board, &newTileLoc );

                    if ( newTileLoc >= 0 ) {
                        XP_ASSERT( tileLoc < nTiles );
                        if ( newTileLoc < nTiles ) {
                            model_moveTileOnTray( board->model, selPlayer, 
                                                  tileLoc, newTileLoc );
                            pti->traySelBits = (1 << newTileLoc);
                        } else {
                            pti->traySelBits = 0; /* clear selection */
                        }
                    }
                }
                pti->trayCursorLoc = newLoc;

                /* Check if we're settling on an empty tile location other
                   than the rightmost one.  If so, loop back and move
                   further. */
                newTileLoc = newLoc;
                adjustForDivider( board, &newTileLoc );

                if ( (newTileLoc > nTiles)
                     && (newLoc != pti->dividerLoc)
                     && (newTileLoc < MAX_TRAY_TILES-1) ) {
                    continue;
                }
            }
            break;              /* always exit loop if we get here */
        }
            
        /* PENDING: don't just inval everything */
        board->dividerInvalid = XP_TRUE;
        board_invalTrayTiles( board, ALLTILES );
        if ( resetEngine ) {
            board_resetEngine( board );
        }
    }
    draw = XP_TRUE;

    *pUp = up;
    return draw;
} /* tray_moveCursor */

void
getFocussedTileCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Rect rect;
    PerTurnInfo* pti = board->selInfo;
    XP_S16 cursorTile = pti->trayCursorLoc;
    XP_Bool cursorOnDivider = pti->dividerLoc == cursorTile;

    if ( cursorOnDivider ) {
        figureDividerRect( board, &rect );
    } else {
        XP_S16 indx = pti->trayCursorLoc;
        adjustForDivider( board, &indx );
        XP_ASSERT( indx >= 0 );
        figureTrayTileRect( board, indx, &rect );
    }
    getRectCenter( &rect, xp, yp );
}

#endif /* KEYBOARD_NAV */

#if defined FOR_GREMLINS
XP_Bool
board_moveDivider( BoardCtxt* board, XP_Bool right )
{
    XP_Bool result = board->trayVisState == TRAY_REVEALED;
    if ( result ) {
        XP_U8 loc = board->selInfo->dividerLoc;
        loc += MAX_TRAY_TILES + 1;
        loc += right? 1:-1;
        loc %= MAX_TRAY_TILES + 1;

        (void)dividerMoved( board, loc );
    }
    return result;
} /* board_moveDivider */
#endif


//
// +W
//

// For sorting.
int compare(const void *a, const void *b) {
    return (*(int *) a - *(int *) b);
}

int getDrawIndex(ModelCtxt *model, XP_S16 turn, XP_U16 index) {
    PlayerCtxt *player;
    PendingTile *pt;
    int i;
    int indexDraw;
    int homeIndex[MAX_TRAY_TILES];

    XP_ASSERT(turn >= 0);
    player = &model->players[turn];

    // Copy and sort copy.
    for (i = 0; i < player->nPending; i++) {
        pt = &player->pendingTiles[i];
        homeIndex[i] = pt->home;
    }
    qsort(homeIndex, player->nPending, sizeof(int), compare);
    // What index should be if pending tiles were back in the tray.
    indexDraw = index;
    for (i = 0; i < player->nPending; i++) {
        if (homeIndex[i] <= indexDraw) {
            indexDraw = indexDraw + 1;
        }
    }
    return indexDraw;
}

int getRealIndex(ModelCtxt *model, XP_S16 turn, XP_U16 index) {
    PlayerCtxt *player;
    PendingTile *pt;
    int i;
    int indexReal;
    int homeIndex[MAX_TRAY_TILES];
    XP_Bool awayFromHome;

    XP_ASSERT(turn >= 0);
    player = &model->players[turn];

    // Copy and sort copy.
    for (i = 0; i < player->nPending; i++) {
        pt = &player->pendingTiles[i];
        homeIndex[i] = pt->home;
    }
    qsort(homeIndex, player->nPending, sizeof(int), compare);
    indexReal = index;
    awayFromHome = 0;
    for (i = 0; i < player->nPending && !awayFromHome; i++) {
        awayFromHome = (homeIndex[i] == indexReal);
    }
    for (i = player->nPending - 1; i >= 0 && !awayFromHome; i--) {
        if (homeIndex[i] < indexReal) {
            indexReal = indexReal - 1;
        }
    }
    if (awayFromHome) {
        indexReal = -1 * indexReal;
        indexReal = indexReal - 1;
    }
    return indexReal;
}

XP_S16
pointToTileIndexW(BoardCtxt *board, XP_U16 x, XP_U16 y, XP_Bool *onDividerP) {
    XP_S16 result = -1; /* not on a tile */
    XP_Rect divider;
    XP_Rect biggerRect;
    XP_Bool onDivider;

    figureDividerRect(board, &divider);

    /* The divider rect is narrower and kinda hard to tap on.  Let's expand
       it just for this test */
    biggerRect = divider;
    if (board->srcIsPen) {
        biggerRect.left -= 2;   /* should be in proportion to tile dims */
        biggerRect.width += 4;
    }
    onDivider = rectContainsPt(&biggerRect, x, y);

    if (!onDivider) {
        if (x > divider.left) {
            XP_ASSERT(divider.width == board->dividerWidth);
            x -= divider.width;
        }

        XP_ASSERT(x >= board->trayBounds.left);
        x -= board->trayBounds.left;
        result = x / board->trayScaleH;
        // Index without the tiles away from home.
        result = getRealIndex(board->model, board->selPlayer, result);
        if (result >= model_getNumTilesInTray(board->model, board->selPlayer)) {
            result *= -1;
            --result;
        }
    }

    if (onDividerP != NULL) {
        *onDividerP = onDivider;
    }

    return result;
}


#ifdef CPLUS
}
#endif

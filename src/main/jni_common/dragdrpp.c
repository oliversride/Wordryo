/* -*-mode: C; fill-column: 78; compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997 - 2010 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef CPLUS
extern "C" {
#endif

#include "dragdrpp.h"
#include "game.h"

/* How many squares must scroll gesture take in to be recognized. */
#ifndef SCROLL_DRAG_THRESHHOLD
# define SCROLL_DRAG_THRESHHOLD 1
#endif

static XP_Bool dragDropContinueImpl( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_U16 offset,
									 XP_Bool scrollBoard, BoardObjectType* onWhichP );
static void invalDragObjRange( BoardCtxt* board, const DragObjInfo* from, 
                               const DragObjInfo* to );
#ifdef XWFEATURE_SEARCHLIMIT
static void invalHintRectDiffs( BoardCtxt* board, const DragObjInfo* cur, 
                                const DragObjInfo* nxt );
static void setLimitsFrom( const BoardCtxt* board, BdHintLimits* limits );
#endif

#ifdef XWFEATURE_CROSSHAIRS
static XP_Bool crosshairs_set( BoardCtxt* board, XP_S16 col, XP_S16 row );
static void crosshairs_clear( BoardCtxt* board );
#else
# define crosshairs_set( board, col, row ) XP_FALSE;
# define crosshairs_clear( board )
#endif

static void startScrollTimerIf( BoardCtxt* board, XP_Bool scrollBoardOn );
static XP_Bool mBoardScrollOn = XP_FALSE;

XP_Bool
dragDropInProgress( const BoardCtxt* board )
{
    const DragState* ds = &board->dragState;
/*     LOG_RETURNF( "%d", ds->dragInProgress ); */
    return ds->dtype != DT_NONE;
} /* dragDropInProgress */

XP_Bool
dragDropHasMoved( const BoardCtxt* board )
{
    XP_Bool moved = dragDropInProgress( board );
    if ( moved ) {
        if ( board->dragState.didMove ) {
            /* something was dragged; do nothing */
        } else {
            const DragState* ds = &board->dragState;
            moved = ds->cellChanged; /* did non-drag movement happen? */
        }
    }
    return moved;
} /* dragDropHasMoved */

static void
clearDS( DragState* ds )
{
    XP_MEMSET( ds, 0, sizeof(*ds) );
#ifdef XWFEATURE_CROSSHAIRS
    ds->crosshairs.col = ds->crosshairs.row = -1;
#endif
}

static XP_Bool
ddStartBoard( BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    DragState* ds = &board->dragState;
    XP_Bool trayVisible;
    XP_U16 col, row;

#ifdef DEBUG
    XP_Bool found =
#endif
        coordToCell( board, xx, yy, &col, &row );
    XP_ASSERT( found );
#ifdef XWFEATURE_CROSSHAIRS
    if ( !board->hideCrosshairs ) {
        (void)crosshairs_set( board, col, row );
    }
#endif

    trayVisible = board->trayVisState == TRAY_REVEALED;
    if ( trayVisible && holdsPendingTile( board, col, row ) ) {
        XP_U16 modelc, modelr;

        ds->dtype = DT_TILE;
        flipIf( board, col, row, &modelc, &modelr );

#ifdef DEBUG
        found = 
#endif
            model_getTile( board->model, modelc, modelr, XP_TRUE, 
                           board->selPlayer, &ds->tile, &ds->isBlank, 
                           NULL, NULL );
        XP_ASSERT( found );
    } else {
        /* If we're not dragging a tile, we can either drag the board (scroll)
           or work on hint regions.  Sometimes scrolling isn't possible.
           Sometimes hint dragging is disabled.  But if both are possible,
           then the alt key determines it.  I figure scrolling will be more
           common than hint dragging when both are possible, but you can turn
           hint dragging off, so if it's on that's probably what you want. */
        ScrollData* vsd = &board->sd[SCROLL_V];
        XP_Bool canScroll = vsd->lastVisible < model_numRows(board->model);
        if ( 0 ) {
#ifdef XWFEATURE_SEARCHLIMIT
        } else if ( !board->gi->hintsNotAllowed && board->gi->allowHintRect
                    && trayVisible ) {
            if ( !util_altKeyDown(board->util) ) {
                ds->dtype = DT_HINTRGN;
            } else if ( canScroll ) {
                ds->dtype = DT_BOARD;
            }
#endif
        } else if ( canScroll ) {
            ds->dtype = DT_BOARD;
        }
    }
    ds->start.u.board.col = col;
    ds->start.u.board.row = row;

    return ds->dtype != DT_NONE;
} /* ddStartBoard */

static XP_Bool
ddStartTray( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool canDrag;
    DragState* ds = &board->dragState;

    XP_Bool onDivider;
    XP_S16 index = pointToTileIndex( board, x, y, &onDivider );
//    XP_S16 index = pointToTileIndexW( board, x, y, &onDivider );
    canDrag = onDivider || index >= 0;
    if ( canDrag ) {
        if ( onDivider ) {
            board->dividerInvalid = XP_TRUE;
            ds->start.u.tray.index = board->selInfo->dividerLoc;

            ds->dtype = DT_DIVIDER;
        } else {
            Tile tile;
            tile = model_getPlayerTile( board->model, board->selPlayer, index );
            ds->isBlank = 
                tile == dict_getBlankTile( model_getDictionary(board->model) );
            ds->tile = tile;

            ds->start.u.tray.index = index;

            /* during drag the moving tile is drawn as selected, so inval
               currently selected tile. */
            board_invalTrayTiles( board, board->selInfo->traySelBits );

            ds->dtype = DT_TILE;
        }
    }

    return canDrag;
} /* ddStartTray */

/* x and y, in board coordinates (not model, should the board be flipped), are
 * already col,row (cells) in the board case, but real x/y (pixels) on the
 * tray.
 */
XP_Bool
dragDropStart( BoardCtxt* board, BoardObjectType obj, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;
    DragState* ds = &board->dragState;
    if ( dragDropInProgress(board) ) {
        XP_LOGF( "warning: starting drag while dragDropInProgress() true" );
    }
    clearDS( ds );

    ds->start.obj = obj;

    if ( OBJ_BOARD == obj ) {
        result = ddStartBoard( board, x, y );
    } else if ( OBJ_TRAY == obj ) {
        result = ddStartTray( board, x, y );
    } else {
        XP_ASSERT(0);
    }

    if ( result ) {
        ds->cur = ds->start;
        invalDragObj( board, &ds->start );
        startScrollTimerIf( board, XP_TRUE );
    }

    return result;
} /* dragDropStart */

/* We're potentially dragging.  If we're leaving one object, inval it.
 * If we're entering another, inval it.  Track where we are so we don't inval
 * again.  If we didn't really move, don't inval anything!
 *
 * Our overriding concern must be to preserve data integrity in the face of
 * buggy OSes that may drop events (esp. MouseUp).  So never own a tile: keep
 * it attached either to the tray or the board.  If the state of the board is
 * to change as dragging happens, that's for the board (view) to display, not
 * the model.  So drawing has to be aware of drag-and-drop, but we don't
 * change the model until the pen's actually lifted.  If we never get the
 * pen-up event at the worst the board's drawn incorrectly for a bit.
 *
 * Exception: since divider location is in the board rather than the model
 * just change it every time we can.
 */
XP_Bool
dragDropContinue( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_U16 offset, XP_Bool scrollBoard )
{
    XP_ASSERT( dragDropInProgress(board) );

    return dragDropContinueImpl( board, xx, yy, offset, scrollBoard, NULL );
}

XP_Bool
dragDropEnd( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_Bool* dragged, XP_Bool* trayToBoard )
{
    DragState* ds = &board->dragState;
    BoardObjectType newObj;

    XP_ASSERT( dragDropInProgress(board) );

    *trayToBoard = XP_FALSE;

    (void)dragDropContinueImpl( board, xx, yy, 0, XP_TRUE, &newObj );
    *dragged = ds->didMove;

    /* If we've dropped on something, put the tile there!  Since we
       don't remove it from its earlier location until it's dropped,
       we need to specify where it's coming from. */
    if ( ds->dtype == DT_DIVIDER ) {
        board->dividerInvalid = XP_TRUE;
        /* nothing to do */
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( ds->dtype == DT_HINTRGN ) {
        if ( OBJ_BOARD == newObj && ds->didMove ) {
            XP_Bool makeActive = ds->start.u.board.row <= ds->cur.u.board.row;
            board->selInfo->hasHintRect = makeActive;
            if ( makeActive ) {
                setLimitsFrom( board, &board->selInfo->limits );
            } else {
                invalHintRectDiffs( board, &ds->cur, NULL );
            }
            board_resetEngine( board );
        } else {
            /* return it to whatever it was */
            invalHintRectDiffs( board, &ds->cur, NULL );
            invalCurHintRect( board, board->selPlayer );
        }
#endif
    } else if ( ds->dtype == DT_BOARD ) {
        /* do nothing */
    } else {
        XP_U16 mod_startc, mod_startr;

        flipIf( board, ds->start.u.board.col, ds->start.u.board.row,
                &mod_startc, &mod_startr );

        if ( newObj == OBJ_TRAY ) {
            if ( ds->start.obj == OBJ_BOARD ) { /* board->tray is pending */
                model_moveBoardToTray( board->model, board->selPlayer, 
                                       mod_startc, mod_startr, 
                                       ds->cur.u.tray.index );
            } else {
                model_moveTileOnTray( board->model, board->selPlayer, 
                                      ds->start.u.tray.index,
                                      ds->cur.u.tray.index );
            }
        } else if ( (newObj == OBJ_BOARD) &&
                    !cellOccupied( board, ds->cur.u.board.col, 
                                   ds->cur.u.board.row, XP_TRUE ) ) {
            if ( ds->start.obj == OBJ_TRAY ) {

                /* moveTileToBoard flips its inputs */
                (void)moveTileToBoard( board, ds->cur.u.board.col, 
                                       ds->cur.u.board.row,
                                       ds->start.u.tray.index, EMPTY_TILE );
                // Tile moved from tray to board.
                *trayToBoard = XP_TRUE;
            } else if ( ds->start.obj == OBJ_BOARD ) {
                XP_U16 mod_curc, mod_curr;
                flipIf( board, ds->cur.u.board.col, ds->cur.u.board.row,
                        &mod_curc, &mod_curr );
                if ( model_moveTileOnBoard( board->model, board->selPlayer, 
                                            mod_startc, mod_startr, mod_curc, 
                                            mod_curr ) ) {
                    /* inval points tile in case score changed */
                    board_invalTrayTiles( board, 1 << (MAX_TRAY_TILES-1) );
                }
            }
        } else {
            /* We're returning it to start, so will be re-inserted in tray */
            if ( OBJ_TRAY == ds->start.obj ) {
                board_invalTrayTiles( board, ALLTILES );
            }
        }

        /* These may change appearance, e.g. from big tile to dropped cell. */
        invalDragObj( board, &ds->cur );
        invalDragObj( board, &ds->start );
    }

    crosshairs_clear( board );
    ds->dtype = DT_NONE;

    return XP_TRUE;
} /* dragDropEnd */

#ifdef XWFEATURE_RAISETILE
XP_Bool
dragDropSetAdd( BoardCtxt* board )
{
    DragState* ds = &board->dragState;
    XP_Bool draw;
    draw = dragDropInProgress(board) && !dragDropIsDividerDrag( board )
        && !dragDropHasMoved( board );
    if ( draw ) {
        if ( draw ) {
            ds->yyAdd = (board->trayBounds.height * 2) / 3;
            draw = dragDropContinueImpl( board, board->penDownX, 
                                         board->penDownY, 0, XP_TRUE, NULL );
        }
    }
    return draw;
}
#endif

XP_Bool
dragDropGetBoardTile( const BoardCtxt* board, XP_U16* col, XP_U16* row )
{
    const DragState* ds = &board->dragState;
    XP_Bool found = ds->dtype == DT_TILE && ds->cur.obj == OBJ_BOARD;
    if ( found ) {
        *col = ds->cur.u.board.col;
        *row = ds->cur.u.board.row;
    }
    return found;
}

XP_Bool
dragDropIsBeingDragged( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                        XP_Bool* isOrigin )
{
    const DragState* ds = &board->dragState;
    XP_Bool result = ds->dtype == DT_TILE && ds->cur.obj == OBJ_BOARD;
    if ( result && !!isOrigin ) {
        const DragState* ds = &board->dragState;
        if ( (ds->cur.obj == OBJ_BOARD) && (ds->cur.u.board.col == col)
             && (ds->cur.u.board.row == row) ) {
            *isOrigin = XP_FALSE;
        } else if ( (ds->start.obj == OBJ_BOARD)
                    && (ds->start.u.board.col == col)
             && (ds->start.u.board.row == row) ) {
            *isOrigin = XP_TRUE;
        } else {
            result = XP_FALSE;
        }

    }
    return result;
}

void
dragDropAppendBlank( const BoardCtxt* board, BlankQueue* bqp )
{
    const DragState* ds = &board->dragState;
    if ( ds->dtype == DT_TILE && ds->cur.obj == OBJ_BOARD ) {
        if ( ds->isBlank ) {
            bqp->col[bqp->nBlanks] = ds->cur.u.board.col;
            bqp->row[bqp->nBlanks] = ds->cur.u.board.row;
            ++bqp->nBlanks;
        }
    }
}

void
dragDropGetTrayChanges( const BoardCtxt* board, XP_U16* rmvdIndx, 
                        XP_U16* addedIndx )
{
    const DragState* ds = &board->dragState;
    *addedIndx = *rmvdIndx = MAX_TRAY_TILES; /* too big means ignore me */
    if ( ds->dtype == DT_TILE ) {
        if ( OBJ_TRAY == ds->start.obj ) {
            *rmvdIndx = ds->start.u.tray.index;
        }
        if ( OBJ_TRAY == ds->cur.obj ) {
            *addedIndx = ds->cur.u.tray.index;
        }
    }
}

#ifdef XWFEATURE_SEARCHLIMIT
XP_Bool
dragDropGetHintLimits( const BoardCtxt* board, BdHintLimits* limits )
{
    XP_Bool result = board->dragState.dtype == DT_HINTRGN;
    if ( result ) {
        setLimitsFrom( board, limits );
    }
    return result;
}
#endif

XP_Bool
dragDropIsDividerDrag( const BoardCtxt* board )
{
    return board->dragState.dtype == DT_DIVIDER;
}

void 
dragDropTileInfo( const BoardCtxt* board, Tile* tile, XP_Bool* isBlank )
{
    const DragState* ds = &board->dragState;
    XP_ASSERT( dragDropInProgress( board ) );
    XP_ASSERT( ds->dtype == DT_TILE );
    XP_ASSERT ( OBJ_BOARD == ds->start.obj || OBJ_TRAY == ds->start.obj );
    *tile = ds->tile;
    *isBlank = ds->isBlank;
}

#ifdef XWFEATURE_SEARCHLIMIT
static void
invalHintRectDiffs( BoardCtxt* board, const DragObjInfo* cur, 
                    const DragObjInfo* nxt )
{
    XP_U16 startCol = board->dragState.start.u.board.col;
    XP_U16 startRow = board->dragState.start.u.board.row;

    /* These two regions will generally have close to 50% of their borders in
       common.  Try not to inval what needn't be inval'd.  But at the moment
       performance seems good enough without adding the complexity and new
       bugs...

       The challenge in doing a smarter diff is that some squares need to be
       invalidated even if they're part of the borders of both limits rects,
       in particular if one is a corner of one and just a side of another.
       One simple but expensive way of accounting for this would be to call
       figureHintAtts() on each square in the borders of both rects and
       invalidate when the hintAttributes aren't the same for both.  That
       misses an opportunity to avoid doing any calculations on those border
       squares that clearly haven't changed at all.
    */

    invalCellRegion( board, startCol, startRow, cur->u.board.col, 
                     cur->u.board.row );
    if ( !!nxt ) {
        BdHintLimits limits;
        setLimitsFrom( board, &limits );
        invalCellRegion( board, startCol, startRow, nxt->u.board.col, 
                         nxt->u.board.row );
    }
} /* invalHintRectDiffs */
#endif

static XP_Bool
dragDropContinueImpl( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_U16 offset, XP_Bool scrollBoard,
                      BoardObjectType* onWhichP )
{
    XP_Bool moving = XP_FALSE;
    DragObjInfo newInfo;
    DragState* ds = &board->dragState;
    XP_Bool draw = XP_FALSE;

#ifdef XWFEATURE_RAISETILE
    yy -= ds->yyAdd;
#endif

    if ( ds->dtype == DT_TILE ){
    	// Offset tiles being dragged so they are easier to see.
    	yy -= offset;
    }

    (void)pointOnSomething( board, xx, yy, &newInfo.obj );
    if ( !!onWhichP ) {
        *onWhichP = newInfo.obj;
    }

    if ( newInfo.obj == OBJ_BOARD ) {
        (void)coordToCell( board, xx, yy, &newInfo.u.board.col,
                           &newInfo.u.board.row );
#ifdef XWFEATURE_CROSSHAIRS
        if ( !board->hideCrosshairs ) {
            draw = crosshairs_set( board, newInfo.u.board.col, 
                                   newInfo.u.board.row );
        }
#endif
    }

    if ( ds->dtype == DT_DIVIDER ) {
        if ( OBJ_TRAY == newInfo.obj ) {
            XP_U16 newloc;
            XP_U16 scale = board->trayScaleH;
            xx -= board->trayBounds.left;
            newloc = xx / scale;
            if ( (xx % scale) > ((scale+board->dividerWidth)/2)) {
                ++newloc;
            }
            moving = dividerMoved( board, newloc );
        }
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( ds->dtype == DT_HINTRGN && newInfo.obj != OBJ_BOARD ) {
            /* do nothing */
#endif
    } else if ( ds->dtype == DT_BOARD ) {
        if ( newInfo.obj == OBJ_BOARD ) {
            XP_S16 diff = newInfo.u.board.col - ds->cur.u.board.col;
            if ( !ds->cellChanged && 0 != diff ) {
                ds->cellChanged = XP_TRUE;
            }
            diff /= SCROLL_DRAG_THRESHHOLD;
            moving = adjustXOffset( board, diff );
            diff = newInfo.u.board.row - ds->cur.u.board.row;
            if ( !ds->cellChanged && 0 != diff ) {
                ds->cellChanged = XP_TRUE;
            }
            diff /= SCROLL_DRAG_THRESHHOLD;
            moving = adjustYOffset( board, diff ) || moving;
        }
    } else if ( ds->dtype == DT_TILE ) {
    	if ( newInfo.obj == OBJ_BOARD ) {
    		moving = (newInfo.u.board.col != ds->cur.u.board.col)
                || (newInfo.u.board.row != ds->cur.u.board.row)
                || (OBJ_TRAY == ds->cur.obj);
        } else if ( newInfo.obj == OBJ_TRAY ) {
            XP_Bool onDivider;
            XP_S16 index = pointToTileIndex( board, xx, yy, &onDivider );
            if ( !onDivider ) {
               if ( index < 0 ) { /* negative means onto empty part of
                                      tray.  Force left. */
                    index = model_getNumTilesInTray( board->model, 
                                                     board->selPlayer );
                    if ( OBJ_TRAY == ds->start.obj ) {
                        --index; /* dragging right into space */
                    }
                }
                moving = (OBJ_BOARD == ds->cur.obj)
                    || (index != ds->cur.u.tray.index);
                if ( moving ) {
                    newInfo.u.tray.index = index;
                }
            }
        }

        if ( moving ) {
            if ( ds->dtype == DT_TILE ) {
                invalDragObjRange( board, &ds->cur, &newInfo );
#ifdef XWFEATURE_SEARCHLIMIT
            } else if ( ds->dtype == DT_HINTRGN ) {
                invalHintRectDiffs( board, &ds->cur, &newInfo );
                if ( !ds->didMove ) { /* first time through */
                    invalCurHintRect( board, board->selPlayer );
                }
#endif
            }
            XP_MEMCPY( &ds->cur, &newInfo, sizeof(ds->cur) );
            startScrollTimerIf( board, scrollBoard );
        }
    } else {
        XP_ASSERT( 0 );
    }

    // Change?
    if (mBoardScrollOn != scrollBoard){
    	// Yes, need this extra kick to get board scrolling going.
        startScrollTimerIf( board, scrollBoard );
        // Only do once.
    	mBoardScrollOn = scrollBoard;
    }

    if ( moving ) {
        draw = XP_TRUE;
        if ( !ds->didMove ) {
            /* This is the first time we've moved!!!  Kill any future timers,
               and if there's a window up kill it.*/
            board->penTimerFired = XP_FALSE;
#ifdef XWFEATURE_MINIWIN
            if ( valHintMiniWindowActive( board ) ) {
                hideMiniWindow( board, XP_TRUE, MINIWINDOW_VALHINT );
            }
#endif
            ds->didMove = XP_TRUE;
            // Don't scroll right away; wait until tile touches edge of board.
            mBoardScrollOn = XP_FALSE;
        }
    }

    return draw;
} /* dragDropContinueImpl */

static void
invalDragObjRange( BoardCtxt* board, const DragObjInfo* from, 
                   const DragObjInfo* to )
{
    invalDragObj( board, from );
    if ( NULL != to ) {
        invalDragObj( board, to );

        if ( (OBJ_TRAY == from->obj) && (OBJ_TRAY == to->obj) ) {
            invalTrayTilesBetween( board, from->u.tray.index, 
                                   to->u.tray.index );
        } else if ( OBJ_TRAY == from->obj ) {
            invalTrayTilesAbove( board, from->u.tray.index );
        } else if ( OBJ_TRAY == to->obj ) {
            invalTrayTilesAbove( board, to->u.tray.index );
        }
    }
}

#ifdef XWFEATURE_SEARCHLIMIT
static void
setLimitsFrom( const BoardCtxt* board, BdHintLimits* limits )
{
    const DragState* ds = &board->dragState;
    limits->left = XP_MIN( ds->start.u.board.col, ds->cur.u.board.col );
    limits->right = XP_MAX( ds->start.u.board.col, ds->cur.u.board.col );
    limits->top = XP_MIN( ds->start.u.board.row, ds->cur.u.board.row );
    limits->bottom = XP_MAX( ds->start.u.board.row, ds->cur.u.board.row );
}
#endif

static XP_Bool
scrollTimerProc( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = (BoardCtxt*)closure;
    DragState* ds = &board->dragState;
    XP_ASSERT( why == TIMER_PENDOWN );

    if ( ds->scrollTimerSet ) {
        XP_S16 changeX = 0;
        XP_S16 changeY = 0;
        ds->scrollTimerSet = XP_FALSE;
        XP_Bool canScroll = onBorderCanScroll( board, SCROLL_H, 
                                               ds->cur.u.board.col, &changeX );
        canScroll = onBorderCanScroll( board, SCROLL_V, ds->cur.u.board.row, 
                                       &changeY ) || canScroll;
        if ( canScroll ) {
            invalDragObj( board, &ds->cur );
            if ( 0 != changeX ) {
                ds->cur.u.board.col += (changeX >0 ? 1 : -1);
            }
            if ( 0 != changeY ) {
                ds->cur.u.board.row += (changeY >0 ? 1 : -1);
            }

            if ( scrollIntoView( board, ds->cur.u.board.col, 
                                 ds->cur.u.board.row ) ) {
                board_draw( board ); /* may fail, e.g. on wince */
                startScrollTimerIf( board, XP_TRUE );
                draw = XP_TRUE;
            }
        }
    }
    return draw;
} /* scrollTimerProc */

static void
startScrollTimerIf( BoardCtxt* board, XP_Bool scrollBoardOn )
{
    DragState* ds = &board->dragState;

    if ( (ds->dtype == DT_TILE) && (ds->cur.obj == OBJ_BOARD) ) {
        XP_S16 ignore;
        if ( onBorderCanScroll( board, SCROLL_H, ds->cur.u.board.col, &ignore )
             || onBorderCanScroll( board, SCROLL_V, ds->cur.u.board.row,
                                   &ignore ) ) {
        	if ( scrollBoardOn ){
                util_setTimer( board->util, TIMER_PENDOWN, 0,
                               scrollTimerProc, (void*) board );
                ds->scrollTimerSet = XP_TRUE;
        	}
        } else {
            /* ignore if we've moved off */
            ds->scrollTimerSet = XP_FALSE;
        }
    }
} /* startScrollTimerIf */

#ifdef XWFEATURE_CROSSHAIRS
void
dragDropInCrosshairs( const BoardCtxt* board, XP_U16 col, XP_U16 row,
                      XP_Bool* inHor, XP_Bool* inVert )
{
    
    if ( dragDropInProgress( board ) ) {
        const DragState* ds = &board->dragState;
        *inHor = ds->crosshairs.row == row;
        *inVert = ds->crosshairs.col == col;
    } else {
        *inHor = *inVert = XP_FALSE;
    }
} /* dragDropInCrosshairs */

static XP_Bool
crosshairs_set( BoardCtxt* board, XP_S16 col, XP_S16 row )
{
    XP_Bool changed = XP_FALSE;
    DragState* ds = &board->dragState;
    if ( ds->crosshairs.col != col ) {
        if ( ds->crosshairs.col >= 0 ) {
            invalCol( board, ds->crosshairs.col );
        }
        if ( col >= 0 ) {
            invalCol( board, col );
        }
        ds->crosshairs.col = col;
        changed = XP_TRUE;
    }
    if ( ds->crosshairs.row != row ) {
        if ( ds->crosshairs.row >= 0 ) {
            invalRow( board, ds->crosshairs.row );
        }
        if ( row >= 0 ) {
            invalRow( board, row );
        }
        ds->crosshairs.row = row;
        changed = XP_TRUE;
    }
    return changed;
}

static void
crosshairs_clear( BoardCtxt* board )
{
    crosshairs_set( board, -1, -1 );
}
#endif

#ifdef CPLUS
}
#endif

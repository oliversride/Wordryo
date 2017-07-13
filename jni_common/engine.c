/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2012 by Eric House (xwords@eehouse.org).  All rights
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

#include "comtypes.h"
#include "engine.h"
#include "dictnry.h"
#include "util.h"

#ifdef CPLUS
extern "C" {
#endif

typedef XP_U8 Engine_rack[MAX_UNIQUE_TILES+1];

#ifndef NUM_SAVED_ENGINE_MOVES
# define NUM_SAVED_ENGINE_MOVES 10
#endif

typedef struct BlankTuple {
    short col;
    Tile tile;
} BlankTuple;

typedef struct PossibleMove {
    XP_U16 score; /* Because I'm doing a memcmp to sort these things,
                     the comparison must be done differently on
                           little-endian platforms. */
    MoveInfo moveInfo;
    //XP_U16 whichBlanks; /* flags */
    Tile blankVals[MAX_COLS]; /* the faces for which we've substituted
                                 blanks */
} PossibleMove;

/* MoveIterationData is a cache of moves so that next and prev searches don't
 * always trigger an actual search.  Instead we save up to
 * NUM_SAVED_ENGINE_MOVES moves that sort together; then iteration is just
 * returning the next or previous in the cache.  The cache, savedMoves[], is
 * sorted in increasing order, with any unused entries at the low end (since
 * they sort as if score == 0).  nInMoveCache is the actual number of entries.
 * curCacheIndex is the index of the move most recently returned, or outside
 * the range if nothing's been returned yet from the current cache.
 *
 * The cache is empty if nInMoveCache == 0, or if curCacheIndex is in a
 * position that, given engine->usePrev, indicates it's been walked through
 * the cache already rather than being poised to enter it.
 */

typedef struct MoveIterationData {
    /* savedMoves: if any entries are unused (because result set doesn't fill,
       they're at the low end (where sort'll put 'em) */
    PossibleMove savedMoves[NUM_SAVED_ENGINE_MOVES];
    //XP_U16 lowestSavedScore;
    PossibleMove lastSeenMove;
    XP_U16 nInMoveCache; /* num entries, 
                            0 <= nInMoveCache < NUM_SAVED_ENGINE_MOVES */
    XP_U16 bottom;   /* lowest non-0 entry */
    XP_S16 curCacheIndex;       /* what we last returned */
} MoveIterationData;

/* one bit per tile that's possible here *\/ */
typedef XP_U32 CrossBits;
typedef struct Crosscheck { CrossBits bits[2]; } Crosscheck;

struct EngineCtxt {
    const ModelCtxt* model;
    const DictionaryCtxt* dict;
    XW_UtilCtxt* util;
    XP_U16 turn;

    Engine_rack rack;
    Tile blankTile;
    XP_Bool usePrev;
    XP_Bool searchInProgress;
    XP_Bool searchHorizontal;
    XP_Bool isFirstMove;
    XP_U16 numRows, numCols;
    XP_U16 curRow;
    XP_U16 blankCount;
    XP_U16 nMovesToSave;
    XP_U16 star_row;
    XP_Bool returnNOW;
    XP_Bool isRobot;
    MoveIterationData miData;

    XP_S16 blankValues[MAX_TRAY_TILES];
    Crosscheck rowChecks[MAX_ROWS]; // also used in xwscore
    XP_U16 scoreCache[MAX_ROWS];

    XP_U16 nTilesMax;
#ifdef XWFEATURE_BONUSALL
    XP_U16 allTilesBonus;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    XP_U16 nTilesMin;
    XP_U16 nTilesMinUser, nTilesMaxUser;
    XP_Bool tileLimitsKnown;
    const BdHintLimits* searchLimits;
#endif
    XP_U16 lastRowToFill;

#ifdef DEBUG
    XP_U16 curLimit;
#endif
    MPSLOT
}; /* EngineCtxt */

static void findMovesOneRow( EngineCtxt* engine );
static Tile localGetBoardTile( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_Bool substBlank );
static XP_Bool scoreQualifies( EngineCtxt* engine, XP_U16 score );
static void findMovesForAnchor( EngineCtxt* engine, XP_S16* prevAnchor, 
                                XP_U16 col, XP_U16 row ) ;
static void figureCrosschecks( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_U16* scoreP,
                               Crosscheck* check );
static XP_Bool isAnchorSquare( EngineCtxt* engine, XP_U16 col, XP_U16 row );
static array_edge* edge_from_tile( const DictionaryCtxt* dict, 
                                   array_edge* from, Tile tile );
static void leftPart( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
                      array_edge* edge, XP_U16 limit, XP_U16 firstCol,
                      XP_U16 anchorCol, XP_U16 row );
static void extendRight( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
                         array_edge* edge, XP_Bool accepting,
                         XP_U16 firstCol, XP_U16 col, XP_U16 row );
static array_edge* consumeFromLeft( EngineCtxt* engine, array_edge* edge, 
                                    short col, short row );
static XP_Bool rack_remove( EngineCtxt* engine, Tile tile, XP_Bool* isBlank );
static void rack_replace( EngineCtxt* engine, Tile tile, XP_Bool isBlank );
static void considerMove( EngineCtxt* engine, Tile* tiles, short tileLength,
                          short firstCol, short lastRow );
static void considerScoreWordHasBlanks( EngineCtxt* engine, XP_U16 blanksLeft,
                                        PossibleMove* posmove,
                                        XP_U16 lastRow,
                                        BlankTuple* usedBlanks,
                                        XP_U16 usedBlanksCount );
static void saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove );
static XP_Bool move_cache_empty( const EngineCtxt* engine );
static void init_move_cache( EngineCtxt* engine );
static PossibleMove* next_from_cache( EngineCtxt* engine );
static void set_search_limits( EngineCtxt* engine );


#if defined __LITTLE_ENDIAN
static XP_S16 cmpMoves( PossibleMove* m1, PossibleMove* m2 );
# define CMPMOVES( m1, m2 )     cmpMoves( m1, m2 )
#elif defined __BIG_ENDIAN
# define CMPMOVES( m1, m2 )    XP_MEMCMP( m1, m2, sizeof(*(m1)))
#else
    error: need to pick one!!!
#endif

/* #define CROSSCHECK_CONTAINS(chk,tile) (((chk) & (1L<<(tile))) != 0) */
#define CROSSCHECK_CONTAINS(chk,tile) checkIsSet( (chk), (tile) )

#define HILITE_CELL( engine, col, row ) \
    util_hiliteCell( (engine)->util, (col), (row) )

/* not implemented yet */
XP_U16
engine_getScoreCache( EngineCtxt* engine, XP_U16 row )
{
    return engine->scoreCache[row];
} /* engine_getScoreCache */

/*****************************************************************************
 * This should be the first executable code in the file in case I want to
 * turn it into a separate code module later.
 ****************************************************************************/ 
EngineCtxt*
engine_make( MPFORMAL XW_UtilCtxt* util )
{
    EngineCtxt* result = (EngineCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->util = util;

    engine_reset( result );

    return result;
} /* engine_make */

void
engine_writeToStream( EngineCtxt* XP_UNUSED(ctxt), 
                      XWStreamCtxt* XP_UNUSED(stream) )
{
    /* nothing to save; see comment below */
} /* engine_writeToStream */

EngineCtxt* 
engine_makeFromStream( MPFORMAL XWStreamCtxt* XP_UNUSED(stream), 
                       XW_UtilCtxt* util )
{
    EngineCtxt* engine = engine_make( MPPARM(mpool) util );

    /* All the engine's data seems to be used only in the process of finding a
       move.  So if we're willing to have the set of moves found lost across
       a save, there's nothing to do! */

    return engine;
} /* engine_makeFromStream */

void
engine_reset( EngineCtxt* engine )
{
    XP_MEMSET( &engine->miData, 0, sizeof(engine->miData) );
    /* set last score to max possible */
    engine->miData.lastSeenMove.score = engine->usePrev? 0 : 0xffff;
    engine->searchInProgress = XP_FALSE;
#ifdef XWFEATURE_SEARCHLIMIT
    engine->tileLimitsKnown = XP_FALSE;      /* indicates not set */
    if ( engine->nTilesMin == 0 ) {
        engine->nTilesMinUser = engine->nTilesMin = 1;
        engine->nTilesMaxUser = engine->nTilesMax = MAX_TRAY_TILES;
    }
#endif
} /* engine_reset */

void
engine_destroy( EngineCtxt* engine )
{
    XP_ASSERT( engine != NULL );
    XP_FREE( engine->mpool, engine );
} /* engine_destroy */

static XP_Bool
initTray( EngineCtxt* engine, const Tile* tiles, XP_U16 numTiles ) 
{
    XP_Bool result = numTiles > 0;

    if ( result ) {
        XP_MEMSET( engine->rack, 0, sizeof(engine->rack) );
        while ( 0 < numTiles-- ) {
            Tile tile = *tiles++;
            XP_ASSERT( tile < MAX_UNIQUE_TILES );
            ++engine->rack[tile];
        }
    }

    return result;
} /* initTray */

#if defined __LITTLE_ENDIAN
static XP_S16
cmpMoves( PossibleMove* m1, PossibleMove* m2 )
{
    if ( m1->score == m2->score ) {
        return XP_MEMCMP( &m1->moveInfo, &m2->moveInfo, 
                          sizeof(*m1) - sizeof( m1->score ) );
    } else if ( m1->score < m2->score ) {
        return -1;
    } else {
        return 1;
    }
} /* cmpMoves */
#endif

#if 0
static void
print_savedMoves( const EngineCtxt* engine, const char* label )
{
    int ii;
    int pos = 0;
    char buf[(NUM_SAVED_ENGINE_MOVES*10) + 3] = {0};
    for ( ii = 0; ii < engine->nMovesToSave; ++ii ) {
        if ( 0 < engine->miData.savedMoves[ii].score ) {
            pos += XP_SNPRINTF( &buf[pos], VSIZE(buf)-pos, "[%d]: %d; ", 
                                ii, engine->miData.savedMoves[ii].score );
        }
    }
    XP_LOGF( "%s: %s", label, buf );
}
#else
# define print_savedMoves( engine, label )
#endif

static XP_Bool
chooseMove( EngineCtxt* engine, PossibleMove** move ) 
{
    XP_U16 ii;
    PossibleMove* chosen = NULL;
    XP_Bool result;
    XP_Bool done;

    print_savedMoves( engine, "unsorted moves" );

    /* First, sort 'em.  Put the higher-scoring moves at the top where they'll
       get picked up first.  Don't sort if we're working for a robot; we've
       only been saving the single best move anyway.  At least not until we
       start applying other criteria than score to moves. */

    done = !move_cache_empty( engine );
    while ( !done ) { /* while so can break */
        done = XP_TRUE;
        PossibleMove* cur = engine->miData.savedMoves;
        for ( ii = 0; ii < engine->nMovesToSave-1; ++ii ) {
            PossibleMove* next = cur + 1;
            if ( CMPMOVES( cur, next ) > 0 ) {
                PossibleMove tmp;
                XP_MEMCPY( &tmp, cur, sizeof(tmp) );
                XP_MEMCPY( cur, next, sizeof(*cur) );
                XP_MEMCPY( next, &tmp, sizeof(*next) );
                done = XP_FALSE;
            }
            cur = next;
        }

        if ( done ) {
            if ( !engine->isRobot ) {
                init_move_cache( engine );
            }
            print_savedMoves( engine, "sorted moves" );
        }
    }

    /* now pick the one we're supposed to return */
    if ( engine->isRobot ) {
        XP_ASSERT( engine->miData.nInMoveCache <= NUM_SAVED_ENGINE_MOVES );
        XP_ASSERT( engine->miData.nInMoveCache <= engine->nMovesToSave );
        /* PENDING not nInMoveCache-1 below?? */
        chosen = &engine->miData.savedMoves[engine->miData.nInMoveCache];
    } else {
        chosen = next_from_cache( engine );
    }

    *move = chosen; /* set either way */

    result = (NULL != chosen) && (chosen->score > 0);

    if ( !result ) {
        engine_reset( engine ); 
    }
    return result;
} /* chooseMove */

/* Robot smartness is a number between 0 and 100, inclusive.  0 means a human
 * player who may want to iterate, so save all moves.  If a robot player, we
 * want a random move within a range proportional to the 1-100 range, so we
 * figure out now what we'll be picking, save only that many moves and take
 * the worst of 'em in chooseMove().
 */
static void
normalizeIQ( EngineCtxt* engine, XP_U16 iq )
{
    engine->isRobot = 0 < iq;
    if ( 0 == iq ) {            /* human */
        engine->nMovesToSave = NUM_SAVED_ENGINE_MOVES; /* save 'em all */
    } else if ( 1 == iq ) {            /* smartest robot */
        engine->nMovesToSave = 1;
    } else {
        XP_U16 count = NUM_SAVED_ENGINE_MOVES * iq / 100;
        engine->nMovesToSave = 1;
        if ( count > 0 ) {
            engine->nMovesToSave += XP_RANDOM() % count;
        }
    }
}

/* Return of XP_TRUE means that we ran to completion.  XP_FALSE means we were
 * interrupted.  Whether an actual move was found is indicated by what's
 * filled in in *newMove.
 */
XP_Bool
engine_findMove( EngineCtxt* engine, const ModelCtxt* model, 
                 XP_U16 turn, const Tile* tiles,
                 XP_U16 nTiles, XP_Bool usePrev,
#ifdef XWFEATURE_BONUSALL
                 XP_U16 allTilesBonus,
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                 const BdHintLimits* searchLimits,
                 XP_Bool useTileLimits,
#endif
                 XP_U16 robotIQ, XP_Bool* canMoveP, MoveInfo* newMove )
{
    XP_Bool result = XP_TRUE;
    XP_U16 star_row;

     engine->nTilesMax = XP_MIN( MAX_TRAY_TILES, nTiles );
#ifdef XWFEATURE_BONUSALL
    engine->allTilesBonus = allTilesBonus;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    if ( useTileLimits ) {
        /* We'll want to use the numbers we've been using already unless
           there's been a reset.  In that case, though, provide the old
           ones as defaults */
        if ( !engine->tileLimitsKnown ) {

            XP_U16 nTilesMin = engine->nTilesMinUser;
            XP_U16 nTilesMax = engine->nTilesMaxUser;

            if ( util_getTraySearchLimits( engine->util, 
                                           &nTilesMin, &nTilesMax ) ) {
                engine->tileLimitsKnown = XP_TRUE;
                engine->nTilesMinUser = nTilesMin;
                engine->nTilesMaxUser = nTilesMax;
            } else {
                *canMoveP = XP_FALSE;                
                return XP_TRUE;
            }
        }

        engine->nTilesMin = engine->nTilesMinUser;
        engine->nTilesMax = engine->nTilesMaxUser;
    } else {
        engine->nTilesMin = 1;
    }
#endif

    engine->model = model;
    engine->dict = model_getPlayerDict( model, turn );
    engine->turn = turn;
    engine->usePrev = usePrev;
    engine->blankTile = dict_getBlankTile( engine->dict );
    engine->returnNOW = XP_FALSE;
#ifdef XWFEATURE_SEARCHLIMIT
    engine->searchLimits = searchLimits;
#endif

    engine->star_row = star_row = model_numRows(model) / 2;
    engine->isFirstMove = 
        EMPTY_TILE == localGetBoardTile( engine, star_row, 
                                         star_row, XP_FALSE );

    /* If we've been asked to generate a move but can't because the
       dictionary's emtpy or there are no tiles, still return TRUE so we don't
       get scheduled again.  Fixes infinite loop with empty dict and a
       robot. */
    *canMoveP = NULL != dict_getTopEdge(engine->dict)
        && initTray( engine, tiles, nTiles );
    if ( *canMoveP  ) {

        util_engineStarting( engine->util, 
                             engine->rack[engine->blankTile] );

        normalizeIQ( engine, robotIQ );

        if ( move_cache_empty( engine ) ) {
            set_search_limits( engine );

            XP_MEMSET( engine->miData.savedMoves, 0,
                       sizeof(engine->miData.savedMoves) );

            if ( engine->searchInProgress ) {
                goto resumePoint;
            } else {
                engine->searchHorizontal = XP_TRUE;
                engine->searchInProgress = XP_TRUE;
            }
            for ( ; ; ) {
                XP_U16 firstRowToFill = 0;
                engine->numRows = model_numRows(engine->model);
                engine->numCols = model_numCols(engine->model);
                if ( !engine->searchHorizontal ) {
                    XP_U16 tmp = engine->numRows;
                    engine->numRows = engine->numCols;
                    engine->numCols = tmp;
                }

                if ( 0 ) {
#ifdef XWFEATURE_SEARCHLIMIT
                } else if ( !!searchLimits ) {
                    if ( engine->searchHorizontal ) {
                        firstRowToFill = searchLimits->top;
                        engine->lastRowToFill = searchLimits->bottom;
                    } else {
                        firstRowToFill = searchLimits->left;
                        engine->lastRowToFill = searchLimits->right;
                    }
#endif
                } else {
                    engine->lastRowToFill = engine->numRows - 1;
                }

                for ( engine->curRow = firstRowToFill;
                      engine->curRow <= engine->lastRowToFill;
                      ++engine->curRow ) {
                resumePoint:
                    if ( engine->isFirstMove && (engine->curRow != star_row)) {
                        continue;
                    }
                    findMovesOneRow( engine );
                    if ( engine->returnNOW ) {
                        goto outer;
                    }
                }

                if ( !engine->searchHorizontal 
#ifdef XWFEATURE_SEARCHLIMIT
                     || (engine->isFirstMove && !searchLimits) 
#endif
                     ) {
                    engine->searchInProgress = XP_FALSE;
                    break;
                } else {
                    engine->searchHorizontal = XP_FALSE;
                }
            } /* forever */
        outer:
            result = result; /* c++ wants a statement after the label */
        }
        /* Search is finished.  Choose (or just return) the best move found. */
        if ( engine->returnNOW ) {
            result = XP_FALSE;
        } else {
            PossibleMove* move;
            if ( chooseMove( engine, &move ) ) {
                XP_ASSERT( !!newMove );
                XP_MEMCPY( newMove, &move->moveInfo, sizeof(*newMove) );
            } else {
                newMove->nTiles = 0;
            }
            result = XP_TRUE;
        }

        util_engineStopping( engine->util );
    } else {
        /* set up a PASS.  I suspect the caller should be deciding how to
           handle this case itself, but this doesn't preclude its doing
           so.  */
        newMove->nTiles = 0;
    }

    return result;
} /* engine_findMove */

static void
findMovesOneRow( EngineCtxt* engine )
{
    XP_U16 lastCol = engine->numCols - 1;
    XP_U16 col, row = engine->curRow;
    XP_S16 prevAnchor;
    XP_U16 firstSearchCol, lastSearchCol;
#ifdef XWFEATURE_SEARCHLIMIT
    const BdHintLimits* searchLimits = engine->searchLimits;
#endif

    if ( 0 ) {
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( !!searchLimits ) {
        if ( engine->searchHorizontal ) {
            firstSearchCol = searchLimits->left;
            lastSearchCol = searchLimits->right;
        } else {
            firstSearchCol = searchLimits->top;
            lastSearchCol = searchLimits->bottom;
        }
#endif        
    } else {
        firstSearchCol = 0;
        lastSearchCol = lastCol;
    }

    XP_MEMSET( &engine->rowChecks, 0, sizeof(engine->rowChecks) ); /* clear */
    for ( col = 0; col <= lastCol; ++col ) {
        if ( col < firstSearchCol || col > lastSearchCol ) {
            engine->scoreCache[col] = 0;
        } else {
            figureCrosschecks( engine, col, row, 
                               &engine->scoreCache[col],
                               &engine->rowChecks[col]);
        }
    }

    prevAnchor = firstSearchCol - 1;
    for ( col = firstSearchCol; col <= lastSearchCol && !engine->returnNOW; 
          ++col ) {
        if ( isAnchorSquare( engine, col, row ) ) { 
            findMovesForAnchor( engine, &prevAnchor, col, row );
        }
    }
} /* findMovesOneRow */

static XP_Bool
lookup( const DictionaryCtxt* dict, array_edge* edge, Tile* buf, 
        XP_U16 tileIndex, XP_U16 length ) 
{
    XP_Bool result = XP_FALSE;
    while ( edge != NULL ) {
        Tile targetTile = buf[tileIndex];
        edge = dict_edge_with_tile( dict, edge, targetTile );
        if ( edge == NULL ) { /* tile not available out of this node */
            break;
        } else {
            if ( ++tileIndex == length ) { /* is this the last tile? */
                result = ISACCEPTING(dict, edge);
                break;
            } else {
                edge = dict_follow( dict, edge );
                continue;
            }
        }
    }
    return result;
} /* lookup */

static void
figureCrosschecks( EngineCtxt* engine, XP_U16 x, XP_U16 y, XP_U16* scoreP,
                   Crosscheck* check )
{
    XP_S16 startY, maybeY;
    XP_U16 numRows = engine->numRows;
    Tile tile;
    array_edge* in_edge;
    array_edge* candidateEdge;
    Tile tiles[MAX_COLS];
    XP_U16 tilesAfter;
    XP_U16 checkScore = 0;
    const DictionaryCtxt* dict = engine->dict;

    if ( localGetBoardTile( engine, x, y, XP_FALSE ) == EMPTY_TILE ) {

        /* find the first tile of any prefix */
        startY = (XP_S16)y;
        for ( ; ; ) {
            maybeY = startY - 1;
            if ( maybeY < 0 ) {
                break;
            }
            if ( localGetBoardTile( engine, x, maybeY, XP_FALSE )
                 == EMPTY_TILE ) {
                break;
            }
            startY = maybeY;
        }

        /* Take care of the "special case" where the square has no neighbors
           in either crosscheck direction */
        if ( (y == startY) &&
             ((y == numRows-1) ||
              (localGetBoardTile( engine, x, y+1, XP_FALSE ) == EMPTY_TILE))){
            /* all tiles legal and checkScore remains 0, as there are no
               neighbors */
            XP_MEMSET( check, 0xFF, sizeof(*check) );
            goto outer;
        }

        /* now walk the DAWG consuming any prefix.  We want in_edge to wind up
           holding the edge that leads to {x,y}, which will be the root edge
           if there's no prefix.  I can't use consumeFromLeft() here because
           here I'm consuming upward.  But I could generalize it.... */
        in_edge = dict_getTopEdge( dict );
        while ( startY < y ) {
            tile = localGetBoardTile( engine, x, startY, XP_TRUE );
            XP_ASSERT( tile != EMPTY_TILE );
            checkScore += dict_getTileValue( dict, tile );
            tile = localGetBoardTile( engine, x, startY, XP_FALSE );
            in_edge = edge_from_tile( dict, in_edge, tile );
            /* If we run into a null edge here we have a prefix that by the
               dictionary is an illegal word.  One way it could have gotten
               there is by being placed by a human.  So it's not something to
               flag here, but we won't be able to put anything in the spot so
               the crosscheck is empty.  And the ASSERT goes.

               Note that if we were disallowing words not in the dictionary
               (as in a robot-only game) then the assertion would be valid:
               only if there's a single letter as the "prefix" of our
               crosscheck would it make sense for there to be no edge leading
               out of it.  But when can that happen?  I.e. what letters don't
               begin words in any reasonable word list? */
            if ( in_edge == NULL ) {
                /* Only way to have gotten here is if a user's played a word
                   not in this dict.  We'll not be able to build on it! */
                XP_ASSERT( check->bits[0] == 0L && check->bits[1] == 0L );
                goto outer;
            }
            ++startY;
        }

        /* now in_edge points to the array of candidate edges.  We'll build up
           a buffer of the Tiles following the candidate square on the board,
           then put each candidate edge's Tile in place and do a lookup
           beginning at in_edge.  Successful candidate tiles get added to the
           Crosscheck */
        for ( tilesAfter = 1, maybeY = y + 1; maybeY < numRows; ++maybeY ) {
            tile = localGetBoardTile( engine, x, maybeY, XP_TRUE );
            if ( tile == EMPTY_TILE ) {
                break;
            } else {
                checkScore += dict_getTileValue( dict, tile );
                tiles[tilesAfter++] = localGetBoardTile( engine, x, maybeY,
                                                         XP_FALSE );
            }
        }

        /* <eeh> would it be possible to use extendRight here?  With an empty
           tray?  No: it calls considerMove etc. */
        candidateEdge = in_edge;
        for ( ; ; ) {
            tile = EDGETILE( dict, candidateEdge ); 
            XP_ASSERT( tile < MAX_UNIQUE_TILES );
            tiles[0] = tile;
            if ( lookup( dict, in_edge, tiles, 0, tilesAfter ) ) {
                XP_ASSERT( (tile >> 5)
                           < (VSIZE(check->bits)) );
                check->bits[tile>>5] |= (1L << (tile & 0x1F));
            }

            if ( IS_LAST_EDGE(dict,candidateEdge ) ) {
                break;
            }
            candidateEdge += dict->nodeSize;
        }
    }
 outer:
    if ( scoreP != NULL ) { 
        *scoreP = checkScore;
    }
} /* figureCrosschecks */

XP_Bool
engine_check( DictionaryCtxt* dict, Tile* tiles, XP_U16 nTiles )
{
    array_edge* in_edge = dict_getTopEdge( dict );

    return lookup( dict, in_edge, tiles, 0, nTiles );
} /* engine_check */

static Tile
localGetBoardTile( EngineCtxt* engine, XP_U16 col, XP_U16 row, 
                   XP_Bool substBlank )
{
    Tile result;
    XP_Bool isBlank;

    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( model_getTile( engine->model, col, row, XP_FALSE,
                        0, /* don't get pending, so turn doesn't matter */
                        &result, &isBlank, NULL, NULL ) ) {
        if ( isBlank && substBlank ) {
            result = engine->blankTile;
        }
    } else {
        result = EMPTY_TILE;
    }
    return result;
} /* localGetBoardTile */

/*****************************************************************************
 * Return true if the tile is empty and has a filled-in square on any of the
 * four sides.  First move is a special case: empty and 7,7
 ****************************************************************************/
static XP_Bool
isAnchorSquare( EngineCtxt* engine, XP_U16 col, XP_U16 row ) 
{
    if ( localGetBoardTile( engine, col, row, XP_FALSE ) != EMPTY_TILE ) {
        return XP_FALSE;
    }

    if ( engine->isFirstMove ) {
        return col == engine->star_row && row == engine->star_row;
    }

    if ( (col != 0) && 
         localGetBoardTile( engine, col-1, row, XP_FALSE ) != EMPTY_TILE ) {
        return XP_TRUE;
    }
    if ( (col < engine->numCols-1) 
         && localGetBoardTile( engine, col+1, row, XP_FALSE ) != EMPTY_TILE) {
        return XP_TRUE;
    }
    if ( (row != 0)
         && localGetBoardTile( engine, col, row-1, XP_FALSE) != EMPTY_TILE ) {
        return XP_TRUE;
    }
    if ( (row < engine->numRows-1)
         && localGetBoardTile( engine, col, row+1, XP_FALSE ) != EMPTY_TILE ){
        return XP_TRUE;
    }
    return XP_FALSE;
} /* isAnchorSquare */

#ifdef XWFEATURE_HILITECELL
static void
hiliteForAnchor( EngineCtxt* engine, XP_U16 col, XP_U16 row )
{
    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( !HILITE_CELL( engine, col, row ) ) {
        engine->returnNOW = XP_TRUE;
    }
} /* hiliteForAnchor */
#else
# define hiliteForAnchor( engine, col, row )
#endif

static void
findMovesForAnchor( EngineCtxt* engine, XP_S16* prevAnchor, 
                    XP_U16 col, XP_U16 row ) 
{
    XP_S16 limit;
    array_edge* edge;
    array_edge* topEdge;
    Tile tiles[MAX_ROWS];

    hiliteForAnchor( engine, col, row );

    if ( engine->returnNOW ) {
        /* time to bail */
    } else {
        limit = col - *prevAnchor - 1;
#ifdef TEST_MINLIMIT
        if ( limit >= MAX_TRAY_TILES ) {
            limit = MAX_TRAY_TILES - 1;
        }
#endif
        topEdge = dict_getTopEdge( engine->dict );
        if ( col == 0 ) {
            edge = topEdge;
        } else if ( localGetBoardTile( engine, col-1, row, XP_FALSE ) 
                    == EMPTY_TILE ) {
            leftPart( engine, tiles, 0, topEdge, limit, col, col, row );
            goto done;
        } else {
            edge = consumeFromLeft( engine, topEdge, col, row );
        }
        DEBUG_ASSIGN(engine->curLimit, 0);
        extendRight( engine, tiles, 0, edge,
                     XP_FALSE, // can't accept without the anchor square
                     col-limit, col, row );

    done:
        *prevAnchor = col;
    }
} /* findMovesForAnchor */

static array_edge*
consumeFromLeft( EngineCtxt* engine, array_edge* edge, short col, short row )
{
    XP_S16 maybeX;
    Tile tile;
    Tile tiles[MAX_ROWS];
    XP_U16 numTiles;

    /* Back up to the left until an empty tile or board edge is reached, saving
       the tiles for cheaper retrieval as we walk forward through the DAWG. */
    for ( numTiles = 0, maybeX = col - 1; maybeX >= 0; --maybeX ) {
        tile = localGetBoardTile( engine, maybeX, row, XP_FALSE );
        if ( tile == EMPTY_TILE ) {
            break;
        }
        tiles[numTiles++] = tile; /* we're building the word backwards */
    }
    XP_ASSERT( numTiles > 0 ); /* we should consume *something* */

    /* <eeh> could I just call lookup() here?  Only if I fixed it to
       communicate back the edge it's at after finishing. */
    while ( numTiles-- ) {
        XP_ASSERT( tiles[numTiles] != EMPTY_TILE );

        edge = edge_from_tile( engine->dict, edge, tiles[numTiles] );
        if ( edge == NULL ) {
            break;
        }
    }
    return edge;
} /* consumeFromLeft */

static void
leftPart( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
          array_edge* edge, XP_U16 limit, XP_U16 firstCol,
          XP_U16 anchorCol, XP_U16 row )
{
    DEBUG_ASSIGN( engine->curLimit, tileLength );

    extendRight( engine, tiles, tileLength, edge, XP_FALSE, firstCol, 
                 anchorCol, row );
    if ( !engine->returnNOW ) {
        if ( (limit > 0) && (edge != NULL) ) {
            XP_U16 nodeSize = engine->dict->nodeSize;
            if ( engine->nTilesMax > 0 ) {
                for ( ; ; ) {
                    XP_Bool isBlank;
                    Tile tile = EDGETILE( engine->dict, edge );
                    if ( rack_remove( engine, tile, &isBlank ) ) {
                        tiles[tileLength] = tile;
                        leftPart( engine, tiles, tileLength+1, 
                                  dict_follow( engine->dict, edge ), 
                                  limit-1, firstCol-1, anchorCol, row );
                        rack_replace( engine, tile, isBlank );
                    }

                    if ( IS_LAST_EDGE( dict, edge ) || engine->returnNOW ) {
                        break;
                    }
                    edge += nodeSize;
                }
            }
        }
    }
} /* leftPart */

static void
extendRight( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
             array_edge* edge, XP_Bool accepting,
             XP_U16 firstCol, XP_U16 col, XP_U16 row )
{
    Tile tile;
    const DictionaryCtxt* dict = engine->dict;

    if ( col == engine->numCols ) { /* we're off the board */
        goto check_exit;
    }
    tile = localGetBoardTile( engine, col, row, XP_FALSE );

    if ( edge == NULL ) { // we're off the dictionary
        if ( tile != EMPTY_TILE ) {
            goto no_check; // don't check at the end
        }
    } else if ( tile == EMPTY_TILE ) {
        if ( engine->nTilesMax > 0 ) {
            CrossBits check = engine->rowChecks[col].bits[0];
            XP_Bool advanced = XP_FALSE;
            for ( ; ; ) {
                XP_Bool contains;
                tile = EDGETILE( dict, edge );

                /* If it's bigger than 32, use the second crosscheck.  This is
                   a hack to optimize for the vastly more common case.  Even
                   with languages that have more than 32 tiles at least half
                   will be less than 32 in value. */
                if ( (tile & ~0x1F) != 0 ) {
                    if ( !advanced ) {
                        check = engine->rowChecks[col].bits[1];
                        advanced = XP_TRUE;
                    }
                    contains = (check & (1L << (tile-32))) != 0;
                } else {
                    contains = (check & (1L << tile)) != 0;
                }

                if ( contains ) {
                    XP_Bool isBlank;
                    if ( rack_remove( engine, tile, &isBlank ) ) {
                        tiles[tileLength] = tile;
                        extendRight( engine, tiles, tileLength+1, 
                                     edge_from_tile( dict, edge, tile ), 
                                     ISACCEPTING( dict, edge ), firstCol, 
                                     col+1, row );
                        rack_replace( engine, tile, isBlank );
                        if ( engine->returnNOW ) {
                            goto no_check;
                        }
                    }
                }

                if ( IS_LAST_EDGE( dict, edge ) ) {
                    break;
                }
                edge += dict->nodeSize;
            }
        }

    } else if ( (edge = dict_edge_with_tile( dict, edge, tile ) ) != NULL ) {
        accepting = ISACCEPTING( dict, edge );
        extendRight( engine, tiles, tileLength, dict_follow(dict, edge), 
                     accepting, firstCol, col+1, row );
        goto no_check; /* don't do the check at the end */
    } else {
        goto no_check;
    }
 check_exit:
    if ( accepting
#ifdef XWFEATURE_SEARCHLIMIT
         && tileLength >= engine->nTilesMin
#endif
         ) {
        considerMove( engine, tiles, tileLength, firstCol, row );
    }
 no_check:
    return;
} /* extendRight */

static XP_Bool
rack_remove( EngineCtxt* engine, Tile tile, XP_Bool* isBlank )
{
    Tile blankIndex = engine->blankTile;

    XP_ASSERT( tile < MAX_UNIQUE_TILES );
    XP_ASSERT( tile != blankIndex );
    XP_ASSERT( engine->nTilesMax > 0 );

    if ( engine->rack[(short)tile] > 0 ) { /* we have the tile itself */
        --engine->rack[(short)tile];
        *isBlank = XP_FALSE;
    } else if ( engine->rack[blankIndex] > 0 ) { /* we have and must use a
                                                    blank */
        --engine->rack[(short)blankIndex];
        engine->blankValues[engine->blankCount++] = tile;
        *isBlank = XP_TRUE;
    } else { /* we can't satisfy the request */
        return XP_FALSE;
    }

    --engine->nTilesMax;
    return XP_TRUE;
} /* rack_remove */

static void
rack_replace( EngineCtxt* engine, Tile tile, XP_Bool isBlank ) 
{
    if ( isBlank ) {
        --engine->blankCount;
        tile = engine->blankTile;
    }
    ++engine->rack[(short)tile];

    ++engine->nTilesMax;
} /* rack_replace */

static void
considerMove( EngineCtxt* engine, Tile* tiles, XP_S16 tileLength,
              XP_S16 firstCol, XP_S16 lastRow )
{
    PossibleMove posmove;
    short col;
    BlankTuple blankTuples[MAX_NUM_BLANKS];

    if ( !util_engineProgressCallback( engine->util ) ) {
        engine->returnNOW = XP_TRUE;
    } else {

        /* if this never gets hit then the top-level caller of leftPart should
           never pass a value greater than 7 for limit.  I think we're always
           guaranteed to run out of tiles before finding a legal move with
           larger values but that it's expensive to look only to fail. */
        XP_ASSERT( engine->curLimit < MAX_TRAY_TILES );

        XP_MEMSET( &posmove, 0, sizeof(posmove) );

        for ( col = firstCol; posmove.moveInfo.nTiles < tileLength; ++col ) {
            /* is it one of the new ones? */
            if ( localGetBoardTile( engine, col, lastRow, XP_FALSE )
                 == EMPTY_TILE ) { 
                posmove.moveInfo.tiles[posmove.moveInfo.nTiles].tile = 
                    tiles[posmove.moveInfo.nTiles];
                posmove.moveInfo.tiles[posmove.moveInfo.nTiles].varCoord 
                    = (XP_U8)col;
                ++posmove.moveInfo.nTiles;
            }
        }
        posmove.moveInfo.isHorizontal = engine->searchHorizontal;
        posmove.moveInfo.commonCoord = (XP_U8)lastRow;


        considerScoreWordHasBlanks( engine, engine->blankCount, &posmove, 
                                    lastRow, blankTuples, 0 );
    }
} /* considerMove */

static void
considerScoreWordHasBlanks( EngineCtxt* engine, XP_U16 blanksLeft,
                            PossibleMove* posmove,
                            XP_U16 lastRow, BlankTuple* usedBlanks,
                            XP_U16 usedBlanksCount )
{
    XP_U16 ii;

    if ( blanksLeft == 0 ) {
        XP_U16 score;

        score = figureMoveScore( engine->model, engine->turn,
                                 &posmove->moveInfo,
                                 engine, (XWStreamCtxt*)NULL,
                                 (WordNotifierInfo*)NULL );
#ifdef XWFEATURE_BONUSALL
        if ( 0 != engine->allTilesBonus && 0 == engine->nTilesMax ) {
            XP_LOGF( "%s: adding bonus: %d becoming %d", __func__, score ,
                     score + engine->allTilesBonus );
            score += engine->allTilesBonus;
        }
#endif
        /* First, check that the score is even what we're interested in.  If
           it is, then go to the expense of filling in a PossibleMove to be
           compared in full */
        if ( scoreQualifies( engine, score ) ) {
            posmove->score = score;
            XP_MEMSET( &posmove->blankVals, 0, sizeof(posmove->blankVals) );
            for ( ii = 0; ii < usedBlanksCount; ++ii ) {
                short col = usedBlanks[ii].col;
                posmove->blankVals[col] = usedBlanks[ii].tile;
            }
            XP_ASSERT( posmove->moveInfo.isHorizontal ==
                       engine->searchHorizontal );
            posmove->moveInfo.commonCoord = (XP_U8)lastRow;
            saveMoveIfQualifies( engine, posmove );
        }
    } else {
        Tile bTile;
        BlankTuple* bt;

        --blanksLeft;
        XP_ASSERT( engine->blankValues[blanksLeft] < 128 );
        bTile = (Tile)engine->blankValues[blanksLeft];
        bt = &usedBlanks[usedBlanksCount++];

        /* for each letter for which the blank might be standing in... */
        for ( ii = 0; ii < posmove->moveInfo.nTiles; ++ii ) {
            CellTile tile = posmove->moveInfo.tiles[ii].tile;
            if ( (tile & TILE_VALUE_MASK) == bTile && !IS_BLANK(tile) ) {
                posmove->moveInfo.tiles[ii].tile |= TILE_BLANK_BIT;
                bt->col = ii;
                bt->tile = bTile;
                considerScoreWordHasBlanks( engine, blanksLeft,
                                            posmove, lastRow,
                                            usedBlanks,
                                            usedBlanksCount );
                /* now put things back */
                posmove->moveInfo.tiles[ii].tile &= ~TILE_BLANK_BIT;
            }
        }
    }
} /* considerScoreWordHasBlanks */

static void
saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove )
{
    XP_S16 mostest = 0;
    XP_S16 cmpVal;
    XP_Bool usePrev = engine->usePrev;
    XP_Bool foundEmpty = XP_FALSE;

    if ( 1 == engine->nMovesToSave ) { /* only saving one */
        mostest = 0;
    } else {
        mostest = -1;
        /* we're not interested if we've seen this */
        cmpVal = CMPMOVES( posmove, &engine->miData.lastSeenMove );
        if ( !usePrev && cmpVal >= 0 ) {
            /* XP_LOGF( "%s: dropping %d: higher than %d", __func__, */
            /*          posmove->score, engine->miData.lastSeenMove.score ); */
        } else if ( usePrev && cmpVal <= 0 ) {
            /* XP_LOGF( "%s: dropping %d: lower than %d", __func__, */
            /*          posmove->score, engine->miData.lastSeenMove.score ); */
        } else {
            XP_S16 ii;
            /* terminate i at 1 because mostest starts at 0 */
            for ( ii = 0; ii < engine->nMovesToSave; ++ii ) {
                /* Find the mostest value move and overwrite it.  Note that
                   there might not be one, as all may have the same or higher
                   scores and those that have the same score may compare
                   higher.

                   <eeh> can't have this asssertion until I start noting the
                   mostest saved score (setting miData.mostestSavedScore)
                   below. */
                /* 1/20/2001  I don't see that this assertion is valid.  I
                   simply don't understand why it isn't tripped all the time
                   in the old crosswords. */
                /* XP_ASSERT( (engine->miData.lastSeenMove.score == 0x7fff) */
                /*    || (engine->miData.savedMoves[i].score */
                /*        <= posmove->score) ); */

                if ( 0 == engine->miData.savedMoves[ii].score ) {
                    foundEmpty = XP_TRUE;
                    mostest = ii;
                    break;
                } else if ( -1 == mostest ) {
                    mostest = ii;
                } else {
                    cmpVal = CMPMOVES( &engine->miData.savedMoves[mostest], 
                                       &engine->miData.savedMoves[ii] );
                    if ( !usePrev && cmpVal > 0 ) {
                        mostest = ii;
                    } else if ( usePrev && cmpVal < 0 ) {
                        mostest = ii;
                    }
                }
            }
        }
    }

    while ( mostest >= 0 ) {     /* while: so we can break */
        /* record the score we're dumping.  No point in considering any scores
           lower than this for the rest of this round. */
        /* engine->miData.lowestSavedScore =  */
        /*     engine->miData.savedMoves[lowest].score; */
        /* XP_DEBUGF( "lowestSavedScore now %d\n",  */
        /* engine->miData.lowestSavedScore ); */
        if ( foundEmpty ) {
            /* we're good */
        } else {
            cmpVal = CMPMOVES( posmove, &engine->miData.savedMoves[mostest]);
            if ( !usePrev && cmpVal <= 0 ) {
                break;
            } else if ( usePrev && cmpVal >= 0 ) {
                break;
            }
        }
        /* XP_LOGF( "saving move with score %d at %d (replacing %d)\n", */
        /*          posmove->score, mostest,  */
        /*          engine->miData.savedMoves[mostest].score ); */
        XP_MEMCPY( &engine->miData.savedMoves[mostest], posmove,
                   sizeof(engine->miData.savedMoves[mostest]) );
        break;
    }
} /* saveMoveIfQualifies */

static void
set_search_limits( EngineCtxt* engine )
{
    /* If we're going to be searching backwards we want our highest cached
       move as the limit; otherwise the lowest */
    if ( 0 < engine->miData.nInMoveCache ) {
        XP_U16 srcIndx = engine->usePrev
            ? engine->nMovesToSave-1 : engine->miData.bottom;
        XP_MEMCPY( &engine->miData.lastSeenMove, 
                   &engine->miData.savedMoves[srcIndx],
                   sizeof(engine->miData.lastSeenMove) );
        //engine->miData.lowestSavedScore = 0;
    } else {
        /* we're doing this for first time */
        engine_reset( engine );
    }
}

static void
init_move_cache( EngineCtxt* engine )
{
    XP_U16 nInMoveCache = engine->nMovesToSave;
    XP_U16 ii;

    XP_ASSERT( engine->nMovesToSave == NUM_SAVED_ENGINE_MOVES );

    for ( ii = 0; ii < NUM_SAVED_ENGINE_MOVES; ++ii ) {
        if ( 0 == engine->miData.savedMoves[ii].score ) {
            --nInMoveCache;
        } else {
            break;
        }
    }
    engine->miData.nInMoveCache = nInMoveCache;
    engine->miData.bottom = NUM_SAVED_ENGINE_MOVES - nInMoveCache;

    if ( engine->usePrev ) {
        engine->miData.curCacheIndex = 
            NUM_SAVED_ENGINE_MOVES - nInMoveCache - 1;
    } else {
        engine->miData.curCacheIndex = NUM_SAVED_ENGINE_MOVES;
    }
}

static PossibleMove*
next_from_cache( EngineCtxt* engine )
{
    PossibleMove* move;
    if ( move_cache_empty( engine ) ) {
        move = NULL;
    } else {
        if ( engine->usePrev ) {
            ++engine->miData.curCacheIndex;
        } else {
            --engine->miData.curCacheIndex;
        }
        move = &engine->miData.savedMoves[engine->miData.curCacheIndex];
    }
    return move;
}

static XP_Bool
move_cache_empty( const EngineCtxt* engine )
{
    XP_Bool empty;
    const MoveIterationData* miData = &engine->miData;

    if ( 0 == miData->nInMoveCache ) {
        empty = XP_TRUE;
    } else if ( engine->usePrev ) {
        empty = miData->curCacheIndex >= NUM_SAVED_ENGINE_MOVES - 1;
    } else {
        empty = miData->curCacheIndex <= miData->bottom;
    }
    return empty;
}

static XP_Bool
scoreQualifies( EngineCtxt* engine, XP_U16 score )
{
    XP_Bool qualifies = XP_FALSE;
    XP_Bool usePrev = engine->usePrev;

    if ( usePrev && score < engine->miData.lastSeenMove.score ) {
        /* drop it */
    } else if ( !usePrev && score > engine->miData.lastSeenMove.score
         /* || (score < engine->miData.lowestSavedScore) */ ) {
        /* drop it */
    } else {
        XP_S16 ii;
        PossibleMove* savedMoves = engine->miData.savedMoves;
        /* Look at each saved score, and return true as soon as one's found
           with a lower or equal score to this.  <eeh> As an optimization,
           consider remembering what the lowest score is *once there are
           NUM_SAVED_ENGINE_MOVES moves in here* and doing a quick test on
           that. Or better, keeping the list in sorted order. */
        for ( ii = 0, savedMoves = engine->miData.savedMoves;
              ii < engine->nMovesToSave; ++ii, ++savedMoves ) {
            if ( savedMoves->score == 0 ) { /* empty slot */
                qualifies = XP_TRUE;
            } else if ( usePrev && score <= savedMoves->score ) {
                qualifies = XP_TRUE;
                break;
            } else if ( !usePrev && score >= savedMoves->score ) {
                qualifies = XP_TRUE;
                break;
            }
        }
    }
    //XP_LOGF( "%s(%d)->%d", __func__, score, qualifies );
    return qualifies;
} /* scoreQualifies */

static array_edge*
edge_from_tile( const DictionaryCtxt* dict, array_edge* from, Tile tile ) 
{
    array_edge* edge = dict_edge_with_tile( dict, from, tile );
    if ( edge != NULL ) {
        edge = dict_follow( dict, edge );
    }
    return edge;
} /* edge_from_tile */

#ifdef CPLUS
}
#endif


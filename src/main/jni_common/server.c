/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997-2009 by Eric House (xwords@eehouse.org).  All rights
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

/* #include <assert.h> */

#include "comtypes.h"
#include "server.h"
#include "util.h"
#include "model.h"
#include "comms.h"
#include "memstream.h"
#include "game.h"
/* #include "board.h" */
#include "states.h"
#include "xwproto.h"
#include "util.h"
#include "pool.h"
#include "engine.h"
#include "strutils.h"

#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define LOCAL_ADDR NULL

enum {
    END_REASON_USER_REQUEST,
    END_REASON_OUT_OF_TILES,
    END_REASON_TOO_MANY_PASSES
};
typedef XP_U8 GameEndReason;

typedef struct ServerPlayer {
    EngineCtxt* engine; /* each needs his own so don't interfere each other */
    XP_S8 deviceIndex;  /* 0 means local, -1 means unknown */
} ServerPlayer;

#define UNKNOWN_DEVICE -1
#define SERVER_DEVICE 0

typedef struct RemoteAddress {
    XP_PlayerAddr channelNo;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion;
#endif
} RemoteAddress;

/* These are the parts of the server's state that needs to be preserved
   across a reset/new game */
typedef struct ServerVolatiles {
    ModelCtxt* model;
    CommsCtxt* comms;
    XW_UtilCtxt* util;
    CurGameInfo* gi;
    TurnChangeListener turnChangeListener;
    void* turnChangeData;
    GameOverListener gameOverListener;
    void* gameOverData;
    XP_Bool showPrevMove;
} ServerVolatiles;

typedef struct ServerNonvolatiles {
    XP_U32 lastMoveTime;    /* seconds of last turn change */
    XP_U8 nDevices;
    XW_State gameState;
    XW_State stateAfterShow;
    XP_S8 currentTurn; /* invalid when game is over */
    XP_S8 quitter;     /* -1 unless somebody resigned */
    XP_U8 pendingRegistrations;
    XP_Bool showRobotScores;
    XP_Bool sortNewTiles;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion;
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16 robotThinkMin, robotThinkMax;   /* not saved (yet) */
    XP_U16 robotTradePct;
#endif

    RemoteAddress addresses[MAX_NUM_PLAYERS];
    XWStreamCtxt* prevMoveStream;     /* save it to print later */
    XWStreamCtxt* prevWordsStream;
} ServerNonvolatiles;

struct ServerCtxt {
    ServerVolatiles vol;
    ServerNonvolatiles nv;

    PoolContext* pool;

    BadWordInfo illegalWordInfo;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 lastMoveSource;
#endif

    ServerPlayer players[MAX_NUM_PLAYERS];
    XP_Bool serverDoing;
#ifdef XWFEATURE_SLOW_ROBOT
    XP_Bool robotWaiting;
#endif
    MPSLOT
};

#ifdef XWFEATURE_SLOW_ROBOT
# define ROBOTWAITING(s) (s)->robotWaiting
#else
# define ROBOTWAITING(s) XP_FALSE
#endif


#define NPASSES_OK(s) model_recentPassCountOk((s)->vol.model)

/******************************* prototypes *******************************/
static void assignTilesToAll( ServerCtxt* server );
static void resetEngines( ServerCtxt* server );
static void nextTurn( ServerCtxt* server, XP_S16 nxtTurn );

static void doEndGame( ServerCtxt* server, XP_S16 quitter );
static void endGameInternal( ServerCtxt* server, GameEndReason why, XP_S16 quitter );
static void badWordMoveUndoAndTellUser( ServerCtxt* server, 
                                        BadWordInfo* bwi );
static XP_Bool tileCountsOk( const ServerCtxt* server );
static void setTurn( ServerCtxt* server, XP_S16 turn );
static XWStreamCtxt* mkServerStream( ServerCtxt* server );

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* messageStreamWithHeader( ServerCtxt* server, 
                                              XP_U16 devIndex, XW_Proto code );
static XP_Bool handleRegistrationMsg( ServerCtxt* server, 
                                      XWStreamCtxt* stream );
static XP_S8 registerRemotePlayer( ServerCtxt* server, XWStreamCtxt* stream );
static void server_sendInitialMessage( ServerCtxt* server );
static void sendBadWordMsgs( ServerCtxt* server );
static XP_Bool handleIllegalWord( ServerCtxt* server, 
                                  XWStreamCtxt* incoming );
static void tellMoveWasLegal( ServerCtxt* server );
static void writeProto( const ServerCtxt* server, XWStreamCtxt* stream, 
                        XW_Proto proto );
#endif

#define PICK_NEXT -1

#if defined DEBUG && ! defined XWFEATURE_STANDALONE_ONLY
static char*
getStateStr( XW_State st )
{
#   define CASESTR(c) case c: return #c
    switch( st ) {
        CASESTR(XWSTATE_NONE);
        CASESTR(XWSTATE_BEGIN);
        CASESTR(XWSTATE_NEED_SHOWSCORE);
        CASESTR(XWSTATE_WAITING_ALL_REG);
        CASESTR(XWSTATE_RECEIVED_ALL_REG);
        CASESTR(XWSTATE_NEEDSEND_BADWORD_INFO);
        CASESTR(XWSTATE_MOVE_CONFIRM_WAIT);
        CASESTR(XWSTATE_MOVE_CONFIRM_MUSTSEND);
        CASESTR(XWSTATE_NEEDSEND_ENDGAME);
        CASESTR(XWSTATE_INTURN);
        CASESTR(XWSTATE_GAMEOVER);
    default:
        return "unknown";
    }
#   undef CASESTR
}
#endif

#if 0 
//def DEBUG
static void
logNewState( XW_State old, XW_State newst )
{
    if ( old != newst ) {
        char* oldStr = getStateStr(old);
        char* newStr = getStateStr(newst);
        XP_LOGF( "state transition %s => %s", oldStr, newStr );
    }
}
# define    SETSTATE( s, st ) { XW_State old = (s)->nv.gameState; \
                                (s)->nv.gameState = (st); \
                                logNewState(old, st); }
#else
# define    SETSTATE( s, st ) (s)->nv.gameState = (st)
#endif

/*****************************************************************************
 *
 ****************************************************************************/
#ifndef XWFEATURE_STANDALONE_ONLY
static void
syncPlayers( ServerCtxt* server )
{
    XP_U16 ii;
    CurGameInfo* gi = server->vol.gi;
    LocalPlayer* lp = gi->players;
    ServerPlayer* player = server->players;
    for ( ii = 0; ii < gi->nPlayers; ++ii, ++lp, ++player ) {
        if ( !lp->isLocal/*  && !lp->name */ ) {
            ++server->nv.pendingRegistrations;
        }
        player->deviceIndex = lp->isLocal? SERVER_DEVICE : UNKNOWN_DEVICE;
    }
}
#else
# define syncPlayers( server )
#endif

static XP_Bool
amServer( const ServerCtxt* server )
{ 
    return SERVER_ISSERVER == server->vol.gi->serverRole;
}

static void
initServer( ServerCtxt* server )
{
    setTurn( server, -1 ); /* game isn't under way yet */

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        SETSTATE( server, XWSTATE_NONE );
#endif
    } else {
        SETSTATE( server, XWSTATE_BEGIN );
    }

    syncPlayers( server );

    server->nv.nDevices = 1; /* local device (0) is always there */
#ifdef STREAM_VERS_BIGBOARD
    server->nv.streamVersion = STREAM_SAVE_PREVWORDS; /* default to old */
#endif
    server->nv.quitter = -1;
} /* initServer */

ServerCtxt* 
server_make( MPFORMAL ModelCtxt* model, CommsCtxt* comms, XW_UtilCtxt* util )
{
    ServerCtxt* result = (ServerCtxt*)XP_MALLOC( mpool, sizeof(*result) );

    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof(*result) );

        MPASSIGN(result->mpool, mpool);

        result->vol.model = model;
        result->vol.comms = comms;
        result->vol.util = util;
        result->vol.gi = util->gameInfo;

        initServer( result );
    }
    return result;
} /* server_make */

static void
getNV( XWStreamCtxt* stream, ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 ii;
    XP_U16 version = stream_getVersion( stream );

    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->lastMoveTime = stream_getU32( stream );
    }

    if ( version < STREAM_VERS_SERVER_SAVES_TOSHOW ) {
        /* no longer used */
        (void)stream_getBits( stream, 3 ); /* was npassesinrow */
    }

    nv->nDevices = (XP_U8)stream_getBits( stream, NDEVICES_NBITS );
    if ( version > STREAM_VERS_41B4 ) {
        ++nv->nDevices;
    }

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    nv->gameState = (XW_State)stream_getBits( stream, XWSTATE_NBITS );
    if ( version >= STREAM_VERS_SERVER_SAVES_TOSHOW ) {
        nv->stateAfterShow = (XW_State)stream_getBits( stream, XWSTATE_NBITS );
    }

    nv->currentTurn = (XP_S8)stream_getBits( stream, NPLAYERS_NBITS ) - 1;
    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->quitter = (XP_S8)stream_getBits( stream, NPLAYERS_NBITS ) - 1;
    }
    nv->pendingRegistrations = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );

    for ( ii = 0; ii < nPlayers; ++ii ) {
        nv->addresses[ii].channelNo =
            (XP_PlayerAddr)stream_getBits( stream, 16 );
#ifdef STREAM_VERS_BIGBOARD
        nv->addresses[ii].streamVersion = STREAM_VERS_BIGBOARD <= version ?
            stream_getBits( stream, 8 ) : STREAM_SAVE_PREVWORDS;
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_SAVE_PREVWORDS < version ) {
        nv->streamVersion = stream_getU8 ( stream );
    }
    XP_LOGF( "%s: read streamVersion: 0x%x", __func__, nv->streamVersion );
#endif
} /* getNV */

static void
putNV( XWStreamCtxt* stream, const ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 ii;

    stream_putU32( stream, nv->lastMoveTime );

    /* number of players is upper limit on device count */
    stream_putBits( stream, NDEVICES_NBITS, nv->nDevices-1 );

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    stream_putBits( stream, XWSTATE_NBITS, nv->gameState );
    stream_putBits( stream, XWSTATE_NBITS, nv->stateAfterShow );

    /* +1: make -1 (NOTURN) into a positive number */
    stream_putBits( stream, NPLAYERS_NBITS, nv->currentTurn+1 );
    stream_putBits( stream, NPLAYERS_NBITS, nv->quitter+1 );
    stream_putBits( stream, NPLAYERS_NBITS, nv->pendingRegistrations );

    for ( ii = 0; ii < nPlayers; ++ii ) {
        stream_putBits( stream, 16, nv->addresses[ii].channelNo );
#ifdef STREAM_VERS_BIGBOARD
        stream_putBits( stream, 8, nv->addresses[ii].streamVersion );
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    stream_putU8( stream, nv->streamVersion );
    XP_LOGF( "%s: wrote streamVersion: 0x%x", __func__, nv->streamVersion );
#endif
} /* putNV */

static XWStreamCtxt*
readStreamIf( ServerCtxt* server, XWStreamCtxt* in )
{
    XWStreamCtxt* result = NULL;
    XP_U16 len = stream_getU16( in );
    if ( 0 < len ) {
        result = mkServerStream( server );
        stream_getFromStream( result, in, len );
    }
    return result;
}

static void
writeStreamIf( XWStreamCtxt* dest, XWStreamCtxt* src )
{
    XP_U16 len = !!src ? stream_getSize( src ) : 0;
    stream_putU16( dest, len );
    if ( 0 < len ) {
        XWStreamPos pos = stream_getPos( src, POS_READ );
        stream_getFromStream( dest, src, len );
        (void)stream_setPos( src, POS_READ, pos );
    }
}

ServerCtxt*
server_makeFromStream( MPFORMAL XWStreamCtxt* stream, ModelCtxt* model, 
                       CommsCtxt* comms, XW_UtilCtxt* util, XP_U16 nPlayers )
{
    ServerCtxt* server;
    XP_U16 version = stream_getVersion( stream );
    short ii;

    server = server_make( MPPARM(mpool) model, comms, util );
    getNV( stream, &server->nv, nPlayers );
    
    if ( stream_getBits(stream, 1) != 0 ) {
        server->pool = pool_makeFromStream( MPPARM(mpool) stream );
    }

    for ( ii = 0; ii < nPlayers; ++ii ) {
        ServerPlayer* player = &server->players[ii];

        player->deviceIndex = stream_getU8( stream );

        if ( stream_getU8( stream ) != 0 ) {
            player->engine = engine_makeFromStream( MPPARM(mpool)
                                                    stream, util );
        }
    }

    if ( STREAM_VERS_ALWAYS_MULTI <= version
#ifndef PREV_WAS_STANDALONE_ONLY
         || XP_TRUE
#endif
         ) { 
        server->lastMoveSource = (XP_U16)stream_getBits( stream, 2 );
    }

    if ( version >= STREAM_SAVE_PREVMOVE ) {
        server->nv.prevMoveStream = readStreamIf( server, stream );
    }
    if ( version >= STREAM_SAVE_PREVWORDS ) {
        server->nv.prevWordsStream = readStreamIf( server, stream );
    }

    util_informMissing( util, server->vol.gi->serverRole == SERVER_ISSERVER,
                        comms_getConType( comms ),
                        server->nv.pendingRegistrations );
    return server;
} /* server_makeFromStream */

void
server_writeToStream( const ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_U16 ii;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    putNV( stream, &server->nv, nPlayers );

    stream_putBits( stream, 1, server->pool != NULL );
    if ( server->pool != NULL ) {
        pool_writeToStream( server->pool, stream );
    }

    for ( ii = 0; ii < nPlayers; ++ii ) {
        const ServerPlayer* player = &server->players[ii];

        stream_putU8( stream, player->deviceIndex );

        stream_putU8( stream, (XP_U8)(player->engine != NULL) );
        if ( player->engine != NULL ) {
            engine_writeToStream( player->engine, stream );
        }
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    stream_putBits( stream, 2, server->lastMoveSource );
#else
    stream_putBits( stream, 2, 0 );
#endif

    writeStreamIf( stream, server->nv.prevMoveStream );
    writeStreamIf( stream, server->nv.prevWordsStream );
} /* server_writeToStream */

static void
cleanupServer( ServerCtxt* server )
{
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(server->players); ++ii ){
        ServerPlayer* player = &server->players[ii];
        if ( player->engine != NULL ) {
            engine_destroy( player->engine );
        }
    }
    XP_MEMSET( server->players, 0, sizeof(server->players) );

    if ( server->pool != NULL ) {
        pool_destroy( server->pool );
        server->pool = (PoolContext*)NULL;
    }

    if ( !!server->nv.prevMoveStream ) {
        stream_destroy( server->nv.prevMoveStream );
    }
    if ( !!server->nv.prevWordsStream ) {
        stream_destroy( server->nv.prevWordsStream );
    }

    XP_MEMSET( &server->nv, 0, sizeof(server->nv) );
} /* cleanupServer */

void
server_reset( ServerCtxt* server, CommsCtxt* comms )
{
    ServerVolatiles vol = server->vol;

    cleanupServer( server );

    vol.comms = comms;
    server->vol = vol;

    initServer( server );
} /* server_reset */

void
server_destroy( ServerCtxt* server )
{
    cleanupServer( server );

    XP_FREE( server->mpool, server );
} /* server_destroy */

#ifdef XWFEATURE_SLOW_ROBOT
static int
figureSleepTime( const ServerCtxt* server )
{
    int result = 0;
    XP_U16 min = server->nv.robotThinkMin;
    XP_U16 max = server->nv.robotThinkMax;
    if ( min < max ) {
        int diff = max - min + 1;
        result = XP_RANDOM() % diff;
    }
    result += min;

    return result;
}
#endif

void
server_prefsChanged( ServerCtxt* server, CommonPrefs* cp )
{
    server->nv.showRobotScores = cp->showRobotScores;
    server->nv.sortNewTiles = cp->sortNewTiles;
#ifdef XWFEATURE_SLOW_ROBOT
    server->nv.robotThinkMin = cp->robotThinkMin;
    server->nv.robotThinkMax = cp->robotThinkMax;
    server->nv.robotTradePct = cp->robotTradePct;
#endif
} /* server_prefsChanged */

XP_S16
server_countTilesInPool( ServerCtxt* server )
{
    XP_S16 result = -1;
    PoolContext* pool = server->pool;
    if ( !!pool ) {
        result = pool_getNTilesLeft( pool );
    }
    return result;
} /* server_countTilesInPool */

/* I'm a client device.  It's my job to start the whole conversation by
 * contacting the server and telling him that I exist (and some other stuff,
 * including what the players here want to be called.)
 */ 
#define NAME_LEN_NBITS 6
#define MAX_NAME_LEN ((1<<(NAME_LEN_NBITS-1))-1)
#ifndef XWFEATURE_STANDALONE_ONLY
void
server_initClientConnection( ServerCtxt* server, XWStreamCtxt* stream )
{
    LOG_FUNC();
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers;
    LocalPlayer* lp;
#ifdef DEBUG
    XP_U16 ii = 0;
#endif

    XP_ASSERT( gi->serverRole == SERVER_ISCLIENT );
    XP_ASSERT( stream != NULL );
    if ( server->nv.gameState == XWSTATE_NONE ) {
        stream_open( stream );

        writeProto( server, stream, XWPROTO_DEVICE_REGISTRATION );

        nPlayers = gi->nPlayers;
        XP_ASSERT( nPlayers > 0 );
        stream_putBits( stream, NPLAYERS_NBITS, 
                        gi_countLocalPlayers( gi, XP_FALSE) );

        for ( lp = gi->players; nPlayers-- > 0; ++lp ) {
            XP_UCHAR* name;
            XP_U8 len;

            XP_ASSERT( ii++ < MAX_NUM_PLAYERS );
            if ( !lp->isLocal ) {
                continue;
            }

            stream_putBits( stream, 1, LP_IS_ROBOT(lp) ); /* better not to
                                                             send this */
            /* The first nPlayers players are the ones we'll use.  The local flag
               doesn't matter when for SERVER_ISCLIENT. */
            name = emptyStringIfNull(lp->name);
            len = XP_STRLEN(name);
            if ( len > MAX_NAME_LEN ) {
                len = MAX_NAME_LEN;
            }
            stream_putBits( stream, NAME_LEN_NBITS, len );
            stream_putBytes( stream, name, len );
        }
#ifdef STREAM_VERS_BIGBOARD
        stream_putU8( stream, CUR_STREAM_VERS );
#endif

    } else {
        XP_LOGF( "%s: wierd state %s; dropping message", __func__,
                 getStateStr(server->nv.gameState) );
    }
    stream_destroy( stream );
} /* server_initClientConnection */
#endif

#ifdef XWFEATURE_CHAT
static void
sendChatTo( ServerCtxt* server, XP_U16 devIndex, const XP_UCHAR const* msg )
{
    XWStreamCtxt* stream = messageStreamWithHeader( server, devIndex, 
                                                    XWPROTO_CHAT );
    stringToStream( stream, msg );
    stream_destroy( stream );
}

static void
sendChatToClientsExcept( ServerCtxt* server, XP_U16 skip, 
                         const XP_UCHAR const* msg )
{
    XP_U16 devIndex;
    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendChatTo( server, devIndex, msg );
        }
    }
}

void
server_sendChat( ServerCtxt* server, const XP_UCHAR const* msg )
{
    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        sendChatTo( server, SERVER_DEVICE, msg );
    } else {
        sendChatToClientsExcept( server, SERVER_DEVICE, msg );
    }
}
#endif

static void
callTurnChangeListener( const ServerCtxt* server )
{
    if ( server->vol.turnChangeListener != NULL ) {
        (*server->vol.turnChangeListener)( server->vol.turnChangeData );
    }
} /* callTurnChangeListener */

#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef STREAM_VERS_BIGBOARD
static void
setStreamVersion( ServerCtxt* server )
{
    XP_U16 devIndex;
    XP_U8 streamVersion = CUR_STREAM_VERS;
    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        XP_U8 devVersion = server->nv.addresses[devIndex].streamVersion;
        if ( devVersion < streamVersion ) {
            streamVersion = devVersion;
        }
    }
    XP_LOGF( "%s: setting streamVersion: 0x%x", __func__, streamVersion );
    server->nv.streamVersion = streamVersion;
}

static void
checkResizeBoard( ServerCtxt* server )
{
    CurGameInfo* gi = server->vol.gi;
    if ( STREAM_VERS_BIGBOARD > server->nv.streamVersion && gi->boardSize > 15) {
        XP_LOGF( "%s: dropping board size from %d to 15", __func__, gi->boardSize );
        gi->boardSize = 15;
        model_setSize( server->vol.model, 15 );
    }
}
# else
#  define setStreamVersion(s)
#  define checkResizeBoard(s)
# endif

static XP_Bool
handleRegistrationMsg( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    XP_U16 playersInMsg;
    XP_S8 clientIndex = 0;      /* quiet compiler */
    XP_U16 ii = 0;
    LOG_FUNC();

    /* code will have already been consumed */
    playersInMsg = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );
    XP_ASSERT( playersInMsg > 0 );

    if ( server->nv.pendingRegistrations < playersInMsg ) {
        util_userError( server->vol.util, ERR_REG_UNEXPECTED_USER );
        success = XP_FALSE;
    } else {
#ifdef DEBUG
        XP_S8 prevIndex = -1;
#endif
        for ( ; ii < playersInMsg; ++ii ) {
            clientIndex = registerRemotePlayer( server, stream );

            /* This is abusing the semantics of turn change -- at least in the
               case where there is another device yet to register -- but we
               need to let the board know to redraw the scoreboard with more
               players there. */
            callTurnChangeListener( server );
#ifdef DEBUG
            XP_ASSERT( ii == 0 || prevIndex == clientIndex );
            prevIndex = clientIndex;
#endif
        }

    }

    if ( success ) {
#ifdef STREAM_VERS_BIGBOARD
        if ( 0 < stream_getSize(stream) ) {
            XP_U8 streamVersion = stream_getU8( stream );
            if ( streamVersion >= STREAM_VERS_BIGBOARD ) {
                XP_LOGF( "%s: upping device %d streamVersion to %d",
                         __func__, clientIndex, streamVersion );
                server->nv.addresses[clientIndex].streamVersion = streamVersion;
            }
        }
#endif

        if ( server->nv.pendingRegistrations == 0 ) {
            XP_ASSERT( ii == playersInMsg ); /* otherwise malformed */
            setStreamVersion( server );
            checkResizeBoard( server );
            assignTilesToAll( server );
            SETSTATE( server, XWSTATE_RECEIVED_ALL_REG );
        }
    }

    return success;
} /* handleRegistrationMsg */
#endif

/* Just for grins....trade in all the tiles that weren't used in the
 * move the robot manage to make.  This is not meant to be strategy, but
 * rather to force me to make the trade-communication stuff work well.
 */
#if 0
static void
robotTradeTiles( ServerCtxt* server, MoveInfo* newMove )
{
    Tile tradeTiles[MAX_TRAY_TILES];
    XP_S16 turn = server->nv.currentTurn;
    Tile* curTiles = model_getPlayerTiles( server->model, turn );
    XP_U16 numInTray = model_getNumPlayerTiles( server->model, turn );
    XP_MEMCPY( tradeTiles, curTiles, numInTray );

    for ( ii = 0; ii < numInTray; ++ii ) { /* for each tile in tray */
        XP_Bool keep = XP_FALSE;
        for ( jj = 0; jj < newMove->numTiles; ++jj ) { /* for each in move */
            Tile movedTile = newMove->tiles[jj].tile;
            if ( newMove->tiles[jj].isBlank ) {
                movedTile |= TILE_BLANK_BIT;
            }
            if ( movedTile == curTiles[ii] ) { /* it's in the move */
                keep = XP_TRUE;
                break;
            }
        }
        if ( !keep ) {
            tradeTiles[numToTrade++] = curTiles[ii];
        }
    }

    
} /* robotTradeTiles */
#endif

static XWStreamCtxt*
mkServerStream( ServerCtxt* server )
{
    XWStreamCtxt* stream;
    stream = mem_stream_make( MPPARM(server->mpool) 
                              util_getVTManager(server->vol.util), 
                              NULL, CHANNEL_NONE, 
                              (MemStreamCloseCallback)NULL );
    XP_ASSERT( !!stream );
    return stream;
} /* mkServerStream */

static XP_Bool
makeRobotMove( ServerCtxt* server )
{
	gbAndroidPlaying = XP_TRUE;  // Android is making a move.

    XP_Bool result = XP_FALSE;
    XP_Bool searchComplete;
    XP_S16 turn;
    const TrayTileSet* tileSet;
    MoveInfo newMove;
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    XP_Bool timerEnabled = gi->timerEnabled;
    XP_Bool canMove;
    XP_U32 time = 0L; /* stupid compiler.... */
    XW_UtilCtxt* util = server->vol.util;
    XP_Bool forceTrade = XP_FALSE;
    
    if ( timerEnabled ) {
        time = util_getCurSeconds( util );
    }

#ifdef XWFEATURE_SLOW_ROBOT
    if ( 0 != server->nv.robotTradePct
         && (server_countTilesInPool( server ) >= MAX_TRAY_TILES) ) {
        XP_U16 pct = XP_RANDOM() % 100;
        forceTrade = pct < server->nv.robotTradePct ;
    }
#endif

    turn = server->nv.currentTurn;
    XP_ASSERT( turn >= 0 );

    /* If the player's been recently turned into a robot while he had some
       pending tiles on the board we'll have problems.  It'd be best to detect
       this and put 'em back when that happens.  But for now we'll just be
       paranoid.  PENDING(ehouse) */
    model_resetCurrentTurn( model, turn );

    if ( !forceTrade ) {
        tileSet = model_getPlayerTiles( model, turn );
#ifdef XWFEATURE_BONUSALL
        XP_U16 allTilesBonus = server_figureFinishBonus( server, turn );
#endif
        XP_ASSERT( !!server_getEngineFor( server, turn ) );
        searchComplete = engine_findMove( server_getEngineFor( server, turn ),
                                          model, turn, tileSet->tiles, 
                                          tileSet->nTiles, XP_FALSE,
#ifdef XWFEATURE_BONUSALL
                                          allTilesBonus, 
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                                          NULL, XP_FALSE,
#endif
                                          server->vol.gi->players[turn].robotIQ,
                                          &canMove, &newMove );
    }
    if ( forceTrade || searchComplete ) {
        const XP_UCHAR* str;
        XWStreamCtxt* stream = NULL;

        XP_Bool trade = forceTrade || 
            ((newMove.nTiles == 0) && canMove &&
             (server_countTilesInPool( server ) >= MAX_TRAY_TILES));

        server->vol.showPrevMove = XP_TRUE;
        if ( server->nv.showRobotScores ) {
            stream = mkServerStream( server );
        }

        /* trade if unable to find a move */
        if ( trade ) {
            TrayTileSet oldTiles = *model_getPlayerTiles( model, turn );
            XP_LOGF( "%s: robot trading %d tiles", __func__, oldTiles.nTiles );
            result = server_commitTrade( server, &oldTiles );
            util_androidExchangedTiles( server->vol.util );

            /* Quick hack to fix gremlin bug where all-robot game seen none
               able to trade for tiles to move and blowing the undo stack.
               This will stop them, and should have no effect if there are any
               human players making real moves. */

            if ( !!stream ) {
                XP_UCHAR buf[64];
                str = util_getUserString(util, STRD_ROBOT_TRADED);
                XP_SNPRINTF( buf, sizeof(buf), str, MAX_TRAY_TILES );

                stream_catString( stream, buf );
                XP_ASSERT( !server->nv.prevMoveStream );
                server->nv.prevMoveStream = stream;
            }
        } else { 
            /* if canMove is false, this is a fake move, a pass */
            if ( canMove || NPASSES_OK(server) ) {
                model_makeTurnFromMoveInfo( model, turn, &newMove );
                XP_LOGF( "%s: robot making %d tile move", __func__, newMove.nTiles );
                if (0 == newMove.nTiles){
                    util_androidNoMove( server->vol.util );
                }

                if ( !!stream ) {
                    XWStreamCtxt* wordsStream = mkServerStream( server );
                    WordNotifierInfo* ni = 
                        model_initWordCounter( model, wordsStream );
                    (void)model_checkMoveLegal( model, turn, stream, ni );
                    XP_ASSERT( !server->nv.prevMoveStream );
                    server->nv.prevMoveStream = stream;
                    server->nv.prevWordsStream = wordsStream;
                }
                result = server_commitMove( server );
            } else {
                result = XP_FALSE;
            }
        }
    }

    if ( timerEnabled ) {
        gi->players[turn].secondsUsed += 
            (XP_U16)(util_getCurSeconds( util ) - time);
    } else {
        XP_ASSERT( gi->players[turn].secondsUsed == 0 );
    }

    gbAndroidPlaying = XP_FALSE;  // Android is finished making a move.
    return result; /* always return TRUE after robot move? */
} /* makeRobotMove */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool 
wakeRobotProc( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    XP_ASSERT( TIMER_SLOWROBOT == why );
    ServerCtxt* server = (ServerCtxt*)closure;
    XP_ASSERT( ROBOTWAITING(server) );
    server->robotWaiting = XP_FALSE;
    util_requestTime( server->vol.util );
    return XP_FALSE;
}
#endif

static XP_Bool
robotMovePending( const ServerCtxt* server )
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = server->nv.currentTurn;
    if ( turn >= 0 && tileCountsOk(server) && NPASSES_OK(server) ) {
        CurGameInfo* gi = server->vol.gi;
        LocalPlayer* player = &gi->players[turn];
        result = LP_IS_ROBOT(player) && LP_IS_LOCAL(player);
    }
    return result;
} /* robotMovePending */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool
postponeRobotMove( ServerCtxt* server )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( robotMovePending(server) );

    if ( !ROBOTWAITING(server) ) {
        XP_U16 sleepTime = figureSleepTime(server);
        if ( 0 != sleepTime ) {
            server->robotWaiting = XP_TRUE;
            util_setTimer( server->vol.util, TIMER_SLOWROBOT, sleepTime,
                           wakeRobotProc, server );
            result = XP_TRUE;
        }
    }
    return result;
}
# define POSTPONEROBOTMOVE(s) postponeRobotMove(s)
#else
# define POSTPONEROBOTMOVE(s) XP_FALSE
#endif

static void
showPrevScore( ServerCtxt* server )
{
    if ( server->nv.showRobotScores ) { /* this can be changed between turns */
        XW_UtilCtxt* util = server->vol.util;
        XWStreamCtxt* stream;
        const XP_UCHAR* str;
        XP_UCHAR buf[128];
        CurGameInfo* gi = server->vol.gi;
        XP_U16 nPlayers = gi->nPlayers;
        XP_U16 prevTurn;
        LocalPlayer* lp;

        prevTurn = (server->nv.currentTurn + nPlayers - 1) % nPlayers;
        lp = &gi->players[prevTurn];

        if ( LP_IS_LOCAL(lp) ) {
            /* Why can't a local non-robot have postponed score? */
            // XP_ASSERT( LP_IS_ROBOT(lp) );
            str = util_getUserString( util, STR_ROBOT_MOVED );
        } else {
            str = util_getUserString( util, STRS_REMOTE_MOVED );
            XP_SNPRINTF( buf, sizeof(buf), str, lp->name );
            str = buf;
        }

        stream = mkServerStream( server );
        stream_catString( stream, str );

        if ( !!server->nv.prevMoveStream ) {
            XWStreamCtxt* prevStream = server->nv.prevMoveStream;
            XP_U16 len = stream_getSize( prevStream );

            server->nv.prevMoveStream = NULL;

            stream_putBytes( stream, stream_getPtr( prevStream ), len );
            stream_destroy( prevStream );
        }

        util_informMove( util, stream, server->nv.prevWordsStream );
        stream_destroy( stream );

        if ( !!server->nv.prevWordsStream ) {
            stream_destroy( server->nv.prevWordsStream );
            server->nv.prevWordsStream = NULL;
        }
    }
    SETSTATE( server, server->nv.stateAfterShow );
} /* showPrevScore */

XP_Bool
server_do( ServerCtxt* server )
{
    XP_Bool result = XP_TRUE;

    if ( server->serverDoing ) {
        result = XP_FALSE;
    } else {
        XP_Bool moreToDo = XP_FALSE;
        server->serverDoing = XP_TRUE;

        switch( server->nv.gameState ) {
        case XWSTATE_BEGIN:
            if ( server->nv.pendingRegistrations == 0 ) { /* all players on
                                                             device */
                assignTilesToAll( server );
                SETSTATE( server, XWSTATE_INTURN );
                setTurn( server, 0 );
                moreToDo = XP_TRUE;
            }
            break;

        case XWSTATE_NEEDSEND_BADWORD_INFO:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
            badWordMoveUndoAndTellUser( server, &server->illegalWordInfo );
#ifndef XWFEATURE_STANDALONE_ONLY
            sendBadWordMsgs( server );
#endif
            nextTurn( server, PICK_NEXT ); /* sets server->nv.gameState */
            //moreToDo = XP_TRUE;   /* why? */
            break;

#ifndef XWFEATURE_STANDALONE_ONLY
        case XWSTATE_RECEIVED_ALL_REG:
            server_sendInitialMessage( server ); 
            /* PENDING isn't INTURN_OFFDEVICE possible too?  Or just
               INTURN?  */
            SETSTATE( server, XWSTATE_INTURN );
            setTurn( server, 0 );
            moreToDo = XP_TRUE;
            break;

        case XWSTATE_MOVE_CONFIRM_MUSTSEND:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
            tellMoveWasLegal( server );
            nextTurn( server, PICK_NEXT );
            break;

#endif /* XWFEATURE_STANDALONE_ONLY */

        case XWSTATE_NEEDSEND_ENDGAME:
            endGameInternal( server, END_REASON_OUT_OF_TILES, -1 );
            break;

        case XWSTATE_NEED_SHOWSCORE:
            showPrevScore( server );
            /* state better have changed or we'll infinite loop... */
            XP_ASSERT( XWSTATE_NEED_SHOWSCORE != server->nv.gameState );
            /* either process turn or end game should come next... */
            moreToDo = XWSTATE_NEED_SHOWSCORE != server->nv.gameState;
            break;
        case XWSTATE_INTURN:
            if ( robotMovePending( server ) && !ROBOTWAITING(server) ) {
                result = makeRobotMove( server );
                /* if robot was interrupted, we need to schedule again */
                moreToDo = !result || 
                    (robotMovePending( server ) && !POSTPONEROBOTMOVE(server));
            }
            break;

        default:
            result = XP_FALSE;
            break;
        } /* switch */

        if ( moreToDo ) {
            util_requestTime( server->vol.util );
        }

        server->serverDoing = XP_FALSE;
    }
    return result;
} /* server_do */

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_S8
getIndexForDevice( ServerCtxt* server, XP_PlayerAddr channelNo )
{
    short ii;
    XP_S8 result = -1;

    for ( ii = 0; ii < server->nv.nDevices; ++ii ) {
        RemoteAddress* addr = &server->nv.addresses[ii];
        if ( addr->channelNo == channelNo ) {
            result = ii;
            break;
        }
    }

    return result;
} /* getIndexForDevice */

static LocalPlayer* 
findFirstPending( ServerCtxt* server, ServerPlayer** playerP )
{
    LocalPlayer* lp;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;
    XP_U16 nPending = server->nv.pendingRegistrations;

    XP_ASSERT( nPlayers > 0 );
    lp = gi->players + nPlayers;

    while ( --lp >= gi->players ) {
        --nPlayers;
        if ( !lp->isLocal ) {
            if ( --nPending == 0 ) {
                break;
            }
        }
    }
    XP_ASSERT( lp >= gi->players ); /* did we find a slot? */
    *playerP = server->players + nPlayers;
    return lp;
} /* findFirstPending */

static XP_S8
registerRemotePlayer( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_S8 deviceIndex;
    XP_PlayerAddr channelNo;
    XP_UCHAR* name;
    XP_U16 nameLen;
    LocalPlayer* lp;
    ServerPlayer* player = (ServerPlayer*)NULL;

    /* The player must already be there with a null name, or it's an error.
       Take the first empty slot. */
    XP_ASSERT( server->nv.pendingRegistrations > 0 );

    /* find the slot to use */
    lp = findFirstPending( server, &player );

    /* get data from stream */
    lp->robotIQ = 1 == stream_getBits( stream, 1 )? 1 : 0;
    nameLen = stream_getBits( stream, NAME_LEN_NBITS );
    name = (XP_UCHAR*)XP_MALLOC( server->mpool, nameLen + 1 );
    stream_getBytes( stream, name, nameLen );
    name[nameLen] = '\0';

    replaceStringIfDifferent( server->mpool, &lp->name, name );
    XP_FREE( server->mpool, name );

    channelNo = stream_getAddress( stream );
    deviceIndex = getIndexForDevice( server, channelNo );

    --server->nv.pendingRegistrations;

    if ( deviceIndex == -1 ) {
        RemoteAddress* addr; 
        addr = &server->nv.addresses[server->nv.nDevices];

        deviceIndex = server->nv.nDevices++;

        XP_ASSERT( channelNo != 0 );
        addr->channelNo = channelNo;
#ifdef STREAM_VERS_BIGBOARD
        addr->streamVersion = STREAM_SAVE_PREVWORDS;
#endif
    }

    player->deviceIndex = deviceIndex;
    return deviceIndex;
} /* registerRemotePlayer */

static void
clearLocalRobots( ServerCtxt* server )
{
    XP_U16 ii;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;

    for ( ii = 0; ii < nPlayers; ++ii ) {
        LocalPlayer* player = &gi->players[ii];
        if ( LP_IS_LOCAL( player ) ) {
            player->robotIQ = 0;
        }
    }
} /* clearLocalRobots */
#endif

static void
sortTilesIf( ServerCtxt* server, XP_S16 turn )
{
    ModelCtxt* model = server->vol.model;
    if ( server->nv.sortNewTiles ) {
        model_sortTiles( model, turn );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
/* Called in response to message from server listing all the names of
 * players in the game (in server-assigned order) and their initial
 * tray contents.
 */
static XP_Bool
client_readInitialMessage( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool accepted = 0 == server->nv.addresses[0].channelNo;

    /* We should never get this message a second time, but very rarely we do.
       Drop it in that case. */
    if ( accepted ) {
        DictionaryCtxt* newDict;
        DictionaryCtxt* curDict;
        XP_U16 nPlayers, nCols;
        XP_PlayerAddr channelNo;
        XP_U16 ii;
        ModelCtxt* model = server->vol.model;
        CurGameInfo* gi = server->vol.gi;
        CurGameInfo localGI;
        XP_U32 gameID;
        PoolContext* pool;
#ifdef STREAM_VERS_BIGBOARD
        XP_UCHAR rmtDictName[128];
        XP_UCHAR rmtDictSum[64];
#endif

        /* version; any dependencies here? */
        XP_U8 streamVersion = stream_getU8( stream );
        XP_LOGF( "%s: set streamVersion to %d", __func__, streamVersion );
        stream_setVersion( stream, streamVersion );
        // XP_ASSERT( streamVersion <= CUR_STREAM_VERS ); /* else do what? */

        gameID = stream_getU32( stream );
        XP_LOGF( "read gameID of %lx; calling comms_setConnID", gameID );
        server->vol.gi->gameID = gameID;
        comms_setConnID( server->vol.comms, gameID );

        XP_MEMSET( &localGI, 0, sizeof(localGI) );
        gi_readFromStream( MPPARM(server->mpool) stream, &localGI );
        localGI.serverRole = SERVER_ISCLIENT;

        localGI.dictName = copyString( server->mpool, gi->dictName );
        gi_copy( MPPARM(server->mpool) gi, &localGI );

        nCols = localGI.boardSize;

        newDict = util_makeEmptyDict( server->vol.util );
        dict_loadFromStream( newDict, stream );

#ifdef STREAM_VERS_BIGBOARD
        if ( STREAM_VERS_DICTNAME <= streamVersion ) {
            stringFromStreamHere( stream, rmtDictName, VSIZE(rmtDictName) );
            stringFromStreamHere( stream, rmtDictSum, VSIZE(rmtDictSum) );
        } else {
            rmtDictName[0] = '\0';
        }
#endif

        channelNo = stream_getAddress( stream );
        XP_ASSERT( channelNo != 0 );
        server->nv.addresses[0].channelNo = channelNo;

        model_setSize( model, nCols );

        nPlayers = localGI.nPlayers;
        XP_STATUSF( "reading in %d players", localGI.nPlayers );

        gi_disposePlayerInfo( MPPARM(server->mpool) &localGI );

        gi->nPlayers = nPlayers;
        model_setNPlayers( model, nPlayers );

        curDict = model_getDictionary( model );

        XP_ASSERT( !!newDict );

        if ( curDict == NULL ) {
            model_setDictionary( model, newDict );
        } else if ( dict_tilesAreSame( newDict, curDict ) ) {
            /* keep the dict the local user installed */
            dict_destroy( newDict );
#ifdef STREAM_VERS_BIGBOARD
            if ( '\0' != rmtDictName[0] ) {
                const XP_UCHAR* ourName = dict_getShortName( curDict );
                util_informNetDict( server->vol.util, 
                                    dict_getLangCode( curDict ),
                                    ourName, rmtDictName,
                                    rmtDictSum, localGI.phoniesAction );
            }
#endif
        } else {
            dict_destroy( curDict );
            model_setDictionary( model, newDict );
            util_userError( server->vol.util, ERR_SERVER_DICT_WINS );
            clearLocalRobots( server );
        }

        XP_ASSERT( !server->pool );
        pool = server->pool = pool_make( MPPARM_NOCOMMA(server->mpool) );
        pool_initFromDict( server->pool, model_getDictionary(model));

        /* now read the assigned tiles for each player from the stream, and
           remove them from the newly-created local pool. */
        for ( ii = 0; ii < nPlayers; ++ii ) {
            TrayTileSet tiles;

            traySetFromStream( stream, &tiles );
            XP_ASSERT( tiles.nTiles <= MAX_TRAY_TILES );

            XP_LOGF( "%s: got %d tiles for player %d", __func__, tiles.nTiles, ii );

            model_assignPlayerTiles( model, ii, &tiles );

            /* remove what the server's assigned so we won't conflict
               later. */
            pool_removeTiles( pool, &tiles );

            sortTilesIf( server, ii );
        }

        syncPlayers( server );

        SETSTATE( server, XWSTATE_INTURN );

        /* Give board a chance to redraw self with the full compliment of known
           players */
        setTurn( server, 0 );
    } else {
        XP_LOGF( "%s: wanted 0; got %d", __func__, 
                 server->nv.addresses[0].channelNo );
    }
    return accepted;
} /* client_readInitialMessage */
#endif

/* For each remote device, send a message containing the dictionary and the
 * names of all the players in the game (including those on the device itself,
 * since they might have been changed in the case of conflicts), in the order
 * that all must use for the game.  Then for each player on the device give
 * the starting tray.
 */
#ifndef XWFEATURE_STANDALONE_ONLY

static void
makeSendableGICopy( ServerCtxt* server, CurGameInfo* giCopy, 
                    XP_U16 deviceIndex )
{
    XP_U16 nPlayers;
    LocalPlayer* clientPl;
    XP_U16 ii;

    XP_MEMCPY( giCopy, server->vol.gi, sizeof(*giCopy) );

    nPlayers = giCopy->nPlayers;

    for ( clientPl = giCopy->players, ii = 0;
          ii < nPlayers; ++clientPl, ++ii ) {
        /* adjust isLocal to client's perspective */
        clientPl->isLocal = server->players[ii].deviceIndex == deviceIndex;
    }

    giCopy->dictName = (XP_UCHAR*)NULL; /* so we don't sent the bytes */
} /* makeSendableGICopy */

static void
server_sendInitialMessage( ServerCtxt* server )
{
    XP_U16 ii;
    XP_U16 deviceIndex;
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    CurGameInfo localGI;
    XP_U32 gameID = server->vol.gi->gameID;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion = server->nv.streamVersion;
#endif

    XP_ASSERT( server->nv.nDevices > 1 );
    for ( deviceIndex = 1; deviceIndex < server->nv.nDevices;
          ++deviceIndex ) {
        RemoteAddress* addr = &server->nv.addresses[deviceIndex];
        XWStreamCtxt* stream = util_makeStreamFromAddr( server->vol.util, 
                                                        addr->channelNo );
        DictionaryCtxt* dict = model_getDictionary(model);
        XP_ASSERT( !!stream );
        stream_open( stream );
        writeProto( server, stream, XWPROTO_CLIENT_SETUP );

#ifdef STREAM_VERS_BIGBOARD
        XP_ASSERT( 0 < streamVersion );
        stream_putU8( stream, streamVersion );
#else
        stream_putU8( stream, CUR_STREAM_VERS );
#endif

        XP_LOGF( "putting gameID %lx into msg", gameID );
        stream_putU32( stream, gameID );

        makeSendableGICopy( server, &localGI, deviceIndex );
        gi_writeToStream( stream, &localGI );

        dict_writeToStream( dict, stream );
#ifdef STREAM_VERS_BIGBOARD
        if ( STREAM_VERS_DICTNAME <= streamVersion ) {
            stringToStream( stream, dict_getShortName(dict) );
            stringToStream( stream, dict_getMd5Sum(dict) );
        }
#endif
        /* send tiles currently in tray */
        for ( ii = 0; ii < nPlayers; ++ii ) {
            model_trayToStream( model, ii, stream );
        }

        stream_destroy( stream );
    }

    /* Set after messages are built so their connID will be 0, but all
       non-initial messages will have a non-0 connID. */
    comms_setConnID( server->vol.comms, gameID );
} /* server_sendInitialMessage */
#endif

static void
freeBWI( MPFORMAL BadWordInfo* bwi )
{
    XP_U16 nWords = bwi->nWords;

    XP_FREEP( mpool, &bwi->dictName );
    while ( nWords-- ) {
        XP_FREEP( mpool, &bwi->words[nWords] );
    }

    bwi->nWords = 0;
} /* freeBWI */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
bwiToStream( XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = bwi->nWords;
    const XP_UCHAR** sp;

    stream_putBits( stream, 4, nWords );
    if ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) ) {
        stringToStream( stream, bwi->dictName );
    }
    for ( sp = bwi->words; nWords > 0; --nWords, ++sp ) {
        stringToStream( stream, *sp );
    }

} /* bwiToStream */

static void
bwiFromStream( MPFORMAL XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = stream_getBits( stream, 4 );
    const XP_UCHAR** sp = bwi->words;

    bwi->nWords = nWords;
    bwi->dictName = ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) )
        ? stringFromStream( mpool, stream ) : NULL;
    for ( sp = bwi->words; nWords; ++sp, --nWords ) {
        *sp = (const XP_UCHAR*)stringFromStream( mpool, stream );
    }
} /* bwiFromStream */

#ifdef DEBUG
#define caseStr(var, s) case s: var = #s; break;
static void
printCode(char* intro, XW_Proto code)
{
    char* str = (char*)NULL;

    switch( code ) {
        caseStr( str, XWPROTO_ERROR );
        caseStr( str, XWPROTO_CHAT );
        caseStr( str, XWPROTO_DEVICE_REGISTRATION );
        caseStr( str, XWPROTO_CLIENT_SETUP );
        caseStr( str, XWPROTO_MOVEMADE_INFO_CLIENT );
        caseStr( str, XWPROTO_MOVEMADE_INFO_SERVER );
        caseStr( str, XWPROTO_UNDO_INFO_CLIENT );
        caseStr( str, XWPROTO_UNDO_INFO_SERVER );
        caseStr( str, XWPROTO_BADWORD_INFO );
        caseStr( str, XWPROTO_MOVE_CONFIRM );
        caseStr( str, XWPROTO_CLIENT_REQ_END_GAME );
        caseStr( str, XWPROTO_END_GAME );
        caseStr( str, XWPROTO_NEW_PROTO );
    }

    XP_STATUSF( "\t%s for %s", intro, str );
} /* printCode */
#undef caseStr
#else
#define printCode(intro, code)
#endif

static XWStreamCtxt*
messageStreamWithHeader( ServerCtxt* server, XP_U16 devIndex, XW_Proto code )
{
    XWStreamCtxt* stream;
    XP_PlayerAddr channelNo = server->nv.addresses[devIndex].channelNo;

    printCode("making", code);

    stream = util_makeStreamFromAddr( server->vol.util, channelNo );
    stream_open( stream );
    writeProto( server, stream, code );

    return stream;
} /* messageStreamWithHeader */

/* Check that the message belongs to this game, whatever.  Pull out the data
 * put in by messageStreamWithHeader -- except for the code, which will have
 * already come out.
 */
static XP_Bool
readStreamHeader( ServerCtxt* XP_UNUSED(server), 
                  XWStreamCtxt* XP_UNUSED(stream) )
{
    return XP_TRUE;
} /* readStreamHeader */

static void
sendBadWordMsgs( ServerCtxt* server )
{
    XP_ASSERT( server->illegalWordInfo.nWords > 0 );

    if ( server->illegalWordInfo.nWords > 0 ) { /* fail gracefully */
        XWStreamCtxt* stream = 
            messageStreamWithHeader( server, server->lastMoveSource, 
                                     XWPROTO_BADWORD_INFO );
        stream_putBits( stream, PLAYERNUM_NBITS, server->nv.currentTurn );

        bwiToStream( stream, &server->illegalWordInfo );

        stream_destroy( stream );

        freeBWI( MPPARM(server->mpool) &server->illegalWordInfo );
    }
} /* sendBadWordMsgs */
#endif

static void
badWordMoveUndoAndTellUser( ServerCtxt* server, BadWordInfo* bwi )
{
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    /* I'm the server.  I need to send a message to everybody else telling
       them the move's rejected.  Then undo it on this side, replacing it with
       model_commitRejectedPhony(); */

    model_rejectPreviousMove( model, server->pool, &turn );

    util_warnIllegalWord( server->vol.util, bwi, turn, XP_TRUE );
} /* badWordMoveUndoAndTellUser */

EngineCtxt*
server_getEngineFor( ServerCtxt* server, XP_U16 playerNum )
{
    ServerPlayer* player;
    EngineCtxt* engine;

    XP_ASSERT( playerNum < server->vol.gi->nPlayers );

    player = &server->players[playerNum];
    engine = player->engine;
    if ( !engine && server->vol.gi->players[playerNum].isLocal ) {
        engine = engine_make( MPPARM(server->mpool)
                              server->vol.util );
        player->engine = engine;
    }

    return engine;
} /* server_getEngineFor */

#ifdef XWFEATURE_CHANGEDICT
void
server_resetEngines( ServerCtxt* server )
{
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    while ( 0 < nPlayers-- ) {
        server_resetEngine( server, nPlayers );
    }
}
#endif

void
server_resetEngine( ServerCtxt* server, XP_U16 playerNum )
{
    ServerPlayer* player = &server->players[playerNum];
    if ( !!player->engine ) {
        XP_ASSERT( player->deviceIndex == 0 );
        engine_reset( player->engine );
    }
} /* server_resetEngine */

static void
resetEngines( ServerCtxt* server )
{
    XP_U16 ii;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    for ( ii = 0; ii < nPlayers; ++ii ) {
        server_resetEngine( server, ii );
    }
} /* resetEngines */

#ifdef TEST_ROBOT_TRADE
static void
makeNotAVowel( ServerCtxt* server, Tile* newTile )
{
    char face[4];
    Tile tile = *newTile;
    PoolContext* pool = server->pool;
    TrayTileSet set;
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
    XP_U8 numGot = 1;

    set.nTiles = 1;

    for ( ; ; ) {

        XP_U16 len = dict_tilesToString( dict, &tile, 1, face );

        if ( len == 1 ) {
            switch ( face[0] ) {
            case 'A':
            case 'E':
            case 'I':
            case 'O':
            case 'U':
            case '_':
                break;
            default:
                *newTile = tile;
                return;
            }
        }

        set.tiles[0] = tile;
        pool_replaceTiles( pool, &set );

        pool_requestTiles( pool, &tile, &numGot );

    }

} /* makeNotAVowel */
#endif

static void
curTrayAsTexts( ServerCtxt* server, XP_U16 turn, const TrayTileSet* notInTray,
                XP_U16* nUsedP, const XP_UCHAR** curTrayText )
{
    const TrayTileSet* tileSet = model_getPlayerTiles( server->vol.model, turn );
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
    XP_U16 ii, jj;
    XP_U16 size = tileSet->nTiles;
    const Tile* tp = tileSet->tiles;
    XP_U16 tradedTiles[MAX_TRAY_TILES];
    XP_U16 nNotInTray = 0;
    XP_U16 nUsed = 0;

    XP_MEMSET( tradedTiles, 0xFF, sizeof(tradedTiles) );
    if ( !!notInTray ) {
        const Tile* tp = notInTray->tiles;
        nNotInTray = notInTray->nTiles;
        for ( ii = 0; ii < nNotInTray; ++ii ) {
            tradedTiles[ii] = *tp++;
        }
    }

    for ( ii = 0; ii < size; ++ii ) {
        Tile tile = *tp++;
        XP_Bool toBeTraded = XP_FALSE;

        for ( jj = 0; jj < nNotInTray; ++jj ) {
            if ( tradedTiles[jj] == tile ) {
                tradedTiles[jj] = 0xFFFF;
                toBeTraded = XP_TRUE;
                break;
            }
        }

        if ( !toBeTraded ) {
            curTrayText[nUsed++] = dict_getTileString( dict, tile );
        }
    }
    *nUsedP = nUsed;
} /* curTrayAsTexts */

/* Get tiles for one user.  If picking is available, let user pick until
 * cancels.  Otherwise, and after cancel, pick for 'im.
 */
static void
fetchTiles( ServerCtxt* server, XP_U16 playerNum, XP_U16 nToFetch, 
            const TrayTileSet* tradedTiles, TrayTileSet* resultTiles )
{
    XP_Bool ask;
    XP_U16 nSoFar = 0;
    XP_U16 nLeft;
    PoolContext* pool = server->pool;
    TrayTileSet oneTile;
    PickInfo pi;
    const XP_UCHAR* curTray[MAX_TRAY_TILES];
#ifdef FEATURE_TRAY_EDIT
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
#endif

    XP_ASSERT( !!pool );
#ifdef FEATURE_TRAY_EDIT
    ask = server->vol.gi->allowPickTiles
        && !LP_IS_ROBOT(&server->vol.gi->players[playerNum]);
#else
    ask = XP_FALSE;
#endif
    
    nLeft = pool_getNTilesLeft( pool );
    if ( nLeft < nToFetch ) {
        nToFetch = nLeft;
    }

    oneTile.nTiles = 1;

    pi.nTotal = nToFetch;
    pi.thisPick = 0;
    pi.curTiles = curTray;

    curTrayAsTexts( server, playerNum, tradedTiles, &pi.nCurTiles, curTray );

#ifdef FEATURE_TRAY_EDIT        /* good compiler would note ask==0, but... */
    /* First ask until cancelled */
    for ( nSoFar = 0; ask && nSoFar < nToFetch;  ) {
        const XP_UCHAR* texts[MAX_UNIQUE_TILES];
        Tile tiles[MAX_UNIQUE_TILES];
        XP_S16 chosen;
        XP_U16 nUsed = MAX_UNIQUE_TILES;

        model_packTilesUtil( server->vol.model, pool,
                             XP_TRUE, &nUsed, texts, tiles );

        chosen = util_userPickTileTray( server->vol.util, &pi, playerNum,
                                        texts, nUsed );

        if ( chosen == PICKER_PICKALL ) {
            ask = XP_FALSE;
        } else if ( chosen == PICKER_BACKUP ) {
            if ( nSoFar > 0 ) {
                TrayTileSet tiles;
                tiles.nTiles = 1;
                tiles.tiles[0] = resultTiles->tiles[--nSoFar];
                pool_replaceTiles( server->pool, &tiles );
                --pi.nCurTiles;
                --pi.thisPick;
            }
        } else {
            Tile tile = tiles[chosen];
            oneTile.tiles[0] = tile;
            pool_removeTiles( pool, &oneTile );
            curTray[pi.nCurTiles++] = dict_getTileString( dict, tile );
            resultTiles->tiles[nSoFar++] = tile;
            ++pi.thisPick;
        }
    }
#endif

    /* Then fetch the rest without asking */
    if ( nSoFar < nToFetch ) {
        XP_U8 nLeft = nToFetch - nSoFar;
        Tile tiles[MAX_TRAY_TILES];

        pool_requestTiles( pool, tiles, &nLeft );

        XP_MEMCPY( &resultTiles->tiles[nSoFar], tiles, 
                   nLeft * sizeof(resultTiles->tiles[0]) );
        nSoFar += nLeft;
    }
   
    XP_ASSERT( nSoFar < 0x00FF );
    resultTiles->nTiles = (XP_U8)nSoFar;
} /* fetchTiles */

static void
assignTilesToAll( ServerCtxt* server )
{
    XP_U16 numAssigned;
    XP_U16 ii;
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    XP_ASSERT( server->vol.gi->serverRole != SERVER_ISCLIENT );
    XP_ASSERT( model_getDictionary(model) != NULL );
    if ( server->pool == NULL ) {
        server->pool = pool_make( MPPARM_NOCOMMA(server->mpool) );
        XP_STATUSF( "initing pool" );
        pool_initFromDict( server->pool, model_getDictionary(model));
    }

    XP_STATUSF( "assignTilesToAll" );

    model_setNPlayers( model, nPlayers );

    numAssigned = pool_getNTilesLeft( server->pool ) / nPlayers;
    if ( numAssigned > MAX_TRAY_TILES ) {
        numAssigned = MAX_TRAY_TILES;
    }
    for ( ii = 0; ii < nPlayers; ++ii ) {
        TrayTileSet newTiles;
        fetchTiles( server, ii, numAssigned, NULL, &newTiles );
        model_assignPlayerTiles( model, ii, &newTiles );
        sortTilesIf( server, ii );
    }

} /* assignTilesToAll */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
getPlayerTime( ServerCtxt* server, XWStreamCtxt* stream, XP_U16 turn )
{
    CurGameInfo* gi = server->vol.gi;

    if ( gi->timerEnabled ) {
        XP_U16 secondsUsed = stream_getU16( stream );

        gi->players[turn].secondsUsed = secondsUsed;
    }
} /* getPlayerTime */
#endif

static void
nextTurn( ServerCtxt* server, XP_S16 nxtTurn )
{
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U16 playerTilesLeft;
    XP_S16 currentTurn = server->nv.currentTurn;
    XP_Bool moreToDo = XP_FALSE;

    if ( nxtTurn == PICK_NEXT ) {
        XP_ASSERT( currentTurn >= 0 );
        playerTilesLeft = model_getNumTilesTotal(server->vol.model, 
                                                 currentTurn);
        nxtTurn = (currentTurn+1) % nPlayers;
    } else {
        /* We're doing an undo, and so won't bother figuring out who the
           previous turn was or how many tiles he had: it's a sure thing he
           "has" enough to be allowed to take the turn just undone. */
        playerTilesLeft = MAX_TRAY_TILES;
    }
    SETSTATE( server, XWSTATE_INTURN ); /* even if game over, if undoing */

    if ( (playerTilesLeft > 0) && tileCountsOk(server) && NPASSES_OK(server) ){

        setTurn( server, nxtTurn );

    } else {
        /* I discover that the game should end.  If I'm the client,
           though, should I wait for the server to deduce this and send
           out a message?  I think so.  Yes, but be sure not to compute
           another PASS move.  Just don't do anything! */
        if ( server->vol.gi->serverRole != SERVER_ISCLIENT ) {
            SETSTATE( server, XWSTATE_NEEDSEND_ENDGAME );
            moreToDo = XP_TRUE;
        } else {
            XP_LOGF( "%s: Doing nothing; waiting for server to end game", 
                     __func__ );
            /* I'm the client. Do ++nothing++. */
        }
    }

    if ( server->vol.showPrevMove ) {
        server->vol.showPrevMove = XP_FALSE;
        if ( server->nv.showRobotScores ) {
            server->nv.stateAfterShow = server->nv.gameState;
            SETSTATE( server, XWSTATE_NEED_SHOWSCORE );
            moreToDo = XP_TRUE;
        }
    }

    /* It's safer, if perhaps not always necessary, to do this here. */
    resetEngines( server );

    XP_Bool androidMove = model_androidMove( server->vol.model );
    XP_S16 nMoves = model_getNMoves( server->vol.model );
    XP_Bool delayUpdate = androidMove && (1 < nMoves);

    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );
    callTurnChangeListener( server );
    util_turnChanged( server->vol.util, server->nv.currentTurn, delayUpdate );

    if ( robotMovePending(server) && !POSTPONEROBOTMOVE(server) ) {
        moreToDo = XP_TRUE;
    }

    if ( moreToDo ) {
        util_requestTime( server->vol.util );
    }
} /* nextTurn */

void
server_setTurnChangeListener( ServerCtxt* server, TurnChangeListener tl,
                              void* data )
{
    server->vol.turnChangeListener = tl;
    server->vol.turnChangeData = data;
} /* server_setTurnChangeListener */

void
server_setGameOverListener( ServerCtxt* server, GameOverListener gol,
                            void* data )
{
    server->vol.gameOverListener = gol;
    server->vol.gameOverData = data;
} /* server_setGameOverListener */

static XP_Bool
storeBadWords( const XP_UCHAR* word, XP_Bool isLegal,
               const DictionaryCtxt* dict,
#ifdef XWFEATURE_BOARDWORDS
               const MoveInfo* XP_UNUSED(movei), XP_U16 XP_UNUSED(start), 
               XP_U16 XP_UNUSED(end), 
#endif
               void* closure )
{
    if ( !isLegal ) {
        ServerCtxt* server = (ServerCtxt*)closure;
        const XP_UCHAR* name = dict_getShortName( dict );

        XP_LOGF( "storeBadWords called with \"%s\" (name=%s)", word, name );
        if ( NULL == server->illegalWordInfo.dictName ) {
            server->illegalWordInfo.dictName = copyString( server->mpool, name );
        }
        server->illegalWordInfo.words[server->illegalWordInfo.nWords++]
            = copyString( server->mpool, word );
    }
    return XP_TRUE;
} /* storeBadWords */

static XP_Bool
checkMoveAllowed( ServerCtxt* server, XP_U16 playerNum )
{
    CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( server->illegalWordInfo.nWords == 0 );

    if ( gi->phoniesAction == PHONIES_DISALLOW ) {
        WordNotifierInfo info;
        info.proc = storeBadWords;
        info.closure = server;
        (void)model_checkMoveLegal( server->vol.model, playerNum, 
                                    (XWStreamCtxt*)NULL, &info );
    }

    return server->illegalWordInfo.nWords == 0;
} /* checkMoveAllowed */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
sendMoveTo( ServerCtxt* server, XP_U16 devIndex, XP_U16 turn,
            XP_Bool legal, TrayTileSet* newTiles, 
            const TrayTileSet* tradedTiles ) /* null if a move, set if a trade */
{
    XWStreamCtxt* stream;
    XP_Bool isTrade = !!tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_MOVEMADE_INFO_CLIENT : XWPROTO_MOVEMADE_INFO_SERVER;

    stream = messageStreamWithHeader( server, devIndex, code );

#ifdef STREAM_VERS_BIGBOARD
    XP_U16 version = stream_getVersion( stream );
    if ( STREAM_VERS_BIGBOARD <= version ) {
        XP_ASSERT( version == server->nv.streamVersion );
        XP_U32 hash = model_getHash( server->vol.model, version );
        // XP_LOGF( "%s: adding hash %x", __func__, (unsigned int)hash );
        stream_putU32( stream, hash );
    }
#endif

    stream_putBits( stream, PLAYERNUM_NBITS, turn ); /* who made the move */

    traySetToStream( stream, newTiles );

    stream_putBits( stream, 1, isTrade );

    if ( isTrade ) {

        traySetToStream( stream, tradedTiles );

    } else {
        stream_putBits( stream, 1, legal );

        model_currentMoveToStream( server->vol.model, turn, stream );

        if ( gi->timerEnabled ) {
            stream_putU16( stream, gi->players[turn].secondsUsed );
            XP_LOGF("%s: wrote secondsUsed for player %d: %d", __func__,
                       turn, gi->players[turn].secondsUsed );
        } else {
            XP_ASSERT( gi->players[turn].secondsUsed == 0 );
        }

        if ( !legal ) {
            XP_ASSERT( server->illegalWordInfo.nWords > 0 );
            stream_putBits( stream, PLAYERNUM_NBITS, turn );
            bwiToStream( stream, &server->illegalWordInfo );
        }
    }

    stream_destroy( stream );
} /* sendMoveTo */

static XP_Bool
readMoveInfo( ServerCtxt* server, XWStreamCtxt* stream,
              XP_U16* whoMovedP, XP_Bool* isTradeP,
              TrayTileSet* newTiles, TrayTileSet* tradedTiles, 
              XP_Bool* legalP )
{
    XP_Bool success = XP_TRUE;
    XP_Bool legalMove = XP_TRUE;
    XP_Bool isTrade;

#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_VERS_BIGBOARD <= stream_getVersion( stream ) ) {
        XP_U32 hashReceived = stream_getU32( stream );
        success = model_hashMatches( server->vol.model, hashReceived );
        if ( !success ) {
            XP_LOGF( "%s: hash mismatch",__func__);
        }
    }
#endif
    if ( success ) {
        XP_U16 whoMoved = stream_getBits( stream, PLAYERNUM_NBITS );
        traySetFromStream( stream, newTiles );
        success = pool_containsTiles( server->pool, newTiles );
        if ( success ) {
            isTrade = stream_getBits( stream, 1 );

            if ( isTrade ) {
                traySetFromStream( stream, tradedTiles );
                XP_LOGF( "%s: got trade of %d tiles", __func__, 
                         tradedTiles->nTiles );
            } else {
                legalMove = stream_getBits( stream, 1 );
                success = model_makeTurnFromStream( server->vol.model, 
                                                    whoMoved, stream );
                getPlayerTime( server, stream, whoMoved );
            }

            if ( success ) {
                pool_removeTiles( server->pool, newTiles );

                *whoMovedP = whoMoved;
                *isTradeP = isTrade;
                *legalP = legalMove;
            }
        }
    }
    return success;
} /* readMoveInfo */

static void
sendMoveToClientsExcept( ServerCtxt* server, XP_U16 whoMoved, XP_Bool legal,
                         TrayTileSet* newTiles, const TrayTileSet* tradedTiles,
                         XP_U16 skip )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendMoveTo( server, devIndex, whoMoved, legal, 
                        newTiles, tradedTiles );
        }
    }
} /* sendMoveToClientsExcept */

static XWStreamCtxt*
makeTradeReportIf( ServerCtxt* server, const TrayTileSet* tradedTiles )
{
    XWStreamCtxt* stream = NULL;
    if ( server->nv.showRobotScores ) {
        XP_UCHAR tradeBuf[64];
        const XP_UCHAR* tradeStr = util_getUserString( server->vol.util,
                                                       STRD_ROBOT_TRADED );
        XP_SNPRINTF( tradeBuf, sizeof(tradeBuf), tradeStr, 
                     tradedTiles->nTiles );
        stream = mkServerStream( server );
        stream_catString( stream, tradeBuf );
    }
    return stream;
} /* makeTradeReportIf */

static XWStreamCtxt*
makeMoveReportIf( ServerCtxt* server, XWStreamCtxt** wordsStream )
{
    XWStreamCtxt* stream = NULL;
    if ( server->nv.showRobotScores ) {
        ModelCtxt* model = server->vol.model;
        stream = mkServerStream( server );
        *wordsStream = mkServerStream( server );
        WordNotifierInfo* ni = model_initWordCounter( model, *wordsStream );
        (void)model_checkMoveLegal( model, server->nv.currentTurn, stream, ni );
    }
    return stream;
} /* makeMoveReportIf */

/* Client is reporting a move made, complete with new tiles and time taken by
 * the player.  Update the model with that information as a tentative move,
 * then sent info about it to all the clients, and finally commit the move
 * here.
 */
static XP_Bool
reflectMoveAndInform( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool success;
    ModelCtxt* model = server->vol.model;
    XP_U16 whoMoved;
    XP_U16 nTilesMoved = 0; /* trade case */
    XP_Bool isTrade;
    XP_Bool isLegalMove;
    XP_Bool doRequest = XP_FALSE;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 sourceClientIndex = 
        getIndexForDevice( server, stream_getAddress( stream ) );
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    XP_ASSERT( gi->serverRole == SERVER_ISSERVER );

    success = readMoveInfo( server, stream, &whoMoved, &isTrade, &newTiles,
                            &tradedTiles, &isLegalMove ); /* modifies model */
    XP_ASSERT( !success || isLegalMove ); /* client should always report as true */
    isLegalMove = XP_TRUE;

    if ( success ) {
        if ( isTrade ) {

            sendMoveToClientsExcept( server, whoMoved, XP_TRUE, &newTiles, 
                                     &tradedTiles, sourceClientIndex );

            model_makeTileTrade( model, whoMoved,
                                 &tradedTiles, &newTiles );
            pool_replaceTiles( server->pool, &tradedTiles );

            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( server, &tradedTiles );

        } else {
            nTilesMoved = model_getCurrentMoveCount( model, whoMoved );
            isLegalMove = (nTilesMoved == 0)
                || checkMoveAllowed( server, whoMoved );

            /* I don't think this will work if there are more than two devices in
               a palm game; need to change state and get out of here before
               returning to send additional messages.  PENDING(ehouse) */
            sendMoveToClientsExcept( server, whoMoved, isLegalMove, &newTiles, 
                                     (TrayTileSet*)NULL, sourceClientIndex );

            server->vol.showPrevMove = XP_TRUE;
            if ( isLegalMove ) {
                mvStream = makeMoveReportIf( server, &wordsStream );
            }

            success = model_commitTurn( model, whoMoved, &newTiles );
            resetEngines( server );
        }

        if ( success && isLegalMove ) {
            XP_U16 nTilesLeft = model_getNumTilesTotal( model, whoMoved );

            if ( (gi->phoniesAction == PHONIES_DISALLOW) && (nTilesMoved > 0) ) {
                server->lastMoveSource = sourceClientIndex;
                SETSTATE( server, XWSTATE_MOVE_CONFIRM_MUSTSEND );
                doRequest = XP_TRUE;
            } else if ( nTilesLeft > 0 ) {
                nextTurn( server, PICK_NEXT );
            } else {
                SETSTATE(server, XWSTATE_NEEDSEND_ENDGAME );
                doRequest = XP_TRUE;
            }

            if ( !!mvStream ) {
                XP_ASSERT( !server->nv.prevMoveStream );
                server->nv.prevMoveStream = mvStream;
                XP_ASSERT( !server->nv.prevWordsStream );
                server->nv.prevWordsStream = wordsStream;
            }
        } else {
            /* The client from which the move came still needs to be told.  But we
               can't send a message now since we're burried in a message handler.
               (Palm, at least, won't manage.)  So set up state to tell that
               client again in a minute. */
            SETSTATE( server, XWSTATE_NEEDSEND_BADWORD_INFO );
            server->lastMoveSource = sourceClientIndex;
            doRequest = XP_TRUE;
        }

        if ( doRequest ) {
            util_requestTime( server->vol.util );
        }
    }
    return success;
} /* reflectMoveAndInform */

static XP_Bool
reflectMove( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool moveOk;
    XP_Bool isTrade;
    XP_Bool isLegal;
    XP_U16 whoMoved;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    ModelCtxt* model = server->vol.model;
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    moveOk = XWSTATE_INTURN == server->nv.gameState;
    if ( moveOk ) {
        moveOk = readMoveInfo( server, stream, &whoMoved, &isTrade, &newTiles, 
                               &tradedTiles, &isLegal ); /* modifies model */
    }
    if ( moveOk ) {
        if ( isTrade ) {
            model_makeTileTrade( model, whoMoved, &tradedTiles, &newTiles );
            pool_replaceTiles( server->pool, &tradedTiles );

            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( server, &tradedTiles );
        } else {
            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeMoveReportIf( server, &wordsStream );
            model_commitTurn( model, whoMoved, &newTiles );
        }

        if ( !!mvStream ) {
            XP_ASSERT( !server->nv.prevMoveStream );
            server->nv.prevMoveStream = mvStream;
            XP_ASSERT( !server->nv.prevWordsStream );
            server->nv.prevWordsStream = wordsStream;
        }

        resetEngines( server );

        if ( !isLegal ) {
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
            handleIllegalWord( server, stream );
        }
    } else {
        XP_LOGF( "%s: dropping move: state=%s", __func__,
                 getStateStr(server->nv.gameState ) );
    }
    return moveOk;
} /* reflectMove */
#endif /* XWFEATURE_STANDALONE_ONLY */

/* A local player is done with his turn.  If a client device, broadcast
 * the move to the server (after which a response should be coming soon.)
 * If the server, then that step can be skipped: go straight to doing what
 * the server does after learning of a move on a remote device.
 *
 * Second cut.  Whether server or client, be responsible for checking the
 * basic legality of the move and taking new tiles out of the pool.  If
 * client, send the move and new tiles to the server.  If the server, fall
 * back to what will do after hearing from client: tell everybody who doesn't
 * already know what's happened: move and new tiles together.
 *
 * What about phonies when DISALLOW is set?  The server's ultimately
 * responsible, since it has the dictionary, so the client can't check.  So
 * when server, check and send move together with a flag indicating legality.
 * Client is responsible for first posting the move to the model and then
 * undoing it.  When client, send the move and go into a state waiting to hear
 * if it was legal -- but only if DISALLOW is set.
 */
XP_Bool
server_commitMove( ServerCtxt* server )
{
    XP_S16 turn = server->nv.currentTurn;
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    TrayTileSet newTiles;
    XP_U16 nTilesMoved;
    XP_Bool isLegalMove = XP_TRUE;
    XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;

#ifdef DEBUG
    if ( LP_IS_ROBOT( &gi->players[turn] ) ) {
        XP_ASSERT( model_checkMoveLegal( model, turn, (XWStreamCtxt*)NULL,
                                         (WordNotifierInfo*)NULL ) );
    }
#endif

    /* commit the move.  get new tiles.  if server, send to everybody.
       if client, send to server.  */
    XP_ASSERT( turn >= 0 );

    nTilesMoved = model_getCurrentMoveCount( model, turn );
    fetchTiles( server, turn, nTilesMoved, NULL, &newTiles );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( isClient ) {
        /* just send to server */
        sendMoveTo( server, SERVER_DEVICE, turn, XP_TRUE, &newTiles,
                    (TrayTileSet*)NULL );
    } else {
        isLegalMove = checkMoveAllowed( server, turn );
        sendMoveToClientsExcept( server, turn, isLegalMove, &newTiles,
                                 (TrayTileSet*)NULL, SERVER_DEVICE );
    }
#else
    isLegalMove = checkMoveAllowed( server, turn );
#endif

    model_commitTurn( model, turn, &newTiles );
    sortTilesIf( server, turn );

    if ( !isLegalMove && !isClient ) {
        badWordMoveUndoAndTellUser( server, &server->illegalWordInfo );
        /* It's ok to free these guys.  I'm the server, and the move was made
           here, so I've notified all clients already by setting the flag (and
           passing the word) in sendMoveToClientsExcept. */
        freeBWI( MPPARM(server->mpool) &server->illegalWordInfo );
    }

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if (isClient && (gi->phoniesAction == PHONIES_DISALLOW)
               && nTilesMoved > 0 ) {
        SETSTATE( server, XWSTATE_MOVE_CONFIRM_WAIT );
#endif
    } else {
        nextTurn( server, PICK_NEXT );
    }
    
    return XP_TRUE;
} /* server_commitMove */

XP_Bool
server_commitTrade( ServerCtxt* server, const TrayTileSet* oldTiles )
{
    TrayTileSet newTiles;
    XP_U16 turn = server->nv.currentTurn;

    fetchTiles( server, turn, oldTiles->nTiles, oldTiles, &newTiles );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        /* just send to server */
        sendMoveTo(server, SERVER_DEVICE, turn, XP_TRUE, &newTiles, oldTiles);
    } else {
        sendMoveToClientsExcept( server, turn, XP_TRUE, &newTiles, oldTiles, 
                                 SERVER_DEVICE );
    }
#endif

    pool_replaceTiles( server->pool, oldTiles );
    XP_ASSERT( turn == server->nv.currentTurn );
    model_makeTileTrade( server->vol.model, turn, oldTiles, &newTiles );
    sortTilesIf( server, turn );

    nextTurn( server, PICK_NEXT );
    return XP_TRUE;
} /* server_commitTrade */

XP_S16
server_getCurrentTurn( ServerCtxt* server )
{
    return server->nv.currentTurn;
} /* server_getCurrentTurn */

XP_Bool
server_getGameIsOver( ServerCtxt* server )
{
    return server->nv.gameState == XWSTATE_GAMEOVER;
} /* server_getGameIsOver */

XP_U16
server_getMissingPlayers( const ServerCtxt* server )
{
    /* list players for which we're reserving slots that haven't shown up yet.
     * If I'm a guest and haven't received the registration message and set
     * server->nv.addresses[0].channelNo, all non-local players are missing.
     * If I'm a host, players whose deviceIndex is -1 are missing.
    */

    XP_U16 result = 0;
    XP_U16 ii;
    if ( SERVER_ISCLIENT == server->vol.gi->serverRole ) {
        if ( 0 == server->nv.addresses[0].channelNo ) {
            CurGameInfo* gi = server->vol.gi;
            const LocalPlayer* lp = gi->players;
            for ( ii = 0; ii < gi->nPlayers; ++ii ) {
                if ( !lp->isLocal ) {
                    result |= 1 << ii;
                }
                ++lp;
            }
        }
    } else {
        XP_ASSERT( SERVER_ISSERVER == server->vol.gi->serverRole );
        if ( 0 < server->nv.pendingRegistrations ) {
            XP_U16 nPlayers = server->vol.gi->nPlayers;
            const ServerPlayer* players = server->players;
            for ( ii = 0; ii < nPlayers; ++ii ) {
                if ( players->deviceIndex == UNKNOWN_DEVICE ) {
                    result |= 1 << ii;
                }
                ++players;
            }
        }
    }

    LOG_RETURNF( "%x", result );
    return result;
}

XP_U32
server_getLastMoveTime( const ServerCtxt* server )
{
    return server->nv.lastMoveTime;
}

static void
doEndGame( ServerCtxt* server, XP_S16 quitter )
{
    XP_ASSERT( quitter < server->vol.gi->nPlayers );
    SETSTATE( server, XWSTATE_GAMEOVER );
    setTurn( server, -1 );
    server->nv.quitter = quitter;

    (*server->vol.gameOverListener)( server->vol.gameOverData, quitter );
} /* doEndGame */

static void 
putQuitter( const ServerCtxt* server, XWStreamCtxt* stream, XP_S16 quitter )
{
    if ( STREAM_VERS_DICTNAME <= server->nv.streamVersion ) {
        stream_putU8( stream, quitter );
    }
}

static void
getQuitter( const ServerCtxt* server, XWStreamCtxt* stream, XP_S8* quitter )
{
    *quitter = STREAM_VERS_DICTNAME <= server->nv.streamVersion
            ? stream_getU8( stream ) : -1;
}

/* Somebody wants to end the game.
 *
 * If I'm the server, I send a END_GAME message to all clients.  If I'm a
 * client, I send the GAME_OVER_REQUEST message to the server.  If I'm the
 * server and this is called in response to a GAME_OVER_REQUEST, send the
 * GAME_OVER message to all clients including the one that requested it.
 */
static void
endGameInternal( ServerCtxt* server, GameEndReason XP_UNUSED(why), XP_S16 quitter )
{
    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );
    XP_ASSERT( quitter < server->vol.gi->nPlayers );

    if ( server->vol.gi->serverRole != SERVER_ISCLIENT ) {

#ifndef XWFEATURE_STANDALONE_ONLY
        XP_U16 devIndex;
        for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
            XWStreamCtxt* stream;
            stream = messageStreamWithHeader( server, devIndex,
                                              XWPROTO_END_GAME );
            putQuitter( server, stream, quitter );
            stream_destroy( stream );
        }
#endif
        doEndGame( server, quitter );

#ifndef XWFEATURE_STANDALONE_ONLY
    } else {
        XWStreamCtxt* stream;
        stream = messageStreamWithHeader( server, SERVER_DEVICE,
                                          XWPROTO_CLIENT_REQ_END_GAME );
        stream_destroy( stream );

        /* Do I want to change the state I'm in?  I don't think so.... */
#endif
    }
} /* endGameInternal */

void
server_endGame( ServerCtxt* server )
{
    XW_State gameState = server->nv.gameState;
    if ( gameState < XWSTATE_GAMEOVER && gameState >= XWSTATE_INTURN ) {
        endGameInternal( server, END_REASON_USER_REQUEST, server->nv.currentTurn );
    }
} /* server_endGame */

/* If game is about to end because one player's out of tiles, we don't want to
 * keep trying to move */
static XP_Bool
tileCountsOk( const ServerCtxt* server )
{
    XP_Bool maybeOver = 0 == pool_getNTilesLeft( server->pool );
    if ( maybeOver ) {
        ModelCtxt* model = server->vol.model;
        XP_U16 nPlayers = server->vol.gi->nPlayers;
        XP_Bool zeroFound = XP_FALSE;

        while ( nPlayers-- ) {
            XP_U16 count = model_getNumTilesTotal( model, nPlayers );
            if ( count == 0 ) {
                zeroFound = XP_TRUE;
                break;
            }
        }
        maybeOver = zeroFound;
    }
    return !maybeOver;
} /* tileCountsOk */

static void
setTurn( ServerCtxt* server, XP_S16 turn )
{
    if ( server->nv.currentTurn != turn ) {
        server->nv.currentTurn = turn;
        server->nv.lastMoveTime = util_getCurSeconds( server->vol.util );
        callTurnChangeListener( server );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
tellMoveWasLegal( ServerCtxt* server )
{
    XWStreamCtxt* stream;

    stream = messageStreamWithHeader( server, server->lastMoveSource,
                                      XWPROTO_MOVE_CONFIRM );
    stream_destroy( stream );
} /* tellMoveWasLegal */

static XP_Bool
handleIllegalWord( ServerCtxt* server, XWStreamCtxt* incoming )
{
    BadWordInfo bwi;

    (void)stream_getBits( incoming, PLAYERNUM_NBITS );
    bwiFromStream( MPPARM(server->mpool) incoming, &bwi );

    badWordMoveUndoAndTellUser( server, &bwi );

    freeBWI( MPPARM(server->mpool) &bwi );

    return XP_TRUE;
} /* handleIllegalWord */

static XP_Bool
handleMoveOk( ServerCtxt* server, XWStreamCtxt* XP_UNUSED(incoming) )
{
    XP_Bool accepted = XP_TRUE;
    XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
    XP_ASSERT( server->nv.gameState == XWSTATE_MOVE_CONFIRM_WAIT );

    nextTurn( server, PICK_NEXT );

    return accepted;
} /* handleMoveOk */

static void
sendUndoTo( ServerCtxt* server, XP_U16 devIndex, XP_U16 nUndone, 
            XP_U16 lastUndone )
{
    XWStreamCtxt* stream;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_UNDO_INFO_CLIENT : XWPROTO_UNDO_INFO_SERVER;

    stream = messageStreamWithHeader( server, devIndex, code );

    stream_putU16( stream, nUndone );
    stream_putU16( stream, lastUndone );

    stream_destroy( stream );
} /* sendUndoTo */

static void
sendUndoToClientsExcept( ServerCtxt* server, XP_U16 skip, 
                         XP_U16 nUndone, XP_U16 lastUndone )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendUndoTo( server, devIndex, nUndone, lastUndone );
        }
    }
} /* sendUndoToClientsExcept */

static XP_Bool
reflectUndos( ServerCtxt* server, XWStreamCtxt* stream, XW_Proto code )
{
    XP_U16 nUndone;
    XP_S16 lastUndone;
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    XP_Bool success = XP_TRUE;

    nUndone = stream_getU16( stream );
    lastUndone = stream_getU16( stream );

    success = model_undoLatestMoves( model, server->pool, nUndone, &turn, 
                                     &lastUndone );
    if ( success ) {
        sortTilesIf( server, turn );

        if ( code == XWPROTO_UNDO_INFO_CLIENT ) { /* need to inform */
            XP_U16 sourceClientIndex = 
                getIndexForDevice( server, stream_getAddress( stream ) );

            sendUndoToClientsExcept( server, sourceClientIndex, nUndone, 
                                     lastUndone );
        }

        util_informUndo( server->vol.util );
        nextTurn( server, turn );
    }

    return success;
} /* reflectUndos */
#endif

XP_Bool
server_handleUndo( ServerCtxt* server, XP_U16 limit )
{
    XP_Bool result = XP_FALSE;
    XP_U16 lastTurnUndone = 0; /* quiet compiler good */
    XP_U16 nUndone = 0;
    ModelCtxt* model;
    CurGameInfo* gi;
    XP_U16 lastUndone = 0xFFFF;

    model = server->vol.model;
    gi = server->vol.gi;
    XP_ASSERT( !!model );

    /* Undo until we find we've just undone a non-robot move.  The point is
       not to stop with a robot about to act (since that's a bit pointless.)
       The exception is that if the first move was a robot move we'll stop
       there, and it will immediately move again. */
    for ( ; ; ) {
        XP_S16 moveNum = -1; /* don't need it checked */
        if ( !model_undoLatestMoves( model, server->pool, 1, &lastTurnUndone, 
                                     &moveNum ) ) {
            break;
        }
        ++nUndone;
        XP_ASSERT( moveNum >= 0 );
        lastUndone = moveNum;
        if ( !LP_IS_ROBOT(&gi->players[lastTurnUndone]) ) {
            break;
        } else if ( 0 != limit && nUndone >= limit ) {
            break;
        }
    }

    result = nUndone > 0 ;
    if ( result ) {
#ifndef XWFEATURE_STANDALONE_ONLY
        XP_ASSERT( lastUndone != 0xFFFF );
        if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
            sendUndoTo( server, SERVER_DEVICE, nUndone, lastUndone );
        } else {
            sendUndoToClientsExcept( server, SERVER_DEVICE, nUndone, 
                                     lastUndone );
        }
#endif
        sortTilesIf( server, lastTurnUndone );
        nextTurn( server, lastTurnUndone );
    } else {
        /* I'm a bit nervous about this.  Is this the ONLY thing that cause
           nUndone to come back 0? */
        util_userError( server->vol.util, ERR_CANT_UNDO_TILEASSIGN );
    }

    return result;
} /* server_handleUndo */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
writeProto( const ServerCtxt* server, XWStreamCtxt* stream, XW_Proto proto )
{
#ifdef STREAM_VERS_BIGBOARD
    XP_ASSERT( server->nv.streamVersion > 0 );
    if ( STREAM_SAVE_PREVWORDS < server->nv.streamVersion ) {
        stream_putBits( stream, XWPROTO_NBITS, XWPROTO_NEW_PROTO );
        stream_putBits( stream, 8, server->nv.streamVersion );
    }
    stream_setVersion( stream, server->nv.streamVersion );
#else
    XP_USE(server);
#endif
    stream_putBits( stream, XWPROTO_NBITS, proto );
}

static XW_Proto
readProto( ServerCtxt* server, XWStreamCtxt* stream )
{
    XW_Proto proto = (XW_Proto)stream_getBits( stream, XWPROTO_NBITS );
    XP_U8 version = STREAM_SAVE_PREVWORDS; /* version prior to fmt change */
#ifdef STREAM_VERS_BIGBOARD
    if ( XWPROTO_NEW_PROTO == proto ) {
        version = stream_getBits( stream, 8 );
        proto = (XW_Proto)stream_getBits( stream, XWPROTO_NBITS );
    }
    server->nv.streamVersion = version;
#else
    XP_USE(server);
#endif
    stream_setVersion( stream, version );
    return proto;
}

XP_Bool
server_receiveMessage( ServerCtxt* server, XWStreamCtxt* incoming )
{
    XP_Bool accepted = XP_FALSE;
    XW_Proto code = readProto( server, incoming );

    printCode( "Receiving", code );

    if ( code == XWPROTO_DEVICE_REGISTRATION && amServer(server) ) {
        /* This message is special: doesn't have the header that's possible
           once the game's in progress and communication's been
           established. */
        XP_LOGF( "%s: somebody's registering!!!", __func__ );
        accepted = handleRegistrationMsg( server, incoming );

    } else if ( code == XWPROTO_CLIENT_SETUP && !amServer( server ) ) {

        XP_STATUSF( "client got XWPROTO_CLIENT_SETUP" );
        XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
        accepted = client_readInitialMessage( server, incoming );

#ifdef XWFEATURE_CHAT
    } else if ( code == XWPROTO_CHAT ) {
        XP_UCHAR* msg = stringFromStream( server->mpool, incoming );
        if ( server->vol.gi->serverRole == SERVER_ISSERVER ) {
            XP_U16 sourceClientIndex = 
                getIndexForDevice( server, stream_getAddress( incoming ) );
            sendChatToClientsExcept( server, sourceClientIndex, msg );
        }
        util_showChat( server->vol.util, msg );
        XP_FREE( server->mpool, msg );
#endif
    } else if ( readStreamHeader( server, incoming ) ) {
        XP_S8 quitter;
        switch( code ) {
/*         case XWPROTO_MOVEMADE_INFO: */
/*             accepted = client_reflectMoveMade( server, incoming ); */
/*             if ( accepted ) { */
/*                 nextTurn( server ); */
/*             } */
/*             break; */
/*         case XWPROTO_TRADEMADE_INFO: */
/*             accepted = client_reflectTradeMade( server, incoming ); */
/*             if ( accepted ) { */
/*                 nextTurn( server ); */
/*             } */
/*             break; */
/*         case XWPROTO_CLIENT_MOVE_INFO: */
/*             accepted = handleClientMoved( server, incoming ); */
/*             break; */
/*         case XWPROTO_CLIENT_TRADE_INFO: */
/*             accepted = handleClientTraded( server, incoming ); */
/*             break; */

        case XWPROTO_MOVEMADE_INFO_CLIENT: /* client is reporting a move */
            accepted = (XWSTATE_INTURN == server->nv.gameState)
                && reflectMoveAndInform( server, incoming );
            break;

        case XWPROTO_MOVEMADE_INFO_SERVER: /* server telling me about a move */
            XP_ASSERT( SERVER_ISCLIENT == server->vol.gi->serverRole );
            accepted = reflectMove( server, incoming );
            if ( accepted ) {
                nextTurn( server, PICK_NEXT );
            }
            break;

        case XWPROTO_UNDO_INFO_CLIENT:
        case XWPROTO_UNDO_INFO_SERVER:
            accepted = reflectUndos( server, incoming, code );
            /* nextTurn is called by reflectUndos */
            break;

        case XWPROTO_BADWORD_INFO:
            accepted = handleIllegalWord( server, incoming );
            if ( accepted && server->nv.gameState != XWSTATE_GAMEOVER ) {
                nextTurn( server, PICK_NEXT );
            }
            break;

        case XWPROTO_MOVE_CONFIRM:
            accepted = handleMoveOk( server, incoming );
            break;

        case XWPROTO_CLIENT_REQ_END_GAME:
            getQuitter( server, incoming, &quitter );
            endGameInternal( server, END_REASON_USER_REQUEST, quitter );
            accepted = XP_TRUE;
            break;
        case XWPROTO_END_GAME:
            getQuitter( server, incoming, &quitter );
            doEndGame( server, quitter );
            accepted = XP_TRUE;
            break;
        default:
            XP_WARNF( "%s: Unknown code on incoming message: %d\n", 
                      __func__, code );
            break;
        } /* switch */
    }

    stream_close( incoming );
    return accepted;
} /* server_receiveMessage */
#endif

void 
server_formatDictCounts( ServerCtxt* server, XWStreamCtxt* stream,
                         XP_U16 nCols )
{
    DictionaryCtxt* dict;
    Tile tile;
    XP_U16 nChars, nPrinted;
    XP_UCHAR buf[48];
    const XP_UCHAR* fmt = util_getUserString( server->vol.util, 
                                              STRS_VALUES_HEADER );
    const XP_UCHAR* langName;

    XP_ASSERT( !!server->vol.model );

    dict = model_getDictionary( server->vol.model );
    langName = dict_getLangName( dict );
    XP_SNPRINTF( buf, sizeof(buf), fmt, langName );
    stream_catString( stream, buf );

    nChars = dict_numTileFaces( dict );

    for ( tile = 0, nPrinted = 0; ; ) {
        XP_UCHAR buf[128];
        XP_U16 count, value;

        count = dict_numTiles( dict, tile );

        if ( count > 0 ) {
            const XP_UCHAR* face = NULL;
            XP_UCHAR faces[48] = {0};
            XP_U16 len = 0;
            for ( ; ; ) {
                face = dict_getNextTileString( dict, tile, face );
                if ( !face ) {
                    break;
                }
                const XP_UCHAR* fmt = len == 0? "%s" : ",%s";
                len += XP_SNPRINTF( faces + len, sizeof(faces) - len, fmt, face );
            }
            value = dict_getTileValue( dict, tile );

            XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%s: %d/%d", 
                         faces, count, value );
            stream_catString( stream, buf );
        }

        if ( ++tile >= nChars ) {
            break;
        } else if ( count > 0 ) {
            if ( ++nPrinted % nCols == 0 ) {
                stream_catString( stream, XP_CR );
            } else {
                stream_catString( stream, (void*)"   " );
            }
        }
    }
} /* server_formatDictCounts */

/* Print the faces of all tiles left in the pool, including those currently in
 * trays !unless! player is >= 0, in which case his tiles get removed from the
 * pool.  The idea is to show him what tiles are left in play.
 */
void
server_formatRemainingTiles( ServerCtxt* server, XWStreamCtxt* stream,
                             XP_S16 XP_UNUSED(player) )
{
    PoolContext* pool = server->pool;
    if ( !!pool ) {
        XP_UCHAR buf[48];
        DictionaryCtxt* dict = model_getDictionary( server->vol.model );
        Tile tile;
        XP_U16 nChars = dict_numTileFaces( dict );
        XP_U16 offset;
        XP_U16 counts[MAX_UNIQUE_TILES+1]; /* 1 for the blank */
        XP_U16 nLeft = pool_getNTilesLeft( pool );
        XP_UCHAR cntsBuf[512];

        XP_ASSERT( !!server->vol.model );

        const XP_UCHAR* fmt = util_getUserString( server->vol.util, 
                                                  STRD_REMAINS_HEADER );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        stream_catString( stream, buf );
        stream_catString( stream, "\n\n" );

        XP_MEMSET( counts, 0, sizeof(counts) );
        model_countAllTrayTiles( server->vol.model, counts, -1 );

        for ( cntsBuf[0] = '\0', offset = 0, tile = 0; 
              offset < sizeof(cntsBuf); ) {
            XP_U16 count = pool_getNTilesLeftFor( pool, tile ) + counts[tile];
            XP_Bool hasCount = count > 0;
            nLeft += counts[tile];

            if ( hasCount ) {
                const XP_UCHAR* face = dict_getTileString( dict, tile );

                for ( ; ; ) {
                    offset += XP_SNPRINTF( &cntsBuf[offset], 
                                           sizeof(cntsBuf) - offset, "%s", 
                                           face );
                    if ( --count == 0 ) {
                        break;
                    }
                    offset += XP_SNPRINTF( &cntsBuf[offset], 
                                           sizeof(cntsBuf) - offset, "." );
                }
            }

            if ( ++tile >= nChars ) {
                break;
            } else if ( hasCount ) {
                offset += XP_SNPRINTF( &cntsBuf[offset], 
                                       sizeof(cntsBuf) - offset, "   " );
            }
            XP_ASSERT( offset < sizeof(cntsBuf) );
        }

        fmt = util_getUserString( server->vol.util, STRD_REMAINS_EXPL );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        stream_catString( stream, buf );

        stream_catString( stream, cntsBuf );
    }
} /* server_formatRemainingTiles */

#ifdef XWFEATURE_BONUSALL
XP_U16
server_figureFinishBonus( const ServerCtxt* server, XP_U16 turn )
{
    XP_U16 result = 0;
    if ( 0 == pool_getNTilesLeft( server->pool ) ) {
        XP_U16 nOthers = server->vol.gi->nPlayers - 1;
        if ( 0 < nOthers ) {
            Tile tile;
            const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
            XP_U16 counts[dict_numTileFaces( dict )];
            XP_MEMSET( counts, 0, sizeof(counts) );
            model_countAllTrayTiles( server->vol.model, counts, turn );
            for ( tile = 0; tile < VSIZE(counts); ++tile ) {
                XP_U16 count = counts[tile];
                if ( 0 < count ) {
                    result += count * dict_getTileValue( dict, tile );
                }
            }
            /* Check this... */
            result += result / nOthers;
        }
    }
    // LOG_RETURNF( "%d", result );
    return result;
}
#endif

#define IMPOSSIBLY_LOW_SCORE -1000
#if 0
static void
printPlayer( const ServerCtxt* server, XWStreamCtxt* stream, XP_U16 index, 
             const XP_UCHAR* placeBuf, ScoresArray* scores, 
             ScoresArray* tilePenalties, XP_U16 place )
{
    XP_UCHAR buf[128];
    CurGameInfo* gi = server->vol.gi;
    ModelCtxt* model = server->vol.model;
    XP_Bool firstDone = model_getNumTilesTotal( model, index ) == 0;
    XP_UCHAR tmpbuf[48];
    XP_U16 addSubKey = firstDone? STRD_REMAINING_TILES_ADD : STRD_UNUSED_TILES_SUB;
    const XP_UCHAR* addSubString = util_getUserString( server->vol.util, addSubKey );
    XP_UCHAR* timeStr = (XP_UCHAR*)"";
    XP_UCHAR timeBuf[16];
    if ( gi->timerEnabled ) {
        XP_U16 penalty = player_timePenalty( gi, index );
        if ( penalty > 0 ) {
            XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                         util_getUserString( server->vol.util,
                                             STRD_TIME_PENALTY_SUB ),
                         penalty ); /* positive for formatting */
            timeStr = timeBuf;
        }
    }

    XP_SNPRINTF( tmpbuf, sizeof(tmpbuf), addSubString,
                 firstDone? 
                 tilePenalties->arr[index]:
                 -tilePenalties->arr[index] );

    XP_SNPRINTF( buf, sizeof(buf), 
                 (XP_UCHAR*)"[%s] %s: %d" XP_CR "  (%d %s%s)",
                 placeBuf, emptyStringIfNull(gi->players[index].name),
                 scores->arr[index], model_getPlayerScore( model, index ),
                 tmpbuf, timeStr );
    if ( 0 < place ) {
        stream_catString( stream, XP_CR );
    }
    stream_catString( stream, buf );
} /* printPlayer */
#endif

void
server_writeFinalScores( ServerCtxt* server, XWStreamCtxt* stream )
{
    ScoresArray scores;
    ScoresArray tilePenalties;
    XP_U16 place, nPlayers;
    XP_S16 quitter = server->nv.quitter;
    XP_Bool quitterDone = XP_FALSE;
    XP_USE(quitter);
    ModelCtxt* model = server->vol.model;
    const XP_UCHAR* addString = util_getUserString( server->vol.util,
                                                    STRD_REMAINING_TILES_ADD );
    const XP_UCHAR* subString = util_getUserString( server->vol.util,
                                                    STRD_UNUSED_TILES_SUB );
    XP_UCHAR* timeStr;
    CurGameInfo* gi = server->vol.gi;

    XP_ASSERT( server->nv.gameState == XWSTATE_GAMEOVER );

    model_figureFinalScores( model, &scores, &tilePenalties );

    nPlayers = gi->nPlayers;

    for ( place = 1; !quitterDone; ++place ) {
        XP_UCHAR timeBuf[16];
        XP_UCHAR buf[128]; 
        XP_S16 highestScore = IMPOSSIBLY_LOW_SCORE;
        XP_S16 highestIndex = -1;
        const XP_UCHAR* placeStr = NULL;
        XP_UCHAR placeBuf[32];
        XP_UCHAR tmpbuf[48];
        XP_U16 ii, placeKey = 0;
        XP_Bool firstDone;

        /* Find the next player we should print */
        for ( ii = 0; ii < nPlayers; ++ii ) {
            if ( quitter != ii && scores.arr[ii] > highestScore ) {
                highestIndex = ii;
                highestScore = scores.arr[ii];
            }
        }

        if ( highestIndex == -1 ) {
            if ( quitter >= 0 ) {
                XP_ASSERT( !quitterDone );
                highestIndex = quitter;
                quitterDone = XP_TRUE;
                placeKey = STR_RESIGNED;
            } else {
                break; /* we're done */
            }
        } else if ( place == 1 ) {
            placeKey = STR_WINNER;
        }

        if ( !placeStr ) {
            if ( 0 < placeKey ) {
                placeStr = util_getUserString( server->vol.util, placeKey );
            } else {
                XP_SNPRINTF( placeBuf, VSIZE(placeBuf), "#%d", place );
                placeStr = placeBuf;
            }
        }

        timeStr = (XP_UCHAR*)"";
        if ( gi->timerEnabled ) {
            XP_U16 penalty = player_timePenalty( gi, highestIndex );
            if ( penalty > 0 ) {
                XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                             util_getUserString( 
                                                server->vol.util,
                                                STRD_TIME_PENALTY_SUB ),
                             penalty ); /* positive for formatting */
                timeStr = timeBuf;
            }
        }

        firstDone = model_getNumTilesTotal( model, highestIndex) == 0;
        XP_SNPRINTF( tmpbuf, sizeof(tmpbuf), 
                     (firstDone? addString:subString),
                     firstDone? 
                     tilePenalties.arr[highestIndex]:
                     -tilePenalties.arr[highestIndex] );

        XP_SNPRINTF( buf, sizeof(buf), 
                     (XP_UCHAR*)"[%s] %s: %d" XP_CR "  (%d %s%s)", placeStr, 
                     emptyStringIfNull(gi->players[highestIndex].name),
                     scores.arr[highestIndex], 
                     model_getPlayerScore( model, highestIndex ),
                     tmpbuf, timeStr );

        if ( 1 < place ) {
            stream_catString( stream, XP_CR );
        }
        stream_catString( stream, buf );

        /* Don't consider this one next time around */
        scores.arr[highestIndex] = IMPOSSIBLY_LOW_SCORE;
    }
} /* server_writeFinalScores */

#ifdef CPLUS
}
#endif

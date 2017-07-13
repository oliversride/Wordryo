/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _COMMS_H_
#define _COMMS_H_

#include "comtypes.h"
#include "mempool.h"
#include "xwrelay.h"
#include "server.h"

EXTERN_C_START

#define CHANNEL_NONE ((XP_PlayerAddr)0)
#define CONN_ID_NONE 0L

typedef XP_U32 MsgID;           /* this is too big!!! PENDING */
typedef XP_U8  XWHostID;

typedef enum {
    COMMS_CONN_NONE           /* I want errors on uninited case */
    ,COMMS_CONN_IR
    ,COMMS_CONN_IP_DIRECT
    ,COMMS_CONN_RELAY
    ,COMMS_CONN_BT
    ,COMMS_CONN_SMS

    ,COMMS_CONN_NTYPES
} CommsConnType;

typedef enum {
    COMMS_RELAYSTATE_UNCONNECTED
    , COMMS_RELAYSTATE_DENIED   /* terminal; new game or reset required to
                                   fix */
    , COMMS_RELAYSTATE_CONNECT_PENDING
    , COMMS_RELAYSTATE_CONNECTED
    , COMMS_RELAYSTATE_RECONNECTED
    , COMMS_RELAYSTATE_ALLCONNECTED
} CommsRelayState;

#ifdef XWFEATURE_BLUETOOTH
# define XW_BT_UUID "7be0d084-ff89-4d6d-9c78-594773a6f963"

# define XW_BT_NAME "Crosswords"
#endif

/* on Palm BtLibDeviceAddressType is a 48-bit quantity.  Linux's typeis the
   same size.  Goal is something all platforms support */
typedef struct XP_BtAddr { XP_U8 bits[6]; } XP_BtAddr;
typedef struct XP_BtAddrStr { XP_UCHAR chars[18]; } XP_BtAddrStr;

#ifdef COMMS_HEARTBEAT
# define IF_CH(a) a,
#else
# define IF_CH(a)
#endif

#define MAX_HOSTNAME_LEN 63
#define MAX_PHONE_LEN    31

typedef struct _CommsAddrRec {
    CommsConnType conType;

    union {
        struct {
            XP_UCHAR hostName_ip[MAX_HOSTNAME_LEN + 1];
            XP_U32 ipAddr_ip;      /* looked up from above */
            XP_U16 port_ip;
        } ip;
        struct {
            XP_UCHAR invite[MAX_INVITE_LEN + 1];
            XP_UCHAR hostName[MAX_HOSTNAME_LEN + 1];
            XP_U32 ipAddr;      /* looked up from above */
            XP_U16 port;
            XP_Bool seeksPublicRoom;
            XP_Bool advertiseRoom;
        } ip_relay;
        struct {
            /* nothing? */
            XP_UCHAR foo;       /* wince doesn't like nothing here */
        } ir;
        struct {
            /* guests can browse for the host to connect to */
            XP_UCHAR hostName[MAX_HOSTNAME_LEN + 1];
            XP_BtAddrStr btAddr;
        } bt;
        struct {
            XP_UCHAR phone[MAX_PHONE_LEN + 1];
            XP_U16   port;
        } sms;
    } u;
} CommsAddrRec;

typedef XP_S16 (*TransportSend)( const XP_U8* buf, XP_U16 len, 
                                 const CommsAddrRec* addr,
                                 XP_U32 gameID, void* closure );
#ifdef COMMS_HEARTBEAT
typedef void (*TransportReset)( void* closure );
#endif

#ifdef XWFEATURE_RELAY
typedef void (*RelayStatusProc)( void* closure, CommsRelayState newState );
typedef void (*RelayConndProc)( void* closure, XP_UCHAR* const room,
                                XP_Bool reconnect,
                                XP_U16 devOrder, /* 1 means created room, etc. */
                                XP_Bool allHere, XP_U16 nMissing );
typedef void (*RelayErrorProc)( void* closure, XWREASON relayErr );
typedef XP_Bool (*RelayNoConnProc)( const XP_U8* buf, XP_U16 len,
                                    const XP_UCHAR* relayID, void* closure );
#endif

typedef enum {
    COMMS_XPORT_FLAGS_NONE = 0
    ,COMMS_XPORT_FLAGS_HASNOCONN = 1
} CommsTransportFlags;

#ifdef COMMS_XPORT_FLAGSPROC
typedef XP_U32 (*FlagsProc)( void* closure );
#endif

typedef struct _TransportProcs {
# ifdef COMMS_XPORT_FLAGSPROC
    FlagsProc getFlags;
#else
    XP_U32 flags;
#endif
    TransportSend send;
#ifdef COMMS_HEARTBEAT
    TransportReset reset;
#endif
#ifdef XWFEATURE_RELAY
    RelayStatusProc rstatus;
    RelayConndProc rconnd;
    RelayErrorProc rerror;
    RelayNoConnProc sendNoConn;
#endif
    void* closure;
} TransportProcs;

CommsCtxt* comms_make( MPFORMAL XW_UtilCtxt* util,
                       XP_Bool isServer, 
                       XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
                       const TransportProcs* procs
#ifdef SET_GAMESEED
                       ,XP_U16 gameSeed
#endif
                       );

void comms_reset( CommsCtxt* comms, XP_Bool isServer, 
                  XP_U16 nPlayersHere, XP_U16 nPlayersTotal );
void comms_resetSame( CommsCtxt* comms );
void comms_transportFailed( CommsCtxt* comms );

void comms_destroy( CommsCtxt* comms );

void comms_setConnID( CommsCtxt* comms, XP_U32 connID );

/* "static" methods work when no comms present */
void comms_getInitialAddr( CommsAddrRec* addr
#ifdef XWFEATURE_RELAY
                           , const XP_UCHAR* relayName
                           , XP_U16 relayPort
#endif
 );
XP_Bool comms_checkAddr( DeviceRole role, const CommsAddrRec* addr,
                         XW_UtilCtxt* util );

void comms_getAddr( const CommsCtxt* comms, CommsAddrRec* addr );
void comms_setAddr( CommsCtxt* comms, const CommsAddrRec* addr );
void comms_getAddrs( const CommsCtxt* comms, CommsAddrRec addr[], 
                     XP_U16* nRecs );

#ifdef XWFEATURE_RELAY
XP_Bool comms_getRelayID( const CommsCtxt* comms, XP_UCHAR* buf, XP_U16* len );
#endif

CommsConnType comms_getConType( const CommsCtxt* comms );
XP_Bool comms_getIsServer( const CommsCtxt* comms );

CommsCtxt* comms_makeFromStream( MPFORMAL XWStreamCtxt* stream, 
                                 XW_UtilCtxt* util, 
                                 const TransportProcs* procs );
void comms_start( CommsCtxt* comms );
void comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream,
                          XP_U16 saveToken );
void comms_saveSucceeded( CommsCtxt* comms, XP_U16 saveToken );

XP_S16 comms_send( CommsCtxt* comms, XWStreamCtxt* stream );
XP_Bool comms_resendAll( CommsCtxt* comms, XP_Bool force );
XP_U16 comms_getChannelSeed( CommsCtxt* comms );

#ifdef XWFEATURE_COMMSACK
void comms_ackAny( CommsCtxt* comms );
#endif

XP_Bool comms_checkIncomingStream( CommsCtxt* comms, XWStreamCtxt* stream, 
                                   const CommsAddrRec* addr );

XP_Bool comms_checkComplete( const CommsAddrRec* const addr );

XP_Bool comms_canChat( const CommsCtxt* comms );
XP_Bool comms_isConnected( const CommsCtxt* const comms );

# ifdef DEBUG
void comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream );
const char* ConnType2Str( CommsConnType typ );
const char* CommsRelayState2Str( CommsRelayState state );
const char* XWREASON2Str( XWREASON reason );
# endif

EXTERN_C_END

#endif /* _COMMS_H_ */

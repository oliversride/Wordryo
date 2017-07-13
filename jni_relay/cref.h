/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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


#ifndef _CREF_H_
#define _CREF_H_

#include <map>
#include <set>
#include <vector>
#include <string>
#include <deque>
#include <pthread.h>
#include "xwrelay_priv.h"
#include "xwrelay.h"
#include "devid.h"
#include "dbmgr.h"
#include "states.h"
#include "addrinfo.h"

typedef vector<unsigned char> MsgBuffer;
typedef deque<MsgBuffer*> MsgBufQueue;

using namespace std;

class CookieMapIterator;        /* forward */

struct HostRec {
public:
HostRec(HostID hostID, const AddrInfo* addr, int nPlayersH, int seed, bool ackPending ) 
        : m_hostID(hostID) 
        , m_addr(*addr) 
        , m_nPlayersH(nPlayersH) 
        , m_seed(seed) 
        , m_lastHeartbeat(uptime()) 
        , m_ackPending(ackPending)
        {
            ::logf( XW_LOGINFO, "created HostRec with id %d", m_hostID);
        }
    HostID m_hostID;
    AddrInfo m_addr;
    int m_nPlayersH;
    int m_seed;
    time_t m_lastHeartbeat;
    bool m_ackPending;
};

struct AckTimer {
public:
    HostID m_hid;
    class CookieRef* m_this;
};

class CookieRef {
 public:
    vector<AddrInfo> GetAddrs( void );

 private:
    /* These classes have access to CookieRef.  All others should go through
       SafeCref instances. */
    friend class CRefMgr;
    friend class SafeCref;
    friend class CookieMapIterator;

    CookieRef( const char* cookie, const char* connName, CookieID cid,
               int langCode, int nPlayersT, int nPlayersH );
    void ReInit( const char* cookie, const char* connName, CookieID cid,
                 int langCode, int nPlayers, int nAlreadyHere );
    ~CookieRef();

    void Clear(void);                /* make clear it's unused */

    bool Lock( void );
    void Unlock( void );

    /* Within this cookie, remember that this hostID and socket go together.
       If the hostID is HOST_ID_SERVER, it's the server. */
    CookieID GetCid() { return m_cid; }
    int GetPlayersSought() { return m_nPlayersSought; }
    int GetPlayersHere() { return m_nPlayersHere; }

    bool HaveRoom( int nPlayers );

    int CountSockets() { return m_sockets.size(); }
    bool HasSocket( const AddrInfo* addr );
    bool HasSocket_locked( const AddrInfo* addr );
    const char* Cookie() const { return m_cookie.c_str(); }
    const char* ConnName() { return m_connName.c_str(); }

    int GetHeartbeat() { return m_heatbeat; }
    const AddrInfo* SocketForHost( HostID dest );
    HostID HostForSocket( const AddrInfo* addr );

    /* connect case */
    bool AlreadyHere( unsigned short seed, const AddrInfo* addr, HostID* prevHostID );
    /* reconnect case */
    bool AlreadyHere( HostID hid, unsigned short seed, const AddrInfo* addr, bool* spotTaken );

    /* for console */
    void _PrintCookieInfo( string& out );
    void PrintSocketInfo( string& out, int socket );
    void _FormatHostInfo( string* hostIds, string* seeds, string* addrs );

    static CookieMapIterator GetCookieIterator();

    /* Nuke an existing */
    static void Delete( CookieID cid );
    static void Delete( const char* name );

    bool _Connect( int clientVersion, DevID* devID,
                   int nPlayersH, int nPlayersS, int seed, bool seenSeed, 
                   const AddrInfo* addr );
    bool _Reconnect( int clientVersion, DevID* devID,
                     HostID srcID, int nPlayersH, int nPlayersS,
                     int seed, const AddrInfo* addr, bool gameDead );
    void _HandleAck( HostID hostID );
    void _PutMsg( HostID srcID, const AddrInfo* addr, HostID destID, 
                  const unsigned char* buf, int buflen );
    void _Disconnect( const AddrInfo* addr, HostID hostID );
    void _DeviceGone( HostID hostID, int seed );
    void _Shutdown();
    void _HandleHeartbeat( HostID id, const AddrInfo* addr );
    void _CheckHeartbeats( time_t now );
    void _Forward( HostID src, const AddrInfo* addr, HostID dest, 
                   const unsigned char* buf, int buflen );
    void _Remove( const AddrInfo* addr );
    void _CheckAllConnected();
    void _CheckNotAcked( HostID hid );

    bool ShouldDie() { return m_curState == XWS_EMPTY; }
    XW_RELAY_STATE CurState() { return m_curState; }

    void logf( XW_LogLevel level, const char* format, ... );

    class CRefEvent {
    public:
        CRefEvent() { type = XWE_NONE; }
        CRefEvent( XW_RELAY_EVENT typ ) { type = typ; }
        CRefEvent( XW_RELAY_EVENT typ, const AddrInfo* addrp ) { type = typ; addr = *addrp; }
        XW_RELAY_EVENT type;
        AddrInfo addr;  /* sender's address */
        union {
            struct {
                HostID src;
                HostID dest;
                const unsigned char* buf;
                int buflen;
            } fwd;
            struct {
                int clientVersion;
                DevID* devID;
                int nPlayersH;
                int nPlayersS;
                int seed;
                HostID srcID;
            } con;
            struct {
                HostID srcID;
            } ack;
            struct {
                HostID srcID;
            } discon;
            struct {
                HostID hid;
                int seed;
            } devgone;
            struct {
                HostID id;
            } heart;
            struct {
                time_t now;
            } htime;
            struct {
            } rmsock;
            struct {
                XWREASON why;
            } disnote;
        } u;
    };

    bool send_with_length( const AddrInfo* addr, HostID hid,
                           const unsigned char* buf, int bufLen, bool cascade );
    void send_msg( const AddrInfo* addr, HostID id, 
                   XWRelayMsg msg, XWREASON why, bool cascade );
    void pushConnectEvent( int clientVersion, DevID* devID,
                           int nPlayersH, int nPlayersS,
                           int seed, const AddrInfo* addr );
    void pushReconnectEvent( int clientVersion, DevID* devID,
                             HostID srcID, int nPlayersH, int nPlayersS,
                             int seed, const AddrInfo* addr );
    void pushHeartbeatEvent( HostID id, const AddrInfo* addr );
    void pushHeartFailedEvent( const AddrInfo* addr );
    
    void pushForwardEvent( HostID src, const AddrInfo* addr, 
                           HostID dest, const unsigned char* buf, int buflen );
    void pushDestBadEvent();
    void pushLastSocketGoneEvent();
    void pushGameDead( const AddrInfo* addr );
    void checkHaveRoom( const CRefEvent* evt );
    void pushRemoveSocketEvent( const AddrInfo* addr );
    void pushNotifyDisconEvent( const AddrInfo* addr, XWREASON why );

    void handleEvents();

    void sendResponse( const CRefEvent* evt, bool initial, 
                       const DevIDRelay* devID );
    void sendAnyStored( const CRefEvent* evt );
    void initPlayerCounts( const CRefEvent* evt );
    bool increasePlayerCounts( CRefEvent* evt, bool reconn, HostID* hidp, 
                               DevIDRelay* devID );
    void updateAck( HostID hostID, bool keep );
    void dropPending( int seed );

    void postCheckAllHere();
    void postDropDevice( HostID hostID );
    void postTellHaveMsgs( const AddrInfo* addr );

    void setAllConnectedTimer();
    void cancelAllConnectedTimer();
    void setAckTimer( HostID hid );
    void cancelAckTimer( HostID hid );

    void forward_or_store( const CRefEvent* evt );
    void send_denied( const CRefEvent* evt, XWREASON why );
    void send_trytell( const CRefEvent* evt );

    void checkFromServer( const CRefEvent* evt );
    void notifyOthers( const AddrInfo* addr, XWRelayMsg msg, XWREASON why );
    void notifyGameDead( const AddrInfo* addr );

    void disconnectSockets( XWREASON why );
    void disconnectSocket( const AddrInfo* addr, XWREASON why );
    void removeDevice( const CRefEvent* const evt );
    void noteHeartbeat(const CRefEvent* evt);
    void notifyDisconn(const CRefEvent* evt);
    void removeSocket( const AddrInfo* addr );
    void sendAllHere( bool initial );

    void assignConnName( void );
    
    time_t GetStarttime( void ) { return m_starttime; }
    int GetLangCode( void ) { return m_langCode; }

    bool notInUse(void) { return m_cid == 0; }

    void store_message( HostID dest, const unsigned char* buf, 
                        unsigned int len );
    void send_stored_messages( HostID dest, const AddrInfo* addr );

    void printSeeds( const char* caller );

    /* timer callback */
    static void s_checkAllConnected( void* closure );
    static void s_checkAck( void* closure );
    
    /* Track sockets (= current connections with games on devices) in this
       game.  There will never be more than four of these */
    pthread_rwlock_t m_socketsRWLock;
    vector<HostRec> m_sockets;

    int m_heatbeat;           /* might change per carrier or something. */
    string m_cookie;            /* cookie used for initial connections */
    string m_connName;          /* globally unique name */
    CookieID m_cid;        /* Unique among current games on this server */

    XW_RELAY_STATE     m_curState;
    deque<CRefEvent>   m_eventQueue;

    int m_nPlayersSought;
    int m_nPlayersHere;
    int m_langCode;

    time_t m_starttime;

    AckTimer m_timers[4];

    pthread_t m_locking_thread;
    bool m_in_handleEvents;     /* for debugging only */
    int m_delayMicros;
}; /* CookieRef */

#endif

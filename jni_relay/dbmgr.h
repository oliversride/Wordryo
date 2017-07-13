/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2010 - 2012 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _DBMGR_H_
#define _DBMGR_H_

#include <string>

#include "xwrelay.h"
#include "xwrelay_priv.h"
#include "devid.h"
#include <libpq-fe.h>

using namespace std;

class DBMgr {
 public:
    /* DevIDs on various platforms are stored in devices table.  This is the
       key, and used in msgs and games tables as a shorter way to refer to
       them. */
    static const DevIDRelay DEVID_NONE = 0;

    static DBMgr* Get();

    ~DBMgr();

    void ClearCIDs( void );

    void AddNew( const char* cookie, const char* connName, CookieID cid, 
                 int langCode, int nPlayersT, bool isPublic );

    bool FindPlayer( DevIDRelay relayID, AddrInfo::ClientToken, 
                     string& connName, HostID* hid, unsigned short* seed );

    CookieID FindGame( const char* connName, char* cookieBuf, int bufLen,
                       int* langP, int* nPlayersTP, int* nPlayersHP,
                       bool* isDead );

    bool SeenSeed( const char* cookie, unsigned short seed,
                   int langCode, int nPlayersT, bool wantsPublic, 
                   char* connNameBuf, int bufLen, int* nPlayersHP,
                   CookieID* cid );

    CookieID FindOpen( const char* cookie, int lang, int nPlayersT, 
                       int nPlayersH, bool wantsPublic, 
                       char* connNameBuf, int bufLen, int* nPlayersHP );
    bool AllDevsAckd( const char* const connName );

    DevIDRelay RegisterDevice( const DevID* host );
    bool updateDevice( DevIDRelay relayID, bool check );

    HostID AddDevice( const char* const connName, HostID curID, int clientVersion,
                      int nToAdd, unsigned short seed, const AddrInfo* addr,
                      DevIDRelay devID, bool unAckd );
    void NoteAckd( const char* const connName, HostID id );
    HostID HIDForSeed( const char* const connName, unsigned short seed );
    bool RmDeviceByHid( const char* const connName, HostID id );
    void RmDeviceBySeed( const char* const connName, unsigned short seed );
    bool HaveDevice( const char* const connName, HostID id, int seed );
    bool AddCID( const char* const connName, CookieID cid );
    void ClearCID( const char* connName );
    void RecordSent( const char* const connName, HostID hid, int nBytes );
    void RecordSent( const int* msgID, int nMsgIDs );
    void RecordAddress( const char* const connName, HostID hid, 
                        const AddrInfo* addr );
    void GetPlayerCounts( const char* const connName, int* nTotal,
                          int* nHere );

    void KillGame( const char* const connName, int hid );

    /* Return list of roomName/playersStillWanted/age for open public games
       matching this language and total game size. Will probably want to cache
       lists locally and only update them every few seconds to avoid to many
       queries.*/
    void PublicRooms( int lang, int nPlayers, int* nNames, string& names );

    /* Get stored address info, if available and valid */
    bool TokenFor( const char* const connName, int hid, DevIDRelay* devid,
                   AddrInfo::ClientToken* token );

    /* Return number of messages pending for connName:hostid pair passed in */
    int PendingMsgCount( const char* const connName, int hid );

    /* message storage -- different DB */
    int CountStoredMessages( const char* const connName );
    int CountStoredMessages( const char* const connName, int hid );
    int CountStoredMessages( DevIDRelay relayID );
    void StoreMessage( const char* const connName, int hid, 
                       const unsigned char* const buf, int len );
    void GetStoredMessageIDs( DevIDRelay relayID, vector<int>& ids );

    bool GetStoredMessage( const char* const connName, int hid, 
                           unsigned char* buf, size_t* buflen, int* msgID );
    bool GetNthStoredMessage( const char* const connName, int hid, int nn,
                              unsigned char* buf, size_t* buflen, int* msgID );
    bool GetStoredMessage( int msgID, unsigned char* buf, size_t* buflen, 
                           AddrInfo::ClientToken* token );

    void RemoveStoredMessages( const int* msgID, int nMsgIDs );
    void RemoveStoredMessages( vector<int>& ids );

 private:
    DBMgr();
    bool execSql( const string& query );
    bool execSql( const char* const query ); /* no-results query */
    void readArray( const char* const connName, int arr[] );
    DevIDRelay getDevID( const char* connName, int hid );
    DevIDRelay getDevID( const DevID* devID );
    int getCountWhere( const char* table, string& test );
    void RemoveStoredMessages( string& msgIDs );
    void decodeMessage( PGresult* result, bool useB64, int b64indx, 
                        int byteaIndex, unsigned char* buf, size_t* buflen );

    PGconn* getThreadConn( void );

    void conn_key_alloc();
    pthread_key_t m_conn_key;
    bool m_useB64;

}; /* DBMgr */


#endif

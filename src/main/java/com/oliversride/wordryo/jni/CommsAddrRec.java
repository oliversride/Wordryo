/* -*- compile-command: "cd ../../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package com.oliversride.wordryo.jni;

import java.net.InetAddress;

import com.oliversride.wordryo.DbgUtils;

public class CommsAddrRec {

    public enum CommsConnType { COMMS_CONN_NONE,
            COMMS_CONN_IR,
            COMMS_CONN_IP_DIRECT,
            COMMS_CONN_RELAY,
            COMMS_CONN_BT,
            COMMS_CONN_SMS,
    };

    // The C equivalent of this struct uses a union for the various
    // data sets below.  So don't assume that any fields will be valid
    // except those for the current conType.
    public CommsConnType conType;

    // relay case
    public String ip_relay_invite;
    public String ip_relay_hostName;
    public InetAddress ip_relay_ipAddr;    // a cache, maybe unused in java
    public int ip_relay_port;
    public boolean ip_relay_seeksPublicRoom;
    public boolean ip_relay_advertiseRoom;

    // bt case
    public String bt_hostName;
    public String bt_btAddr;

    // sms case
    public String sms_phone;
    public int sms_port;                // SMS port, if they still use those

    public CommsAddrRec( CommsConnType cTyp ) 
    {
        conType = cTyp;
    }

    public CommsAddrRec() 
    {
        this( CommsConnType.COMMS_CONN_NONE );
    }

    public CommsAddrRec( String host, int port ) 
    {
        this( CommsConnType.COMMS_CONN_RELAY );
        ip_relay_hostName = host;
        ip_relay_port = port;
        ip_relay_seeksPublicRoom = false;
        ip_relay_advertiseRoom = false;
    }

    public CommsAddrRec( String btHost, String btAddr ) 
    {
        this( CommsConnType.COMMS_CONN_BT );
        bt_hostName = btHost;
        bt_btAddr = btAddr;
    }

    public CommsAddrRec( String phone ) 
    {
        this( CommsConnType.COMMS_CONN_SMS );
        sms_phone = phone;
    }

    public CommsAddrRec( final CommsAddrRec src ) 
    {
        this.copyFrom( src );
    }

    public boolean changesMatter( final CommsAddrRec other )
    {
        boolean matter = conType != other.conType;
        if ( !matter ) {
            switch( conType ) {
            case COMMS_CONN_RELAY:
                matter = ! ip_relay_invite.equals( other.ip_relay_invite )
                    || ! ip_relay_hostName.equals( other.ip_relay_hostName )
                    || ip_relay_port != other.ip_relay_port;
                break;
            default:
                DbgUtils.logf( "changesMatter: not handling case: %s", 
                               conType.toString() );
                break;
            }
        }
        return matter;
    }

    private void copyFrom( CommsAddrRec src )
    {
        conType = src.conType;
        ip_relay_invite = src.ip_relay_invite;
        ip_relay_hostName = src.ip_relay_hostName;
        ip_relay_port = src.ip_relay_port;
        ip_relay_seeksPublicRoom = src.ip_relay_seeksPublicRoom;
        ip_relay_advertiseRoom = src.ip_relay_advertiseRoom;

        bt_hostName = src.bt_hostName;
        bt_btAddr = src.bt_btAddr;

        sms_phone = src.sms_phone;
        sms_port = src.sms_port;
    }
}

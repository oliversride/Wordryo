/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

package com.oliversride.wordryo;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class BTReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( XWApp.BTSUPPORTED ) {
            DbgUtils.logf( "BTReceiver.onReceive()" );
            String action = intent.getAction();
            DbgUtils.logf( "BTReceiver.onReceive(action=%s)", action );
            if ( action.equals( BluetoothDevice.ACTION_ACL_CONNECTED ) ) {
                BTService.startService( context );
            } else if ( action.equals( BluetoothAdapter.ACTION_STATE_CHANGED ) ) {
                int newState = 
                    intent.getIntExtra( BluetoothAdapter.EXTRA_STATE, -1 );
                switch ( newState ) {
                case BluetoothAdapter.STATE_OFF:
                    BTService.radioChanged( context, false );
                    break;
                case BluetoothAdapter.STATE_ON:
                    BTService.radioChanged( context, true );
                    break;
                case BluetoothAdapter.STATE_TURNING_ON:
                case BluetoothAdapter.STATE_TURNING_OFF:
                    break;
                }
            }
        }
    }
}

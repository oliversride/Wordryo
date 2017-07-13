/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

public class RelayReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( null != intent && null != intent.getAction() 
             && intent.getAction().equals( Intent.ACTION_BOOT_COMPLETED ) ) {
            DbgUtils.logf("RelayReceiver.onReceive: launching timer on boot");
            RestartTimer( context );
        } else {
            // DbgUtils.logf( "RelayReceiver::onReceive()" );
            // Toast.makeText(context, "RelayReceiver: fired", 
            //                Toast.LENGTH_SHORT).show();
            Intent service = new Intent( context, RelayService.class );
            context.startService( service );
        }
    }

    public static void RestartTimer( Context context, boolean force )
    {
        RestartTimer( context, 
                      1000 * XWPrefs.getProxyInterval( context ), force );
    }

    public static void RestartTimer( Context context )
    {
        RestartTimer( context, false );
    }

    public static void RestartTimer( Context context, long interval_millis, 
                                     boolean force )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        if ( force || interval_millis > 0 ) {
            long first_millis = 0;
            if ( !force ) {
                first_millis = SystemClock.elapsedRealtime() + interval_millis;
            }
            am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                    first_millis, // first firing
                                    interval_millis, pi );
        } else {
            am.cancel( pi );
        }
    }

    public static void RestartTimer( Context context, long interval_millis )
    {
        RestartTimer( context, interval_millis, false );
    }

}

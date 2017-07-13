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

import junit.framework.Assert;
import android.content.Context;

import com.oliversride.wordryo.R;

public class LocalPlayer {
    public String name;
    public String password;
    public String dictName;
    public int secondsUsed;
    public int robotIQ;
    public boolean isLocal;

    public LocalPlayer( Context context, int num )
    {
        isLocal = true;
        robotIQ = 0;            // human
        String fmt = context.getString( R.string.playerf );
        name = String.format( fmt, num + 1 );
        password = "";
    }

    public LocalPlayer( final LocalPlayer src )
    {
        isLocal = src.isLocal;
        robotIQ = src.robotIQ;
        name = src.name;
        password = src.password;
        dictName = src.dictName;
        secondsUsed = src.secondsUsed;
    }

    public boolean isRobot() 
    {
        return robotIQ > 0;
    }

    public void setIsRobot( boolean isRobot )
    {
        robotIQ = isRobot ? 1 : 0;
    }

    public void setRobotSmartness( int iq )
    {
        Assert.assertTrue( iq > 0 );
        robotIQ = iq;
    }
}


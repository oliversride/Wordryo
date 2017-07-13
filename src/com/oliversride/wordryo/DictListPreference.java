/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All
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

package com.oliversride.wordryo;

import android.content.Context;
import android.util.AttributeSet;

public class DictListPreference extends XWListPreference {

    public DictListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );

        DictUtils.DictAndLoc[] dals = DictUtils.dictList( context  );
        String[] dictEntries = new String[dals.length];
        String[] values = new String[dals.length];
        for ( int ii = 0; ii < dals.length; ++ii ) {
            values[ii] = dals[ii].name;
            dictEntries[ii] = 
                DictLangCache.annotatedDictName( context, dals[ii] );
        }
        setEntries( dictEntries );
        setEntryValues( values );
    }
}

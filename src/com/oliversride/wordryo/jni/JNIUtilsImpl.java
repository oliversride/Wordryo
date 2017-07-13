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

import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;
import java.security.MessageDigest;
import java.util.ArrayList;

import junit.framework.Assert;
import android.content.Context;

import com.oliversride.wordryo.DBUtils;
import com.oliversride.wordryo.DbgUtils;
import com.oliversride.wordryo.Utils;

public class JNIUtilsImpl implements JNIUtils {

    private static final char SYNONYM_DELIM = ' ';

    private static JNIUtilsImpl s_impl = null;
    private Context m_context;

    private JNIUtilsImpl(){}

    public static JNIUtils get( Context context )
    {
        if ( null == s_impl ) {
            s_impl = new JNIUtilsImpl();
        }
        s_impl.m_context = context;
        return s_impl;
    }

    /** Working around lack of utf8 support on the JNI side: given a
     * utf-8 string with embedded small number vals starting with 0,
     * convert into individual strings.  The 0 is the problem: it's
     * not valid utf8.  So turn it and the other nums into strings and
     * catch them on the other side.
     *
     * Changes for "synonyms" (A and a being the same tile): return an
     * array of Strings for each face.  Each face is
     * <letter>[<delim><letter]*, so for each loop until the delim
     * isn't found.
     */
    public String[][] splitFaces( byte[] chars, boolean isUTF8 )
    {
        ArrayList<String[]> faces = new ArrayList<String[]>();
        ByteArrayInputStream bais = new ByteArrayInputStream( chars );
        InputStreamReader isr;
        try {
            isr = new InputStreamReader( bais, isUTF8? "UTF8" : "ISO8859_1" );
        } catch( java.io.UnsupportedEncodingException uee ) {
            DbgUtils.logf( "splitFaces: %s", uee.toString() );
            isr = new InputStreamReader( bais );
        }
        
        int[] codePoints = new int[1];

        // "A aB bC c"
        boolean lastWasDelim = false;
        ArrayList<String> face = null;
        for ( ; ; ) {
            int chr = -1;
            try {
                chr = isr.read();
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( ioe.toString() );
            }
            if ( -1 == chr ) {
                addFace( faces, face );
                break;
            } else if ( SYNONYM_DELIM == chr ) {
                Assert.assertNotNull( face );
                lastWasDelim = true;
                continue;
            } else {
                String letter;
                if ( chr < 32 ) {
                    letter = String.format( "%d", chr );
                } else {
                    codePoints[0] = chr;
                    letter = new String( codePoints, 0, 1 );
                }
                // Ok, we have a letter.  Is it part of an existing
                // one or the start of a new?  If the latter, insert
                // what we have before starting over.
                if ( null == face ) { // start of a new, clearly
                    // do nothing
                } else {
                    Assert.assertTrue( 0 < face.size() );
                    if ( !lastWasDelim ) {
                        addFace( faces, face );
                        face = null;
                    }
                }
                lastWasDelim = false;
                if ( null == face ) {
                    face = new ArrayList<String>();
                }
                face.add( letter );
            }
        }
        
        String[][] result = faces.toArray( new String[faces.size()][] );
        return result;
    }

    private void addFace( ArrayList<String[]> faces, ArrayList<String> face )
    {
        faces.add( face.toArray( new String[face.size()] ) );
    }

    public String getMD5SumFor( String dictName, byte[] bytes )
    {
        String result = null;
        if ( null == bytes ) {
            result = DBUtils.dictsGetMD5Sum( m_context, dictName );
        } else {
            byte[] digest = null;
            try {
                MessageDigest md = MessageDigest.getInstance("MD5");
                byte[] buf = new byte[128];
                int nLeft = bytes.length;
                int offset = 0;
                while ( 0 < nLeft ) {
                    int len = Math.min( buf.length, nLeft );
                    System.arraycopy( bytes, offset, buf, 0, len );
                    md.update( buf, 0, len );
                    nLeft -= len;
                    offset += len;
                }
                digest = md.digest();
            } catch ( java.security.NoSuchAlgorithmException nsae ) {
                DbgUtils.loge( nsae );
            }
            result = Utils.digestToString( digest );

            // Is this needed?  Caller might be doing it anyway.
            DBUtils.dictsSetMD5Sum( m_context, dictName, result );
        }
        return result;
    }
}

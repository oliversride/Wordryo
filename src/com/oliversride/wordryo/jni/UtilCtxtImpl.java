/* -*- compile-command: "cd ../../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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
import android.telephony.PhoneNumberUtils;

import com.oliversride.wordryo.DbgUtils;
import com.oliversride.wordryo.R;
import com.oliversride.wordryo.XWApp;
import com.oliversride.wordryo.XWPrefs;

public class UtilCtxtImpl implements UtilCtxt {
    private Context m_context;

    private UtilCtxtImpl() {}   // force subclasses to pass context

    public UtilCtxtImpl( Context context )
    {
        super();
        m_context = context;
    }

    public void requestTime() {
        subclassOverride( "requestTime" );
    }

    public int userPickTileBlank( int playerNum, String[] texts )
    {
        subclassOverride( "userPickTileBlank" );
        return 0;
    }

    public int userPickTileTray( int playerNum, String[] texts, 
                                 String[] curTiles, int nPicked )
    {
        subclassOverride( "userPickTileTray" );
        return 0;
    }

    public String askPassword( String name )
    {
        subclassOverride( "askPassword" );
        return null;
    }

    public void turnChanged( int newTurn, boolean delayUpdate )
    {
        subclassOverride( "turnChanged" );
    }

    public boolean engineProgressCallback()
    {
        // subclassOverride( "engineProgressCallback" );
        return true;
    }

    public void setTimer( int why, int when, int handle )
    {
        subclassOverride( "setTimer" );
    }

    public void clearTimer( int why )
    {
        subclassOverride( "clearTimer" );
    }

    public void remSelected()
    {
        subclassOverride( "remSelected" );
    }

    public void setIsServer( boolean isServer )
    {
        subclassOverride( "setIsServer" );
    }

    public String getDevID( /*out*/ byte[] typa )
    {
        byte typ = UtilCtxt.ID_TYPE_NONE;
        String result = XWPrefs.getRelayDevID( m_context );
        if ( null != result ) {
            typ = UtilCtxt.ID_TYPE_RELAY;
        } else {
            result = XWPrefs.getGCMDevID( m_context );
            if ( result.equals("") ) {
                result = null;
            } else {
                typ = UtilCtxt.ID_TYPE_ANDROID_GCM;
            }
        }
        typa[0] = typ;
        return result;
    }

    public void deviceRegistered( int devIDType, String idRelay )
    {
        switch ( devIDType ) {
        case UtilCtxt.ID_TYPE_RELAY:
            XWPrefs.setRelayDevID( m_context, idRelay );
            break;
        case UtilCtxt.ID_TYPE_NONE:
            XWPrefs.clearRelayDevID( m_context );
            break;
        default:
            Assert.fail();
            break;
        }
    }

    public void bonusSquareHeld( int bonus )
    {
    }

    public void playerScoreHeld( int player )
    {
    }
    public void androidNoMove( )
    {
    }
    public void androidExchangedTiles( )
    {
    }
    public void noHintAvailable( )
    {
    }

    public void cellSquareHeld( String words )
    {
    }

    public String getUserString( int stringCode )
    {
        int id = 0;
        switch( stringCode ) {
        case UtilCtxt.STRD_ROBOT_TRADED:
            id = R.string.strd_robot_traded;
            break;
        case UtilCtxt.STR_ROBOT_MOVED:
            id = R.string.str_robot_moved;
            break;
        case UtilCtxt.STRS_VALUES_HEADER:
            id = R.string.strs_values_header;
            break;
        case UtilCtxt.STRD_REMAINING_TILES_ADD:
            id = R.string.strd_remaining_tiles_add;
            break;
        case UtilCtxt.STRD_UNUSED_TILES_SUB:
            id = R.string.strd_unused_tiles_sub;
            break;
        case UtilCtxt.STRS_REMOTE_MOVED:
            id = R.string.str_remote_movedf;
            break;
        case UtilCtxt.STRD_TIME_PENALTY_SUB:
            id = R.string.strd_time_penalty_sub;
            break;
        case UtilCtxt.STR_PASS:
            id = R.string.str_pass;
            break;
        case UtilCtxt.STRS_MOVE_ACROSS:
            id = R.string.strs_move_across;
            break;
        case UtilCtxt.STRS_MOVE_DOWN:
            id = R.string.strs_move_down;
            break;
        case UtilCtxt.STRS_TRAY_AT_START:
            id = R.string.strs_tray_at_start;
            break;
        case UtilCtxt.STRSS_TRADED_FOR:
            id = R.string.strss_traded_for;
            break;
        case UtilCtxt.STR_PHONY_REJECTED:
            id = R.string.str_phony_rejected;
            break;
        case UtilCtxt.STRD_CUMULATIVE_SCORE:
            id = R.string.strd_cumulative_score;
            break;
        case UtilCtxt.STRS_NEW_TILES:
            id = R.string.strs_new_tiles;
            break;
        case UtilCtxt.STR_PASSED:
            id = R.string.str_passed;
            break;
        case UtilCtxt.STRSD_SUMMARYSCORED:
            id = R.string.strsd_summaryscored;
            break;
        case UtilCtxt.STRD_TRADED:
            id = R.string.strd_traded;
            break;
        case UtilCtxt.STR_LOSTTURN:
            id = R.string.str_lostturn;
            break;
        case UtilCtxt.STR_COMMIT_CONFIRM:
            id = R.string.str_commit_confirm;
            break;
        case UtilCtxt.STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case UtilCtxt.STRD_TURN_SCORE:
            id = R.string.strd_turn_score;
            break;
        case UtilCtxt.STRD_REMAINS_HEADER:
            id = R.string.strd_remains_header;
            break;
        case UtilCtxt.STRD_REMAINS_EXPL:
            id = R.string.strd_remains_expl;
            break;
        case UtilCtxt.STR_RESIGNED:
            id = R.string.str_resigned;
            break;
        case UtilCtxt.STR_WINNER:
            id = R.string.str_winner;
            break;

        default:
            DbgUtils.logf( "no such stringCode: %d", stringCode );
        }

        String result;
        if ( 0 == id ) {
            result = "";
        } else {
            result = m_context.getString( id );
        }
        return result;
    }

    public boolean userQuery( int id, String query )
    {
        subclassOverride( "userQuery" );
        return false;
    }

    public boolean confirmTrade( String[] tiles )
    {
        subclassOverride( "confirmTrade" );
        return false;
    }

    public void userError( int id )
    {
        subclassOverride( "userError" );
    }

    public void informMove( String expl, String words )
    {
        subclassOverride( "informMove" );
    }

    public void informUndo()
    {
        subclassOverride( "informUndo" );
    }

    public void informNetDict( int lang, String oldName, 
                               String newName, String newSum, 
                               CurGameInfo.XWPhoniesChoice phonies )
    {
        subclassOverride( "informNetDict" );
    }

    public void informMissing( boolean isServer, 
                               CommsAddrRec.CommsConnType connType,
                               int nMissingPlayers )
    {
        subclassOverride( "informMissing" );
    }

    // Probably want to cache the fact that the game over notification
    // showed up and then display it next time game's opened.
    public void notifyGameOver()
    {
        subclassOverride( "notifyGameOver" );
    }

    public boolean warnIllegalWord( String dict, String[] words, int turn, 
                                    boolean turnLost )
    {
        subclassOverride( "warnIllegalWord" );
        return false;
    }

    // These need to go into some sort of chat DB, not dropped.
    public void showChat( String msg )
    {
        subclassOverride( "showChat" );
    }

    public boolean phoneNumbersSame( String num1, String num2 )
    {
        Assert.assertTrue( XWApp.SMSSUPPORTED );
        boolean same = PhoneNumberUtils.compare( m_context, num1, num2 );
        DbgUtils.logf( "phoneNumbersSame => %b", same );
        return same;
    }

    private void subclassOverride( String name ) {
        // DbgUtils.logf( "%s::%s() called", getClass().getName(), name );
    }

}

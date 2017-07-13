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

public interface UtilCtxt {
    static final int BONUS_NONE = 0;
    static final int BONUS_DOUBLE_LETTER = 1;
    static final int BONUS_DOUBLE_WORD = 2;
    static final int BONUS_TRIPLE_LETTER = 3;
    static final int BONUS_TRIPLE_WORD = 4;

    public static final int TRAY_HIDDEN = 0;
    public static final int TRAY_REVERSED = 1;
    public static final int TRAY_REVEALED = 2;

    // must match defns in util.h
    public static final int PICKER_PICKALL = -1;
    public static final int PICKER_BACKUP = -2;

    int userPickTileBlank( int playerNum, String[] texts );
    int userPickTileTray( int playerNum, String[] tiles, 
                          String[] curTiles, int nPicked );

    String askPassword( String name );
    void turnChanged( int newTurn, boolean delayUpdate );

    boolean engineProgressCallback();

    // Values for why; should be enums
    public static final int TIMER_PENDOWN = 1;
    public static final int TIMER_TIMERTICK = 2;
    public static final int TIMER_COMMS = 3;
    public static final int TIMER_SLOWROBOT = 4;
    void setTimer( int why, int when, int handle );
    void clearTimer( int why );

    void requestTime();
    void remSelected();
    void setIsServer( boolean isServer );

    // Possible values for typ[0], these must match enum in xwrelay.sh
    public static final int ID_TYPE_NONE = 0;
    public static final int ID_TYPE_RELAY = 1;
    public static final int ID_TYPE_ANDROID_GCM = 3;

    String getDevID( /*out*/ byte[] typ );
    void deviceRegistered( int devIDType, String idRelay );

    void bonusSquareHeld( int bonus );
    void playerScoreHeld( int player );
    void androidNoMove( );
    void androidExchangedTiles( );
    void noHintAvailable( );
    void cellSquareHeld( String words );

    static final int STRD_ROBOT_TRADED =                  1;
    static final int STR_ROBOT_MOVED =                    2;
    static final int STRS_VALUES_HEADER =                 3;
    static final int STRD_REMAINING_TILES_ADD =           4;
    static final int STRD_UNUSED_TILES_SUB =              5;
    static final int STRS_REMOTE_MOVED =                  6;
    static final int STRD_TIME_PENALTY_SUB =              7;
    static final int STR_PASS =                           8;
    static final int STRS_MOVE_ACROSS =                   9;
    static final int STRS_MOVE_DOWN =                    10;
    static final int STRS_TRAY_AT_START =                11;
    static final int STRSS_TRADED_FOR =                  12;
    static final int STR_PHONY_REJECTED =                13;
    static final int STRD_CUMULATIVE_SCORE =             14;
    static final int STRS_NEW_TILES =                    15;
    static final int STR_PASSED =                        16;
    static final int STRSD_SUMMARYSCORED =               17;
    static final int STRD_TRADED =                       18;
    static final int STR_LOSTTURN =                      19;
    static final int STR_COMMIT_CONFIRM =                20;
    static final int STR_BONUS_ALL =                     21;
    static final int STRD_TURN_SCORE =                   22;
    static final int STRD_REMAINS_HEADER =               23;
    static final int STRD_REMAINS_EXPL =                 24;
    static final int STR_RESIGNED =                      25;
    static final int STR_WINNER =                        26;

    String getUserString( int stringCode );

    static final int QUERY_COMMIT_TURN = 0;
    static final int QUERY_ROBOT_TRADE = 1;
    boolean userQuery( int id, String query );

    boolean confirmTrade( String[] tiles );

    // These oughtto be an enum but then I'd have to cons one up in C.
    static final int ERR_NONE = 0;
    static final int ERR_TILES_NOT_IN_LINE = 1;
    static final int ERR_NO_EMPTIES_IN_TURN = 2;
    static final int ERR_TWO_TILES_FIRST_MOVE = 3;
    static final int ERR_MUST_START_ON_STAR = 4;
    static final int ERR_TILES_MUST_CONTACT = 5;
    static final int ERR_TOO_FEW_TILES_LEFT_TO_TRADE = 6;
    static final int ERR_NOT_YOUR_TURN = 7;
    static final int ERR_NO_PEEK_ROBOT_TILES = 8;
    static final int ERR_SERVER_DICT_WINS = 9;
    static final int ERR_NO_PEEK_REMOTE_TILES = 10;
    static final int ERR_REG_UNEXPECTED_USER = 11;
    static final int ERR_REG_SERVER_SANS_REMOTE = 12;
    static final int STR_NEED_BT_HOST_ADDR = 13;
    static final int ERR_NO_EMPTY_TRADE = 14;
    static final int ERR_CANT_UNDO_TILEASSIGN = 15;
    static final int ERR_CANT_HINT_WHILE_DISABLED = 16;
    static final int ERR_RELAY_BASE = 17;
    void userError( int id );

    void informMove( String expl, String words );
    void informUndo();

    void informNetDict( int lang, String oldName, String newName, 
                        String newSum, CurGameInfo.XWPhoniesChoice phonies );

    void informMissing( boolean isServer, CommsAddrRec.CommsConnType connType,
                        int nMissingPlayers );

    void notifyGameOver();
    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int maxOffset, int oldOffset, int newOffset );

    boolean warnIllegalWord( String dict, String[] words, int turn, 
                             boolean turnLost );

    void showChat( String msg );

    boolean phoneNumbersSame( String num1, String num2 );
}

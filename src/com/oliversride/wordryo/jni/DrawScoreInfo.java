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

public class DrawScoreInfo {
    public String name;
    public int playerNum;
    public int totalScore;
    public int nTilesLeft;   /* < 0 means don't use */
    public int flags;        // was CellFlags; use CELL_ constants above
    public boolean isTurn;
    public boolean selected;
    public boolean isRemote;
    public boolean isRobot;
    
	public DrawScoreInfo() {

	}
	
	public DrawScoreInfo(DrawScoreInfo other) {
	    name = other.name;
	    playerNum = other.playerNum;
	    totalScore = other.totalScore;
	    nTilesLeft = other.nTilesLeft;
	    flags = other.flags;
	    isTurn = other.isTurn;
	    selected = other.selected;
	    isRemote = other.isRemote;
	    isRobot = other.isRobot;
	}
};

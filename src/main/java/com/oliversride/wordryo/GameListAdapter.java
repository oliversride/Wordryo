/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import android.content.Context;
import android.database.DataSetObserver;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;

import com.oliversride.wordryo.DBUtils.GameGroupInfo;
import com.oliversride.wordryo.jni.GameSummary;

public class GameListAdapter implements ExpandableListAdapter {
	private static final String TAG = "GameListAdapter";
    private Context m_context;
    private ExpandableListView m_list;
    private int m_fieldID;
    private Handler m_handler;
    private LoadItemCB m_cb;
    private long[] m_positions;

    public interface LoadItemCB {
        public void itemClicked( long rowid, GameSummary summary );
    }

    public GameListAdapter( Context context, ExpandableListView list, 
                            Handler handler, LoadItemCB cb, long[] positions,
                            String fieldName ) 
    {
        m_context = context;
        m_list = list;
        m_handler = handler;
        m_cb = cb;
        m_positions = checkPositions( positions );

        m_fieldID = fieldToID( fieldName );
    }

    public long[] getPositions()
    {
        Set<Long> keys = gameInfo().keySet(); // do not modify!!!!
        if ( null == m_positions || m_positions.length != keys.size() ) {
            HashSet<Long> unused = new HashSet<Long>( keys );
            long[] newArray = new long[unused.size()];

            // First copy the existing values, in order
            int nextIndx = 0;
            if ( null != m_positions ) {
                for ( long id: m_positions ) {
                    if ( unused.contains( id ) ) {
                        newArray[nextIndx++] = id;
                        unused.remove( id );
                    }
                }
            }

            // Then copy in what's left
            Iterator<Long> iter = unused.iterator();
            while ( iter.hasNext() ) {
                newArray[nextIndx++] = iter.next();
            }
            m_positions = newArray;
        }
        return m_positions;
    }

    public boolean moveGroup( long groupid, int moveBy )
    {
        int src = getGroupPosition( groupid );
        int dest = src + moveBy;
        long[] positions = getPositions();
        boolean success = 0 <= dest && dest < positions.length;
        if ( success ) {
            long tmp = positions[src];
            positions[src] = positions[dest];
            positions[dest] = tmp;
        }
        return success;
    }

    public void expandGroups( ExpandableListView view )
    {
        HashMap<Long,GameGroupInfo> info = gameInfo();
        for ( int ii = 0; ii < info.size(); ++ii ) {
            GameGroupInfo ggi = getInfoForGroup( ii );
            if ( ggi.m_expanded ) {
                view.expandGroup( ii );
            }
        }
    }

    public long getRowIDFor( int group, int child )
    {
        long rowid = DBUtils.ROWID_NOTFOUND;
        long[] rows = getRows( getPositions()[group] );
        if ( child < rows.length ) {
            rowid = rows[child];
        }
        return rowid;
    }

    public long getRowIDFor( long packedPosition )
    {
        int childPosition = ExpandableListView.
            getPackedPositionChild( packedPosition );
        int groupPosition = ExpandableListView.
            getPackedPositionGroup( packedPosition );
        return getRowIDFor( groupPosition, childPosition );
    }

    public long getGroupIDFor( int groupPos )
    {
        long id = getPositions()[groupPos];
        return id;
    }

    public String groupName( long groupid )
    {
        HashMap<Long,GameGroupInfo> info = gameInfo();
        GameGroupInfo ggi = info.get( groupid );
        return ggi.m_name;
    }

    //////////////////////////////////////////////////////////////////////////
    // ExpandableListAdapter interface
    //////////////////////////////////////////////////////////////////////////
    public long getCombinedGroupId( long groupId )
    {
        return groupId;
    }

    public long getCombinedChildId( long groupId, long childId )
    {
        return groupId << 16 | childId;
    }

    public boolean isEmpty() { return false; }

    public void onGroupCollapsed( int groupPosition )
    {
        long groupid = getGroupIDFor( groupPosition );
        DBUtils.setGroupExpanded( m_context, groupid, false );
    }

    public void onGroupExpanded( int groupPosition )
    {
        long groupid = getGroupIDFor( groupPosition );
        DBUtils.setGroupExpanded( m_context, groupid, true );
    }

    public boolean areAllItemsEnabled() { return true; }

    public boolean isChildSelectable( int groupPosition, int childPosition ) 
    { return true; }

    public View getChildView( int groupPosition, int childPosition, 
                              boolean isLastChild, View convertView, 
                              ViewGroup parent)
    {
        View result = null;
        if ( null != convertView ) {
            // DbgUtils.logf( "getChildView gave non-null convertView" );
            if ( convertView instanceof GameListItem ) {
                GameListItem child = (GameListItem)convertView;
                long rowid = getRowIDFor( groupPosition, childPosition );
                if ( child.getRowID() == rowid ) {
                    DbgUtils.logf( "reusing child for rowid %d", rowid );
                    result = child;
                }
            }
        }
        if ( null == result ) {
            result = getChildView( groupPosition, childPosition );
        }
        return result;
    }

    private View getChildView( int groupPosition, int childPosition )
    {
        long rowid = getRowIDFor( groupPosition, childPosition );
        GameListItem result = 
            GameListItem.makeForRow( m_context, rowid, m_handler, 
                                     groupPosition, m_fieldID, m_cb );
        return result;
    }

    public View getGroupView( int groupPosition, boolean isExpanded, 
                              View convertView, ViewGroup parent )
    {
        // if ( null != convertView ) {
        //     DbgUtils.logf( "getGroupView gave non-null convertView" );
        // }
        GameListGroup view = (GameListGroup)
            Utils.inflate(m_context, R.layout.game_list_group );
        view.setGroupPosition( groupPosition );

        if ( !isExpanded ) {
            GameGroupInfo ggi = getInfoForGroup( groupPosition );
            view.setPct( m_handler, ggi.m_hasTurn, ggi.m_turnLocal, 
                         ggi.m_lastMoveTime );
        }

        int nKids = getChildrenCount( groupPosition );
        String name = m_context.getString( R.string.group_namef, 
                                           groupNames()[groupPosition], nKids );
        view.setText( name );

        return view;
    }

    public boolean hasStableIds() { return false; }
    
    public long getChildId( int groupPosition, int childPosition )
    {
        return childPosition;
    }

    public long getGroupId( int groupPosition )
    {
        return groupPosition;
    }

    public Object getChild( int groupPosition, int childPosition )
    {
        return null;
    }
    
    public Object getGroup( int groupPosition )
    {
        return null;
    }

    public int getChildrenCount( int groupPosition )
    {
        long[] rows = getRows( getPositions()[groupPosition] );
        return rows.length;
    }

    public int getGroupCount()
    {
        return gameInfo().size();
    }

    public void registerDataSetObserver( DataSetObserver obs ){}
    public void unregisterDataSetObserver( DataSetObserver obs ){}

    public void inval( long rowid )
    {
        GameListItem child = getGameItemFor( rowid );
        int groupPosition;
        if ( null != child && child.getRowID() == rowid ) {
            child.forceReload();

            groupPosition = child.getGroupPosition();
        } else {
            // DbgUtils.logf( "no child for rowid %d", rowid );
            GameListItem.inval( rowid );
            m_list.invalidate();

            long groupID = DBUtils.getGroupForGame( m_context, rowid );
            groupPosition = getGroupPosition( groupID );
        }
        reloadGroup( groupPosition );
    }

    public void invalName( long rowid )
    {
        GameListItem item = getGameItemFor( rowid );
        if ( null != item ) {
            item.invalName();
        }
    }

    private long[] getRows( long groupID )
    {
        return DBUtils.getGroupGames( m_context, groupID );
    }

    public String[] groupNames()
    {
        HashMap<Long,GameGroupInfo> info = gameInfo();
        long[] positions = getPositions();
        String[] names = new String[ positions.length ];
        for ( int ii = 0; ii < names.length; ++ii ) {
            names[ii] = info.get(positions[ii]).m_name;
        }
        return names;
    }
    
    public int getGroupPosition( long groupid )
    {
        int result = -1;
        long[] positions = getPositions();
        for ( int pos = 0; pos < positions.length; ++pos ) {
            if ( positions[pos] == groupid ) {
                result = pos;
                break;
            }
        }
        return result;
    }

    public boolean setField( String fieldName )
    {
        boolean changed = false;
        int newID = fieldToID( fieldName );
        if ( -1 == newID ) {
            DbgUtils.logf( "GameListAdapter.setField(): unable to match"
                           + " fieldName %s", fieldName );
        } else if ( m_fieldID != newID ) {
            m_fieldID = newID;
            // return true so caller will do onContentChanged.
            // There's no other way to signal GameListItem instances
            // since we don't maintain a list of them.
            changed = true;
        }
        return changed;
    }

    private GameGroupInfo getInfoForGroup( int groupPosition )
    {
        return gameInfo().get( getPositions()[groupPosition] );
    }

    private GameListItem getGameItemFor( long rowid )
    {
        GameListItem result = null;
        int count = m_list.getChildCount();
        for ( int ii = 0; ii < count; ++ii ) {
            View view = m_list.getChildAt( ii );
            if ( view instanceof GameListItem ) {
                GameListItem tryme = (GameListItem)view;
                if ( tryme.getRowID() == rowid ) {
                    result = tryme;
                    break;
                }
            }
        }
        return result;
    }

    private GameListGroup getGroupItemFor( int groupPosition )
    {
        GameListGroup result = null;
        int count = m_list.getChildCount();
        for ( int ii = 0; ii < count; ++ii ) {
            View view = m_list.getChildAt( ii );
            if ( view instanceof GameListGroup ) {
                GameListGroup tryme = (GameListGroup)view;
                if ( tryme.getGroupPosition() == groupPosition ) {
                    result = tryme;
                    break;
                }
            }
        }
        return result;
    }

    private int fieldToID( String fieldName )
    {
        int[] ids = {
            R.string.game_summary_field_empty
            ,R.string.game_summary_field_language
            ,R.string.game_summary_field_opponents
            ,R.string.game_summary_field_state
        };
        int result = -1;
        for ( int id : ids ) {
            if ( m_context.getString( id ).equals( fieldName ) ) {
                result = id;
                break;
            }
        }
        return result;
    }

    private void reloadGroup( int groupPosition )
    {
        GameListGroup group = getGroupItemFor( groupPosition );
        if ( null != group ) {
            GameGroupInfo ggi = getInfoForGroup( groupPosition );
            group.setPct( ggi.m_hasTurn, ggi.m_turnLocal, ggi.m_lastMoveTime );
        }
    }

    private HashMap<Long,GameGroupInfo> gameInfo()
    {
        return DBUtils.getGroups( m_context );
    }

    private long[] checkPositions( long[] positions )
    {
        long[] result = positions;
        if ( null != positions ) {
            Set<Long> posns = gameInfo().keySet();
            for ( long id : positions ) {
                if ( ! posns.contains( id ) ) {
                    result = null;
                    break;
                }
            }
        }
        return result;
    }
}

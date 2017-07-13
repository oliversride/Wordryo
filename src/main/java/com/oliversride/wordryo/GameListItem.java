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

import java.text.DateFormat;
import java.util.Date;
import java.util.HashSet;

import android.content.Context;
import android.graphics.Canvas;
import android.os.AsyncTask;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.oliversride.wordryo.jni.CommsAddrRec.CommsConnType;
import com.oliversride.wordryo.jni.GameSummary;

public class GameListItem extends LinearLayout 
    implements View.OnClickListener {

	private static final String TAG = "GameListItem";
    private static HashSet<Long> s_invalRows = new HashSet<Long>();

    private Context m_context;
    private boolean m_loaded;
    private long m_rowid;
    private View m_hideable;
    private ExpiringTextView m_name;
    private boolean m_expanded, m_haveTurn, m_haveTurnLocal;
    private long m_lastMoveTime;
    private ImageButton m_expandButton;
    private Handler m_handler;
    private GameSummary m_summary;
    private GameListAdapter.LoadItemCB m_cb;
    private int m_fieldID;
    private int m_loadingCount;
    private int m_groupPosition;

    public GameListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
        m_context = cx;
        m_loaded = false;
        m_rowid = DBUtils.ROWID_NOTFOUND;
        m_lastMoveTime = 0;
        m_loadingCount = 0;
    }

    private void init( Handler handler, long rowid, int groupPosition,
                       int fieldID, GameListAdapter.LoadItemCB cb )
    {
        m_handler = handler;
        m_rowid = rowid;
        m_groupPosition = groupPosition;
        m_fieldID = fieldID;
        m_cb = cb;

        forceReload();
    }

    public void forceReload()
    {
        // DbgUtils.logf( "GameListItem.forceReload: rowid=%d", m_rowid );
        m_summary = null;
        setLoaded( false );
        // Apparently it's impossible to reliably cancel an existing
        // AsyncTask, so let it complete, but drop the results as soon
        // as we're back on the UI thread.
        ++m_loadingCount;
        new LoadItemTask().execute();
    }

    public void invalName()
    {
        setName();
    }

    @Override
    protected void onDraw( Canvas canvas ) 
    {
        super.onDraw( canvas );
        if ( DBUtils.ROWID_NOTFOUND != m_rowid ) {
            synchronized( s_invalRows ) {
                if ( s_invalRows.contains( m_rowid ) ) {
                    forceReload();
                }
            }
        }
    }

    private void update( boolean expanded, long lastMoveTime, boolean haveTurn,
                         boolean haveTurnLocal )
    {
        m_expanded = expanded;
        m_lastMoveTime = lastMoveTime;
        m_haveTurn = haveTurn;
        m_haveTurnLocal = haveTurnLocal;
        m_hideable = (LinearLayout)findViewById( R.id.hideable );
        m_name = (ExpiringTextView)findViewById( R.id.game_name );
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_expandButton.setOnClickListener( this );
        showHide();
    }

    public long getRowID()
    {
        return m_rowid;
    }

    public int getGroupPosition()
    {
        return m_groupPosition;
    }

    // View.OnClickListener interface
    public void onClick( View view ) {
        m_expanded = !m_expanded;
        DBUtils.setExpanded( m_rowid, m_expanded );
        showHide();
    }

    private void setLoaded( boolean loaded )
    {
        if ( loaded != m_loaded ) {
            m_loaded = loaded;
            // This should be enough to invalidate
            findViewById( R.id.view_unloaded )
                .setVisibility( loaded ? View.GONE : View.VISIBLE );
            findViewById( R.id.view_loaded )
                .setVisibility( loaded ? View.VISIBLE : View.GONE );
        }
    }

    private void showHide()
    {
        m_expandButton.setImageResource( m_expanded ?
                                         R.drawable.expander_ic_maximized :
                                         R.drawable.expander_ic_minimized);
        m_hideable.setVisibility( m_expanded? View.VISIBLE : View.GONE );

        m_name.setBackgroundColor( android.R.color.transparent );
        m_name.setPct( m_handler, m_haveTurn && !m_expanded, 
                       m_haveTurnLocal, m_lastMoveTime );
    }

    private String setName()
    {
        String state = null;    // hack to avoid calling summarizeState twice
        if ( null != m_summary ) {
            state = m_summary.summarizeState();
            TextView view = (TextView)findViewById( R.id.game_name );
            String value = null;
            switch ( m_fieldID ) {
            case R.string.game_summary_field_empty:
                break;
            case R.string.game_summary_field_language:
                value = 
                    DictLangCache.getLangName( m_context, 
                                               m_summary.dictLang );
                break;
            case R.string.game_summary_field_opponents:
                value = m_summary.playerNames();
                break;
            case R.string.game_summary_field_state:
                value = state;
                break;
            }

            if ( null != value ) {
                String name = GameUtils.getName( m_context, m_rowid );
                value = m_context.getString( R.string.str_game_namef, 
                                             name, value );
            } else {
                value = GameUtils.getName( m_context, m_rowid );
            }
                        
            view.setText( value );
        }
        return state;
    }

    private void setData( final GameSummary summary )
    {
        if ( null != summary ) {
            TextView view;
            String state = setName();

            setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View v ) {
                        m_cb.itemClicked( m_rowid, summary );
                    }
                } );

            LinearLayout list =
                (LinearLayout)findViewById( R.id.player_list );
            list.removeAllViews();
            boolean haveATurn = false;
            boolean haveALocalTurn = false;
            boolean[] isLocal = new boolean[1];
            for ( int ii = 0; ii < summary.nPlayers; ++ii ) {
                ExpiringLinearLayout tmp = (ExpiringLinearLayout)
                    Utils.inflate( m_context, R.layout.player_list_elem );
                view = (TextView)tmp.findViewById( R.id.item_name );
                view.setText( summary.summarizePlayer( ii ) );
                view = (TextView)tmp.findViewById( R.id.item_score );
                view.setText( String.format( "  %d", summary.scores[ii] ) );
                boolean thisHasTurn = summary.isNextToPlay( ii, isLocal );
                if ( thisHasTurn ) {
                    haveATurn = true;
                    if ( isLocal[0] ) {
                        haveALocalTurn = true;
                    }
                }
                tmp.setPct( m_handler, thisHasTurn, isLocal[0], 
                            summary.lastMoveTime );
                list.addView( tmp, ii );
            }

            view = (TextView)findViewById( R.id.state );
            view.setText( state );
            view = (TextView)findViewById( R.id.modtime );
            long lastMoveTime = summary.lastMoveTime;
            lastMoveTime *= 1000;

            DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
                                                            DateFormat.SHORT );
            view.setText( df.format( new Date( lastMoveTime ) ) );

            int iconID;
            ImageView marker =
                (ImageView)findViewById( R.id.msg_marker );
            CommsConnType conType = summary.conType;
            if ( CommsConnType.COMMS_CONN_RELAY == conType ) {
                iconID = R.drawable.relaygame;
            } else if ( CommsConnType.COMMS_CONN_BT == conType ) {
                iconID = android.R.drawable.stat_sys_data_bluetooth;
            } else if ( CommsConnType.COMMS_CONN_SMS == conType ) {
                iconID = android.R.drawable.sym_action_chat;
            } else {
                iconID = R.drawable.sologame;
            }
            marker.setImageResource( iconID );

            view = (TextView)findViewById( R.id.role );
            String roleSummary = summary.summarizeRole();
            if ( null != roleSummary ) {
                view.setText( roleSummary );
            } else {
                view.setVisibility( View.GONE );
            }

            boolean expanded = DBUtils.getExpanded( m_context, m_rowid );

            update( expanded, summary.lastMoveTime, haveATurn, 
                    haveALocalTurn );
        }
    }

    private class LoadItemTask extends AsyncTask<Void, Void, GameSummary> {
        @Override
        protected GameSummary doInBackground( Void... unused ) 
        {
            return DBUtils.getSummary( m_context, m_rowid, 150 );
        } // doInBackground

        @Override
        protected void onPostExecute( GameSummary summary )
        {
            if ( 0 == --m_loadingCount ) {
                m_summary = summary;
                setData( summary );
                setLoaded( null != m_summary );
                synchronized( s_invalRows ) {
                    s_invalRows.remove( m_rowid );
                }
            }
            // DbgUtils.logf( "LoadItemTask for row %d finished; "
            //                + "inval rows now %s", 
            //                m_rowid, invalRowsToString() );
        }
    } // class LoadItemTask

    public static GameListItem makeForRow( Context context, long rowid, 
                                           Handler handler, int groupPosition,
                                           int fieldID, 
                                           GameListAdapter.LoadItemCB cb )
    {
        GameListItem result = 
            (GameListItem)Utils.inflate( context, R.layout.game_list_item );
        result.init( handler, rowid, groupPosition, fieldID, cb );
        return result;
    }

    public static void inval( long rowid ) 
    {
        synchronized( s_invalRows ) {
            s_invalRows.add( rowid );
        }
        // DbgUtils.logf( "GameListItem.inval(rowid=%d); inval rows now %s",
        //                rowid, invalRowsToString() );
    }

    // private static String invalRowsToString()
    // {
    //     String[] strs;
    //     synchronized( s_invalRows ) {
    //         strs = new String[s_invalRows.size()];
    //         Iterator<Long> iter = s_invalRows.iterator();
    //         for ( int ii = 0; iter.hasNext(); ++ii ) {
    //             strs[ii] = String.format("%d", iter.next() );
    //         }
    //     }
    //     return TextUtils.join(",", strs );
    // }

}

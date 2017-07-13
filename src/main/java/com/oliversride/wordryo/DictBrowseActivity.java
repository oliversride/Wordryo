/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2011 by Eric House (xwords@eehouse.org).  All
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

import java.util.Arrays;

import junit.framework.Assert;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.SectionIndexer;
import android.widget.Spinner;
import android.widget.TextView;

import com.oliversride.wordryo.jni.JNIUtilsImpl;
import com.oliversride.wordryo.jni.XwJNI;

public class DictBrowseActivity extends XWListActivity
    implements View.OnClickListener, OnItemSelectedListener {

    private static final String DICT_NAME = "DICT_NAME";
    private static final String DICT_LOC = "DICT_LOC";

    private static final int MIN_LEN = 2;
    private static final int FINISH_ACTION = 1;

    private int m_dictClosure = 0;
    private int m_lang;
    private String m_name;
    private DictUtils.DictLoc m_loc;
    private Spinner m_minSpinner;
    private Spinner m_maxSpinner;
    private DBUtils.DictBrowseState m_browseState;
    private int m_minAvail;
    private int m_maxAvail;


// - Steps to reproduce the problem:
// Create ListView, set custom adapter which implements ListAdapter and
// SectionIndexer but do not extends BaseAdapter. Enable fast scroll in
// layout. This will effect in ClassCastException.


    private class DictListAdapter extends BaseAdapter
        implements SectionIndexer {

        private String[] m_prefixes;
        private int[] m_indices;
        private int m_nWords;

        public DictListAdapter()
        {
            super();

            XwJNI.dict_iter_setMinMax( m_dictClosure, m_browseState.m_minShown,
                                       m_browseState.m_maxShown );
            m_nWords = XwJNI.dict_iter_wordCount( m_dictClosure );

            int format = m_browseState.m_minShown == m_browseState.m_maxShown ?
                R.string.dict_browse_title1f : R.string.dict_browse_titlef;
            setTitle( Utils.format( DictBrowseActivity.this, format,
                                    m_name, m_nWords, m_browseState.m_minShown, 
                                    m_browseState.m_maxShown ));

            String desc = XwJNI.dict_iter_getDesc( m_dictClosure );
            if ( null != desc ) {
                TextView view = (TextView)findViewById( R.id.desc );
                Assert.assertNotNull( view );
                view.setVisibility( View.VISIBLE );
                view.setText( desc );
            }
        }

        public Object getItem( int position ) 
        {
            TextView text =
                (TextView)Utils.inflate( DictBrowseActivity.this,
                                         android.R.layout.simple_list_item_1 );
            String str = XwJNI.dict_iter_nthWord( m_dictClosure, position );
            if ( null != str ) {
                text.setText( str );
                text.setOnClickListener( DictBrowseActivity.this );
            }
            return text;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

        public long getItemId( int position ) { return position; }

        public int getCount() { 
            Assert.assertTrue( 0 != m_dictClosure );
            return m_nWords;
        }

        // SectionIndexer
        public int getPositionForSection( int section )
        {
            return m_indices[section];
        }
        
        public int getSectionForPosition( int position )
        {
            int section = Arrays.binarySearch( m_indices, position );
            if ( section < 0 ) {
                section *= -1;
            }
            if ( section >= m_indices.length ) {
                section = m_indices.length - 1;
            }
            return section;
        }
        
        public Object[] getSections() 
        {
            m_prefixes = XwJNI.dict_iter_getPrefixes( m_dictClosure );
            m_indices = XwJNI.dict_iter_getIndices( m_dictClosure );
            return m_prefixes;
        }
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        String name = null == intent? null:intent.getStringExtra( DICT_NAME );
        if ( null == name ) {
            finish();
        } else {
            m_name = name;
            m_loc = 
                DictUtils.DictLoc.values()[intent.getIntExtra( DICT_LOC, 0 )];
            m_lang = DictLangCache.getDictLangCode( this, name );

            String[] names = { name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( this, names );
            m_dictClosure = XwJNI.dict_iter_init( pairs.m_bytes[0],
                                                  name, pairs.m_paths[0],
                                                  JNIUtilsImpl.get(this) );

            m_browseState = DBUtils.dictsGetOffset( this, name, m_loc );
            boolean newState = null == m_browseState;
            if ( newState ) {
                m_browseState = new DBUtils.DictBrowseState();
                m_browseState.m_pos = 0;
                m_browseState.m_top = 0;
            }
            if ( null == m_browseState.m_counts ) {
                m_browseState.m_counts = 
                    XwJNI.dict_iter_getCounts( m_dictClosure );
            }

            if ( null == m_browseState.m_counts ) {
                // empty dict?  Just close down for now.  Later if
                // this is extended to include tile info -- it should
                // be -- then use an empty list elem and disable
                // search/minmax stuff.
                String msg = Utils.format( this, R.string.alert_empty_dictf,
                                           name );
                showOKOnlyDialogThen( msg, FINISH_ACTION );
            } else {
                figureMinMax( m_browseState.m_counts );
                if ( newState ) {
                    m_browseState.m_minShown = m_minAvail;
                    m_browseState.m_maxShown = m_maxAvail;
                }

                setContentView( R.layout.dict_browser );

                Button button = (Button)findViewById( R.id.search_button );
                button.setOnClickListener( new View.OnClickListener() {
                        public void onClick( View view )
                        {
                            findButtonClicked();
                        }
                    } );

                setUpSpinners();

                setListAdapter( new DictListAdapter() );
                getListView().setFastScrollEnabled( true );
                getListView().setSelectionFromTop( m_browseState.m_pos,
                                                   m_browseState.m_top );
            }
        }
    } // onCreate

    @Override
    protected void onPause()
    {
        if ( null != m_browseState ) { // already saved?
            ListView list = getListView();
            m_browseState.m_pos = list.getFirstVisiblePosition();
            View view = list.getChildAt( 0 );
            m_browseState.m_top = (view == null) ? 0 : view.getTop();
            m_browseState.m_prefix = getFindText();
            DBUtils.dictsSetOffset( this, m_name, m_loc, m_browseState );
            m_browseState = null;
        }

        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        if ( null == m_browseState ) {
            m_browseState = DBUtils.dictsGetOffset( this, m_name, m_loc );
        }
        setFindText( m_browseState.m_prefix );
    }

    @Override
    protected void onDestroy()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        m_dictClosure = 0;
        super.onDestroy();
    }


    // Just in case onDestroy didn't get called....
    @Override
    public void finalize()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        try {
            super.finalize();
        } catch ( java.lang.Throwable err ){
            DbgUtils.logf( "%s", err.toString() );
        }
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        TextView text = (TextView)view;
        String[] words = { text.getText().toString() };
        launchLookup( words, m_lang, true );
    }


    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    public void onItemSelected( AdapterView<?> parent, View view, 
                                int position, long id )
    {
        TextView text = (TextView)view;
        int newval = Integer.parseInt( text.getText().toString() );
        if ( parent == m_minSpinner ) {
            setMinMax( newval, m_browseState.m_maxShown );
        } else if ( parent == m_maxSpinner ) {
            setMinMax( m_browseState.m_minShown, newval );
        }
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        Assert.assertTrue( FINISH_ACTION == id ); 
        finish();
    }

    private void findButtonClicked()
    {
        String text = getFindText();
        if ( null != text && 0 < text.length() ) {
            m_browseState.m_prefix = text;
            showPrefix();
        }
    }

    private String getFindText()
    {
        EditText edit = (EditText)findViewById( R.id.word_edit );
        return edit.getText().toString();
    }

    private void setFindText( String text )
    {
        EditText edit = (EditText)findViewById( R.id.word_edit );
        edit.setText( text );
    }

    private void showPrefix() 
    {
        String text = m_browseState.m_prefix;
        if ( null != text && 0 < text.length() ) {
            int pos = XwJNI.dict_iter_getStartsWith( m_dictClosure, text );
            if ( 0 <= pos ) {
                getListView().setSelection( pos );
            } else {
                DbgUtils.showf( this, R.string.dict_browse_nowordsf, 
                                m_name, text );
            }
        }
    }

    private void setMinMax( int min, int max )
    {
        // I can't make a second call to setListAdapter() work, nor
        // does notifyDataSetChanged do anything toward refreshing the
        // adapter/making it recognized a changed dataset.  So, as a
        // workaround, relaunch the activity with different
        // parameters.
        if ( m_browseState.m_minShown != min || 
             m_browseState.m_maxShown != max ) {

            m_browseState.m_pos = 0;
            m_browseState.m_top = 0;
            m_browseState.m_minShown = min;
            m_browseState.m_maxShown = max;
            m_browseState.m_prefix = getFindText();
            DBUtils.dictsSetOffset( this, m_name, m_loc, m_browseState );
            m_browseState = null;

            startActivity( getIntent() );

            finish();
        }
    }

    private void figureMinMax( int[] counts )
    {
        Assert.assertTrue( counts.length == XwJNI.MAX_COLS_DICT + 1 );
        m_minAvail = 0;
        while ( 0 == counts[m_minAvail] ) {
            ++m_minAvail;
        }
        m_maxAvail = XwJNI.MAX_COLS_DICT;
        while ( 0 == counts[m_maxAvail] ) { // 
            --m_maxAvail;
        }
    }

    private void makeAdapter( Spinner spinner, int min, int max, int cur )
    {
        int sel = -1;
        String[] nums = new String[max - min + 1];
        for ( int ii = 0; ii < nums.length; ++ii ) {
            int val = min + ii;
            if ( val == cur ) {
                sel = ii;
            }
            nums[ii] = String.format( "%d", min + ii );
        }
        ArrayAdapter<String> adapter = new
            ArrayAdapter<String>( this, 
                                  //android.R.layout.simple_spinner_dropdown_item,
                                  android.R.layout.simple_spinner_item,
                                  nums );
        adapter.setDropDownViewResource( android.R.layout.
                                         simple_spinner_dropdown_item );
        spinner.setAdapter( adapter );
        spinner.setSelection( sel );
    }

    private void setUpSpinners()
    {
        // Min and max-length spinners.  To avoid empty lists,
        // don't allow min to exceed max.  Do that by making the
        // current max the largest min allowed, and the current
        // min the smallest max allowed.
        m_minSpinner = (Spinner)findViewById( R.id.wordlen_min );
        makeAdapter( m_minSpinner, m_minAvail, m_browseState.m_maxShown, 
                     m_browseState.m_minShown );
        m_minSpinner.setOnItemSelectedListener( this );

        m_maxSpinner = (Spinner)findViewById( R.id.wordlen_max );
        makeAdapter( m_maxSpinner, m_browseState.m_minShown, 
                     m_maxAvail, m_browseState.m_maxShown );
        m_maxSpinner.setOnItemSelectedListener( this );
    }


    public static void launch( Context caller, String name, 
                               DictUtils.DictLoc loc )
    {
        Intent intent = new Intent( caller, DictBrowseActivity.class );
        intent.putExtra( DICT_NAME, name );
        intent.putExtra( DICT_LOC, loc.ordinal() );
        caller.startActivity( intent );
    }

    public static void launch( Context caller, String name )
    {
        DictUtils.DictLoc loc = DictUtils.getDictLoc( caller, name );
        launch( caller, name, loc );
    }
}

/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

package com.oliversride.wordryo;

import java.util.Arrays;
import java.util.HashSet;

import junit.framework.Assert;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.ExpandableListView.ExpandableListContextMenuInfo;
import android.widget.TextView;

import com.oliversride.wordryo.DictUtils.DictAndLoc;
import com.oliversride.wordryo.DictUtils.DictLoc;

public class DictsActivity extends XWExpandableListActivity 
    implements View.OnClickListener, XWListItem.DeleteCallback,
               MountEventReceiver.SDCardNotifiee, DlgDelegate.DlgClickNotify,
               DictImportActivity.DownloadFinishedListener {

    private static final String DICT_DOLAUNCH = "do_launch";
    private static final String DICT_LANG_EXTRA = "use_lang";
    private static final String DICT_NAME_EXTRA = "use_dict";
    private static final String PACKED_POSITION = "packed_position";
    private static final String DELETE_DICT = "delete_dict";
    private static final String NAME = "name";
    private static final String LANG = "lang";
    private static final String MOVEFROMLOC = "movefromloc";

    private HashSet<String> m_closedLangs;

    // For new callback alternative
    private static final int DELETE_DICT_ACTION = 1;

    private static final int MOVE_DICT = DlgDelegate.DIALOG_LAST + 1;
    private static final int SET_DEFAULT = DlgDelegate.DIALOG_LAST + 2;
    private static final int DICT_OR_DECLINE = DlgDelegate.DIALOG_LAST + 3;
    private int m_lang = 0;
    private String[] m_langs;
    private String m_name = null;
    private String m_deleteDict = null;
    private String m_download;
    private ExpandableListView m_expView;
    private String[] m_locNames;
    private DictListAdapter m_adapter;

    private long m_packedPosition;
    private DictLoc m_moveFromLoc;
    private int m_moveFromItem;
    private int m_moveToItm;
    private boolean m_launchedForMissing = false;

    private LayoutInflater m_factory;

    private class DictListAdapter implements ExpandableListAdapter {
        private Context m_context;
        private XWListItem[][] m_cache;

        public DictListAdapter( Context context ) {
            m_context = context;
        }

        public boolean areAllItemsEnabled() { return false; }

        public Object getChild( int groupPosition, int childPosition )
        {
            return null;
        }

        public long getChildId( int groupPosition, int childPosition )
        {
            return childPosition;
        }

        public View getChildView( int groupPosition, int childPosition, 
                                  boolean isLastChild, View convertView, 
                                  ViewGroup parent)
        {
            return getChildView( groupPosition, childPosition );
        }

        private View getChildView( int groupPosition, int childPosition )
        {
            XWListItem view = null;
            if ( null != m_cache && null != m_cache[groupPosition] ) {
                view = m_cache[groupPosition][childPosition];
            }

            if ( null == view ) {
                view = (XWListItem)
                    m_factory.inflate( R.layout.list_item, null );

                int lang = (int)getGroupId( groupPosition );
                DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, 
                                                                   lang );

                if ( null != dals && childPosition < dals.length ) {
                    DictAndLoc dal;
                    dal = dals[childPosition];
                    view.setText( dal.name );

                    DictLoc loc = dal.loc;
                    view.setComment( m_locNames[loc.ordinal()] );
                    view.cache( loc );
                    if ( DictLoc.BUILT_IN != loc ) {
                        view.setDeleteCallback( DictsActivity.this );
                    }
                } else {
                    view.setText( m_download );
                }

                addToCache( groupPosition, childPosition, view );
                view.setOnClickListener( DictsActivity.this );
            }
            return view;
        }

        public int getChildrenCount( int groupPosition )
        {
            int lang = (int)getGroupId( groupPosition );
            DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, lang );
            int result = 0; // 1;     // 1 for the download option
            if ( null != dals ) {
                result += dals.length;
            }
            return result;
        }

        public long getCombinedChildId( long groupId, long childId )
        {
            return groupId << 16 | childId;
        }

        public long getCombinedGroupId( long groupId )
        {
            return groupId;
        }

        public Object getGroup( int groupPosition )
        {
            return null;
        }

        public int getGroupCount()
        {
            return m_langs.length;
        }

        public long getGroupId( int groupPosition )
        {
            int lang = DictLangCache.getLangLangCode( m_context, 
                                                      m_langs[groupPosition] );
            return lang;
        }

        public View getGroupView( int groupPosition, boolean isExpanded, 
                                  View convertView, ViewGroup parent )
        {
            View row = 
                Utils.inflate(DictsActivity.this, 
                              android.R.layout.simple_expandable_list_item_1 );
            TextView view = (TextView)row.findViewById( android.R.id.text1 );
            view.setText( m_langs[groupPosition] );
            return view;
        }

        public boolean hasStableIds() { return false; }
        public boolean isChildSelectable( int groupPosition, 
                                          int childPosition ) { return true; }
        public boolean isEmpty() { return false; }
        public void onGroupCollapsed(int groupPosition)
        {
            m_closedLangs.add( m_langs[groupPosition] );
            saveClosed();
        }
        public void onGroupExpanded(int groupPosition){
            m_closedLangs.remove( m_langs[groupPosition] );
            saveClosed();
        }
        public void registerDataSetObserver( DataSetObserver obs ){}
        public void unregisterDataSetObserver( DataSetObserver obs ){}

        protected XWListItem getSelChildView()
        {
            int groupPosition = 
                ExpandableListView.getPackedPositionGroup( m_packedPosition );
            int childPosition = 
                ExpandableListView.getPackedPositionChild( m_packedPosition );
            return (XWListItem)getChildView( groupPosition, childPosition );
        }

        private void addToCache( int group, int child, XWListItem view )
        {
            if ( null == m_cache ) {
                m_cache = new XWListItem[getGroupCount()][];
            }
            if ( null == m_cache[group] ) {
                m_cache[group] = new XWListItem[getChildrenCount(group)];
            }
            Assert.assertTrue( null == m_cache[group][child] );
            m_cache[group][child] = view;
        }
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        OnClickListener lstnr, lstnr2;
        Dialog dialog;
        String format;
        String message;
        boolean doRemove = true;

        switch( id ) {
        case MOVE_DICT:
            message = Utils.format( this, R.string.move_dictf,
                                    m_adapter.getSelChildView().getText() );

            OnClickListener newSelLstnr =
                new OnClickListener() {
                    public void onClick( DialogInterface dlgi, int item ) {
                        m_moveToItm = item;
                        AlertDialog dlg = (AlertDialog)dlgi;
                        Button btn = 
                            dlg.getButton( AlertDialog.BUTTON_POSITIVE ); 
                        btn.setEnabled( m_moveToItm != m_moveFromItem );
                    }
                };

            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        XWListItem rowView = m_adapter.getSelChildView();
                        Assert.assertTrue( m_moveToItm != m_moveFromItem );
                        DictLoc toLoc = itemToRealLoc( m_moveToItm );
                        if ( DictUtils.moveDict( DictsActivity.this,
                                                 rowView.getText(),
                                                 m_moveFromLoc,
                                                 toLoc ) ) {
                            rowView.setComment( m_locNames[toLoc.ordinal()] );
                            rowView.cache( toLoc );
                            rowView.invalidate();
                            DBUtils.dictsMoveInfo( DictsActivity.this,
                                                   rowView.getText(),
                                                   m_moveFromLoc, toLoc );
                        } else {
                            DbgUtils.logf( "moveDict(%s) failed", 
                                           rowView.getText() );
                        }
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( message )
                .setSingleChoiceItems( makeDictDirItems(), m_moveFromItem, 
                                       newSelLstnr )
                .setPositiveButton( R.string.button_move, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;

        case SET_DEFAULT:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( DialogInterface.BUTTON_NEGATIVE == item
                             || DialogInterface.BUTTON_POSITIVE == item ) {
                            setDefault( R.string.key_default_dict );
                        }
                        if ( DialogInterface.BUTTON_NEGATIVE == item 
                             || DialogInterface.BUTTON_NEUTRAL == item ) {
                            setDefault( R.string.key_default_robodict );
                        }
                    }
                };
            XWListItem rowView = m_adapter.getSelChildView();
            String lang = 
                DictLangCache.getLangName( this, rowView.getText() );
            message = getString( R.string.set_default_messagef, lang );
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( message )
                .setPositiveButton( R.string.button_default_human, lstnr )
                .setNeutralButton( R.string.button_default_robot, lstnr )
                .setNegativeButton( R.string.button_default_both, lstnr )
                .create();
            break;

        case DICT_OR_DECLINE:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        Intent intent = getIntent();
                        int lang = intent.getIntExtra( MultiService.LANG, -1 );
                        String name = intent.getStringExtra( MultiService.DICT );
                        m_launchedForMissing = true;
                        DictImportActivity
                            .downloadDictInBack( DictsActivity.this, lang, 
                                                 name, DictsActivity.this );
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        finish();
                    }
                };

            dialog = MultiService.missingDictDialog( this, getIntent(), 
                                                     lstnr, lstnr2 );
            break;

        default:
            dialog = super.onCreateDialog( id );
            doRemove = false;
            break;
        }

        if ( doRemove && null != dialog ) {
            Utils.setRemoveOnDismiss( this, dialog, id );
        }

        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );

        if ( MOVE_DICT == id ) {
            // The move button should always start out disabled
            // because the selected location should be where it
            // currently is.
            ((AlertDialog)dialog).getButton( AlertDialog.BUTTON_POSITIVE )
                .setEnabled( false );
        }
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        m_closedLangs = new HashSet<String>();
        String[] closed = XWPrefs.getClosedLangs( this );
        if ( null != closed ) {
            for ( String str : closed ) {
                m_closedLangs.add( str );
            }
        }

        Resources res = getResources();
        m_locNames = res.getStringArray( R.array.loc_names );

        m_factory = LayoutInflater.from( this );

        m_download = getString( R.string.download_dicts );
            
        setContentView( R.layout.dict_browse );
        m_expView = getExpandableListView();
        registerForContextMenu( m_expView );

        Button download = (Button)findViewById( R.id.download );
        download.setOnClickListener( this );

        mkListAdapter();

        // showNotAgainDlg( R.string.not_again_dicts, 
        //                  R.string.key_notagain_dicts );

        Intent intent = getIntent();
        if ( null != intent ) {
            if ( MultiService.isMissingDictIntent( intent ) ) {
                showDialog( DICT_OR_DECLINE );
            } else {
                boolean downloadNow = intent.getBooleanExtra( DICT_DOLAUNCH, false );
                if ( downloadNow ) {
                    int lang = intent.getIntExtra( DICT_LANG_EXTRA, 0 );
                    String name = intent.getStringExtra( DICT_NAME_EXTRA );
                    startDownload( lang, name );
                }

                downloadNewDict( intent );
            }
        }
    } // onCreate

    @Override
    protected void onResume()
    {
        super.onResume();
        MountEventReceiver.register( this );

        mkListAdapter();
        expandGroups();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );

        outState.putLong( PACKED_POSITION, m_packedPosition );
        outState.putString( NAME, m_name );
        outState.putInt( LANG, m_lang );
        outState.putString( DELETE_DICT, m_deleteDict );
        if ( null != m_moveFromLoc ) {
            outState.putInt( MOVEFROMLOC, m_moveFromLoc.ordinal() );
        }
    }

    private void getBundledData( Bundle savedInstanceState )
    {
        if ( null != savedInstanceState ) {
            m_packedPosition = savedInstanceState.getLong( PACKED_POSITION );
            m_name = savedInstanceState.getString( NAME );
            m_lang = savedInstanceState.getInt( LANG );
            m_deleteDict = savedInstanceState.getString( DELETE_DICT );

            int tmp = savedInstanceState.getInt( MOVEFROMLOC, -1 );
            if ( -1 != tmp ) {
                m_moveFromLoc = DictLoc.values()[tmp];
            }
        }
    }

    @Override
    protected void onStop() {
        MountEventReceiver.unregister( this );
        super.onStop();
    }

    public void onClick( View view ) 
    {
        if ( view instanceof Button ) {
            startDownload( 0, null );
        } else {
            XWListItem item = (XWListItem)view;
            DictBrowseActivity.launch( this, item.getText(), 
                                       (DictLoc)item.getCached() );
        }
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) 
    {
        super.onCreateContextMenu( menu, view, menuInfo );

        ExpandableListView.ExpandableListContextMenuInfo info
            = (ExpandableListView.ExpandableListContextMenuInfo)menuInfo;
        long packedPosition = info.packedPosition;
        int childPosition = ExpandableListView.
            getPackedPositionChild( packedPosition );
        // int groupPosition = ExpandableListView.
        //     getPackedPositionGroup( packedPosition );
        // DbgUtils.logf( "onCreateContextMenu: group: %d; child: %d", 
        //             groupPosition, childPosition );

        // We don't have a menu yet for languages, just for their dict
        // children
        if ( childPosition >= 0 ) {
            MenuInflater inflater = getMenuInflater();
            inflater.inflate( R.menu.dicts_item_menu, menu );
            
            XWListItem row = (XWListItem)info.targetView;
            DictLoc loc = (DictLoc)row.getCached();
            if ( loc == DictLoc.BUILT_IN
                 || ! DictUtils.haveWriteableSD() ) {
                menu.removeItem( R.id.dicts_item_move );
            }

            String title = getString( R.string.game_item_menu_titlef,
                                      row.getText() );
            menu.setHeaderTitle( title );
        }
    }
   
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = false;
        ExpandableListContextMenuInfo info = null;
        try {
            info = (ExpandableListContextMenuInfo)item.getMenuInfo();
        } catch (ClassCastException cce) {
            DbgUtils.loge( cce );
            return false;
        }
        
        m_packedPosition = info.packedPosition;

        int id = item.getItemId();
        switch( id ) {
        case R.id.dicts_item_move:
            askMoveDict( (XWListItem)info.targetView );
            break;
        case R.id.dicts_item_select:
            showDialog( SET_DEFAULT );
            break;
        }

        return handled;
    }

    private void downloadNewDict( Intent intent )
    {
        int loci = intent.getIntExtra( UpdateCheckReceiver.NEW_DICT_LOC, 0 );
        if ( 0 < loci ) {
            String url = 
                intent.getStringExtra( UpdateCheckReceiver.NEW_DICT_URL );
            DictImportActivity.downloadDictInBack( this, url );
            finish();
        }
    }

    private void setDefault( int keyId )
    {
        XWListItem view = m_adapter.getSelChildView();
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( this );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        String name = view.getText();
        editor.putString( key, name );
        editor.commit();
    }

    // Move dict.  Put up dialog asking user to confirm move from XX
    // to YY.  So we need both XX and YY.  There may be several
    // options for YY?
    private void askMoveDict( XWListItem item )
    {
        m_moveFromLoc = (DictLoc)item.getCached();
        showDialog( MOVE_DICT );
    }

    // XWListItem.DeleteCallback interface
    public void deleteCalled( XWListItem item )
    {
        String dict = item.getText();
        String msg = getString( R.string.confirm_delete_dictf, dict );

        m_deleteDict = dict;
        m_moveFromLoc = (DictLoc)item.getCached();

        // When and what to warn about.  First off, if there's another
        // identical dict, simply confirm.  Or if nobody's using this
        // dict *and* it's not the last of a language that somebody's
        // using, simply confirm.  If somebody is using it, then we
        // want different warnings depending on whether it's the last
        // available dict in its language.

        if ( 1 < DictLangCache.getDictCount( this, dict ) ) {
            // there's another; do nothing
        } else {
            int fmtid = 0;
            int langcode = DictLangCache.getDictLangCode( this, dict );
            DictAndLoc[] langDals = DictLangCache.getDALsHaveLang( this, 
                                                                   langcode );
            int nUsingLang = DBUtils.countGamesUsingLang( this, langcode );

            if ( 1 == langDals.length ) { // last in this language?
                if ( 0 < nUsingLang ) {
                    fmtid = R.string.confirm_deleteonly_dictf;
                }
            } else if ( 0 < DBUtils.countGamesUsingDict( this, dict ) ) {
                fmtid = R.string.confirm_deletemore_dictf;
            }
            if ( 0 != fmtid ) {
                msg += getString( fmtid, DictLangCache.
                                  getLangName( this, langcode ) );
            }
        }

        showConfirmThen( msg, R.string.button_delete, DELETE_DICT_ACTION );
    }

    // MountEventReceiver.SDCardNotifiee interface
    public void cardMounted( boolean nowMounted )
    {
        DbgUtils.logf( "DictsActivity.cardMounted(%b)", nowMounted );
        // post so other SDCardNotifiee implementations get a chance
        // to process first: avoid race conditions
        post( new Runnable() {
                public void run() {
                    mkListAdapter();
                    expandGroups();
                }
            } );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( int id, int which )
    {
        switch( id ) {
        case DELETE_DICT_ACTION:
            if ( DialogInterface.BUTTON_POSITIVE == which ) {
                deleteDict( m_deleteDict, m_moveFromLoc );
            }
            break;
        default:
            Assert.fail();
        }
    }

    private DictLoc itemToRealLoc( int item )
    {
        item += DictLoc.INTERNAL.ordinal();
        return DictLoc.values()[item];
    }

    private void deleteDict( String dict, DictLoc loc )
    {
        DictUtils.deleteDict( this, dict, loc );
        DictLangCache.inval( this, dict, loc, false );
        mkListAdapter();
        expandGroups();
    }

    private void startDownload( int lang, String name )
    {
        Intent intent = mkDownloadIntent( this, lang, name );
        startDownload( intent );
    }

    private void startDownload( Intent downloadIntent )
    {
        try {
            startActivity( downloadIntent );
        } catch ( android.content.ActivityNotFoundException anfe ) {
            Utils.showToast( this, R.string.no_download_warning );
        }
    }

    private void mkListAdapter()
    {
        m_langs = DictLangCache.listLangs( this );
        Arrays.sort( m_langs );
        m_adapter = new DictListAdapter( this );
        setListAdapter( m_adapter );
    }

    private void expandGroups()
    {
        for ( int ii = 0; ii < m_langs.length; ++ii ) {
            boolean open = true;
            String lang = m_langs[ii];
            if ( ! m_closedLangs.contains( lang ) ) {
                m_expView.expandGroup( ii );
            }
        }
    }

    private void saveClosed()
    {
        String[] asArray = m_closedLangs.toArray( new String[m_closedLangs.size()] );
        XWPrefs.setClosedLangs( this, asArray );
    }


    private String[] makeDictDirItems() 
    {
        boolean showDownload = DictUtils.haveDownloadDir( this );
        int nItems = showDownload ? 3 : 2;
        int nextI = 0;
        String[] items = new String[nItems];
        for ( int ii = 0; ii < 3; ++ii ) {
            DictLoc loc = itemToRealLoc(ii);
            if ( !showDownload && DictLoc.DOWNLOAD == loc ) {
                continue;
            }
            if ( loc.equals( m_moveFromLoc ) ) {
                m_moveFromItem = nextI;
            }
            items[nextI++] = m_locNames[loc.ordinal()];
        }
        return items;
    }

    private static Intent mkDownloadIntent( Context context, String dict_url )
    {
        Uri uri = Uri.parse( dict_url );
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    private static Intent mkDownloadIntent( Context context,
                                            int lang, String dict )
    {
        String dict_url = Utils.makeDictUrl( context, lang, dict );
        return mkDownloadIntent( context, dict_url );
    }

    public static void launchAndDownload( Activity activity, int lang, 
                                          String name )
    {
        Intent intent = new Intent( activity, DictsActivity.class );
        intent.putExtra( DICT_DOLAUNCH, true );
        if ( lang > 0 ) {
            intent.putExtra( DICT_LANG_EXTRA, lang );
        }
        if ( null != name ) {
            Assert.assertTrue( lang != 0 );
            intent.putExtra( DICT_NAME_EXTRA, name );
        }

        activity.startActivity( intent );
    }

    public static void launchAndDownload( Activity activity, int lang )
    {
        launchAndDownload( activity, lang, null );
    }

    public static void launchAndDownload( Activity activity )
    {
        launchAndDownload( activity, 0, null );
    }

    // DictImportActivity.DownloadFinishedListener interface
    public void downloadFinished( String name, final boolean success )
    {
        if ( m_launchedForMissing ) {
            post( new Runnable() {
                    public void run() {
                        if ( success ) {
                            Intent intent = getIntent();
                            if ( MultiService.returnOnDownload( DictsActivity.this,
                                                                intent ) ) {
                                finish();
                            }
                        } else {
                            Utils.showToast( DictsActivity.this, 
                                             R.string.download_failed );
                        }
                    }
                } );
        }
    }


}
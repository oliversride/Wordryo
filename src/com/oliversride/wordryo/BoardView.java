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

import junit.framework.Assert;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.FloatMath;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import com.oliversride.wordryo.jni.BoardHandler;
import com.oliversride.wordryo.jni.CommonPrefs;
import com.oliversride.wordryo.jni.CommsAddrRec;
import com.oliversride.wordryo.jni.CurGameInfo;
import com.oliversride.wordryo.jni.DrawCtx;
import com.oliversride.wordryo.jni.DrawScoreInfo;
import com.oliversride.wordryo.jni.JNIThread;
import com.oliversride.wordryo.jni.JNIThread.JNICmd;
import com.oliversride.wordryo.jni.SyncedDraw;
import com.oliversride.wordryo.jni.XwJNI;

public class BoardView extends RelativeLayout implements DrawCtx, BoardHandler,
                                               SyncedDraw {
	private static final String TAG = "BoardView";
    private static final float MIN_FONT_DIPS = 14.0f;
    private static final int MULTI_INACTIVE = -1;

    private static Bitmap s_bitmap;    // the board
    private static final int IN_TRADE_ALPHA = 0x3FFFFFFF;
    private static final int PINCH_THRESHOLD = 40;
    private static final int SCORE_HT_DROP = 2;
    private static final boolean DEBUG_DRAWFRAMES = false;

    private Context m_context;
    private Paint m_drawPaint;
    private Paint m_fillPaint;
    private Paint m_strokePaint;
    private Paint m_bigTilePaint;
    private int m_defaultFontHt;
    private int m_mediumFontHt;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private CommonPrefs m_prefs;
    private int m_layoutWidth;
    private int m_layoutHeight;
    private Canvas m_canvas;    // owns the bitmap
    private int m_trayOwner = -1;
    private Drawable m_rightArrow;
    private Drawable m_downArrow;
    private boolean m_blackArrow;
    private boolean m_inTrade = false;
    private boolean m_hasSmallScreen;
    // m_backgroundUsed: alpha not set ensures inequality
    private int m_backgroundUsed = 0x00000000;
    private boolean m_darkOnLight;
    private Drawable m_origin;
    private JNIThread m_jniThread;
    private XWActivity m_parent;
    private String[][] m_scores;
    private String[] m_dictChars;
    private Rect m_boundsScratch;
    private String m_remText;
    private int m_dictPtr = 0;
    private int m_lastSecsLeft;
    private int m_lastTimerPlayer;
    private int m_pendingScore;
    private CommsAddrRec.CommsConnType m_connType = 
        CommsAddrRec.CommsConnType.COMMS_CONN_NONE;
    private int m_lastSpacing = MULTI_INACTIVE;


    // FontDims: exists to translate space available to the largest
    // font we can draw within that space taking advantage of our use
    // being limited to a known small subset of glyphs.  We need two
    // numbers from this: the textHeight to pass to Paint.setTextSize,
    // and the descent to use when drawing.  Both can be calculated
    // proportionally.  We know the ht we passed to Paint to get the
    // height we've now measured; that gives a percent to multiply any
    // future wantHt by.  Ditto for the descent
    private class FontDims {
        FontDims( float askedHt, int topRow, int bottomRow, float width ) {
            // DbgUtils.logf( "FontDims(): askedHt=" + askedHt );
            // DbgUtils.logf( "FontDims(): topRow=" + topRow );
            // DbgUtils.logf( "FontDims(): bottomRow=" + bottomRow );
            // DbgUtils.logf( "FontDims(): width=" + width );
            float gotHt = bottomRow - topRow + 1;
            m_htProportion = (gotHt / askedHt);
            Assert.assertTrue( (bottomRow+1) >= askedHt );
            float descent = (bottomRow+1) - askedHt;
            // DbgUtils.logf( "descent: " + descent );
            m_descentProportion = (descent / askedHt);
            Assert.assertTrue( m_descentProportion >= 0 );
            m_widthProportion = (width / askedHt);
            // DbgUtils.logf( "m_htProportion: " + m_htProportion );
            // DbgUtils.logf( "m_descentProportion: " + m_descentProportion );
        }
        public float m_htProportion;
        private float m_descentProportion;
        private float m_widthProportion;
        int heightFor( int ht ) { return (int)(ht / m_htProportion); }
        int descentFor( int ht ) { return (int)(ht * m_descentProportion); }
        int widthFor( int width ) { return (int)(width / m_widthProportion); }
    }
    private FontDims m_fontDims;

    private static final int BLACK = 0xFF000000;
    private static final int WHITE = 0xFFFFFFFF;
    private static final int FRAME_GREY = 0xFF101010;
    private int[] m_bonusColors;
    private int[] m_playerColors;
    private int[] m_otherColors;
    private String[] m_bonusSummaries;

    // called when inflating xml
    public BoardView( Context context, AttributeSet attrs ) 
    {
        super( context, attrs );
        m_context = context;
        
        m_hasSmallScreen = Utils.hasSmallScreen( context );

      	BIG_TILE_INSET = 3;
      	BIG_TILE_FRAME = 2;
      	SMALL_TILE_INSET = 1;
      	SMALL_TILE_FRAME = 1;
      	BONUS_TEXT_INSET = 0.25f;
      	TILE_TEXT_INSET = 0.18f;
      	TILE_TEXT_OFFSET = 0.05f;
      	TILE_VALUE_SCALE = 0.36f;
      	TILE_VALUE_OFFSET_Y = 0.07f;
      	TILE_VALUE_OFFSET_X = 0.02f;
      	TWO_DIGIT_VALUE_OFFSET = 0.06f;
      	TWO_DIGIT_TEXT_OFFSET = 0.14f;
        RADIUSX = 4f;
        RADIUSY = 4f;
      	BIG_RADIUS_X = 4f;
      	BIG_RADIUS_Y = 4f;
      	STAR_INSET = 3;
      	STROKE_TILE_DRAG = 4;
      	IN_MOVE_OFFSET = 2.1f;

      	final boolean largeLayout = ("layout-large".equals(getTag()));
      	if ( largeLayout ){
      		BIG_TILE_INSET = 5;
      		BIG_TILE_FRAME = 3;
      		SMALL_TILE_INSET = 2;
      		SMALL_TILE_FRAME = 2;
      		BONUS_TEXT_INSET = 0.25f;
          	TILE_TEXT_INSET = 0.16f;
      		RADIUSX = 4f;
      		RADIUSY = 4f;
      		BIG_RADIUS_X = 8f;
      		BIG_RADIUS_Y = 8f;
      		STAR_INSET = 6;
      		IN_MOVE_OFFSET = 1.6f;
      	}

      	DisplayMetrics metrics = getResources().getDisplayMetrics();
      	final boolean lowDensity = (DisplayMetrics.DENSITY_LOW == metrics.densityDpi);
      	final boolean mediumDensity = (DisplayMetrics.DENSITY_MEDIUM == metrics.densityDpi);
      	final boolean sharper = lowDensity || (mediumDensity && largeLayout);
      	if ( sharper ){
      		RADIUSX = RADIUSX / 2;
      		RADIUSY = RADIUSY / 2;
      		BIG_RADIUS_X = BIG_RADIUS_X / 2;
      		BIG_RADIUS_Y = BIG_RADIUS_Y / 2;
      		if ( largeLayout ){
      			SMALL_TILE_INSET = 1;
      		}
  	      	STROKE_TILE_DRAG = 2;
      	}
      	final boolean mediumLayout = !m_hasSmallScreen && !largeLayout;
      	final boolean shrinkTileText = lowDensity && mediumLayout;
      	if ( shrinkTileText ){
          	TILE_TEXT_INSET = 0.2f;
          	TILE_TEXT_OFFSET = 0.09f;
  	      	TWO_DIGIT_TEXT_OFFSET = 0.18f;
  	      	STROKE_TILE_DRAG = 2;
      	}
      	
        final float scale = getResources().getDisplayMetrics().density;
        m_defaultFontHt = (int)(MIN_FONT_DIPS * scale + 0.5f);
        m_mediumFontHt = m_defaultFontHt * 3 / 2;

        m_drawPaint = new Paint();
        m_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );        
        m_strokePaint = new Paint();
        m_strokePaint.setAntiAlias(true);
        m_strokePaint.setStyle( Paint.Style.STROKE );
        m_strokePaint.setStrokeWidth(SMALL_TILE_FRAME);       
        m_bigTilePaint = new Paint();
        m_bigTilePaint.setAntiAlias(true);
        m_bigTilePaint.setStyle( Paint.Style.STROKE );
        m_bigTilePaint.setStrokeWidth(BIG_TILE_FRAME);
//        float curWidth = m_strokePaint.getStrokeWidth();
//        curWidth *= 2;
//        if ( curWidth < 2 ) {
//            curWidth = 2;
//        }
//      m_strokePaint.setStrokeWidth( curWidth );
        
        Resources res = getResources();
        m_origin = res.getDrawable( R.drawable.star5 );

        m_boundsScratch = new Rect();

        m_prefs = CommonPrefs.get(context);
        m_playerColors = m_prefs.playerColors;
        m_bonusColors = m_prefs.bonusColors;
        m_otherColors = m_prefs.otherColors;

        m_bonusSummaries = new String[5];
        int[] ids = { R.string.bonus_l2x_summary,
                      R.string.bonus_w2x_summary ,
                      R.string.bonus_l3x_summary,
                      R.string.bonus_w3x_summary };
        for ( int ii = 0; ii < ids.length; ++ii ) {
            m_bonusSummaries[ ii+1 ] = getResources().getString( ids[ii] );
        }

		final ViewConfiguration configuration = ViewConfiguration.get(getContext());
		mTouchSlop = configuration.getScaledTouchSlop();
        m_bonusColors = new int[5];
        m_bonusColors[0] = m_prefs.bonusColors[0];
        m_bonusColors[1] = BONUS_2L;
        m_bonusColors[2] = BONUS_2W;
        m_bonusColors[3] = BONUS_3L;
        m_bonusColors[4] = BONUS_3W;

        m_typeRegular = Typeface.createFromAsset(getContext().getAssets(), "Roboto-Regular.ttf");
      	m_typeNarrow = Typeface.createFromAsset(getContext().getAssets(), "Roboto-Condensed.ttf");
        m_fillPaint.setTypeface(m_typeRegular);
    }

    @Override
    public boolean onTouchEvent( MotionEvent event ) 
    {
        int action = event.getAction();
        int xx = (int)event.getX();
        int yy = (int)event.getY();
        
        switch ( action ) {
        case MotionEvent.ACTION_DOWN:
        	mDownXX = xx;
        	mDownYY = yy;
            m_lastSpacing = MULTI_INACTIVE;
            mInMove = false;
            mInGrid = (mDownYY < mTrayTop);
            final boolean belowTiles = mTrayTop+mTrayHt <= yy && yy <= getHeight();
            if (belowTiles) yy = yy - mInMoveOffset;  // Helps pick up from below rack.
            if ( !ConnStatusHandler.handleDown( xx, yy ) ) {
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_DOWN, xx, yy );
            }
            break;
        case MotionEvent.ACTION_MOVE:
            if ( ConnStatusHandler.handleMove( xx, yy ) ) {
            } else if ( MULTI_INACTIVE == m_lastSpacing ) {
            	final int deltaX = mDownXX - xx;
            	final int deltaY = mDownYY - yy;
            	if ( !mInMove ){
                	mInMove = (Math.abs(deltaX) > mTouchSlop) || (deltaY > mTouchSlop);
            	}
            	mInGrid = (yy < mTrayTop);
            	// Want to scroll board if tile touches edge of visible board.
            	final boolean atEdgeX = (xx - m_rDragView.width()/2 < 0) || (xx + m_rDragView.width()/2) > getWidth();
            	final boolean atEdgeY = (yy - mInMoveOffset - m_rDragView.height()/2 < mScoreHt) ||
            			(yy - mInMoveOffset + m_rDragView.height()/2 > mTrayTop);
            	final boolean scrollBoard = atEdgeX || atEdgeY;
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_MOVE, xx, yy, mInMoveOffset, scrollBoard );
                mMoveXX = xx;
                mMoveYY = yy;
                updateDragView(DRAG_VIEW_NO_CHANGE);
            } else {
                int zoomBy = figureZoom( event );
                if ( 0 != zoomBy ) {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_ZOOM, 
                                        zoomBy < 0 ? -2 : 2 );
                }
            }
            break;
        case MotionEvent.ACTION_UP:
            if ( ConnStatusHandler.handleUp( xx, yy ) ) {
                String msg = ConnStatusHandler.getStatusText( m_context,
                                                              m_connType );
                m_parent.showOKOnlyDialog( msg );
            } else {
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy, mInMoveOffset );
                mInMove = false;
                mInGrid = false;
                updateDragView(DRAG_VIEW_HIDE);
            }
            break;
        case MotionEvent.ACTION_POINTER_DOWN:
        case MotionEvent.ACTION_POINTER_2_DOWN:
            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy, mInMoveOffset );
            m_lastSpacing = getSpacing( event );
            break;
        case MotionEvent.ACTION_POINTER_UP:
        case MotionEvent.ACTION_POINTER_2_UP:
            m_lastSpacing = MULTI_INACTIVE;
            break;
        case MotionEvent.ACTION_CANCEL:
        	mInMove = false;
        	mInGrid = false;
        	updateDragView(DRAG_VIEW_HIDE);
        	break;
        default:
            DbgUtils.logf( "onTouchEvent: unknown action: %d", action );
            break;
        }

        return true;             // required to get subsequent events
    }
// Old way...
//    @Override
//    public boolean onTouchEvent( MotionEvent event ) 
//    {
//        int action = event.getAction();
//        int xx = (int)event.getX();
//        int yy = (int)event.getY();
//        
//        switch ( action ) {
//        case MotionEvent.ACTION_DOWN:
//        	mDownXX = xx;
//        	mDownYY = yy;
//            m_lastSpacing = MULTI_INACTIVE;
//            mInMove = false;
//            mInGrid = (mDownYY < mTrayTop);
//            final boolean belowTiles = mTrayTop+mTrayHt <= yy && yy <= getHeight();
//            if (belowTiles) yy = yy - mInMoveOffset;
//            if ( !ConnStatusHandler.handleDown( xx, yy ) ) {
//                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_DOWN, xx, yy );
//            }
//            break;
//        case MotionEvent.ACTION_MOVE:
//            if ( ConnStatusHandler.handleMove( xx, yy ) ) {
//            } else if ( MULTI_INACTIVE == m_lastSpacing ) {
//            	final int deltaX = mDownXX - xx;
//            	final int deltaY = mDownYY - yy;
//            	if ( !mInMove ){
//                	mInMove = (Math.abs(deltaX) > mTouchSlop) || (deltaY > mTouchSlop);
//            	}
//            	mInGrid = (yy < mTrayTop);
//            	if (mInMove) yy = yy - mInMoveOffset;
//                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_MOVE, xx, yy );
//                mMoveXX = xx;
//                mMoveYY = yy;
//                updateDragView(DRAG_VIEW_NO_CHANGE);
//            } else {
//                int zoomBy = figureZoom( event );
//                if ( 0 != zoomBy ) {
//                    m_jniThread.handle( JNIThread.JNICmd.CMD_ZOOM, 
//                                        zoomBy < 0 ? -2 : 2 );
//                }
//            }
//            break;
//        case MotionEvent.ACTION_UP:
//            if ( ConnStatusHandler.handleUp( xx, yy ) ) {
//                String msg = ConnStatusHandler.getStatusText( m_context,
//                                                              m_connType );
//                m_parent.showOKOnlyDialog( msg );
//            } else {
//            	if (mInMove) yy = yy - mInMoveOffset;
//                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
//                mInMove = false;
//                mInGrid = false;
//                updateDragView(DRAG_VIEW_HIDE);
//            }
//            break;
//        case MotionEvent.ACTION_POINTER_DOWN:
//        case MotionEvent.ACTION_POINTER_2_DOWN:
//        	if (mInMove) yy = yy - mInMoveOffset;
//            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
//            m_lastSpacing = getSpacing( event );
//            break;
//        case MotionEvent.ACTION_POINTER_UP:
//        case MotionEvent.ACTION_POINTER_2_UP:
//            m_lastSpacing = MULTI_INACTIVE;
//            break;
//        case MotionEvent.ACTION_CANCEL:
//        	mInMove = false;
//        	mInGrid = false;
//        	updateDragView(DRAG_VIEW_HIDE);
//        	break;
//        default:
//            DbgUtils.logf( "onTouchEvent: unknown action: %d", action );
//            break;
//        }
//
//        return true;             // required to get subsequent events
//    }

    // private void printMode( String comment, int mode )
    // {
    //     comment += ": ";
    //     switch( mode ) {
    //     case View.MeasureSpec.AT_MOST:
    //         comment += "AT_MOST";
    //         break;
    //     case View.MeasureSpec.EXACTLY:
    //         comment += "EXACTLY";
    //         break;
    //     case View.MeasureSpec.UNSPECIFIED:
    //         comment += "UNSPECIFIED";
    //         break;
    //     default:
    //         comment += "<bogus>";
    //     }
    //     DbgUtils.logf( comment );
    // }

    private int measureWidth(int dimsWidth, int widthMeasureSpec) {
    	int result = dimsWidth;
    	int specMode = MeasureSpec.getMode(widthMeasureSpec);
    	int specSize = MeasureSpec.getSize(widthMeasureSpec);
    	if (specMode == MeasureSpec.EXACTLY){
    		result = specSize;
    	} else {
    		if (specMode == MeasureSpec.AT_MOST) {
    			result = Math.min(result, specSize);
    		} // else ANY
    	}
    	return result;
    }

    private int measureHeight(int dimsHeight, int heightMeasureSpec) {
    	int result = dimsHeight;    	
    	int specMode = MeasureSpec.getMode(heightMeasureSpec);
    	int specSize = MeasureSpec.getSize(heightMeasureSpec);
    	if (specMode == MeasureSpec.EXACTLY){
    		result = specSize;
    	} else {
    		if (specMode == MeasureSpec.AT_MOST) {
    			result = Math.min(result, specSize);
    		} // else ANY
    	}
    	return result;
    }

    @Override
    protected void onMeasure( int widthMeasureSpec, int heightMeasureSpec ){
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int width = View.MeasureSpec.getSize( widthMeasureSpec );
        int height = View.MeasureSpec.getSize( heightMeasureSpec );
        BoardDims dims = figureBoardDims( width, height );
    	setMeasuredDimension(measureWidth(dims.width, widthMeasureSpec),
    			measureHeight(dims.height, heightMeasureSpec));
    }

    // @Override
    // protected void onLayout( boolean changed, int left, int top, 
    //                          int right, int bottom )
    // {
    //     DbgUtils.logf( "BoardView.onLayout(%b, %d, %d, %d, %d)",
    //                    changed, left, top, right, bottom );
    //     super.onLayout( changed, left, top, right, bottom );
    // }

    // @Override
    // protected void onSizeChanged( int width, int height, 
    //                               int oldWidth, int oldHeight )
    // {
    //     DbgUtils.logf( "BoardView.onSizeChanged(%d,%d,%d,%d)", width, height, 
    //                    oldWidth, oldHeight );
    //     super.onSizeChanged( width, height, oldWidth, oldHeight );
    // }

    // This will be called from the UI thread
    @Override
    protected void onDraw( Canvas canvas ) 
    {
        synchronized( this ) {
            if ( layoutBoardOnce() ) {
                canvas.drawBitmap( s_bitmap, 0, 0, m_drawPaint );
                ConnStatusHandler.draw( m_context, canvas, getResources(), 
                                        0, 0, m_connType );
            }
        }
    }

    private BoardDims figureBoardDims( int width, int height )
    {
        BoardDims result = new BoardDims();
        int nCells = m_gi.boardSize;
        int maxCellSize = 4 * m_defaultFontHt;
        int trayHt;
        int scoreHt;
        int wantHt;
        int nToScroll;

        for ( boolean firstPass = true; ; ) {
            result.width = width;

            int cellSize = width / nCells;
            if ( cellSize > maxCellSize ) {
                cellSize = maxCellSize;
                int boardWidth = nCells * cellSize;
                result.width = boardWidth;
            }
            int vizColsAtMax = result.width / maxCellSize;
            if (0 == (vizColsAtMax % 2)) vizColsAtMax = vizColsAtMax - 1;
            if (vizColsAtMax < 7) vizColsAtMax = 7;
            
            maxCellSize = result.width / vizColsAtMax;
            result.maxCellSize = maxCellSize;

            // Now determine if vertical scrolling will be necessary.
            // There's a minimum tray and scoreboard height.  If we can
            // fit them and all cells no scrolling's needed.  Otherwise
            // determine the minimum number that must be hidden to fit.
            // Finally grow scoreboard and tray to use whatever's left.
            trayHt = 2 * cellSize;
            scoreHt = (cellSize * 3) / 2;
            if (!OLD_SCOREBOARD) scoreHt = 0;
            wantHt = trayHt + scoreHt + (cellSize * nCells);
            if ( wantHt <= height ) {
                nToScroll = 0;
            } else {
//                // Scrolling's required if we use cell width sufficient to
//                // fill the screen.  But perhaps we don't need to.
//                int cellWidth = 2 * (height / ( 4 + 3 + (2*nCells)));
//                if ( firstPass && cellWidth >= m_defaultFontHt ) {
//                    firstPass = false;
//                    width = nCells * cellWidth;
//                    continue;
//                } else {
//                    nToScroll = nCells - ((height - trayHt - scoreHt) / cellSize);
//                }
                nToScroll = nCells - ((height - trayHt - scoreHt) / cellSize);
            }
            
          int heightUsed = trayHt + scoreHt + (nCells - nToScroll) * cellSize;
          int heightLeft = height - heightUsed;
          int trayHtLong = 0;
          int trayHtSquare = trayHt;
          if ( 0 < heightLeft ) {
              if ( heightLeft > (cellSize * 3 / 2) ) {
                  heightLeft = cellSize * 3 / 2;
              }
              heightLeft /= 3;
              if (OLD_SCOREBOARD) scoreHt += heightLeft;

              trayHt += heightLeft * 2;
              trayHtLong = trayHt;
              if ( XWPrefs.getSquareTiles( m_context ) 
                   && trayHt > (width / 7) ) {
                  trayHt = width / 7;
                  trayHtSquare = trayHt;
              }
              heightUsed = trayHt + scoreHt + ((nCells - nToScroll) * cellSize);
          }

          result.trayHt = trayHt;
          result.scoreHt = scoreHt;
          result.boardHt = cellSize * nCells;
          result.trayTop = scoreHt + (cellSize * (nCells-nToScroll));
          result.height = heightUsed;
          result.cellSize = cellSize;
          
          //
          // Calculate for long tiles but draw square(r) tiles; extra used for
          // space between board and top of tiles.
          //
          result.traySteal = trayHtLong - trayHtSquare;
          result.traySteal = result.traySteal - (result.traySteal / 6);
          result.tileInset = (int) ((BIG_TILE_INSET / 2.0f) + 0.51f);
          
          if ( m_gi.timerEnabled ) {
              Paint paint = new Paint();
              paint.setTextSize( m_mediumFontHt );
              paint.getTextBounds( "-00:00", 0, 6, m_boundsScratch );
              result.timerWidth = m_boundsScratch.width();
          }
          
          break;
      }
        
        mInMoveOffset = (int) (IN_MOVE_OFFSET * result.cellSize);
        mScoreHt = result.scoreHt;
        mTrayTop = result.trayTop;
        mTrayHt = result.trayHt;
        return result;
    } // figureBoardDims

    private boolean layoutBoardOnce() 
    {
        final int width = getWidth();
        final int height = getHeight();
        boolean layoutDone = width == m_layoutWidth && height == m_layoutHeight;
        if ( layoutDone ) {
            // nothing to do
        } else if ( null == m_gi ) {
            // nothing to do either
        } else {
            m_layoutWidth = width;
            m_layoutHeight = height;
            m_fontDims = null; // force recalc of font

            BoardDims dims = figureBoardDims( width, height );

            // If board size has changed we need a new bitmap
            int bmHeight = 1 + dims.height;
            int bmWidth = 1 + dims.width;
            if ( null != s_bitmap ) {
                if ( s_bitmap.getHeight() != bmHeight
                     || s_bitmap.getWidth() != bmWidth ) {
                    s_bitmap = null;
                }
            }

            if ( null == s_bitmap ) {
                s_bitmap = Bitmap.createBitmap( bmWidth, bmHeight,
                                                Bitmap.Config.ARGB_8888 );
            }
            m_canvas = new Canvas( s_bitmap );

            // Clear it
            fillRect( m_canvas, new Rect( 0, 0, width, height ), COLOUR_GRID );

            // need to synchronize??
            m_jniThread.handle( JNIThread.JNICmd.CMD_LAYOUT, dims );
            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
            layoutDone = true;
        }
        return layoutDone;
    } // layoutBoardOnce

    // BoardHandler interface implementation
    public void startHandling( XWActivity parent, JNIThread thread, 
                               int gamePtr, CurGameInfo gi, 
                               CommsAddrRec.CommsConnType connType ) 
    {
        m_parent = parent;
        m_jniThread = thread;
        m_jniGamePtr = gamePtr;
        m_gi = gi;
        m_connType = connType;
        m_layoutWidth = 0;
        m_layoutHeight = 0;

        // Make sure we draw.  Sometimes when we're reloading after
        // an obsuring Activity goes away we otherwise won't.
        invalidate();
    }

    // SyncedDraw interface implementation
    public void doJNIDraw()
    {
        boolean drew;
        synchronized( this ) {
            drew = XwJNI.board_draw( m_jniGamePtr );
        }
        if ( !drew ) {
            DbgUtils.logf( "doJNIDraw: draw not complete" );
        }
    }

    public void setInTrade( boolean inTrade ) 
    {
        m_inTrade = inTrade;
        m_jniThread.handle( JNIThread.JNICmd.CMD_INVALALL );
    }

    public int getCurPlayer()
    {
        return m_trayOwner;
    }

    public int curPending() 
    {
        return m_pendingScore;
    }

    // DrawCtxt interface implementation
    public boolean scoreBegin( Rect rect, int numPlayers, int[] scores, 
                               int remCount )
    {
        fillRectOther( m_canvas, rect, CommonPrefs.COLOR_BACKGRND );
        m_scores = new String[numPlayers][];
        return true;
    }

    public boolean measureRemText( Rect r, int nTilesLeft, int[] width, 
                                   int[] height ) 
    {
        boolean showREM = 0 <= nTilesLeft;
        if ( showREM ) {
            // should cache a formatter
            m_remText = String.format( "%d", nTilesLeft );
            m_fillPaint.setTextSize( m_mediumFontHt );
            m_fillPaint.getTextBounds( m_remText, 0, m_remText.length(), 
                                       m_boundsScratch );

            int minWidth = m_boundsScratch.width();
            if ( minWidth < 20 ) {
                minWidth = 20; // it's a button; make it bigger
            }
            width[0] = minWidth;
            height[0] = m_boundsScratch.height();
        }
        return showREM;
    }

    public void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft, 
                             boolean focussed )
    {
//        int indx = focussed ? CommonPrefs.COLOR_FOCUS
//            : CommonPrefs.COLOR_TILE_BACK;
//        fillRectOther( rOuter, indx );
//
//        m_fillPaint.setColor( adjustColor(BLACK) );
//        drawCentered( m_remText, rInner, null );
    }

    public void measureScoreText( Rect rect, DrawScoreInfo dsi, 
                                  int[] width, int[] height )
    {
        String[] scoreInfo = new String[dsi.isTurn?1:2];
        int indx = 0;
        StringBuffer sb = new StringBuffer();

        // If it's my turn I get one line.  Otherwise squeeze into
        // two.

        if ( dsi.isTurn ) {
            sb.append( dsi.name );
            sb.append( ":" );
        } else {
            scoreInfo[indx++] = dsi.name;
        }
        sb.append( dsi.totalScore );
        if ( dsi.nTilesLeft >= 0 ) {
            sb.append( ":" );
            sb.append( dsi.nTilesLeft );
        }
        scoreInfo[indx] = sb.toString();
        m_scores[dsi.playerNum] = scoreInfo;

        int rectHt = rect.height();
        if ( !dsi.isTurn ) {
            rectHt /= 2;
        }
        int textHeight = rectHt - SCORE_HT_DROP;
        if ( textHeight < m_defaultFontHt ) {
            textHeight = m_defaultFontHt;
        }
        m_fillPaint.setTextSize( textHeight );

        int needWidth = 0;
        for ( int ii = 0; ii < scoreInfo.length; ++ii ) {
            m_fillPaint.getTextBounds( scoreInfo[ii], 0, scoreInfo[ii].length(), 
                                       m_boundsScratch );
            if ( needWidth < m_boundsScratch.width() ) {
                needWidth = m_boundsScratch.width();
            }
        }
        if ( needWidth > rect.width() ) {
            needWidth = rect.width();
        }
        width[0] = needWidth;

        height[0] = rect.height();
    }

    public void score_drawPlayer( Rect rInner, Rect rOuter, 
                                  int gotPct, DrawScoreInfo dsi )
    {
        if ( 0 != (dsi.flags & CELL_ISCURSOR) ) {
            fillRectOther( m_canvas, rOuter, CommonPrefs.COLOR_FOCUS );
        } else if ( DEBUG_DRAWFRAMES && dsi.selected ) {
            fillRectOther( m_canvas, rOuter, CommonPrefs.COLOR_FOCUS );
        }
        String[] texts = m_scores[dsi.playerNum];
        int color = m_playerColors[dsi.playerNum];
        if ( !m_prefs.allowPeek ) {
            color = adjustColor( color );
        }
        m_fillPaint.setColor( color );

        int height = rOuter.height() / texts.length;
        rOuter.bottom = rOuter.top + height;
        for ( String text : texts ) {
            drawCentered( m_canvas, text, rOuter, null );
            rOuter.offset( 0, height );
        }
        if ( DEBUG_DRAWFRAMES ) {
            m_strokePaint.setColor( BLACK );
            m_canvas.drawRect( rInner, m_strokePaint );
        }

        // Update score board (we are currently a jni thread but need to be in UI to touch Views).
        final boolean androidScore = m_delayTurnChange && (0 == dsi.playerNum) && dsi.isRobot;
        if (androidScore){
        	mDSIAndroid = new DrawScoreInfo(dsi);        	
        }
        if (m_delayTurnChange && androidScore){
        	// Draw later after tiles shown.
        } else {
            final DrawScoreInfo fdsi = new DrawScoreInfo(dsi);
            ((BoardActivity) m_context).runOnUiThread(new Runnable(){
    			@Override
    			public void run() {
    		        ((BoardActivity) m_context).updateScoreBoard(fdsi);
    			}
            });        	
        }
    }

    // public boolean drawRemText( int nTilesLeft, boolean focussed, Rect rect )
    // {
    //     boolean willDraw = 0 <= nTilesLeft;
    //     if ( willDraw ) {
    //         String remText = null;
    //         // should cache a formatter
    //         remText = String.format( "%d", nTilesLeft );
    //         m_fillPaint.setTextSize( m_mediumFontHt );
    //         m_fillPaint.getTextBounds( remText, 0, remText.length(), 
    //                                    m_boundsScratch );

    //         int width = m_boundsScratch.width();
    //         if ( width < 20 ) {
    //             width = 20; // it's a button; make it bigger
    //         }
    //         rect.right = rect.left + width;

    //         Rect drawRect = new Rect( rect );
    //         int height = m_boundsScratch.height();
    //         if ( height < drawRect.height() ) {
    //             drawRect.inset( 0, (drawRect.height() - height) / 2 );
    //         }

    //         int indx = focussed ? CommonPrefs.COLOR_FOCUS
    //             : CommonPrefs.COLOR_TILE_BACK;
    //         fillRectOther( rect, indx );

    //         m_fillPaint.setColor( adjustColor(BLACK) );
    //         drawCentered( remText, drawRect, null );
    //     }
    //     return willDraw;
    // }

    // public void score_drawPlayers( Rect scoreRect, DrawScoreInfo[] playerData, 
    //                                Rect[] playerRects )
    // {
    //     Rect tmp = new Rect();
    //     int nPlayers = playerRects.length;
    //     int width = scoreRect.width() / (nPlayers + 1);
    //     int left = scoreRect.left;
    //     int right;
    //     StringBuffer sb = new StringBuffer();
    //     String[] scoreStrings = new String[2];
    //     for ( int ii = 0; ii < nPlayers; ++ii ) {
    //         DrawScoreInfo dsi = playerData[ii];
    //         boolean isTurn = dsi.isTurn;
    //         int indx = 0;
    //         sb.delete( 0, sb.length() );

    //         if ( isTurn ) {
    //             sb.append( dsi.name );
    //             sb.append( ":" );
    //         } else {
    //             scoreStrings[indx++] = dsi.name;
    //         }
    //         sb.append( dsi.totalScore );
    //         if ( dsi.nTilesLeft >= 0 ) {
    //             sb.append( ":" );
    //             sb.append( dsi.nTilesLeft );
    //         }
    //         scoreStrings[indx] = sb.toString();

    //         int color = m_playerColors[dsi.playerNum];
    //         if ( !m_prefs.allowPeek ) {
    //             color = adjustColor( color );
    //         }
    //         m_fillPaint.setColor( color );

    //         right = left + (width * (isTurn? 2 : 1) );
    //         playerRects[ii].set( left, scoreRect.top, right, scoreRect.bottom );
    //         left = right;

    //         tmp.set( playerRects[ii] );
    //         tmp.inset( 2, 2 );
    //         int height = tmp.height() / (isTurn? 1 : 2);
    //         tmp.bottom = tmp.top + height;
    //         for ( String str : scoreStrings ) {
    //             drawCentered( str, tmp, null );
    //             if ( isTurn ) {
    //                 break;
    //             }
    //             tmp.offset( 0, height );
    //         }
    //         if ( DEBUG_DRAWFRAMES ) {
    //             m_canvas.drawRect( playerRects[ii], m_strokePaint );
    //         }
    //     }
    // }

    public void drawTimer( Rect rect, int player, int secondsLeft )
    {
        if ( null != m_canvas && (m_lastSecsLeft != secondsLeft
                                  || m_lastTimerPlayer != player) ) {
            m_lastSecsLeft = secondsLeft;
            m_lastTimerPlayer = player;

            String negSign = secondsLeft < 0? "-":"";
            secondsLeft = Math.abs( secondsLeft );
            String time = String.format( "%s%d:%02d", negSign, secondsLeft/60, 
                                         secondsLeft%60 );

            fillRectOther( m_canvas, rect, CommonPrefs.COLOR_BACKGRND );
            m_fillPaint.setColor( m_playerColors[player] );

            Rect shorter = new Rect( rect );
            shorter.inset( 0, shorter.height() / 5 );
            drawCentered( m_canvas, time, shorter, null );

            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
        }
    }

    public boolean boardBegin( Rect rect, int cellWidth, int cellHeight )
    {
        return true;
    }

    public boolean drawCell( final Rect rect, String text, int tile, int value,
                             int owner, int bonus, int hintAtts, 
                             final int flags ) 
    {
        boolean canDraw = figureFontDims();
        if ( canDraw ) {
            final boolean androidPlaying = 0 != (flags & CELL_ANDROID_PLAYING);
            if (androidPlaying){
                Tile t = makeTile(rect);
                if (null != t){
                    drawCellToCanvas(t.getCanvas(), t.getTileRect(), text, tile, value, owner, bonus, hintAtts, flags);
                    startAndroidPlaying();
                }
                else {
                    drawCellToCanvas(m_canvas, rect, text, tile, value, owner, bonus, hintAtts, flags);                	
                }
            } else {
                drawCellToCanvas(m_canvas, rect, text, tile, value, owner, bonus, hintAtts, flags);            	
            }
        }
        return canDraw;
    } // drawCell

    public boolean drawCellToCanvas(Canvas canvas, final Rect rect, String text, int tile, int value,
    		int owner, int bonus, int hintAtts, 
    		final int flags ) 
    {
    	boolean canDraw = figureFontDims();
    	if ( canDraw ) {
    		final boolean cellDrag = (0 != (flags & CELL_DRAGCUR));
    		final boolean empty = 0 != (flags & (CELL_DRAGSRC|CELL_ISEMPTY));
    		final boolean pending = 0 != (flags & CELL_HIGHLIGHT);
       		String bonusStr = null;
       		
    		// Clear cell.
    		m_fillPaint.setColor(COLOUR_GRID);
    		canvas.drawRect(rect, m_fillPaint);    	
    		// Set tile's rect.
    		smr.set(rect);
    		smr.inset(SMALL_TILE_INSET, SMALL_TILE_INSET);

    		if ( m_inTrade ) {
    			if (0 == rect.left) rect.left = rect.left + 1;
    			fillRectOther( m_canvas, rect, CommonPrefs.COLOR_BACKGRND );
    		}

    		if ( owner < 0 ) {
    			owner = 0;
    		}
    		int backColor;
    		int edgeColor = Color.TRANSPARENT;
    		int foreColor = m_playerColors[owner];

    		if ( 0 != (flags & CELL_ISCURSOR) ) {
    			backColor = m_otherColors[CommonPrefs.COLOR_FOCUS];
    		} else if ( empty ) {
    			if ( 0 == bonus ) {
    				backColor = m_otherColors[CommonPrefs.COLOR_NOTILE];
    				backColor = COLOUR_EMPTY_CELL;
    			} else {
    				bonusStr = m_bonusSummaries[bonus];
    				if (BONUS_AS_BKGND){
    					backColor = m_bonusColors[bonus];
    				} else {
    					backColor = COLOUR_BONUS_BKGND;
    				}
    				edgeColor = Color.TRANSPARENT;
    			}
    		} else if ( pending ) {
    			backColor = COLOUR_TILE_BKGND;
    			edgeColor = COLOUR_HIGHLIGHT_LAST;
    			foreColor = COLOUR_TILE_TEXT;
    		} else {
    			backColor = m_otherColors[CommonPrefs.COLOR_TILE_BACK];
    			backColor = COLOUR_TILE_BKGND;
    			edgeColor = COLOUR_TILE_HIGHLIGHT;
    			foreColor = COLOUR_TILE_TEXT;
    		}

    		// Draw cell's background.
    		m_fillPaint.setColor( adjustColor( backColor ) );
    		myDrawRoundedRect(canvas, smr, m_fillPaint);

    		if ( empty ) {
    			if ( (CELL_ISSTAR & flags) != 0 ) {
    				// Draw the star.
    				int colour = BONUS_2W;
    				if ( BONUS_AS_BKGND ) colour = COLOUR_BONUS_TEXT;
    				Rect brect = new Rect( smr );
    				brect.inset(STAR_INSET, STAR_INSET);
    				m_origin.setBounds( brect );
    				m_origin.setAlpha( m_inTrade? IN_TRADE_ALPHA >> 24 : 255 );
    				ColorFilter clrFilter = new PorterDuffColorFilter(colour, PorterDuff.Mode.SRC_ATOP );
    				m_origin.setColorFilter(clrFilter);
    				m_origin.draw( canvas );
    			} else if ( null != bonusStr ) {
    				// Bonus cells.
    				int color = m_bonusColors[bonus];
    				if ( BONUS_AS_BKGND ){
    					color = COLOUR_BONUS_TEXT;
    				}
    				m_fillPaint.setColor( adjustColor(color) );
    				drawBonusText(canvas, bonusStr, smr, m_fontDims);
    			}
    		} else {
    			// Tiles on the board.
    			m_fillPaint.setColor( adjustColor(foreColor) );
    			drawTileText( canvas, text, String.valueOf(value), smr, m_fontDims );
    		}

    		if ( (CELL_ISBLANK & flags) != 0 ) {
    			markBlank( canvas, rect, backColor );
    		}

    		// frame the cell
    		m_strokePaint.setColor(edgeColor);
    		myDrawRoundedRect(canvas, smr, m_strokePaint);            	

    		if ( empty && cellDrag ){
        		// Frame for drag.
        		m_strokePaint.setColor(COLOUR_TILE_DRAG);
                m_strokePaint.setStrokeWidth(STROKE_TILE_DRAG);
   				Rect brect = new Rect( smr );
				brect.inset(STROKE_TILE_DRAG/2, STROKE_TILE_DRAG/2);                
        		myDrawRoundedRect(canvas, brect, m_strokePaint);
                m_strokePaint.setStrokeWidth(SMALL_TILE_FRAME);       
    		}

    		drawCrosshairs( canvas, rect, flags );
    	}
    	return canDraw;
    } // drawCell
  
    private boolean m_arrowHintShown = false;
    public void drawBoardArrow( Rect rect, int bonus, boolean vert, 
                                int hintAtts, int flags )
    {
//        // figure out if the background is more dark than light
//        boolean useDark = darkOnLight();
//        if ( m_blackArrow != useDark ) {
//            m_blackArrow = useDark;
//            m_downArrow = m_rightArrow = null;
//        }
//        Drawable arrow;
//        if ( vert ) {
//            if ( null == m_downArrow ) {
//                m_downArrow = loadAndRecolor( R.drawable.downarrow, useDark );
//            }
//            arrow = m_downArrow;
//        } else {
//            if ( null == m_rightArrow ) {
//                m_rightArrow = loadAndRecolor( R.drawable.rightarrow, useDark );
//            }
//            arrow = m_rightArrow;
//        }
//
//        rect.inset( 2, 2 );
//        arrow.setBounds( rect );
//        arrow.draw( m_canvas );
//
//        if ( !m_arrowHintShown ) {
//            m_arrowHintShown = true;
//            m_viewHandler.post( new Runnable() {
//                    public void run() {
//                        m_parent.
//                            showNotAgainDlgThen( R.string.not_again_arrow, 
//                                                 R.string.key_notagain_arrow );
//                    }
//                } );
//        }
    }

    public boolean trayBegin ( Rect rect, int owner, int score ) 
    {
        m_trayOwner = owner;
        m_pendingScore = score;
        return true;
    }

    public void drawTile( Rect rect, String text, int val, int flags ) 
    {
//    	if (mInGrid) return;
//    	if (mInGrid){
//          drawTileImpl( rect, text, val, flags, false );
//    		
//    	} else {
//          drawTileImpl( rect, text, val, flags, true );
//    		
//    	}
        drawTileImpl( m_canvas, rect, text, val, flags, true );
        updateDragView(DRAG_VIEW_HIDE);
    }

    public void drawTileMidDrag( Rect rect, String text, int val, int owner, 
                                 int flags ) 
    {
    	m_rDragView.set(rect);
    	if (null == mDragTile) mDragTile = new Tile(m_rDragView);
    	drawTileImpl( mDragTile.getCanvas(), mDragTile.getTileRect(), text, val, flags, false );
    	updateDragView(DRAG_VIEW_SHOW);
    }

    public void drawTileBack( Rect rect, int flags ) 
    {
        drawTileImpl( m_canvas, rect, "?", -1, flags, true );
    }

    public void drawTrayDivider( Rect rect, int flags ) 
    {
    	if (!OLD_TRAYDIVIDER) return;
        boolean isCursor = 0 != (flags & CELL_ISCURSOR);
        boolean selected = 0 != (flags & CELL_HIGHLIGHT);

        int index = isCursor? CommonPrefs.COLOR_FOCUS : CommonPrefs.COLOR_BACKGRND;
        rect.inset( 0, 1 );
        fillRectOther( m_canvas, rect, index );

        rect.inset( rect.width()/4, 0 );
        if ( selected ) {
            m_canvas.drawRect( rect, m_strokePaint );
        } else {
            fillRect( m_canvas, rect, m_playerColors[m_trayOwner] );
        }
    }

    public void score_pendingScore( Rect rect, int score, int playerNum, 
                                    int flags ) 
    {
//        String text = score >= 0? String.format( "%d", score ) : "??";
//        int otherIndx = (0 == (flags & CELL_ISCURSOR)) 
//            ? CommonPrefs.COLOR_BACKGRND : CommonPrefs.COLOR_FOCUS;
//        ++rect.top;
//        fillRectOther( rect, otherIndx );
//        m_fillPaint.setColor( m_playerColors[playerNum] );
//
//        rect.bottom -= rect.height() / 2;
//        drawCentered( text, rect, null );
//
//        rect.offset( 0, rect.height() );
//        drawCentered( getResources().getString( R.string.pts ), rect, null );

    	// Update score board (we are currently a jni thread but need to be in UI to touch Views).
    	final int fscore = score;
    	final int fplayerNum = playerNum;
    	((BoardActivity) m_context).runOnUiThread(new Runnable(){
    		@Override
    		public void run() {
    			((BoardActivity) m_context).updateScorePending(fscore, fplayerNum);
    		}
    	});
    }
    
    public void objFinished( /*BoardObjectType*/int typ, Rect rect )
    {
        if ( DrawCtx.OBJ_BOARD == typ ) {
            // On squat screens, where I can't use the full width for
            // the board (without scrolling), the right-most cells
            // don't draw their right borders due to clipping, so draw
            // for them.
            m_strokePaint.setColor( adjustColor(FRAME_GREY) );
            m_strokePaint.setColor( COLOUR_GRID );
            int xx = rect.left + rect.width() - 1;
            m_canvas.drawLine( xx, rect.top, xx, rect.top + rect.height(),
                               m_strokePaint );
        }
    }

    public void dictChanged( int dictPtr )
    {
        if ( m_dictPtr != dictPtr ) {
            if ( 0 == dictPtr ) {
                m_fontDims = null;
                m_dictChars = null;
            } else if ( m_dictPtr == 0 || 
                        !XwJNI.dict_tilesAreSame( m_dictPtr, dictPtr ) ) {
                m_fontDims = null;
                m_dictChars = XwJNI.dict_getChars( dictPtr );
            }
            m_dictPtr = dictPtr;
        }
    }

    private void drawTileImpl( Canvas canvas, Rect rect, String text, int val, 
                               int flags, boolean clearBack )
    {
        final boolean notEmpty = (flags & CELL_ISEMPTY) == 0;
        final boolean isCursor = (flags & CELL_ISCURSOR) != 0;
        final boolean isInDrag = (0 != (flags & CELL_DRAGCUR));

    	smr.set(rect);
    	smr.inset(BIG_TILE_INSET, BIG_TILE_INSET / 2);
    	
    	canvas.save( Canvas.CLIP_SAVE_FLAG );
        canvas.clipRect( rect );
        if ( clearBack ) {
            m_fillPaint.setColor( COLOUR_GRID );
            canvas.drawRect(rect, m_fillPaint);    
        }

        if ( isCursor || notEmpty ) {
            m_fillPaint.setColor(COLOUR_TILE_BKGND);
            myDrawBigRoundedRect(canvas, smr, m_fillPaint);
            
            m_fillPaint.setColor( m_playerColors[m_trayOwner] );
            m_fillPaint.setColor( COLOUR_TILE_TEXT );

            if ( notEmpty ) {
            	drawBigTileText(canvas, smr, text, val);
                m_bigTilePaint.setColor( COLOUR_BIG_TILE_EDGE );                    	
                myDrawBigRoundedRect(canvas, smr, m_bigTilePaint);    
                if ( 0 != (flags & CELL_HIGHLIGHT) ) {
                    if (m_inTrade){
                        m_bigTilePaint.setColor( COLOUR_EMPTY_CELL );                 	
                        myDrawBigRoundedRect(canvas, smr, m_bigTilePaint);    
                    	smr.inset( 2, 2 );
                    	myDrawBigRoundedRect(canvas, smr, m_bigTilePaint);
                    } else {
                        m_bigTilePaint.setColor( COLOUR_BIG_TILE_EDGE );                    	
                    	smr.inset( 2, 2 );
                    	myDrawBigRoundedRect(canvas, smr, m_bigTilePaint);
                    }
                }
            }
        }
        canvas.restoreToCount(1); // in case new canvas....
    } // drawTileImpl

    private void drawCentered( Canvas canvas, String text, Rect rect, FontDims fontDims ) 
    {
        int descent = -1;
        int textSize;
        if ( null == fontDims ) {
            textSize = rect.height() - SCORE_HT_DROP;
        } else {
            int height = rect.height() - 4; // borders and padding, 2 each 
            descent = fontDims.descentFor( height );
            textSize = fontDims.heightFor( height );
        }
        m_fillPaint.setTextSize( textSize );
        if ( descent == -1 ) {
            descent = m_fillPaint.getFontMetricsInt().descent;
        }
        descent += 1;

        m_fillPaint.getTextBounds( text, 0, text.length(), m_boundsScratch );
        int extra = rect.width() - m_boundsScratch.width();
        if ( 0 >= extra ) {
            m_fillPaint.setTextAlign( Paint.Align.LEFT );
            drawScaled( canvas, text, rect, m_boundsScratch, descent );
        } else {
            int bottom = rect.bottom - descent;
            int origin = rect.left + rect.width() / 2;
            m_fillPaint.setTextAlign( Paint.Align.CENTER );
            canvas.drawText( text, origin, bottom, m_fillPaint );
        }
    }
    
    private void drawBonusText( Canvas canvas, String text, Rect rect, FontDims fontDims ){
        int descent;
        int textSize;
        int bottom;
        int origin;
        int height;

        smrLetter.set(rect);
        smrLetter.inset((int) (BONUS_TEXT_INSET * rect.width()), (int) (BONUS_TEXT_INSET * rect.height()));
        height = smrLetter.height(); // borders and padding, 2 each 
        descent = fontDims.descentFor( height );
        textSize = fontDims.heightFor( height );
        descent = descent - (int)(descent * 0.25f);
        //smrLetter.offset((int) -(smrLetter.width() *0.1f), 0);    
        m_fillPaint.setTextSize( textSize );

        // Squeeze.
        Typeface savetf = m_fillPaint.getTypeface();
        m_fillPaint.setTypeface(m_typeNarrow);     
        bottom = smrLetter.bottom - descent;
        origin = smrLetter.left;
        origin += smrLetter.width() / 2;
        m_fillPaint.setTextAlign( Paint.Align.CENTER );
        canvas.drawText( text, origin, bottom, m_fillPaint );
        m_fillPaint.setTypeface(savetf);    	
    }

    private void drawScaled( Canvas canvas, String text, final Rect rect, 
                             Rect textBounds, int descent )
    {
        textBounds.bottom = rect.height();

        Bitmap bitmap = Bitmap.createBitmap( textBounds.width(),
                                             rect.height(), 
                                             Bitmap.Config.ARGB_8888 );

        Canvas canvas2 = new Canvas( bitmap );
        int bottom = textBounds.bottom - descent;
        canvas2.drawText( text, -textBounds.left, bottom, m_fillPaint );

        canvas.drawBitmap( bitmap, null, rect, m_drawPaint );
    }

//    private void positionDrawTile( final Rect rect, String text, int val )
//    {
//        if ( figureFontDims() ) {
//            final int offset = 2;
//            if ( null != text ) {
//                if ( null == m_letterRect ) {
//                    m_letterRect = new Rect( 0, 0, rect.width() - offset,
//                                             rect.height() * 3 / 4 );
//                }
//                m_letterRect.offsetTo( rect.left + offset, rect.top + offset );
//                drawIn( text, m_letterRect, m_fontDims, Paint.Align.LEFT );
//                if ( FRAME_TRAY_RECTS ) {
//                    m_canvas.drawRect( m_letterRect, m_strokePaint );
//                }
//            }
//
//            if ( val >= 0 ) {
//                int divisor = m_hasSmallScreen ? 3 : 4;
//                if ( null == m_valRect ) {
//                    m_valRect = new Rect( 0, 0, rect.width() / divisor, 
//                                          rect.height() / divisor );
//                    m_valRect.inset( offset, offset );
//                }
//                m_valRect.offsetTo( rect.right - (rect.width() / divisor),
//                                    rect.bottom - (rect.height() / divisor) );
//                text = String.format( "%d", val );
//                m_fillPaint.setTextSize( m_valRect.height() );
//                m_fillPaint.setTextAlign( Paint.Align.RIGHT );
//                m_canvas.drawText( text, m_valRect.right, m_valRect.bottom, 
//                                   m_fillPaint );
//                if ( FRAME_TRAY_RECTS ) {
//                    m_canvas.drawRect( m_valRect, m_strokePaint );
//                }
//            }
//        }
//    }

    private void drawTileText( Canvas canvas, String text, String value, Rect rect, FontDims fontDims ) {
        int descent;
        int textSize;
        int bottom;
        int origin;
        int height;

        //
        // Value.
        //
        final boolean twoDigitValue = Integer.valueOf(value) >= 10;
        if ( !twoDigitValue ){
            smrValue.set(rect);
            smrValue.left = (int) (smrValue.right - TILE_VALUE_SCALE * rect.width());
            smrValue.top = (int) (smrValue.bottom - TILE_VALUE_SCALE * rect.height());
            smrValue.offset((int) (TILE_VALUE_OFFSET_X * rect.width()), (int) (-TILE_VALUE_OFFSET_Y * rect.height()));        	
        } else {
            smrValue.set(rect);
            smrValue.left = (int) (smrValue.right - TILE_VALUE_SCALE * rect.width());
            smrValue.top = (int) (smrValue.bottom - TILE_VALUE_SCALE * rect.height());
            smrValue.offset((int) (-TWO_DIGIT_VALUE_OFFSET * rect.width()), (int) (-TILE_VALUE_OFFSET_Y * rect.height()));
        }

        height = smrValue.height();
        textSize = fontDims.heightFor( height );
        descent = fontDims.descentFor( height );
        descent += 2;
        m_fillPaint.setTextSize( textSize );
        m_fillPaint.setTextAlign( Paint.Align.CENTER );
        if ( !twoDigitValue ){
            bottom = smrValue.bottom - descent;
            origin = smrValue.left + smrValue.width() / 2;
            canvas.drawText(value, origin, bottom, m_fillPaint);        	        	
        } else {
        	final String dig2 = value.substring(1, 2);
            bottom = smrValue.bottom - descent;
            origin = smrValue.left + smrValue.width() / 4;
            canvas.drawText("1", origin, bottom, m_fillPaint);
            smrValue.left = smrValue.left + smrValue.width() / 2;
            origin = smrValue.left + smrValue.width() / 2;
            canvas.drawText(dig2, origin, bottom, m_fillPaint);        	
        }

        //
        // Letter.
        //
        smrLetter.set(rect);
        smrLetter.inset((int) (TILE_TEXT_INSET * rect.width()), (int) (TILE_TEXT_INSET * rect.height()));
        if (!twoDigitValue){
        	smrLetter.offset((int) (-TILE_TEXT_OFFSET * rect.width()), 0);
        } else {
        	smrLetter.offset((int) (-TWO_DIGIT_TEXT_OFFSET * rect.width()), 0);        	
        }
        height = smrLetter.height(); // borders and padding, 2 each 
        descent = fontDims.descentFor( height );
        textSize = fontDims.heightFor( height );
        descent = descent - (int)(descent * 0.25f);
        smrLetter.offset((int) -(smrLetter.width() *0.1f), 0);    
        m_fillPaint.setTextSize( textSize );

        // Squeeze fat letters and ones with two-digit values.
        Typeface savetf = m_fillPaint.getTypeface();
    	m_fillPaint.getTextWidths("M", m_fatTextWidth);        	
        m_fillPaint.getTextWidths(text, m_textWidth);
        if (m_textWidth[0] >= m_fatTextWidth[0] || twoDigitValue){
        	m_fillPaint.setTypeface(m_typeNarrow);
        }
        
        m_fillPaint.getTextBounds( text, 0, text.length(), m_boundsScratch );
        int extra = smrLetter.width() - m_boundsScratch.width();
        if ( 0 >= extra ) {
            m_fillPaint.setTextAlign( Paint.Align.LEFT );
            drawScaled( canvas, text, smrLetter, m_boundsScratch, descent );
        } else {
            bottom = smrLetter.bottom - descent;
            origin = smrLetter.left;
            origin += smrLetter.width() / 2;
            m_fillPaint.setTextAlign( Paint.Align.CENTER );
            canvas.drawText( text, origin, bottom, m_fillPaint );
        }
        
        m_fillPaint.setTypeface(savetf);        
    }

    private void drawBigTileText( Canvas canvas, final Rect rect, String text, int val ){
    	final boolean canDraw = figureFontDims();
    	if (canDraw){
            if (null == text) return;  // Known to happen drawing big tiles.
            drawTileText(canvas, text, String.valueOf(val), rect, m_fontDims);
    	}
    }

    private void drawCrosshairs( Canvas canvas, final Rect rect, final int flags )
    {
//        int color = m_otherColors[CommonPrefs.COLOR_FOCUS];
//        if ( 0 != (flags & CELL_CROSSHOR) ) {
//            Rect hairRect = new Rect( rect );
//            hairRect.inset( 0, hairRect.height() / 3 );
//            fillRect( hairRect, color );
//        }
//        if ( 0 != (flags & CELL_CROSSVERT) ) {
//            Rect hairRect = new Rect( rect );
//            hairRect.inset( hairRect.width() / 3, 0 );
//            fillRect( hairRect, color );
//        }
    }

    private void fillRectOther( Canvas canvas, Rect rect, int index )
    {
        fillRect( canvas, rect, m_otherColors[index] );
    }

    private void fillRect( Canvas canvas, Rect rect, int color )
    {
        m_fillPaint.setColor( color );
        canvas.drawRect( rect, m_fillPaint );
    }

    private boolean figureFontDims()
    {
        if ( null == m_fontDims && null != m_dictChars  ) {

            final int ht = 24;
            final int width = 20;

            Paint paint = new Paint( m_fillPaint ); // CommonPrefs.getFontFlags()??
            paint.setStyle( Paint.Style.STROKE );
            paint.setTextAlign( Paint.Align.LEFT );
            paint.setTextSize( ht );

            Bitmap bitmap = Bitmap.createBitmap( width, (ht*3)/2, 
                                                 Bitmap.Config.ARGB_8888 );
            Canvas canvas = new Canvas( bitmap );

            // FontMetrics fmi = paint.getFontMetrics();
            // DbgUtils.logf( "ascent: " + fmi.ascent );
            // DbgUtils.logf( "bottom: " + fmi.bottom );
            // DbgUtils.logf( "descent: " + fmi.descent );
            // DbgUtils.logf( "leading: " + fmi.leading );
            // DbgUtils.logf( "top : " + fmi.top );
            // DbgUtils.logf( "using as baseline: " + ht );

            Rect bounds = new Rect();
            int maxWidth = 0;
            for ( String str : m_dictChars ) {
                if ( str.length() == 1 && str.charAt(0) >= 32 ) {
                    canvas.drawText( str, 0, ht, paint );
                    paint.getTextBounds( str, 0, 1, bounds );
                    if ( maxWidth < bounds.right ) {
                        maxWidth = bounds.right;
                    }
                }
            }

            // for ( int row = 0; row < bitmap.getHeight(); ++row ) {
            //     StringBuffer sb = new StringBuffer( bitmap.getWidth() );
            //     for ( int col = 0; col < bitmap.getWidth(); ++col ) {
            //         int pixel = bitmap.getPixel( col, row );
            //         sb.append( pixel==0? "." : "X" );
            //     }
            //     DbgUtils.logf( sb.append(row).toString() );
            // }

            int topRow = 0;
            findTop:
            for ( int row = 0; row < bitmap.getHeight(); ++row ) {
                for ( int col = 0; col < bitmap.getWidth(); ++col ) {
                    if ( 0 != bitmap.getPixel( col, row ) ){
                        topRow = row;
                        break findTop;
                    }
                }
            }

            int bottomRow = 0;
            findBottom:
            for ( int row = bitmap.getHeight() - 1; row > topRow; --row ) {
                for ( int col = 0; col < bitmap.getWidth(); ++col ) {
                    if ( 0 != bitmap.getPixel( col, row ) ){
                        bottomRow = row;
                        break findBottom;
                    }
                }
            }
 
            m_fontDims = new FontDims( ht, topRow, bottomRow, maxWidth );
        }
        return null != m_fontDims;
    } // figureFontDims

    private boolean isLightColor( int color )
    {
        int sum = 0;
        for ( int ii = 0; ii < 3; ++ii ) {
            sum += color & 0xFF;
            color >>= 8;
        }
        boolean result = sum > (127*3);
        return result;
    }

    private void markBlank( Canvas canvas, final Rect rect, int backColor )
    {
//        RectF oval = new RectF( rect.left, rect.top, rect.right, rect.bottom );
//        int curColor = 0;
//        boolean whiteOnBlack = !isLightColor( backColor );
//        if ( whiteOnBlack ) {
//            curColor = m_strokePaint.getColor();
//            m_strokePaint.setColor( WHITE );
//        }
//        m_canvas.drawArc( oval, 0, 360, false, m_strokePaint );
//        if ( whiteOnBlack ) {
//            m_strokePaint.setColor( curColor );
//        }
    }

    private boolean darkOnLight()
    {
        int background = m_otherColors[ CommonPrefs.COLOR_NOTILE ];
        if ( background != m_backgroundUsed ) {
            m_backgroundUsed = background;
            m_darkOnLight = isLightColor( background );
        }
        return m_darkOnLight;
    }

    private Drawable loadAndRecolor( int resID, boolean useDark )
    {
         Resources res = getResources();
         Drawable arrow = res.getDrawable( resID );

         if ( !useDark ) {
             Bitmap src = ((BitmapDrawable)arrow).getBitmap();
             Bitmap bitmap = src.copy( Bitmap.Config.ARGB_8888, true );
             for ( int xx = 0; xx < bitmap.getWidth(); ++xx ) {
                 for( int yy = 0; yy < bitmap.getHeight(); ++yy ) {
                     if ( BLACK == bitmap.getPixel( xx, yy ) ) {
                         bitmap.setPixel( xx, yy, WHITE );
                     }
                 }
             }

             arrow = new BitmapDrawable(bitmap); 
         }
         return arrow;
    }

    private int adjustColor( int color )
    {
        if ( m_inTrade ) {
            color = color & IN_TRADE_ALPHA;
        }
        return color;
    }

    private int getSpacing( MotionEvent event ) 
    {
        int result;
        if ( 1 == event.getPointerCount() ) {
            result = MULTI_INACTIVE;
        } else {
            float xx = event.getX( 0 ) - event.getX( 1 );
            float yy = event.getY( 0 ) - event.getY( 1 );
            result = (int)FloatMath.sqrt( (xx * xx) + (yy * yy) );
        }
        return result;
    }

    private int figureZoom( MotionEvent event )
    {
        int zoomDir = 0;
        if ( MULTI_INACTIVE != m_lastSpacing ) {
            int newSpacing = getSpacing( event );
            int diff = Math.abs( newSpacing - m_lastSpacing );
            if ( diff > PINCH_THRESHOLD ) {
                zoomDir = newSpacing < m_lastSpacing? -1 : 1;
                m_lastSpacing = newSpacing;
            }
        }
        return zoomDir;
    }

    
    //
    // +W
    //
    
  	private Tile makeTile(final Rect rect){
  		Tile t = null;

  		synchronized( mSaveTiles ){
  			if (mNSave < 7){
  				t = new Tile(rect);
  				mSaveTiles[mNSave] = t;
  				mNSave++;  			
  			}			
  		}
  		return t;
  	}

    private void myDrawRoundedRect(final Canvas canvas, final Rect rect, final Paint paint){
        smrf.set(rect);
        canvas.drawRoundRect(smrf, RADIUSX, RADIUSY, paint);    	
    }
    private void myDrawBigRoundedRect(final Canvas canvas, final Rect rect, final Paint paint){
        smrf.set(rect);
        canvas.drawRoundRect(smrf, BIG_RADIUS_X, BIG_RADIUS_Y, paint);    	
    }
           
	@Override
	protected void onLayout(boolean changed, int l, int t, int r, int b) {
		// Drag
		if (null == mDragView){
			mDragView = (ImageView) ((Activity) getContext()).getLayoutInflater().inflate(R.layout.drag_view, null);
			if (null != mDragView){
				addView(mDragView);
				mDragView.setVisibility(View.INVISIBLE);
			}
		} else {
			final int w2 = m_rDragView.width() / 2;
			final int h2 = m_rDragView.height() / 2;
			mDragView.layout(mMoveXX - w2, mMoveYY - mInMoveOffset - h2, mMoveXX + w2, mMoveYY - mInMoveOffset + h2);							
		}
		// Commit
		if (null == mCommitView){
			mCommitView = (ImageView) ((Activity) getContext()).getLayoutInflater().inflate(R.layout.commit_view, null);
			if (null != mCommitView){
				addView(mCommitView);
				mCommitView.setVisibility(View.INVISIBLE);
				mCommitView.setOnClickListener(mCommitListener);
				mCommitView.setOnLongClickListener(mCommitLongListener);
			}
		} else {
			int x = m_r7thTile.left;
			int y = m_r7thTile.top;
			int w = m_r7thTile.width();
			int h = m_r7thTile.height();
			mCommitView.layout(x, y, x + w, y + h);	
		}
		// Undo
		if (null == mUndoView){
			mUndoView = (ImageView) ((Activity) getContext()).getLayoutInflater().inflate(R.layout.undo_view, null);
			if (null != mUndoView){
				addView(mUndoView);
				mUndoView.setVisibility(View.INVISIBLE);
				mUndoView.setOnClickListener(mUndoListener);
				mUndoView.setOnLongClickListener(mUndoLongListener);
			}			
		} else {
			int x = m_rUndo.left;
			int y = m_rUndo.top;
			int w = m_rUndo.width();
			int h = m_rUndo.height();
			mUndoView.layout(x, y, x + w, y + h);				
		}
	}

  	private void updateDragView(int viz){
  		final int fviz = viz;
  		((BoardActivity) m_context).runOnUiThread(new Runnable(){
			@Override
			public void run() {
				if (null == mDragView || null == mDragTile) return;
		  		if (DRAG_VIEW_SHOW == fviz) mDragView.setVisibility(View.VISIBLE);
		  		if (DRAG_VIEW_HIDE == fviz) mDragView.setVisibility(View.INVISIBLE);
		  		mDragView.setImageDrawable(mDragTile.getTileDrawable());
		  		mDragView.requestLayout();
			}
        });     
  	}

  	public void updateTrayButtons(Rect rect1, Rect rect2, int numInTray){
  		final boolean fshowCommit = (7 > numInTray);
  		final boolean fshowUndo = (6 > numInTray);
  		final boolean fCommitChanged =
  				(m_r7thTile.left != rect1.left) || (m_r7thTile.top != rect1.top) || (m_r7thTile.right != rect1.right) || (m_r7thTile.bottom != rect1.bottom);
  		final boolean fUndoChanged =
  				(m_rUndo.left != rect2.left) || (m_rUndo.top != rect2.top) || (m_rUndo.right != rect2.right) || (m_rUndo.bottom != rect2.bottom);
  		if ( fCommitChanged ) m_r7thTile.set(rect1);
  		if ( fUndoChanged ) m_rUndo.set(rect2);
  		((BoardActivity) m_context).runOnUiThread(new Runnable(){
			@Override
			public void run() {
				if (fshowCommit){
					mCommitView.setVisibility(View.VISIBLE);
				} else {
					mCommitView.setVisibility(View.INVISIBLE);			
				}
				if (fshowUndo){
					mUndoView.setVisibility(View.VISIBLE);
				} else {
					mUndoView.setVisibility(View.INVISIBLE);			
				}
				if (fCommitChanged) mCommitView.requestLayout();
				if (fUndoChanged) mUndoView.requestLayout();
			}
        });     
  	}

    private static final boolean OLD_SCOREBOARD = false; // Show old score board flag.
    private static final boolean OLD_TRAYDIVIDER = false; // Show old tray divider also set width in JNIThread.java.
    private static final boolean BONUS_AS_BKGND = false;

    private boolean mInMove = false;
    private boolean mInGrid = false;

    private float[] m_fatTextWidth = new float[1];
    private float[] m_textWidth = new float[1];
 
    private RectF smrf = new RectF(0, 0, 0, 0);
    private Rect smr = new Rect(0, 0, 0, 0);
    private Rect smrLetter = new Rect(0, 0, 0, 0);
    private Rect smrValue = new Rect(0, 0, 0, 0);
	private Rect m_r7thTile = new Rect(0, 0, 0, 0);
	private Rect m_rDragView = new Rect(0, 0, 0, 0);
	private Rect m_rUndo = new Rect(0, 0, 0, 0);
    
    private int mInMoveOffset = 0;
    private int mTouchSlop = 0;
    private int mDownXX = 0;
    private int mDownYY = 0;
    private int mScoreHt = 0;
    private int mTrayTop = 0;
    private int mTrayHt;
    private int mMoveXX;
    private int mMoveYY;

	public boolean m_delayTurnChange = false;  // public for BoardActivity
	public int m_newTurn = 0;
	private DrawScoreInfo mDSIAndroid = null;

    private ImageView mCommitView = null;
    private ImageView mDragView = null;
    private ImageView mUndoView = null;
  	private static int DRAG_VIEW_SHOW = 1;
  	private static int DRAG_VIEW_HIDE = 2;
  	private static int DRAG_VIEW_NO_CHANGE = 3;

    private Typeface m_typeRegular = null;
    private Typeface m_typeNarrow = null;
	
    private final int BONUS_3W = getResources().getColor(R.color.bonus3w);
    private final int BONUS_2W = getResources().getColor(R.color.bonus2w);
    private final int BONUS_3L = getResources().getColor(R.color.bonus3l);
    private final int BONUS_2L = getResources().getColor(R.color.bonus2l);
    private final int COLOUR_BIG_TILE_EDGE = getResources().getColor(R.color.tile_highlight);
    private final int COLOUR_BONUS_BKGND = getResources().getColor(R.color.bonus_bkgnd);
    private final int COLOUR_BONUS_TEXT = getResources().getColor(R.color.white);
    private final int COLOUR_EMPTY_CELL = getResources().getColor(R.color.empty_cell);
    private final int COLOUR_GRID = getResources().getColor(R.color.grid);    
    private final int COLOUR_HIGHLIGHT_LAST = getResources().getColor(R.color.tile_last);
    private final int COLOUR_TILE_BKGND = getResources().getColor(R.color.tile_bkgnd);    
    private final int COLOUR_TILE_DRAG = getResources().getColor(R.color.tile_drag);
    private final int COLOUR_TILE_HIGHLIGHT = getResources().getColor(R.color.tile_highlight);
    private final int COLOUR_TILE_TEXT = getResources().getColor(R.color.black);
    
    private static float BONUS_TEXT_INSET;
    private static int BIG_TILE_FRAME;
    private static int BIG_TILE_INSET;
    private static int SMALL_TILE_FRAME;
    private static int SMALL_TILE_INSET;
  	private static int STAR_INSET;
  	private static int STROKE_TILE_DRAG;
  	private static float TILE_TEXT_INSET;
  	private static float TILE_TEXT_OFFSET;
    private static float TILE_VALUE_OFFSET_X;
    private static float TILE_VALUE_OFFSET_Y;
  	private static float TWO_DIGIT_VALUE_OFFSET;
  	private static float TWO_DIGIT_TEXT_OFFSET;
  	private static float RADIUSX;
    private static float RADIUSY;
    private static float BIG_RADIUS_X;
    private static float BIG_RADIUS_Y;
    private static float TILE_VALUE_SCALE;
 	private static float IN_MOVE_OFFSET;
 	  	
	private View.OnClickListener mCommitListener = new View.OnClickListener() {		
		@Override
		public void onClick(View v) {
			((BoardActivity) m_context).handleCommit();
		}
	};

	private View.OnClickListener mUndoListener = new View.OnClickListener() {		
		@Override
		public void onClick(View v) {
    		m_jniThread.handle( JNICmd.CMD_UNDO_CUR );
		}
	};

	private View.OnLongClickListener mCommitLongListener = new View.OnLongClickListener() {
		@Override
		public boolean onLongClick(View v) {
			int id = R.string.commit;
			Utils.showToast( ((BoardActivity) m_context), m_context.getString(id) );
			return false;
		}
	};
	
	private View.OnLongClickListener mUndoLongListener = new View.OnLongClickListener() {
		@Override
		public boolean onLongClick(View v) {
			final int id = R.string.return_tiles;
			Utils.showToast( ((BoardActivity) m_context), m_context.getString(id) );
			return false;
		}
	};

  	private Runnable mDrawTilesRunnable = new Runnable(){
		@Override
		public void run() {
			synchronized( mSaveTiles ){
				if (mNSave > 0){
					mSaveTiles[0].drawToCanvas(m_canvas);
					mNSave--;
					for(int i = 0; i < mNSave; i++){
						mSaveTiles[i] = mSaveTiles[i+1];
					}
					BoardView.this.invalidate();
					postDelayed(mDrawTilesRunnable, 375);
				} else {
					if (m_delayTurnChange){
						((BoardActivity) m_context).updateTurnIndicator(m_newTurn);
						if (null != mDSIAndroid){
							// Show Android's score.
							((BoardActivity) m_context).updateScoreBoard(mDSIAndroid);							
						}
						m_delayTurnChange = false;
					}
				}
				
			}		
		}
  	};
  	private void startAndroidPlaying(){
  		removeCallbacks(mDrawTilesRunnable);
  		postDelayed(mDrawTilesRunnable, 1500);
  	}
  	
  	private final int mDragTileAlpha = (int) (255.0/100) * 75;
  	private int mNSave = 0;
  	private Tile[] mSaveTiles = new Tile[7];
  	private Tile mDragTile = null;
  	
  	private class Tile {
  		private Bitmap mBitmap = null;
  		private Canvas mCanvas = null;
  		private Rect mRect = null;
  		
  		Tile(final Rect rect){
  			mRect = new Rect(rect);
  			mBitmap = Bitmap.createBitmap(rect.width(), rect.height(), Bitmap.Config.ARGB_8888);
  			mCanvas = new Canvas(mBitmap);
  		}
 
  		Drawable getTileDrawable(){
  			BitmapDrawable bd = new BitmapDrawable(getResources(), mBitmap);
  			bd.setAlpha(mDragTileAlpha);
  			return bd;
  		}
  		
  		Canvas getCanvas(){
  			return mCanvas;
  		}
  		
  		Rect getTileRect(){
  			return new Rect(0, 0, mRect.width(), mRect.height());
  		}
  		
  		private void drawToCanvas(Canvas canvas){  			   			
  			canvas.drawBitmap(mBitmap, mRect.left, mRect.top, new Paint());
  		}
  		
  	}
  	  	
}

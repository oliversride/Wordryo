package com.oliversride.wordryo;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.text.TextPaint;
import android.util.AttributeSet;
import android.widget.TextView;

public class LogoText extends TextView {

	private static final String TAG = "LogoText";
	private TextPaint mTxPaint = null;
	private int mOverSize = 0;

	public LogoText(Context context) {
		super(context);
		init(context);
	}

	public LogoText(Context context, AttributeSet attrs) {
		super(context, attrs);
		init(context);
	}

	public LogoText(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		init(context);
	}

	private void init(Context context){
		mTxPaint = new TextPaint();
    	Typeface font = Typeface.createFromAsset(getResources().getAssets(), "Nexa Bold.otf");
    	mTxPaint.setTextSize(getTextSize());
    	mTxPaint.setTypeface(font);
    	mTxPaint.setColor(Color.WHITE);
    	mTxPaint.setTextAlign( Paint.Align.CENTER );
    	mTxPaint.setAntiAlias(true);
    	mTxPaint.setSubpixelText(true);
    	mOverSize = 0;
	}
	
    @Override
    protected void onDraw(Canvas canvas) {
        int bottom = (int)(getHeight() - mTxPaint.descent() + (mOverSize/2) );
        int origin = getWidth() / 2;        
    	canvas.drawText(getText().toString(), origin, bottom, mTxPaint);
    }

    /**
     * @see android.view.View#measure(int, int)
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    	super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        setMeasuredDimension(measureWidth(widthMeasureSpec),
                measureHeight(heightMeasureSpec));
    }

    /**
     * Determines the width of this view
     * @param measureSpec A measureSpec packed into an int
     * @return The width of the view, honoring constraints from measureSpec
     */
    private int measureWidth(int measureSpec) {
        int result = 0;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);

        if (specMode == MeasureSpec.EXACTLY) {
            // We were told how big to be
            result = specSize;
        } else {
            // Measure the text
            result = (int) mTxPaint.measureText(getText().toString()) + getPaddingLeft()
                    + getPaddingRight();
            if (specMode == MeasureSpec.AT_MOST) {
                // Respect AT_MOST value if that was what is called for by measureSpec
                result = Math.min(result, specSize);
            }
        }
        return result;
    }

    /**
     * Determines the height of this view
     * @param measureSpec A measureSpec packed into an int
     * @return The height of the view, honoring constraints from measureSpec
     */
    final int fix = 2; // Maybe font not quite right?
    private int measureHeight(int measureSpec) {
        int result = 0;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);

        int mAscent = (int) mTxPaint.ascent();
        if (specMode == MeasureSpec.EXACTLY) {
            // We were told how big to be
            result = specSize;
        } else {
            // Measure the text (beware: ascent is a negative number)
            result = (int) (-mAscent + mTxPaint.descent()) + getPaddingTop()
                    + getPaddingBottom() + fix;
            if (specMode == MeasureSpec.AT_MOST) {
                // Respect AT_MOST value if that was what is called for by measureSpec
            	if (result > specSize){
            		mOverSize = result - specSize;
            		result = specSize;
            	}
            }
        }
        return result;
    }

}

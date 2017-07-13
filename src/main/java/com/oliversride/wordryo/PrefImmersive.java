package com.oliversride.wordryo;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.os.Handler;
import android.preference.Preference;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

/**
 * Created by richard on 2017-05-31.
 */

public class PrefImmersive extends Preference {
    private static final String TAG = "PrefImmersive";
    private boolean mCurrentValue = false;
    private View mArrowsImmersive;
    final private Handler mHandler = new Handler();

    public PrefImmersive(Context context) {
        super(context);
    }

    public PrefImmersive(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public PrefImmersive(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();
        updateLabel();
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
        mArrowsImmersive = view.findViewById(R.id.arrows_immersive);
        mArrowsImmersive.setSelected(mCurrentValue);
        updateLabel();
    }

    @Override
    protected void onClick() {
        super.onClick();
        // Toggle.
        mCurrentValue = !mCurrentValue;
        // Save.
        persistBoolean(mCurrentValue);

        // Set arrow (starts animation).
        mArrowsImmersive.setSelected(mCurrentValue);
        // Delay and show on/off.
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                updateLabel();
                OtherUtils.hideShowSystemUI((Activity) getContext());
            }
        }, 300);
    }

    @Override
    protected Object onGetDefaultValue(TypedArray ta, int index) {
        boolean defaultValue = ta.getBoolean(index, false);
        return defaultValue;
    }

    @Override
    protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
        if (restoreValue) {
            mCurrentValue = getPersistedBoolean(mCurrentValue);
        } else {
            mCurrentValue = Boolean.valueOf(defaultValue.toString());
        }
    }

    private void updateLabel() {
        if (mCurrentValue) {
            String s = getContext().getResources().getString(R.string.on);
            setSummary(s);
        } else {
            String s = getContext().getResources().getString(R.string.off);
            setSummary(s);
        }
    }

}

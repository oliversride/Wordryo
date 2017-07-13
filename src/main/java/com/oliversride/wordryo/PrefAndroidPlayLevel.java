package com.oliversride.wordryo;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;

/**
 * Created by richard on 2017-06-25.
 */

public class PrefAndroidPlayLevel extends Preference {
    private static final String TAG = "PrefAndroidPlayLevel";
    private String mCurrentValue = "";
    private AnimatedVectorDrawable mGrowBar0;
    private AnimatedVectorDrawable mGrowBar1;
    private AnimatedVectorDrawable mGrowBar2;
    private View mLevel0;
    private View mLevel1;
    private View mLevel2;
    private String mLevelText0 = "";
    private String mLevelText1 = "";
    private String mLevelText2 = "";


    public PrefAndroidPlayLevel(Context context) {
        super(context);
    }

    public PrefAndroidPlayLevel(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public PrefAndroidPlayLevel(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();
        mLevelText0 = getContext().getString(R.string.robot_smart);
        mLevelText1 = getContext().getString(R.string.robot_smarter);
        mLevelText2 = getContext().getString(R.string.robot_smartest);
        mGrowBar0 = (AnimatedVectorDrawable) getContext().getDrawable(R.drawable.avd_strength_bar_grow);
        mGrowBar1 = (AnimatedVectorDrawable) getContext().getDrawable(R.drawable.avd_strength_bar_grow);
        mGrowBar2 = (AnimatedVectorDrawable) getContext().getDrawable(R.drawable.avd_strength_bar_grow);
        // Mutate so they don't all grow at the same time.
        mGrowBar0.mutate();
        mGrowBar1.mutate();
        mGrowBar2.mutate();
        showPlayLevelText();
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
        mLevel0 = view.findViewById(R.id.playlevel_0);
        mLevel1 = view.findViewById(R.id.playlevel_1);
        mLevel2 = view.findViewById(R.id.playlevel_2);
        showPlayLevelText();
        showPlayLevelBars(false);
    }

    @Override
    protected void onClick() {
        super.onClick();
        int current = Integer.valueOf(mCurrentValue);
        current = (current + 1) % 3;
        mCurrentValue = String.valueOf(current);
        // Save.
        persistString(mCurrentValue);
        // Show.
        showPlayLevelText();
        showPlayLevelBars(true);
    }

    @Override
    protected Object onGetDefaultValue(TypedArray ta, int index) {
        String defaultValue = ta.getString(index);
        return defaultValue;
    }

    @Override
    protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
        if (restoreValue) {
            mCurrentValue = getPersistedString(mCurrentValue);
        } else {
            String temp = (String) defaultValue;
            persistString(temp);
            mCurrentValue = temp;
        }
    }

    //
    // Show strength bars.
    //
    private void showPlayLevelBars(boolean animate) {
        if ("0".equals(mCurrentValue)) {
            // Set then animate.
            mLevel2.setBackground(null);
            mLevel1.setBackground(null);
            mLevel0.setBackground(mGrowBar0);
            if (animate) mGrowBar0.start();
        } else if ("1".equals(mCurrentValue)) {
            mLevel2.setBackground(null);
            mLevel1.setBackground(mGrowBar1);
            mLevel0.setBackground(mGrowBar0);
            if (animate) mGrowBar1.start();

        } else if ("2".equals(mCurrentValue)) {
            mLevel2.setBackground(mGrowBar2);
            mLevel1.setBackground(mGrowBar1);
            mLevel0.setBackground(mGrowBar0);
            if (animate) mGrowBar2.start();
        }
    }

    //
    // Show play level text.
    //
    private void showPlayLevelText() {
        if ("0".equals(mCurrentValue)) {
            setSummary(mLevelText0);
        } else if ("1".equals(mCurrentValue)) {
            setSummary(mLevelText1);
        } else if ("2".equals(mCurrentValue)) {
            setSummary(mLevelText2);
        }
    }

}

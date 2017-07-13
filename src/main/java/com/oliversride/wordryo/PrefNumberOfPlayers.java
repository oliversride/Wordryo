package com.oliversride.wordryo;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.os.Handler;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

/**
 * Created by richard on 2017-06-24.
 */

public class PrefNumberOfPlayers extends Preference {
    private static final String TAG = "PrefPlayers";
    private String mCurrentValue = "";
    private boolean mUp = true;
    private ImageView mPlusMinus;
    private int mPlusFlashDuration = 0;
    private int mMinusFlashDuration = 0;
    final private Handler mHandler = new Handler();

    public PrefNumberOfPlayers(Context context) {
        super(context);
    }

    public PrefNumberOfPlayers(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public PrefNumberOfPlayers(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();
        mUp = OtherUtils.getPlayersUp((Activity) getContext());
        final String tempPlus = ((Activity) getContext()).getString(R.string.avd_plus_flash_duration);
        mPlusFlashDuration = Integer.valueOf(tempPlus);
        final String tempMinus = ((Activity) getContext()).getString(R.string.avd_minus_flash_duration);
        mMinusFlashDuration = Integer.valueOf(tempMinus);
        showPlusMinus();
        showNumberOfPlayers();
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
        mPlusMinus = (ImageView) view.findViewById(R.id.players_plusminus);
        showPlusMinus();
        showNumberOfPlayers();
    }

    @Override
    protected void onClick() {
        super.onClick();
        int current = Integer.valueOf(mCurrentValue);
        boolean up = mUp;
        boolean flash = true;
        if (up) {
            current = current + 1;
            if (current > 4) {
                current = 4;
                up = false;
                flash = false;
            }
        } else {
            current = current - 1;
            if (current < 2) {
                current = 2;
                up = true;
                flash = false;
            }
        }
        // Flash animiation?  (Brighten drawable and fade.)
        if (flash) {
            // Use selected state for flash.
            final int[] stateSet = {android.R.attr.state_selected, android.R.attr.state_checked * (mUp ? -1 : +1)};
            mPlusMinus.setImageState(stateSet, true);
            final int fcurrent = current;
            final boolean fup = up;
            final int fduration = mUp ? mPlusFlashDuration : mMinusFlashDuration;
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    // After the animation is done, save--will cause onBind.
                    updatePreference(fcurrent, fup);
                }
            }, fduration);
        } else {
            // Just update; checked state used to switch +/-.
            updatePreference(current, up);
        }

    }

    private void updatePreference(int current, boolean up) {
        mCurrentValue = String.valueOf(current);
        mUp = up;
        // Save.  (Will cause an onBind.)
        persistString(mCurrentValue);
        OtherUtils.savePlayersUp((Activity) getContext(), mUp);
        // Show.
        showPlusMinus();
        showNumberOfPlayers();
    }

    @Override
    protected Object onGetDefaultValue(TypedArray ta, int index) {
        String defaultValue = ta.getString(index);
        mUp = true;
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
    // Show plus or minus sign.
    //
    private void showPlusMinus() {
        if (null != mPlusMinus) {
            final int[] stateSet = {android.R.attr.state_checked * (mUp ? -1 : +1)};
            mPlusMinus.setImageState(stateSet, true);
        }
    }

    //
    // Show number of players for next game.
    //
    private void showNumberOfPlayers() {
        setSummary(mCurrentValue + " players" + '\n' + '\n' + "(Next game)");
    }

}

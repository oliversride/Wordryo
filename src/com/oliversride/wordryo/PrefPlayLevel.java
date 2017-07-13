package com.oliversride.wordryo;

import android.content.Context;
import android.content.res.TypedArray;
import android.preference.Preference;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.RadioButton;
import android.widget.TextView;

public class PrefPlayLevel extends Preference {
	private static final String TAG = "PrefPlayLevel";
	private String mCurrentValue = "";

	public PrefPlayLevel(Context context) {
		super(context);
	}

	public PrefPlayLevel(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public PrefPlayLevel(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
	}

	@Override
	protected void onBindView (View view){
		super.onBindView(view);	
		final View radio1 = view.findViewById(R.id.radio_robot_smart);
		radio1.setOnClickListener(mClickListener);
		final View radio2 = view.findViewById(R.id.radio_robot_smarter);
		radio2.setOnClickListener(mClickListener);
		final View radio3 = (TextView) view.findViewById(R.id.radio_robot_smartest);
		radio3.setOnClickListener(mClickListener);
		((RadioButton) radio1).setChecked(0 == Integer.valueOf(mCurrentValue));
		((RadioButton) radio2).setChecked(1 == Integer.valueOf(mCurrentValue));
		((RadioButton) radio3).setChecked(2 == Integer.valueOf(mCurrentValue));
//		setRadio((RadioButton) radio1, (0 == Integer.valueOf(mCurrentValue)));
//		setRadio((RadioButton) radio2, (1 == Integer.valueOf(mCurrentValue)));
//		setRadio((RadioButton) radio3, (2 == Integer.valueOf(mCurrentValue)));
//		blackblackboard.getParent().requestDisallowInterceptTouchEvent(true);
//		greenblackboard.getParent().requestDisallowInterceptTouchEvent(true);
	}

	@Override 
	protected Object onGetDefaultValue(TypedArray ta, int index){
		String defaultValue = ta.getString(index);
		return defaultValue;    
	}

	@Override
	protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
		if(restoreValue) {
			mCurrentValue = getPersistedString(mCurrentValue);
		}
		else {
			String temp = (String)defaultValue;
			persistString(temp);
			mCurrentValue = temp;
		}
	}	

	View.OnClickListener mClickListener = new View.OnClickListener() {
		
		@Override
		public void onClick(View v) {
			switch(v.getId()){
			case R.id.radio_robot_smart:
				mCurrentValue = "0";
				break;
			case R.id.radio_robot_smarter:
				mCurrentValue = "1";
				break;
			case R.id.radio_robot_smartest:
				mCurrentValue = "2";
				break;
			}
			// Save.
			persistString(mCurrentValue);			
		}
	};
	
	private void setRadio(RadioButton rb, boolean onOFF){
		if (onOFF){
			rb.setChecked(true);
		} else {
			rb.setChecked(false);
		}
	}

}

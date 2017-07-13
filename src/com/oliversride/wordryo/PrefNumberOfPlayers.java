package com.oliversride.wordryo;

import android.content.Context;
import android.content.res.TypedArray;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;

public class PrefNumberOfPlayers extends Preference {
	private static final String TAG = "PrefNumberOfPlayers";
	private String mCurrentValue = "";

	public PrefNumberOfPlayers(Context context) {
		super(context);
		showNumberOfPlayers();
	}

	public PrefNumberOfPlayers(Context context, AttributeSet attrs) {
		super(context, attrs);
		showNumberOfPlayers();
	}

	public PrefNumberOfPlayers(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		showNumberOfPlayers();
	}

	@Override
	protected void onAttachedToActivity (){
		super.onAttachedToActivity();
		showNumberOfPlayers();		
	}
	
	@Override
	protected void onBindView (View view){
		super.onBindView(view);	
		final View fewer = view.findViewById(R.id.fewer_players);
		fewer.setOnClickListener(mOnClickListener);
		final View more = view.findViewById(R.id.more_players);
		more.setOnClickListener(mOnClickListener);
		showNumberOfPlayers();
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

	View.OnClickListener mOnClickListener = new View.OnClickListener() {		
		@Override
		public void onClick(View v) {
			int current = Integer.valueOf(mCurrentValue);
			switch(v.getId()){
			case R.id.fewer_players:
				current = current - 1;
				break;
			case R.id.more_players:
				current = current + 1;
				break;
			}
			if (current < 2) current = 2;
			if (4 < current) current = 4;
			mCurrentValue = String.valueOf(current);
			// Save.
			persistString(mCurrentValue);	
			// Show.
			showNumberOfPlayers();		
		}
	};

	public void showNumberOfPlayers(){
		setSummary(mCurrentValue + " players" + '\n' + '\n' + "(Next game)");
	}
	
}

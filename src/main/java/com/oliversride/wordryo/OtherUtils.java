package com.oliversride.wordryo;

import java.io.UnsupportedEncodingException;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.TypedValue;
import android.view.View;

import static com.oliversride.wordryo.R.id.state;

public class OtherUtils {

	public OtherUtils() {
	}

	private static final String TAG = "OtherUtils";
	private static Rect smrect = new Rect(0, 0, 0, 0);
    
	private enum ORIENTATION {
		LANDSCAPE, PORTRAIT
	}

	//
	// Get current device orientation.
	//
	static ORIENTATION getOrientation(Activity activity){
		ORIENTATION orientation = ORIENTATION.PORTRAIT;
		Configuration config = activity.getResources().getConfiguration();
		if (config.orientation == Configuration.ORIENTATION_LANDSCAPE){
			orientation = ORIENTATION.LANDSCAPE;
		}
		return orientation;
	}
	
	public static boolean inLandscapeMode(Activity activity){
		return ORIENTATION.LANDSCAPE == getOrientation(activity);
	}
	public static boolean inPortraitMode(Activity activity){
		return !inLandscapeMode(activity);
	}
	
	public static int dipsToPixels(Activity activity, int dips){
    	int pixels = (int) (dips * activity.getResources().getDisplayMetrics().density);
    	return pixels;
    }
    
    public static int pixelsToDips(Activity activity, int pixels){
    	int dips = (int) (pixels / activity.getResources().getDisplayMetrics().density);
    	return dips;
    }

    //
    // Hide or show the status bar etc. according to user's preference.
    //
    public static void hideShowSystemUI(Activity activity) {
        final View decorView = activity.getWindow().getDecorView();
        final SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(activity);
        final Boolean immersive = sp.getBoolean("immersive", false);
        // Hide or show system UI.
        if (immersive) {
            decorView.setSystemUiVisibility(

                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                            | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        } else {
            decorView.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_VISIBLE);
        }
    }

    public static void offsetScreenRectToViewCoords(View v, Rect rcConvert){
		int loc[] = new int[2];
		v.getLocationOnScreen(loc);
		rcConvert.offset(-1 * loc[0], -1 * loc[1]);
    }

    // http://stackoverflow.com/questions/4165765/how-to-get-a-view-from-an-event-coordinates-in-android
    // htafoya
    /**
     * Determines if given points are inside view
     * @param x - x coordinate of point
     * @param y - y coordinate of point
     * @param view - view object to compare
     * @return true if the points are within view bounds, false otherwise
     */
    public static boolean isPointInsideView(float x, float y, View view){
        int location[] = new int[2];
        view.getLocationOnScreen(location);
        int viewX = location[0];
        int viewY = location[1];

        //point is inside view bounds
        if(( x > viewX && x < (viewX + view.getWidth())) &&
                ( y > viewY && y < (viewY + view.getHeight()))){
            return true;
        } else {
            return false;
        }
    }

    //
    // Calculate ActionBar height.
    //
    public static int getActionBarHeight(Activity a){
    	int height = -1;

        TypedValue tv = new TypedValue();
        if (a.getTheme().resolveAttribute(android.R.attr.actionBarSize, tv, true))
        {
            height = TypedValue.complexToDimensionPixelSize(tv.data, a.getResources().getDisplayMetrics());
        }

    	return height;
    }
 
    final protected static char[] hexArray = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    public static String bytesToHexString(byte[] bytes) {
        char[] hexChars = new char[bytes.length * 2];
        int v;
        for ( int j = 0; j < bytes.length; j++ ) {
            v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }    

    public static byte[] hexStringToBytes(String s) {
        int len = s.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                                 + Character.digit(s.charAt(i+1), 16));
        }
        return data;
    }
    
//    public static Drawable getLogoDrawable(Activity a){
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "SansitaOne.ttf");
//        return new LogoDrawable(font, 42);
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "ArchivoBlack-Regular.otf");
//        return new LogoDrawable(font, 40);
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "Roboto-BoldItalic.ttf");
//        return new LogoDrawable(font, 40);
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "Quicksand_Bold.otf");
//        return new LogoDrawable(font, 48);
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "virgo.ttf");
//        return new LogoDrawable(font, 48);
//        Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "spincycle_ot.otf");
//        return new LogoDrawable(font, 46);
//      Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "Comfortaa_Bold.ttf");
//      return new LogoDrawable(font, 46);
//    	Typeface font = Typeface.createFromAsset(a.getResources().getAssets(), "helsinki.ttf");
//    	return new LogoDrawable(font, 46);
//    }

    public static void savePlayersUp(Activity activity, boolean up) {
        final SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(activity);
        SharedPreferences.Editor ed = sp.edit();
        ed.putBoolean("players_up", up);
        ed.apply();
    }

    public static boolean getPlayersUp(Activity activity) {
        final SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(activity);
        final boolean up = sp.getBoolean("players_up", true);
        return up;
    }
}

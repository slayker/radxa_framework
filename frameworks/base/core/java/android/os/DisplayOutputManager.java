/* $_FOR_ROCKCHIP_RBOX_$ */
//$_rbox_$_modify_$_zhengyang_20120220: Rbox android display manager class

/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.os;

import android.content.Context;
import android.os.IBinder;
import android.os.IDisplayDeviceManagementService;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.util.Log;
import android.view.IWindowManager;
import android.view.Display;

public class DisplayOutputManager {
	private static final String TAG = "DisplayOutputManager";
	private final boolean DBG = true;
	public final int MAIN_DISPLAY = 0;
	public final int AUX_DISPLAY = 1;
	
	public final int DISPLAY_IFACE_TV = 1;
	public final int DISPLAY_IFACE_YPbPr = 2;
	public final int DISPLAY_IFACE_VGA = 3;
	public final int DISPLAY_IFACE_HDMI = 4;
	public final int DISPLAY_IFACE_LCD = 5;
	
	private final String DISPLAY_TYPE_TV = "TV";
	private final String DISPLAY_TYPE_YPbPr = "YPbPr";
	private final String DISPLAY_TYPE_VGA = "VGA";
	private final String DISPLAY_TYPE_HDMI = "HDMI";
	private final String DISPLAY_TYPE_LCD = "LCD";
	
	public final int DISPLAY_SCALE_X = 0;
	public final int DISPLAY_SCALE_Y = 1;
	
	public final int DISPLAY_3D_NONE = -1;
	public final int DISPLAY_3D_FRAME_PACKING = 0;
	public final int DISPLAY_3D_TOP_BOTTOM = 6;
	public final int DISPLAY_3D_SIDE_BY_SIDE_HALT = 8;
	
	
	private int m_main_iface[] = null;
	private int m_aux_iface[] = null;
	
	private IDisplayDeviceManagementService mService;
	
	public DisplayOutputManager() throws RemoteException {
		IBinder b = ServiceManager.getService("display_device_management");
		if(b == null) {
			Log.e(TAG, "Unable to connect to display device management service! - is it running yet?");
			return;
		}
		mService = IDisplayDeviceManagementService.Stub.asInterface(b);
		try {
			// Get main display interface
			String[] display_iface = mService.listInterfaces(MAIN_DISPLAY);
			if(DBG) Log.d(TAG, "main display iface num is " + display_iface.length);
			if(display_iface.length > 0) {
				m_main_iface = new int[display_iface.length];
				for(int i = 0; i < m_main_iface.length; i++) {
					if(DBG) Log.d(TAG, display_iface[i]);
					m_main_iface[i] = ifacetotype(display_iface[i]);
				}
			}
			else
				m_main_iface = null;
			
			// Get aux display interface
			display_iface = mService.listInterfaces(AUX_DISPLAY);
			if(DBG) Log.d(TAG, "aux display iface num is " + display_iface.length);
			if(display_iface.length > 0) {
				m_aux_iface = new int[display_iface.length];
				for(int i = 0; i < m_aux_iface.length; i++) {
					if(DBG) Log.d(TAG, display_iface[i]);
					m_aux_iface[i] = ifacetotype(display_iface[i]);
				}
			}
			else
				m_aux_iface = null;
			
        } catch (Exception e) {
            Log.e(TAG, "Error listing Interfaces :" + e);
            return;
        }
	}
	
	private int ifacetotype(String iface) {
		int ifaceType;
		if(iface.equals(DISPLAY_TYPE_TV)) {
			ifaceType = DISPLAY_IFACE_TV;
		} else if(iface.equals(DISPLAY_TYPE_YPbPr)) {
			ifaceType = DISPLAY_IFACE_YPbPr;
		} else if(iface.equals(DISPLAY_TYPE_VGA)) {
			ifaceType = DISPLAY_IFACE_VGA;
		} else if(iface.equals(DISPLAY_TYPE_HDMI)) {
			ifaceType = DISPLAY_IFACE_HDMI;
		} else if(iface.equals(DISPLAY_TYPE_LCD)) {
			ifaceType = DISPLAY_IFACE_LCD;
		} else {
			ifaceType = 0;
		}
		return ifaceType;
	}
	
	private String typetoface(int type) {
		String iface;
		
		if(type == DISPLAY_IFACE_TV)
			iface = DISPLAY_TYPE_TV;
		else if(type == DISPLAY_IFACE_YPbPr)
			iface = DISPLAY_TYPE_YPbPr;
		else if(type == DISPLAY_IFACE_VGA)
			iface = DISPLAY_TYPE_VGA;
		else if(type == DISPLAY_IFACE_HDMI)
			iface = DISPLAY_TYPE_HDMI;
		else if(type == DISPLAY_IFACE_LCD)
			iface = DISPLAY_TYPE_LCD;
		else
			return null;
		return iface;
	}
	
	public int getDisplayNumber() {
		int number = 0;
		
		if(m_main_iface != null)
			number++;
		if(m_aux_iface != null)
			number++;
		
		return number;
	}
	
	public int[] getIfaceList(int display) {
		if(display == MAIN_DISPLAY)
			return m_main_iface;
		else if(display == AUX_DISPLAY)
			return m_aux_iface;
		else
			return null;
	}
	
	public int getCurrentInterface(int display) {
		try {
			String iface = mService.getCurrentInterface(display);
			return ifacetotype(iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get current Interface :" + e);
            return 0;
        }
	}
	
	public String[] getModeList(int display, int type) {
		String iface = typetoface(type);
		if(iface.equals(null))
			return null;
		try {
			return mService.getModelist(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get list mode :" + e);
            return null;
        }
	}
	
	public String getCurrentMode(int display, int type) {
		String iface = typetoface(type);
		if(iface.equals(null))
			return null;
		
		try {
			return mService.getMode(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get current mode :" + e);
            return null;
        }
	}
	
	public void setInterface(int display, int type, boolean enable ) {
		try {
			String iface = typetoface(type);
			if(iface.equals(null))
				return;
			mService.enableInterface(display, iface, enable);
        } catch (Exception e) {
            Log.e(TAG, "Error set interface :" + e);
            return;
        }
	}
	
	public void setMode(int display, int type, String mode) {
		String iface = typetoface(type);
		if(iface.equals(null))
			return;
		
		try {
			mService.setMode(display, iface, mode);
        } catch (Exception e) {
            Log.e(TAG, "Error set mode :" + e);
            return;
        }	
	}
	
	public boolean getUtils() {
		String enable;
		
		try {
			enable = mService.getEnableSwitchFB();
        } catch (Exception e) {
            Log.e(TAG, "Error getUtils :" + e);
            return false;
        }
        if(enable.equals("true"))
        	return true;
        else
        	return false;
	}
	
	public void switchNextDisplayInterface() {
		try {
			mService.switchNextDisplayInterface(MAIN_DISPLAY);
        } catch (Exception e) {
            Log.e(TAG, "Error set interface :" + e);
            return;
        }
	}
	
	public void setScreenScale(int direction, int value) {
		try {
			if(m_main_iface != null)
				mService.setScreenScale(MAIN_DISPLAY, direction, value);
			if(m_aux_iface != null)
				mService.setScreenScale(AUX_DISPLAY, direction, value);
		}catch (Exception e) {
            Log.e(TAG, "Error setScreenScale :" + e);
            return;
        }
	}
	
	public int getScreenScale(int direction) {
		if(direction == DISPLAY_SCALE_X)
			return  SystemProperties.getInt("persist.sys.scalerate_x", 100);
		else if(direction == DISPLAY_SCALE_Y)
			return  SystemProperties.getInt("persist.sys.scalerate_y", 100);
		else
			return 100;
	}
	
	public void setDisplaySize(int display, int width, int height)
	{
		if(getUtils() == true) {
			IWindowManager wm = IWindowManager.Stub.asInterface(ServiceManager.checkService(
            Context.WINDOW_SERVICE));
        	if (wm == null) {
            	 Log.e(TAG, "Error setDisplaySize get widow manager");
            	 return;
        	}
        	try {
	            if (width >= 0 && height >= 0) {
	                wm.setForcedDisplaySize(display, width, height);
	                mService.setDisplaySize(display, width, height);
	            } else {
	                wm.clearForcedDisplaySize(display);
	            }
	        } catch (RemoteException e) {
	        	Log.e(TAG, "Error setDisplaySize :" + e);
	        }
		}
	}
	
	public int get3DModes(int display, int type)
	{
		String iface = typetoface(type);
		if(iface.equals(null))
			return 0;
			
		try {
			return mService.get3DModes(display, iface);
        } catch (Exception e) {
            Log.e(TAG, "Error get 3d modes :" + e);
            return 0;
        }
	}
	
	public int getCur3DMode(int display, int type)
	{
		String iface = typetoface(type);
		if(iface.equals(null))
			return -1;
			
		try {
			return mService.getCur3DMode(display, iface);			
        } catch (Exception e) {
            Log.e(TAG, "Error get cur 3d mode :" + e);
            return -1;
        }
	}
	
	public void set3DMode(int display, int type, int mode)
	{
		String iface = typetoface(type);
		if(iface.equals(null))
			return;
			
		try {
			mService.set3DMode(display, iface, mode);
        } catch (Exception e) {
            Log.e(TAG, "Error set 3d modes :" + e);
            return;
        }
	}

}

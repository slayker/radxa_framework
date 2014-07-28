/*  
 *  Copyright(C), 2009-2010, Fuzhou Rockchip Co. ,Ltd.  All Rights Reserved.
 *
 *  File:   PppoeService.java
 *  Desc:   
 *  Usage:        
 *  Note:
 *  Author: cz@rock-chips.com
 *  Version:
 *          v1.0
 *  Log:
    ----Thu Sep 8 2011            v1.0
 */

package com.android.server;

import android.app.AlarmManager;
import android.app.PendingIntent;

import android.content.BroadcastReceiver;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.pppoe.IPppoeManager;
import android.net.pppoe.PppoeManager;
import android.net.NetworkStateTracker;
import android.net.DhcpInfo;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.Process;
import android.os.RemoteException;
import android.os.UEventObserver;
import android.provider.Settings;
import android.util.Log;
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.BitSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.io.FileDescriptor;
import java.io.PrintWriter;

public class PppoeService extends IPppoeManager.Stub {
    private static final String TAG = "PppoeService";
    private static final boolean DEBUG = true;
    private static void LOG(String msg) {
        if ( DEBUG ) {
            Log.d(TAG, msg);
        }
    }

    private Context mContext;
    private PppoeObserver mObserver;
    private NetworkInfo mNetworkInfo;
    int mPppoeState = PppoeManager.PPPOE_STATE_DISCONNECTED;
    private String mIface;
    
    /*-------------------------------------------------------*/

    PppoeService(Context context) {
        LOG("PppoeService() : Entered.");
       
        mContext = context;
        mObserver = new PppoeObserver(mContext);
    }

    public int getPppoeState() {
        return mPppoeState;
    }
    
    private void setPppoeStateAndSendBroadcast(int newState) {
        int preState = mPppoeState;
        mPppoeState = newState;
        
        final Intent intent = new Intent(PppoeManager.PPPOE_STATE_CHANGED_ACTION);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);     
        intent.putExtra(PppoeManager.EXTRA_PPPOE_STATE, newState);
        intent.putExtra(PppoeManager.EXTRA_PREVIOUS_PPPOE_STATE, preState);
        LOG("setPppoeStateAndSendBroadcast() : preState = " + preState +", curState = " + newState);
        mContext.sendStickyBroadcast(intent);
    }
    
    public boolean startPppoe() {
        LOG("startPppoe");
        setPppoeStateAndSendBroadcast(PppoeManager.PPPOE_STATE_CONNECTING);
        if ( 0 != startPppoeNative() ) {
            LOG("startPppoe() : fail to start pppoe!");
            setPppoeStateAndSendBroadcast(PppoeManager.PPPOE_STATE_DISCONNECTED);
            return false;
        } else {
            setPppoeStateAndSendBroadcast(PppoeManager.PPPOE_STATE_CONNECTED);
            return true;
        }
    }
    
    public boolean stopPppoe() {
        setPppoeStateAndSendBroadcast(PppoeManager.PPPOE_STATE_DISCONNECTING);    
        if ( 0 != stopPppoeNative() ) {
            LOG("stopPppoe() : fail to stop pppoe!");
            return false;
        } else {
            setPppoeStateAndSendBroadcast(PppoeManager.PPPOE_STATE_DISCONNECTED);
            return true;
        }
    }
    
    public boolean setupPppoe(String user, String iface, String dns1, String dns2, String password) {
        int ret;
        
        LOG("setupPppoe: ");
        LOG("user="+user);
        LOG("iface="+iface);
        LOG("dns1="+dns1);
        LOG("dns2="+dns2);
//        LOG("password="+password);

        mIface = iface;

        if (user==null || iface==null || password==null) return false;
        if (dns1==null) dns1="";
        if (dns2==null) dns2="";

        if (0 == setupPppoeNative(user, iface, dns1, dns2, password)) {
            return true;
        } else {
            return false;
        }
    }

    public String getPppoePhyIface() {
        return mIface;	
    }
    	
    private class PppoeObserver extends UEventObserver {
        private static final String PPPOE_UEVENT_MATCH = "SUBSYSTEM=net";
        
        private Context mContext;
        
        public PppoeObserver(Context context) {
            mContext = context;
            LOG("PppoeObserver() : to start observing, to catch uevent with '" + PPPOE_UEVENT_MATCH + "'.");
            startObserving(PPPOE_UEVENT_MATCH);
            init();
        }

        private synchronized final void init() {
        }

        @Override
        public void onUEvent(PppoeObserver.UEvent event) {
            LOG("onUEvent() : get uevent : '" + event + "'.");

            String netInterface = event.get("INTERFACE");
            String action = event.get("ACTION");            
            if ( null != netInterface && netInterface.equals("ppp0") ) {
                if ( action.equals("add") ) {
                    LOG("onUEvent() : pppoe started");
//                    setEthernetEnabled(true);
                }
                else if ( action.equals("remove") ) {
                    LOG("onUEvent() : pppoe stopped");
//                    setEthernetEnabled(false);
                }
           }
        }
    }
    
    public native static int setupPppoeNative(String user, String iface,String dns1, String dns2, String password);
    public native static int startPppoeNative();
    public native static int stopPppoeNative();
    public native static int isPppoeUpNative();
}


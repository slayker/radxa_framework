/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.net.pppoe;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.NetworkInfo;
import android.net.NetworkUtils;
import android.net.NetworkInfo.DetailedState;
import android.net.ConnectivityManager;
import android.net.NetworkStateTracker;
import android.net.LinkCapabilities;
import android.net.LinkProperties;
import android.net.INetworkManagementEventObserver;
import android.os.Handler;
import android.os.IBinder;
import android.os.INetworkManagementService;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import android.content.Intent;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.net.pppoe.PppoeManager;
import android.net.wifi.WifiManager;
import android.net.wifi.IWifiManager;
import android.net.pppoe.IPppoeManager;
import android.net.EthernetDataTracker;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.FileNotFoundException;

/**
 * This class tracks the data connection associated with Ethernet
 * This is a singleton class and an instance will be created by
 * ConnectivityService.
 * @hide
 */
public class PppoeStateTracker implements NetworkStateTracker {
    private static final String NETWORKTYPE = "PPPOE";
    private static final String TAG = "PppoeStateTracker";

    private AtomicBoolean mTeardownRequested = new AtomicBoolean(false);
    private AtomicBoolean mPrivateDnsRouteSet = new AtomicBoolean(false);
    private AtomicInteger mDefaultGatewayAddr = new AtomicInteger(0);
    private AtomicBoolean mDefaultRouteSet = new AtomicBoolean(false);

    private static boolean mLinkUp;
    private LinkProperties mLinkProperties;
    private LinkCapabilities mLinkCapabilities;
    private NetworkInfo mNetworkInfo;
    private InterfaceObserver mInterfaceObserver;

    /* For sending events to connectivity service handler */
    private Handler mCsHandler;
    private Context mContext;
    private BroadcastReceiver mPppoeStateReceiver;
    private int mState;

    private static PppoeStateTracker sInstance;
    private static String sIfaceMatch = "ppp\\d";
    private static String mIface = "";
    public static final String PPPOE_STATE_CHANGED_ACTION = "android.net.pppoe.PPPOE_STATE_CHANGED";
    public static final String EXTRA_PPPOE_STATE = "pppoe_state";
    public int pppoeCurrentState = PPPOE_STATE_DISCONNECTED;
    public static final int PPPOE_STATE_DISCONNECTED=0;
    public static final int PPPOE_STATE_CONNECTING=1;
    public static final int PPPOE_STATE_CONNECTED=2;

    private static class InterfaceObserver extends INetworkManagementEventObserver.Stub {
        private PppoeStateTracker mTracker;

        InterfaceObserver(PppoeStateTracker tracker) {
            super();
            mTracker = tracker;
        }

        public void interfaceClassDataActivityChanged(String label, boolean active) {
            // Ignored.
        }

        public void interfaceStatusChanged(String iface, boolean up) {
            Log.d(TAG, "Interface status changed: " + iface + (up ? "up" : "down"));
        }

        public void interfaceLinkStateChanged(String iface, boolean up) {
            if (mIface.equals(iface) && mLinkUp != up) {
                Log.d(TAG, "Interface " + iface + " link " + (up ? "up" : "down"));
                mLinkUp = up;

            // Ignored.
            }
        }

        public void interfaceAdded(String iface) {
            mTracker.interfaceAdded(iface);
        }

        public void interfaceRemoved(String iface) {
            mTracker.interfaceRemoved(iface);
        }

        public void limitReached(String limitName, String iface) {
            // Ignored.
        }
    }

    private PppoeStateTracker() {
        mNetworkInfo = new NetworkInfo(ConnectivityManager.TYPE_PPPOE, 0, NETWORKTYPE, "");
        mLinkProperties = new LinkProperties();
        mLinkCapabilities = new LinkCapabilities();
        mLinkUp = false;

        mNetworkInfo.setIsAvailable(false);
        setTeardownRequested(false);
    }

    private void interfaceAdded(String iface) {
        if (!iface.matches(sIfaceMatch))
            return;

        Log.d(TAG, "Adding " + iface);

        synchronized(mIface) {
            if(!mIface.isEmpty())
                return;
            mIface = iface;
        }
    }

    private void interfaceRemoved(String iface) {

        synchronized(mIface) {
            if (!iface.equals(mIface))
                return;

            Log.d(TAG, "Removing " + iface);

            mIface = "";
	}
    }

    public static synchronized PppoeStateTracker getInstance() {
        if (sInstance == null) sInstance = new PppoeStateTracker();
        return sInstance;
    }

    public Object Clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    public void setTeardownRequested(boolean isRequested) {
        mTeardownRequested.set(isRequested);
    }

    public boolean isTeardownRequested() {
        return mTeardownRequested.get();
    }

    /**
     * Begin monitoring connectivity
     */
    public void startMonitoring(Context context, Handler target) {
        IBinder b = ServiceManager.getService(Context.NETWORKMANAGEMENT_SERVICE);
        INetworkManagementService service = INetworkManagementService.Stub.asInterface(b);

        mInterfaceObserver = new InterfaceObserver(this);
        IntentFilter filter = new IntentFilter();
        filter.addAction(PppoeManager.PPPOE_STATE_CHANGED_ACTION);
        filter.addAction(EthernetDataTracker.ETHERNET_STATE_CHANGED_ACTION);
        filter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        mPppoeStateReceiver = new PppoeStateReceiver();
        context.registerReceiver(mPppoeStateReceiver, filter);
        mCsHandler = target;

        try {
            service.registerObserver(mInterfaceObserver);
        } catch (RemoteException e) {
            Log.e(TAG, "Could not register InterfaceObserver " + e);
        }

        return;
    }

    /**
     * Disable connectivity to a network
     * TODO: do away with return value after making MobileDataStateTracker async
     */
    public boolean teardown() {
        mTeardownRequested.set(true);
        return true;
    }

    /**
     * Re-enable connectivity to a network after a {@link #teardown()}.
     */
    public boolean reconnect() {
        mTeardownRequested.set(false);
        return true;
    }

    /**
     * Turn the wireless radio off for a network.
     * @param turnOn {@code true} to turn the radio on, {@code false}
     */
    public boolean setRadio(boolean turnOn) {
        return true;
    }

    /**
     * @return true - If are we currently tethered with another device.
     */
    public synchronized boolean isAvailable() {
        return mNetworkInfo.isAvailable();
    }

    /**
     * Tells the underlying networking system that the caller wants to
     * begin using the named feature. The interpretation of {@code feature}
     * is completely up to each networking implementation.
     * @param feature the name of the feature to be used
     * @param callingPid the process ID of the process that is issuing this request
     * @param callingUid the user ID of the process that is issuing this request
     * @return an integer value representing the outcome of the request.
     * The interpretation of this value is specific to each networking
     * implementation+feature combination, except that the value {@code -1}
     * always indicates failure.
     * TODO: needs to go away
     */
    public int startUsingNetworkFeature(String feature, int callingPid, int callingUid) {
        return -1;
    }

    /**
     * Tells the underlying networking system that the caller is finished
     * using the named feature. The interpretation of {@code feature}
     * is completely up to each networking implementation.
     * @param feature the name of the feature that is no longer needed.
     * @param callingPid the process ID of the process that is issuing this request
     * @param callingUid the user ID of the process that is issuing this request
     * @return an integer value representing the outcome of the request.
     * The interpretation of this value is specific to each networking
     * implementation+feature combination, except that the value {@code -1}
     * always indicates failure.
     * TODO: needs to go away
     */
    public int stopUsingNetworkFeature(String feature, int callingPid, int callingUid) {
        return -1;
    }

    @Override
    public void setUserDataEnable(boolean enabled) {
        Log.w(TAG, "ignoring setUserDataEnable(" + enabled + ")");
    }

    @Override
    public void setPolicyDataEnable(boolean enabled) {
        Log.w(TAG, "ignoring setPolicyDataEnable(" + enabled + ")");
    }

    /**
     * Check if private DNS route is set for the network
     */
    public boolean isPrivateDnsRouteSet() {
        return mPrivateDnsRouteSet.get();
    }

    /**
     * Set a flag indicating private DNS route is set
     */
    public void privateDnsRouteSet(boolean enabled) {
        mPrivateDnsRouteSet.set(enabled);
    }

    /**
     * Fetch NetworkInfo for the network
     */
    public synchronized NetworkInfo getNetworkInfo() {
        return mNetworkInfo;
    }

    /**
     * Fetch LinkProperties for the network
     */
    public synchronized LinkProperties getLinkProperties() {
        return new LinkProperties(mLinkProperties);
    }

   /**
     * A capability is an Integer/String pair, the capabilities
     * are defined in the class LinkSocket#Key.
     *
     * @return a copy of this connections capabilities, may be empty but never null.
     */
    public LinkCapabilities getLinkCapabilities() {
        return new LinkCapabilities(mLinkCapabilities);
    }

    /**
     * Fetch default gateway address for the network
     */
    public int getDefaultGatewayAddr() {
        return mDefaultGatewayAddr.get();
    }

    /**
     * Check if default route is set
     */
    public boolean isDefaultRouteSet() {
        return mDefaultRouteSet.get();
    }

    /**
     * Set a flag indicating default route is set for the network
     */
    public void defaultRouteSet(boolean enabled) {
        mDefaultRouteSet.set(enabled);
    }

    /**
     * Return the system properties name associated with the tcp buffer sizes
     * for this network.
     */
    public String getTcpBufferSizesPropName() {
        return "net.tcp.buffersize.wifi";
    }

    public void setDependencyMet(boolean met) {
        // not supported on this network
    }

    public void captivePortalCheckComplete() {

    }

    private void pppoeSetDns(String[] dnses) {
        try {
            File file = new File ("/data/misc/ppp/resolv.conf");
            FileInputStream fis = new FileInputStream(file);
            InputStreamReader input = new InputStreamReader(fis);
            BufferedReader br =  new BufferedReader(input, 128);
            String str;
            int i = 0;

            Log.d(TAG, "pppoeSetDns");

            while((str=br.readLine()) != null) {
                String dns = str.substring(11);
                Log.d(TAG, "dns"+i+":="+dns);
//	 	mLinkProperties.addDns(NetworkUtils.numericToInetAddress(dns));
		dnses[i] = dns;
		i++;
            }
            br.close();
        } catch (FileNotFoundException e) {
            Log.e(TAG, "resolv.conf not found");
        } catch (IOException e) {
            Log.e(TAG, "handle resolv.conf failed");
        }
    }

    private void pppoeConnected() {
	String[] dnses = new String[2];
	pppoeSetDns(dnses);

        mLinkProperties.setInterfaceName(mIface);
        mNetworkInfo.setIsAvailable(true);
        mNetworkInfo.setDetailedState(DetailedState.CONNECTED, null, null);
        Message msg = mCsHandler.obtainMessage(EVENT_STATE_CHANGED, mNetworkInfo);
        msg.sendToTarget();

	mLinkProperties.addDns(NetworkUtils.numericToInetAddress(dnses[0]));
	mLinkProperties.addDns(NetworkUtils.numericToInetAddress(dnses[1]));
        msg = mCsHandler.obtainMessage(EVENT_CONFIGURATION_CHANGED, mNetworkInfo);
        msg.sendToTarget();
    }

    private void pppoeDisconnected() {
        mLinkProperties.clear();
        mNetworkInfo.setIsAvailable(false);
        mNetworkInfo.setDetailedState(DetailedState.DISCONNECTED, null, null);

        Message msg = mCsHandler.obtainMessage(EVENT_CONFIGURATION_CHANGED, mNetworkInfo);
        msg.sendToTarget();

        msg = mCsHandler.obtainMessage(EVENT_STATE_CHANGED, mNetworkInfo);
        msg.sendToTarget();
    }

    private class PppoeStateReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(PppoeManager.PPPOE_STATE_CHANGED_ACTION)) {
                mState = (int) intent.getIntExtra(PppoeManager.EXTRA_PPPOE_STATE,
                                            PppoeManager.PPPOE_STATE_DISCONNECTED);
                if (mState == PppoeManager.PPPOE_STATE_CONNECTED) {
		    pppoeConnected();
                } else if (mState == PppoeManager.PPPOE_STATE_DISCONNECTED) {
                    pppoeDisconnected();
                }
            } else if (intent.getAction().equals(EthernetDataTracker.ETHERNET_STATE_CHANGED_ACTION)) {
                int ethState = intent.getIntExtra(EthernetDataTracker.EXTRA_ETHERNET_STATE,
                                                    EthernetDataTracker.ETHER_STATE_DISCONNECTED);
                if ((ethState == EthernetDataTracker.ETHER_STATE_DISCONNECTED) &&
                                    (mState == PppoeManager.PPPOE_STATE_CONNECTED)) {
                    try {
                        IPppoeManager service = IPppoeManager.Stub.asInterface(
                        ServiceManager.getService(Context.PPPOE_SERVICE));
                        if(service.getPppoePhyIface().equals("eth0")) {
                            service.stopPppoe();
                        }
                    } catch (RemoteException e) {
                        Log.e(TAG, "Could not stopPppoe " + e);
                    }
                }
            } else if (intent.getAction().equals(WifiManager.WIFI_STATE_CHANGED_ACTION)) {
                int wifiState = intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE,
		                                        WifiManager.WIFI_STATE_UNKNOWN);
		if ((wifiState == WifiManager.WIFI_STATE_DISABLED)  &&
                          (mState == PppoeManager.PPPOE_STATE_CONNECTED)) {
		    try {
		        IPppoeManager service = IPppoeManager.Stub.asInterface(
		        ServiceManager.getService(Context.PPPOE_SERVICE));
		        Log.d(TAG, "phyiface:" + service.getPppoePhyIface());
		        if(service.getPppoePhyIface().equals("wlan0")) {
		            service.stopPppoe();
		        }
		    } catch (RemoteException e) {
		        Log.e(TAG, "Could not stopPppoe " + e);
		    }
		}
            }
        }
    }
}


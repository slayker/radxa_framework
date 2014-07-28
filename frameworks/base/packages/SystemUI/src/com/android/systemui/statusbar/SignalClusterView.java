/*$_FOR_ROCKCHIP_RBOX_$*/
/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.systemui.statusbar;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Slog;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;

import com.android.systemui.statusbar.policy.NetworkController;

import com.android.systemui.R;

// Intimately tied to the design of res/layout/signal_cluster_view.xml
public class SignalClusterView
        extends LinearLayout
        implements NetworkController.SignalCluster {

    static final boolean DEBUG = false;
    static final String TAG = "SignalClusterView";

    NetworkController mNC;

    private boolean mWifiVisible = false;
    private int mWifiStrengthId = 0, mWifiActivityId = 0;
//$_rbox_$_modify_$_chenzhi_20120309
//$_rbox_$_modify_$_begin
    private boolean mEthVisible = false;
    private int mEthStrengthId = 0;
//$_rbox_$_modify_$_chenzhi_20120309
//$_rbox_$_modify_$_begin
    private boolean mPppoeVisible = false;
    private int mPppoeStrengthId = 0;
//$_rbox_$_modify_$_end
    private boolean mMobileVisible = false;
    private int mMobileStrengthId = 0, mMobileActivityId = 0, mMobileTypeId = 0;
    private boolean mIsAirplaneMode = false;
    private int mAirplaneIconId = 0;
    private String mWifiDescription, mMobileDescription, mMobileTypeDescription;
//$_rbox_$_modify_$_chenzhi_20120309
//$_rbox_$_modify_$_begin
    ViewGroup mWifiGroup, mMobileGroup,mEthGroup,mPppoeGroup;
    ImageView mWifi, mMobile, mWifiActivity, mMobileActivity, mMobileType, mAirplane, mEth,mPppoe;
    View mSpacer,mSpacer1, mSpacer2;
//$_rbox_$_modify_$_end
    public SignalClusterView(Context context) {
        this(context, null);
    }

    public SignalClusterView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SignalClusterView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public void setNetworkController(NetworkController nc) {
        if (DEBUG) Slog.d(TAG, "NetworkController=" + nc);
        mNC = nc;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mWifiGroup      = (ViewGroup) findViewById(R.id.wifi_combo);
        mWifi           = (ImageView) findViewById(R.id.wifi_signal);
        mWifiActivity   = (ImageView) findViewById(R.id.wifi_inout);
        mMobileGroup    = (ViewGroup) findViewById(R.id.mobile_combo);
        mMobile         = (ImageView) findViewById(R.id.mobile_signal);
        mMobileActivity = (ImageView) findViewById(R.id.mobile_inout);
        mMobileType     = (ImageView) findViewById(R.id.mobile_type);
        mSpacer         =             findViewById(R.id.spacer);
        mAirplane       = (ImageView) findViewById(R.id.airplane);
//$_rbox_$_modify_$_chenzhi_20120309: for ethernet

//$_rbox_$_modify_$_begin
        mSpacer1         =             findViewById(R.id.spacer1);
        mEthGroup      = (ViewGroup) findViewById(R.id.eth_combo);
        mEth			= (ImageView) findViewById(R.id.eth_state);
//$_rbox_$_modify_$_end
//$_rbox_$_modify_$_chenzhi_20120309: for pppoe
//$_rbox_$_modify_$_begin
		mSpacer2         =             findViewById(R.id.spacer2);
		mPppoeGroup      = (ViewGroup) findViewById(R.id.pppoe_combo);
		mPppoe			= (ImageView) findViewById(R.id.pppoe_state);
//$_rbox_$_modify_$_end
        apply();
    }

    @Override
    protected void onDetachedFromWindow() {
        mWifiGroup      = null;
        mWifi           = null;
        mWifiActivity   = null;
        mMobileGroup    = null;
        mMobile         = null;
        mMobileActivity = null;
        mMobileType     = null;
        mSpacer         = null;
        mAirplane       = null;
//$_rbox_$_modify_$_chenzhi_20120309: for ethernet
//$_rbox_$_modify_$_begin		
        mEthGroup		= null;
        mEth			= null;
//$_rbox_$_modify_$_end: for pppoe
//$_rbox_$_modify_$_chenzhi_20120309
//$_rbox_$_modify_$_begin
		mPppoeGroup		= null;
		mPppoe			= null;
//$_rbox_$_modify_$_end
        super.onDetachedFromWindow();
    }

    @Override
    public void setWifiIndicators(boolean visible, int strengthIcon, int activityIcon,
            String contentDescription) {
        mWifiVisible = visible;
        mWifiStrengthId = strengthIcon;
        mWifiActivityId = activityIcon;
        mWifiDescription = contentDescription;

        apply();
    }
//$_rbox_$_modify_$_chenzhi_20120309: for ethernet
//$_rbox_$_modify_$_begin
    public void setEthIndicators(boolean visible, int stateIcon) {
        mEthVisible = visible;
        mEthStrengthId = stateIcon;
        apply();
    }

//$_rbox_$_modify_$_chenzhi_20120309: for pppoe
//$_rbox_$_modify_$_begin
    public void setPppoeIndicators(boolean visible, int stateIcon) {
        mPppoeVisible = visible;
        mPppoeStrengthId = stateIcon;
        apply();
    }
//$_rbox_$_modify_$_end

    @Override
    public void setMobileDataIndicators(boolean visible, int strengthIcon, int activityIcon,
            int typeIcon, String contentDescription, String typeContentDescription) {
        mMobileVisible = visible;
        mMobileStrengthId = strengthIcon;
        mMobileActivityId = activityIcon;
        mMobileTypeId = typeIcon;
        mMobileDescription = contentDescription;
        mMobileTypeDescription = typeContentDescription;

        apply();
    }

    @Override
    public void setIsAirplaneMode(boolean is, int airplaneIconId) {
        mIsAirplaneMode = is;
        mAirplaneIconId = airplaneIconId;

        apply();
    }

    @Override
    public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event) {
        // Standard group layout onPopulateAccessibilityEvent() implementations
        // ignore content description, so populate manually
        if (mWifiVisible && mWifiGroup.getContentDescription() != null)
            event.getText().add(mWifiGroup.getContentDescription());
        if (mMobileVisible && mMobileGroup.getContentDescription() != null)
            event.getText().add(mMobileGroup.getContentDescription());
        return super.dispatchPopulateAccessibilityEvent(event);
    }

    // Run after each indicator change.
    private void apply() {
        if (mWifiGroup == null) return;

        if (mWifiVisible) {
            mWifiGroup.setVisibility(View.VISIBLE);
            mWifi.setImageResource(mWifiStrengthId);
            mWifiActivity.setImageResource(mWifiActivityId);
            mWifiGroup.setContentDescription(mWifiDescription);
        } else {
            mWifiGroup.setVisibility(View.GONE);
        }

//$_rbox_$_modify_$_chenzhi_20120309: for ethernet
//$_rbox_$_modify_$_begin
		if(mEthVisible)
		{
			mEthGroup.setVisibility(View.VISIBLE);
			mEth.setVisibility(View.VISIBLE);
			mEth.setImageResource(mEthStrengthId);
			mSpacer1.setVisibility(View.INVISIBLE);
		}
		else
		{
            mEthGroup.setVisibility(View.GONE);
			mSpacer1.setVisibility(View.GONE);
		}
//$_rbox_$_modify_$_end

//$_rbox_$_modify_$_chenzhi_20120309: for pppoe
//$_rbox_$_modify_$_begin
		if(mPppoeVisible)
		{
			mPppoeGroup.setVisibility(View.VISIBLE);
			mPppoe.setVisibility(View.VISIBLE);
			mPppoe.setImageResource(mPppoeStrengthId);
			mSpacer2.setVisibility(View.INVISIBLE);
		}
		else
		{
            mPppoeGroup.setVisibility(View.GONE);
			mSpacer2.setVisibility(View.GONE);
		}
//$_rbox_$_modify_$_end
        if (DEBUG) Slog.d(TAG,
                String.format("wifi: %s sig=%d act=%d",
                    (mWifiVisible ? "VISIBLE" : "GONE"),
                    mWifiStrengthId, mWifiActivityId));

        if (mMobileVisible && !mIsAirplaneMode) {
            mMobileGroup.setVisibility(View.VISIBLE);
            mMobile.setImageResource(mMobileStrengthId);
            mMobileActivity.setImageResource(mMobileActivityId);
            mMobileType.setImageResource(mMobileTypeId);
            mMobileGroup.setContentDescription(mMobileTypeDescription + " " + mMobileDescription);
        } else {
            mMobileGroup.setVisibility(View.GONE);
        }

        if (mIsAirplaneMode) {
            mAirplane.setVisibility(View.VISIBLE);
            mAirplane.setImageResource(mAirplaneIconId);
        } else {
            mAirplane.setVisibility(View.GONE);
        }

        if (mMobileVisible && mWifiVisible && mIsAirplaneMode) {
            mSpacer.setVisibility(View.INVISIBLE);
        } else {
            mSpacer.setVisibility(View.GONE);
        }

        if (DEBUG) Slog.d(TAG,
                String.format("mobile: %s sig=%d act=%d typ=%d",
                    (mMobileVisible ? "VISIBLE" : "GONE"),
                    mMobileStrengthId, mMobileActivityId, mMobileTypeId));

        mMobileType.setVisibility(
                !mWifiVisible ? View.VISIBLE : View.GONE);
    }
}


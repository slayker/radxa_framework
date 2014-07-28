/*$_FOR_ROCKCHIP_RBOX_$*/

package com.android.server;

import java.io.FileNotFoundException;
import java.io.FileWriter;

import android.app.ActivityManagerNative;
import android.content.Context;
import android.content.Intent;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.util.Slog;
import android.media.AudioManager;

/* 
 * AudioCommon
 * @Author zxg ,TV Dept.
 * 
 * {@hide}
 */
public class AudioCommon {

	private static final String TAG = AudioCommon.class.getSimpleName();

	public static final String AUDIOPCMLISTUPDATE = "com.android.server.audiopcmlistupdate";
	public static final String HW_AUDIO_CURRENTPLAYBACK = "persist.audio.currentplayback";
	public static final String HW_AUDIO_CURRENTCAPTURE = "persist.audio.currentcapture";
	public static final String HW_AUDIO_LASTSOCPLAYBACK = "persist.audio.lastsocplayback";
	public static final String SOC_AND_SPDIF_KEY = "9";

	private static final String USB_AUDIO_PLAYBACK_SWITCH_STATE_FILE = "/sys/class/switch/usb_audio_playback/state";
	private static final String USB_AUDIO_CAPTURE_SWITCH_STATE_FILE = "/sys/class/switch/usb_audio_capture/state";

	public static final int SND_DEV_TYPE_BASE = 0;
	public static final int SND_DEV_TYPE_USB = SND_DEV_TYPE_BASE + 1;
	public static final int SND_DEV_TYPE_SPDIF = SND_DEV_TYPE_BASE + 2;
	public static final int SND_DEV_TYPE_SOC_SPDIF = SND_DEV_TYPE_BASE + 3;
    public static final int SND_DEV_TYPE_HDMI_5POINT1 = SND_DEV_TYPE_BASE + 4;
    public static final int SND_DEV_TYPE_HDMI_PASSTHROUGH = SND_DEV_TYPE_BASE + 5;
	public static final int SND_DEV_TYPE_DEFAULT = SND_DEV_TYPE_BASE;
	public static final int SND_PCM_STREAM_PLAYBACK = 0;
	public static final int SND_PCM_STREAM_CAPTURE = 1;

	private static AudioManager mAudioManager = null;

	/*
	 * at the moment ,if the audio device is not usb audio. we recorgnise it as
	 * soc audio device. soc audio must be at the "0" place.
	 */
	public static String getCurrentPlaybackDevice() {
		// Slog.v(TAG, "mCurPlaybackDevice: "+mCurPlaybackDevice);
		// return mCurPlaybackDevice;
		return SystemProperties.get(HW_AUDIO_CURRENTPLAYBACK, "0");
	}

	public static String getCurrentCaptureDevice() {
		// Slog.v(TAG, "mCurCaptureDevice: "+mCurCaptureDevice);
		// return mCurCaptureDevice;
		return SystemProperties.get(HW_AUDIO_CURRENTCAPTURE, "0");
	}

	public static void setCurrentPlaybackDevice(String str) {
		// WiredAccessoryObserver.mCurPlaybackDevice = str;
		SystemProperties.set(HW_AUDIO_CURRENTPLAYBACK, str);
	}

	public static void setCurrentCaptureDevice(String str) {
		// mCurCaptureDevice = str;
		SystemProperties.set(HW_AUDIO_CURRENTCAPTURE, str);
	}

	public static void setLastSocPlayback(String str) {
		SystemProperties.set(HW_AUDIO_LASTSOCPLAYBACK, str);
	}

	public static String getLastSocPlayback() {
		return SystemProperties.get(HW_AUDIO_LASTSOCPLAYBACK, "0");
	}

	public static void setDeviceConnectionState(Context ctx, int device,
			int state) {
		if (mAudioManager == null)
			mAudioManager = (AudioManager) ctx
					.getSystemService(Context.AUDIO_SERVICE);

		mAudioManager.setWiredDeviceConnectionState(device, state, "");

	}

	/*
	 * currently, we just deal with usb audio, spdif devices routing. ofcourse,
	 * it can be extended.
	 */
	public static void doAudioDevicesRouting(Context ctx, int deviceType,
			int streamType, String state) {

		switch (deviceType) {
		case SND_DEV_TYPE_SOC_SPDIF:
			doUsbAudioDevicesRouting(streamType, "-1");
			if (streamType == SND_PCM_STREAM_PLAYBACK) {
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_ANLG_DOCK_HEADSET, 1);
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_AUX_DIGITAL, 1);
				setLastSocPlayback(state);
			}
			break;

		case SND_DEV_TYPE_SPDIF:
			doUsbAudioDevicesRouting(streamType, "-1");
			if (streamType == SND_PCM_STREAM_PLAYBACK) {
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_AUX_DIGITAL, 0);
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_ANLG_DOCK_HEADSET, 1);
				setLastSocPlayback(state);
			}
			break;

		case SND_DEV_TYPE_USB:
			doUsbAudioDevicesRouting(streamType, state);
			break;

        case SND_DEV_TYPE_HDMI_5POINT1:
            doUsbAudioDevicesRouting(streamType, "-1");
			if(streamType == SND_PCM_STREAM_PLAYBACK){
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_ANLG_DOCK_HEADSET, 0);
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_AUX_DIGITAL, 1);				
				setLastSocPlayback(state);
			}			
            break;
            
        case SND_DEV_TYPE_HDMI_PASSTHROUGH:
            doUsbAudioDevicesRouting(streamType, "-1");
			if(streamType == SND_PCM_STREAM_PLAYBACK){
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_ANLG_DOCK_HEADSET, 0);
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_AUX_DIGITAL, 1);				
				setLastSocPlayback(state);
			}			
            break;

		default:
			doUsbAudioDevicesRouting(streamType, "-1");
			if (streamType == SND_PCM_STREAM_PLAYBACK) {
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_ANLG_DOCK_HEADSET, 0);
				setDeviceConnectionState(ctx,
						AudioManager.DEVICE_OUT_AUX_DIGITAL, 0);
				setLastSocPlayback(state);
			}
			break;
		}

		if (deviceType == SND_DEV_TYPE_DEFAULT)
			setCurrentPlaybackDevice("0");
		else
			setCurrentPlaybackDevice(state);

		ActivityManagerNative.broadcastStickyIntent(new Intent(
				AUDIOPCMLISTUPDATE), null, UserHandle.USER_ALL);
	}

	public static void doUsbAudioDevicesRouting(int streamType, String state) {
		FileWriter fw;
		int card = Integer.parseInt(state);
		if (card > 0)
			card = card * 10;
		state = Integer.toString(card);
		switch (streamType) {
		case SND_PCM_STREAM_PLAYBACK:
			try {
				fw = new FileWriter(USB_AUDIO_PLAYBACK_SWITCH_STATE_FILE);
				fw.write(state);
				fw.close();
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (Exception e) {
				e.printStackTrace();
			}
			break;
		case SND_PCM_STREAM_CAPTURE:
			try {
				fw = new FileWriter(USB_AUDIO_CAPTURE_SWITCH_STATE_FILE);
				fw.write(state);
				fw.close();
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (Exception e) {
				e.printStackTrace();
			}
			break;

		default:
			Slog.e(TAG, "unknown exception!");
			break;
		}
	}

}


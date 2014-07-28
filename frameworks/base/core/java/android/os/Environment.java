/*$_FOR_ROCKCHIP_RBOX_$*/
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

import android.content.res.Resources;
import android.os.storage.IMountService;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import java.io.File;

/**
 * Provides access to environment variables.
 */
public class Environment {
    private static final String TAG = "Environment";

    private static final String ENV_EXTERNAL_STORAGE = "EXTERNAL_STORAGE";
    private static final String ENV_EMULATED_STORAGE_SOURCE = "EMULATED_STORAGE_SOURCE";
    private static final String ENV_EMULATED_STORAGE_TARGET = "EMULATED_STORAGE_TARGET";
    private static final String ENV_MEDIA_STORAGE = "MEDIA_STORAGE";

    /** {@hide} */
    public static String DIRECTORY_ANDROID = "Android";

    private static final File ROOT_DIRECTORY
            = getDirectory("ANDROID_ROOT", "/system");

    private static final String SYSTEM_PROPERTY_EFS_ENABLED = "persist.security.efs.enabled";

/*$_rbox_$_modify_$_lijiehong_begin$20120319$*/
    private static IMountService mMntSvc = null;
/*$_rbox_$_modify_$_lijiehong_end$20120319$*/
    private static UserEnvironment sCurrentUser;

    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static volatile StorageVolume sPrimaryVolume;

    private static StorageVolume getPrimaryVolume() {
        if (sPrimaryVolume == null) {
            synchronized (sLock) {
                if (sPrimaryVolume == null) {
                    try {
                        IMountService mountService = IMountService.Stub.asInterface(ServiceManager
                                .getService("mount"));
                        final StorageVolume[] volumes = mountService.getVolumeList();
                        sPrimaryVolume = StorageManager.getPrimaryVolume(volumes);
                    } catch (Exception e) {
                        Log.e(TAG, "couldn't talk to MountService", e);
                    }
                }
            }
        }
        return sPrimaryVolume;
    }

    static {
        initForCurrentUser();
    }

    /** {@hide} */
    public static void initForCurrentUser() {
        final int userId = UserHandle.myUserId();
        sCurrentUser = new UserEnvironment(userId);

        synchronized (sLock) {
            sPrimaryVolume = null;
        }
    }

    /** {@hide} */
    public static class UserEnvironment {
        // TODO: generalize further to create package-specific environment

        private final File mExternalStorage;
        private final File mExternalStorageAndroidData;
        private final File mExternalStorageAndroidMedia;
        private final File mExternalStorageAndroidObb;
        private final File mMediaStorage;

        public UserEnvironment(int userId) {
            // See storage config details at http://source.android.com/tech/storage/
            String rawExternalStorage = System.getenv(ENV_EXTERNAL_STORAGE);
            String rawEmulatedStorageTarget = System.getenv(ENV_EMULATED_STORAGE_TARGET);
            String rawMediaStorage = System.getenv(ENV_MEDIA_STORAGE);
            if (TextUtils.isEmpty(rawMediaStorage)) {
                rawMediaStorage = "/data/media";
            }

            if (!TextUtils.isEmpty(rawEmulatedStorageTarget)) {
                // Device has emulated storage; external storage paths should have
                // userId burned into them.
                final String rawUserId = Integer.toString(userId);
                final File emulatedBase = new File(rawEmulatedStorageTarget);
                final File mediaBase = new File(rawMediaStorage);

                // /storage/emulated/0
                mExternalStorage = buildPath(emulatedBase, rawUserId);
                // /data/media/0
                mMediaStorage = buildPath(mediaBase, rawUserId);

            } else {
                // Device has physical external storage; use plain paths.
                if (TextUtils.isEmpty(rawExternalStorage)) {
                    Log.w(TAG, "EXTERNAL_STORAGE undefined; falling back to default");
                    rawExternalStorage = "/storage/sdcard0";
                }

                // /storage/sdcard0
                mExternalStorage = new File(rawExternalStorage);
                // /data/media
                mMediaStorage = new File(rawMediaStorage);
            }

            mExternalStorageAndroidObb = buildPath(mExternalStorage, DIRECTORY_ANDROID, "obb");
            mExternalStorageAndroidData = buildPath(mExternalStorage, DIRECTORY_ANDROID, "data");
            mExternalStorageAndroidMedia = buildPath(mExternalStorage, DIRECTORY_ANDROID, "media");
        }

        public File getExternalStorageDirectory() {
            return mExternalStorage;
        }

        public File getExternalStorageObbDirectory() {
            return mExternalStorageAndroidObb;
        }

        public File getExternalStoragePublicDirectory(String type) {
            return new File(mExternalStorage, type);
        }

        public File getExternalStorageAndroidDataDir() {
            return mExternalStorageAndroidData;
        }

        public File getExternalStorageAppDataDirectory(String packageName) {
            return new File(mExternalStorageAndroidData, packageName);
        }

        public File getExternalStorageAppMediaDirectory(String packageName) {
            return new File(mExternalStorageAndroidMedia, packageName);
        }

        public File getExternalStorageAppObbDirectory(String packageName) {
            return new File(mExternalStorageAndroidObb, packageName);
        }

        public File getExternalStorageAppFilesDirectory(String packageName) {
            return new File(new File(mExternalStorageAndroidData, packageName), "files");
        }

        public File getExternalStorageAppCacheDirectory(String packageName) {
            return new File(new File(mExternalStorageAndroidData, packageName), "cache");
        }

        public File getMediaStorageDirectory() {
            return mMediaStorage;
        }
    }

    /**
     * Gets the Android root directory.
     */
    public static File getRootDirectory() {
        return ROOT_DIRECTORY;
    }

    /**
     * Gets the system directory available for secure storage.
     * If Encrypted File system is enabled, it returns an encrypted directory (/data/secure/system).
     * Otherwise, it returns the unencrypted /data/system directory.
     * @return File object representing the secure storage system directory.
     * @hide
     */
    public static File getSystemSecureDirectory() {
        if (isEncryptedFilesystemEnabled()) {
            return new File(SECURE_DATA_DIRECTORY, "system");
        } else {
            return new File(DATA_DIRECTORY, "system");
        }
    }

    /**
     * Gets the data directory for secure storage.
     * If Encrypted File system is enabled, it returns an encrypted directory (/data/secure).
     * Otherwise, it returns the unencrypted /data directory.
     * @return File object representing the data directory for secure storage.
     * @hide
     */
    public static File getSecureDataDirectory() {
        if (isEncryptedFilesystemEnabled()) {
            return SECURE_DATA_DIRECTORY;
        } else {
            return DATA_DIRECTORY;
        }
    }

    /**
     * Return directory used for internal media storage, which is protected by
     * {@link android.Manifest.permission#WRITE_MEDIA_STORAGE}.
     *
     * @hide
     */
    public static File getMediaStorageDirectory() {
        throwIfSystem();
        return sCurrentUser.getMediaStorageDirectory();
    }

    /**
     * Return the system directory for a user. This is for use by system services to store
     * files relating to the user. This directory will be automatically deleted when the user
     * is removed.
     *
     * @hide
     */
    public static File getUserSystemDirectory(int userId) {
        return new File(new File(getSystemSecureDirectory(), "users"), Integer.toString(userId));
    }

    /**
     * Returns whether the Encrypted File System feature is enabled on the device or not.
     * @return <code>true</code> if Encrypted File System feature is enabled, <code>false</code>
     * if disabled.
     * @hide
     */
    public static boolean isEncryptedFilesystemEnabled() {
        return SystemProperties.getBoolean(SYSTEM_PROPERTY_EFS_ENABLED, false);
    }

    private static final File DATA_DIRECTORY
            = getDirectory("ANDROID_DATA", "/data");

    /**
     * @hide
     */
    private static final File SECURE_DATA_DIRECTORY
            = getDirectory("ANDROID_SECURE_DATA", "/data/secure");

/*$_rbox_$_modify_$_lijiehong_begin$20120319$*/
    private static final File SECOND_VOLUME_STORAGE_DIRECTORY
            = getDirectory("SECOND_VOLUME_STORAGE", "/mnt/external_sd");

    private static final File INTERNAL_DISK_STORAGE_DIRECTORY
            = getDirectory("INTERNAL_DISK_STORAGE", "/interdisk");
/*$_rbox_$_modify_$_lijiehong_end$20120319$*/
    private static final File DOWNLOAD_CACHE_DIRECTORY = getDirectory("DOWNLOAD_CACHE", "/cache");
/*$_rbox_$_modify_$_lijiehong_begin$20120319$*/
    private static final File HOST_STORAGE_DIRECTORY
            = getDirectory("HOST_STORAGE_DIRECTORY", "/mnt/usb_storage");

    private static final File OTG_STORAGE_DIRECTORY
            = getDirectory("OTG_STORAGE_DIRECTORY", "/mnt/usbb_storage/USB_DISK1");
	
    private static final File HOST_STORAGE_DIRECTORY_EXTERN_0
            = getDirectory("THIRD_VOLUME_STORAGE", "/mnt/usb_storage/USB_DISK0");

    private static final File HOST_STORAGE_DIRECTORY_EXTERN_1
            = getDirectory("HOST_STORAGE_DIRECTORY_EXTERN_1", "/mnt/usb_storage/USB_DISK1");

    private static final File HOST_STORAGE_DIRECTORY_EXTERN_2
            = getDirectory("HOST_STORAGE_DIRECTORY_EXTERN_2", "/mnt/usb_storage/USB_DISK2");

    private static final File HOST_STORAGE_DIRECTORY_EXTERN_3
            = getDirectory("HOST_STORAGE_DIRECTORY_EXTERN_3", "/mnt/usb_storage/USB_DISK3");

    private static final File HOST_STORAGE_DIRECTORY_EXTERN_4
            = getDirectory("HOST_STORAGE_DIRECTORY_EXTERN_4", "/mnt/usb_storage/USB_DISK4");

    private static final File HOST_STORAGE_DIRECTORY_EXTERN_5
            = getDirectory("HOST_STORAGE_DIRECTORY_EXTERN_5", "/mnt/usb_storage/USB_DISK5");
/*$_rbox_$_modify_$_lijiehong_end$20120319$*/

    /**
     * Gets the Android data directory.
     */
    public static File getDataDirectory() {
        return DATA_DIRECTORY;
    }

    /**
     * Gets the Android external storage directory.  This directory may not
     * currently be accessible if it has been mounted by the user on their
     * computer, has been removed from the device, or some other problem has
     * happened.  You can determine its current state with
     * {@link #getExternalStorageState()}.
     * 
     * <p><em>Note: don't be confused by the word "external" here.  This
     * directory can better be thought as media/shared storage.  It is a
     * filesystem that can hold a relatively large amount of data and that
     * is shared across all applications (does not enforce permissions).
     * Traditionally this is an SD card, but it may also be implemented as
     * built-in storage in a device that is distinct from the protected
     * internal storage and can be mounted as a filesystem on a computer.</em></p>
     *
     * <p>On devices with multiple users (as described by {@link UserManager}),
     * each user has their own isolated external storage. Applications only
     * have access to the external storage for the user they're running as.</p>
     *
     * <p>In devices with multiple "external" storage directories (such as
     * both secure app storage and mountable shared storage), this directory
     * represents the "primary" external storage that the user will interact
     * with.</p>
     *
     * <p>Applications should not directly use this top-level directory, in
     * order to avoid polluting the user's root namespace.  Any files that are
     * private to the application should be placed in a directory returned
     * by {@link android.content.Context#getExternalFilesDir
     * Context.getExternalFilesDir}, which the system will take care of deleting
     * if the application is uninstalled.  Other shared files should be placed
     * in one of the directories returned by
     * {@link #getExternalStoragePublicDirectory}.</p>
     *
     * <p>Writing to this path requires the
     * {@link android.Manifest.permission#WRITE_EXTERNAL_STORAGE} permission. In
     * a future platform release, access to this path will require the
     * {@link android.Manifest.permission#READ_EXTERNAL_STORAGE} permission,
     * which is automatically granted if you hold the write permission.</p>
     *
     * <p>This path may change between platform versions, so applications
     * should only persist relative paths.</p>
     * 
     * <p>Here is an example of typical code to monitor the state of
     * external storage:</p>
     * 
     * {@sample development/samples/ApiDemos/src/com/example/android/apis/content/ExternalStorage.java
     * monitor_storage}
     *
     * @see #getExternalStorageState()
     * @see #isExternalStorageRemovable()
     */
    public static File getExternalStorageDirectory() {
        throwIfSystem();
        return sCurrentUser.getExternalStorageDirectory();
    }

/*$_rbox_$_modify_$public static File getSecondVolumeStorageDirectory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: 1. Gets the Android flash storage directory */
    /**
     * 
     * Gets the Android flash storage directory.
     */
    public static File getSecondVolumeStorageDirectory() {
        return SECOND_VOLUME_STORAGE_DIRECTORY;
    }

/*$_rbox_$_modify_$public static File getInterHardDiskStorageDirectory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log:  Gets the Android internal harddisk storage directory */
    /**
     * 
     * Gets the Android internal harddisk storage directory.
     */
    public static File getInterHardDiskStorageDirectory() {
        return INTERNAL_DISK_STORAGE_DIRECTORY;
    }

/*$_rbox_$_modify_$public static File getHostStorageDirectory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log:  Gets the Android host storage directory */
     /**
     * * 
     * * Gets the Android host storage directory
     */
    public static File getHostStorageDirectory() {
        return HOST_STORAGE_DIRECTORY;
    }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_0_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_0 directory */
    /**
     * * 
     * * Gets the Android host storage extern_0  directory
     */
    public static File getHostStorage_Extern_0_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_0;
    }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_1_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_1 directory */
    /**
     * * 
     * * Gets the Android host storage extern_1  directory
     */
    public static File getHostStorage_Extern_1_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_1;
    }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_2_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_2 directory */
    /**
     * * 
     * * Gets the Android host storage extern_2 directory
     */
    public static File getHostStorage_Extern_2_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_2;
    }

/*$_rbox_$_modify_$public static File getOTGStorageDirectory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage OTG directory */
	public static File getOTGStorageDirectory() {
		 return OTG_STORAGE_DIRECTORY;
	 }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_3_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_3 directory */
    /**
     * * 
     * * Gets the Android host storage extern_3 directory
     */
    public static File getHostStorage_Extern_3_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_3;
    }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_4_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_4 directory */
    /**
     * * 
     * * Gets the Android host storage extern_4 directory
     */
    public static File getHostStorage_Extern_4_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_4;
    }

/*$_rbox_$_modify_$public static File getHostStorage_Extern_5_Directory()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the Android host storage extern_5 directory */
     /**
     * * 
     * * Gets the Android host storage extern_5 directory
     */
    public static File getHostStorage_Extern_5_Directory() {
        return HOST_STORAGE_DIRECTORY_EXTERN_5;
    }

    /** {@hide} */
    public static File getLegacyExternalStorageDirectory() {
        return new File(System.getenv(ENV_EXTERNAL_STORAGE));
    }

    /** {@hide} */
    public static File getLegacyExternalStorageObbDirectory() {
        return buildPath(getLegacyExternalStorageDirectory(), DIRECTORY_ANDROID, "obb");
    }

    /** {@hide} */
    public static File getEmulatedStorageSource(int userId) {
        // /mnt/shell/emulated/0
        return new File(System.getenv(ENV_EMULATED_STORAGE_SOURCE), String.valueOf(userId));
    }

    /** {@hide} */
    public static File getEmulatedStorageObbSource() {
        // /mnt/shell/emulated/obb
        return new File(System.getenv(ENV_EMULATED_STORAGE_SOURCE), "obb");
    }

    /**
     * Standard directory in which to place any audio files that should be
     * in the regular list of music for the user.
     * This may be combined with
     * {@link #DIRECTORY_PODCASTS}, {@link #DIRECTORY_NOTIFICATIONS},
     * {@link #DIRECTORY_ALARMS}, and {@link #DIRECTORY_RINGTONES} as a series
     * of directories to categories a particular audio file as more than one
     * type.
     */
    public static String DIRECTORY_MUSIC = "Music";
    
    /**
     * Standard directory in which to place any audio files that should be
     * in the list of podcasts that the user can select (not as regular
     * music).
     * This may be combined with {@link #DIRECTORY_MUSIC},
     * {@link #DIRECTORY_NOTIFICATIONS},
     * {@link #DIRECTORY_ALARMS}, and {@link #DIRECTORY_RINGTONES} as a series
     * of directories to categories a particular audio file as more than one
     * type.
     */
    public static String DIRECTORY_PODCASTS = "Podcasts";
    
    /**
     * Standard directory in which to place any audio files that should be
     * in the list of ringtones that the user can select (not as regular
     * music).
     * This may be combined with {@link #DIRECTORY_MUSIC},
     * {@link #DIRECTORY_PODCASTS}, {@link #DIRECTORY_NOTIFICATIONS}, and
     * {@link #DIRECTORY_ALARMS} as a series
     * of directories to categories a particular audio file as more than one
     * type.
     */
    public static String DIRECTORY_RINGTONES = "Ringtones";
    
    /**
     * Standard directory in which to place any audio files that should be
     * in the list of alarms that the user can select (not as regular
     * music).
     * This may be combined with {@link #DIRECTORY_MUSIC},
     * {@link #DIRECTORY_PODCASTS}, {@link #DIRECTORY_NOTIFICATIONS},
     * and {@link #DIRECTORY_RINGTONES} as a series
     * of directories to categories a particular audio file as more than one
     * type.
     */
    public static String DIRECTORY_ALARMS = "Alarms";
    
    /**
     * Standard directory in which to place any audio files that should be
     * in the list of notifications that the user can select (not as regular
     * music).
     * This may be combined with {@link #DIRECTORY_MUSIC},
     * {@link #DIRECTORY_PODCASTS},
     * {@link #DIRECTORY_ALARMS}, and {@link #DIRECTORY_RINGTONES} as a series
     * of directories to categories a particular audio file as more than one
     * type.
     */
    public static String DIRECTORY_NOTIFICATIONS = "Notifications";
    
    /**
     * Standard directory in which to place pictures that are available to
     * the user.  Note that this is primarily a convention for the top-level
     * public directory, as the media scanner will find and collect pictures
     * in any directory.
     */
    public static String DIRECTORY_PICTURES = "Pictures";
    
    /**
     * Standard directory in which to place movies that are available to
     * the user.  Note that this is primarily a convention for the top-level
     * public directory, as the media scanner will find and collect movies
     * in any directory.
     */
    public static String DIRECTORY_MOVIES = "Movies";
    
    /**
     * Standard directory in which to place files that have been downloaded by
     * the user.  Note that this is primarily a convention for the top-level
     * public directory, you are free to download files anywhere in your own
     * private directories.  Also note that though the constant here is
     * named DIRECTORY_DOWNLOADS (plural), the actual file name is non-plural for
     * backwards compatibility reasons.
     */
    public static String DIRECTORY_DOWNLOADS = "Download";
    
    /**
     * The traditional location for pictures and videos when mounting the
     * device as a camera.  Note that this is primarily a convention for the
     * top-level public directory, as this convention makes no sense elsewhere.
     */
    public static String DIRECTORY_DCIM = "DCIM";
    
    /**
     * Get a top-level public external storage directory for placing files of
     * a particular type.  This is where the user will typically place and
     * manage their own files, so you should be careful about what you put here
     * to ensure you don't erase their files or get in the way of their own
     * organization.
     * 
     * <p>On devices with multiple users (as described by {@link UserManager}),
     * each user has their own isolated external storage. Applications only
     * have access to the external storage for the user they're running as.</p>
     *
     * <p>Here is an example of typical code to manipulate a picture on
     * the public external storage:</p>
     * 
     * {@sample development/samples/ApiDemos/src/com/example/android/apis/content/ExternalStorage.java
     * public_picture}
     * 
     * @param type The type of storage directory to return.  Should be one of
     * {@link #DIRECTORY_MUSIC}, {@link #DIRECTORY_PODCASTS},
     * {@link #DIRECTORY_RINGTONES}, {@link #DIRECTORY_ALARMS},
     * {@link #DIRECTORY_NOTIFICATIONS}, {@link #DIRECTORY_PICTURES},
     * {@link #DIRECTORY_MOVIES}, {@link #DIRECTORY_DOWNLOADS}, or
     * {@link #DIRECTORY_DCIM}.  May not be null.
     * 
     * @return Returns the File path for the directory.  Note that this
     * directory may not yet exist, so you must make sure it exists before
     * using it such as with {@link File#mkdirs File.mkdirs()}.
     */
    public static File getExternalStoragePublicDirectory(String type) {
        throwIfSystem();
        return sCurrentUser.getExternalStoragePublicDirectory(type);
    }

    /**
     * Returns the path for android-specific data on the SD card.
     * @hide
     */
    public static File getExternalStorageAndroidDataDir() {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAndroidDataDir();
    }
    
    /**
     * Generates the raw path to an application's data
     * @hide
     */
    public static File getExternalStorageAppDataDirectory(String packageName) {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAppDataDirectory(packageName);
    }
    
    /**
     * Generates the raw path to an application's media
     * @hide
     */
    public static File getExternalStorageAppMediaDirectory(String packageName) {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAppMediaDirectory(packageName);
    }
    
    /**
     * Generates the raw path to an application's OBB files
     * @hide
     */
    public static File getExternalStorageAppObbDirectory(String packageName) {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAppObbDirectory(packageName);
    }
    
    /**
     * Generates the path to an application's files.
     * @hide
     */
    public static File getExternalStorageAppFilesDirectory(String packageName) {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAppFilesDirectory(packageName);
    }

    /**
     * Generates the path to an application's cache.
     * @hide
     */
    public static File getExternalStorageAppCacheDirectory(String packageName) {
        throwIfSystem();
        return sCurrentUser.getExternalStorageAppCacheDirectory(packageName);
    }
    
    /**
     * Gets the Android download/cache content directory.
     */
    public static File getDownloadCacheDirectory() {
        return DOWNLOAD_CACHE_DIRECTORY;
    }

    /**
     * {@link #getExternalStorageState()} returns MEDIA_REMOVED if the media is not present.
     */
    public static final String MEDIA_REMOVED = "removed";
     
    /**
     * {@link #getExternalStorageState()} returns MEDIA_UNMOUNTED if the media is present
     * but not mounted. 
     */
    public static final String MEDIA_UNMOUNTED = "unmounted";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_CHECKING if the media is present
     * and being disk-checked
     */
    public static final String MEDIA_CHECKING = "checking";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_NOFS if the media is present
     * but is blank or is using an unsupported filesystem
     */
    public static final String MEDIA_NOFS = "nofs";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_MOUNTED if the media is present
     * and mounted at its mount point with read/write access. 
     */
    public static final String MEDIA_MOUNTED = "mounted";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_MOUNTED_READ_ONLY if the media is present
     * and mounted at its mount point with read only access. 
     */
    public static final String MEDIA_MOUNTED_READ_ONLY = "mounted_ro";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_SHARED if the media is present
     * not mounted, and shared via USB mass storage. 
     */
    public static final String MEDIA_SHARED = "shared";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_BAD_REMOVAL if the media was
     * removed before it was unmounted. 
     */
    public static final String MEDIA_BAD_REMOVAL = "bad_removal";

    /**
     * {@link #getExternalStorageState()} returns MEDIA_UNMOUNTABLE if the media is present
     * but cannot be mounted.  Typically this happens if the file system on the
     * media is corrupted. 
     */
    public static final String MEDIA_UNMOUNTABLE = "unmountable";

    /**
     * Gets the current state of the primary "external" storage device.
     * 
     * @see #getExternalStorageDirectory()
     */
    public static String getExternalStorageState() {
        try {
            IMountService mountService = IMountService.Stub.asInterface(ServiceManager
                    .getService("mount"));
            final StorageVolume primary = getPrimaryVolume();
            return mountService.getVolumeState(primary.getPath());
        } catch (RemoteException rex) {
            Log.w(TAG, "Failed to read external storage state; assuming REMOVED: " + rex);
            return Environment.MEDIA_REMOVED;
        }
    }

/*$_rbox_$_modify_$public static String getSecondVolumeStorageState()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the flash storage device */
    /**
     * 
     * Gets the current state of the flash storage device.
     */
    public static String getSecondVolumeStorageState() {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getSecondVolumeStorageDirectory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }

    }

/*$_rbox_$_modify_$public static String getInterHardDiskStorageState()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the flash storage device */
    /**
     * 
     * Gets the current state of the flash storage device.
     */
    public static String getInterHardDiskStorageState() {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getInterHardDiskStorageDirectory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }

    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_0_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the host storage device,usb0 */
    /**
     *
     *  Gets the current state of the host storage device,usb0.
     */
    public static String getHostStorage_Extern_0_State()
    {
        try {
			
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
			String volState;
			volState =mMntSvc.getVolumeState(getHostStorage_Extern_0_Directory().toString());
            return volState;
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_1_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the host storage device,usb1 */
    /**
     *
     *  Gets the current state of the host storage device,usb1.
     */
    public static String getHostStorage_Extern_1_State()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
			String volState;
			volState =mMntSvc.getVolumeState(getHostStorage_Extern_1_Directory().toString());
            return volState;

        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_2_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the host storage device,usb2 */
    /**
     *
     *  Gets the current state of the host storage device,usb2.
     */
    public static String getHostStorage_Extern_2_State()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
			String volState;
			volState =mMntSvc.getVolumeState(getHostStorage_Extern_1_Directory().toString());
            return volState;

        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_3_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the host storage device,usb3 */
    /**
     *
     *  Gets the current state of the host storage device,usb3.
     */
    public static String getHostStorage_Extern_3_State()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getHostStorage_Extern_3_Directory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_4_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current state of the host storage device,usb4 */
    /**
     *
     *  Gets the current state of the host storage device,usb4.
     */
    public static String getHostStorage_Extern_4_State()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getHostStorage_Extern_4_Directory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getHostStorage_Extern_5_State()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: 1. Gets the current state of the host storage device,usb5 */
    /**
     * 
     *  Gets the current state of the host storage device,usb5.
     */
    public static String getHostStorage_Extern_5_State()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getHostStorage_Extern_5_Directory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

/*$_rbox_$_modify_$public static String getOTGStorageState()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current removable state of the OTG storage device */
    public static String getOTGStorageState()
    {
        try {
            if (mMntSvc == null) {
                mMntSvc = IMountService.Stub.asInterface(ServiceManager
                                                         .getService("mount"));
            }
            return mMntSvc.getVolumeState(getOTGStorageDirectory().toString());
        } catch (Exception rex) {
            return Environment.MEDIA_REMOVED;
        }
    
    }

    /**
     * Returns whether the primary "external" storage device is removable.
     * If true is returned, this device is for example an SD card that the
     * user can remove.  If false is returned, the storage is built into
     * the device and can not be physically removed.
     *
     * <p>See {@link #getExternalStorageDirectory()} for more information.
     */
    public static boolean isExternalStorageRemovable() {
        final StorageVolume primary = getPrimaryVolume();
        return (primary != null && primary.isRemovable());
    }

/*$_rbox_$_modify_$public static boolean isSecondVolumeStorageRemovable()*/
/*$_rbox_$_modify_$_lijiehong_$_20120319_$*/
/*$_rbox_$_modify_$ log: Gets the current removable state of the sdcard storage device */
    /**
     * 
     * Gets the current removable state of the sdcard storage device.
     */
    public static boolean isSecondVolumeStorageRemovable() {
        return Resources.getSystem().getBoolean(
                com.android.internal.R.bool.config_secondVolumeStorageRemovable);
    }

    /**
     * Returns whether the device has an external storage device which is
     * emulated. If true, the device does not have real external storage, and the directory
     * returned by {@link #getExternalStorageDirectory()} will be allocated using a portion of
     * the internal storage system.
     *
     * <p>Certain system services, such as the package manager, use this
     * to determine where to install an application.
     *
     * <p>Emulated external storage may also be encrypted - see
     * {@link android.app.admin.DevicePolicyManager#setStorageEncryption(
     * android.content.ComponentName, boolean)} for additional details.
     */
    public static boolean isExternalStorageEmulated() {
        final StorageVolume primary = getPrimaryVolume();
        return (primary != null && primary.isEmulated());
    }

    static File getDirectory(String variableName, String defaultPath) {
        String path = System.getenv(variableName);
        return path == null ? new File(defaultPath) : new File(path);
    }

    private static void throwIfSystem() {
        if (Process.myUid() == Process.SYSTEM_UID) {
            //Log.wtf(TAG, "Static storage paths aren't available from AID_SYSTEM", new Throwable());
            Log.w(TAG, "Static storage paths aren't available from AID_SYSTEM not throw err");
        }
    }

    private static File buildPath(File base, String... segments) {
        File cur = base;
        for (String segment : segments) {
            if (cur == null) {
                cur = new File(segment);
            } else {
                cur = new File(cur, segment);
            }
        }
        return cur;
    }
}

/*
**
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "MediaPlayerFactory"
#include <utils/Log.h>

#include <cutils/properties.h>
#include <media/IMediaPlayer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/Errors.h>
#include <utils/misc.h>

#include "MediaPlayerFactory.h"

#include "MidiFile.h"
#include "TestPlayerStub.h"
#include "StagefrightPlayer.h"
#include "nuplayer/NuPlayerDriver.h"
#include "ApePlayer.h"
#include "FFPlayer.h"

namespace android {

Mutex MediaPlayerFactory::sLock;
MediaPlayerFactory::tFactoryMap MediaPlayerFactory::sFactoryMap;
bool MediaPlayerFactory::sInitComplete = false;

static status_t getFileName(int fd,String8 *FilePath)
{
    static ssize_t link_dest_size;
    static char link_dest[PATH_MAX];
    const char *ptr = NULL;
    String8 path;
    path.appendFormat("/proc/%d/fd/%d", getpid(), fd);
    if ((link_dest_size = readlink(path.string(), link_dest, sizeof(link_dest)-1)) < 0) {
        return errno;
    } else {
        link_dest[link_dest_size] = '\0';
    }
    path = link_dest;
    ptr = path.string();
    *FilePath = String8(ptr);
    return OK;
}

status_t MediaPlayerFactory::registerFactory_l(IFactory* factory,
                                               player_type type) {
    if (NULL == factory) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, factory is"
              " NULL.", type);
        return BAD_VALUE;
    }

    if (sFactoryMap.indexOfKey(type) >= 0) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, type is"
              " already registered.", type);
        return ALREADY_EXISTS;
    }

    if (sFactoryMap.add(type, factory) < 0) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, failed to add"
              " to map.", type);
        return UNKNOWN_ERROR;
    }

    return OK;
}

player_type MediaPlayerFactory::getDefaultPlayerType() {
	ALOGE("MediaPlayerFactory::getDefaultPlayerType");
    char value[PROPERTY_VALUE_MAX];
    if (property_get("media.stagefright.use-nuplayer", value, NULL)
            && (!strcmp("1", value) || !strcasecmp("true", value))) {
        return NU_PLAYER;
    }
   ALOGI("getDefaultPlayerType in");
   return FF_PLAYER;

}

status_t MediaPlayerFactory::registerFactory(IFactory* factory,
                                             player_type type) {
    Mutex::Autolock lock_(&sLock);
    return registerFactory_l(factory, type);
}

void MediaPlayerFactory::unregisterFactory(player_type type) {
    Mutex::Autolock lock_(&sLock);
    sFactoryMap.removeItem(type);
}

#define GET_PLAYER_TYPE_IMPL(a...)                      \
    Mutex::Autolock lock_(&sLock);                      \
                                                        \
  	player_type ret = FF_PLAYER;               \
  	char value_pro[PROPERTY_VALUE_MAX];										\
		if(property_get("sys.ffmpeg_sf.switch", value_pro, NULL)){	\
			if (atoi(value_pro) > 0){																	\
				ret =  STAGEFRIGHT_PLAYER;															\
			}																													\
		}																													\ 
    float bestScore = 0.0;                              \
                                                        \
    for (size_t i = 0; i < sFactoryMap.size(); ++i) {   \
                                                        \
        IFactory* v = sFactoryMap.valueAt(i);           \
        float thisScore;                                \
        CHECK(v != NULL);                               \
        thisScore = v->scoreFactory(a, bestScore);      \
        if (thisScore > bestScore) {                    \
            ret = sFactoryMap.keyAt(i);                 \
            bestScore = thisScore;                      \
        }                                               \
    }                                                   \
                                                        \
    if (0.0 == bestScore) {                             \
        bestScore = getDefaultPlayerType();             \
    }                                                   \
                                                        \
    return ret;

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              const char* url,KeyedVector<String8, String8> *headers ) {
    if (!strncasecmp("iptv://", url, 7)) {
        return STAGEFRIGHT_PLAYER;
    }

	if (!strncasecmp("udpwimo", url, 7)) {
        return STAGEFRIGHT_PLAYER;
    }

	if (!strncasecmp("DVBTV://", url, 8)) {
        return STAGEFRIGHT_PLAYER;
    }
/*
    if (!strncasecmp("http://", url, 7)
            || !strncasecmp("https://", url, 8)) {

        if (strstr(url,"hls")) {
            return NU_PLAYER;
        }
    }     */
	//add by xhr, in order for camere can run sf.
    if(strstr(url,".ogg")){
		return STAGEFRIGHT_PLAYER;	
    }

    if(strstr(url,".wvm")){
           return STAGEFRIGHT_PLAYER;
    }
    
    GET_PLAYER_TYPE_IMPL(client, url, headers);
}

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              int fd,
                                              int64_t offset,
                                              int64_t length) {
    String8 filePath;
    getFileName(fd,&filePath);
    if(strstr(filePath.string(),".apk"))
    {
        ALOGD("FILE:(%s) get STAGEFRIGHT_PLAYER",filePath.string());
        return STAGEFRIGHT_PLAYER;
    }
    
	//add by xhr, in order for camere can run sf.
    if(strstr(filePath.string(),".ogg")){
		return STAGEFRIGHT_PLAYER;	
	}

    if(strstr(filePath.string(),".wvm")){
                return STAGEFRIGHT_PLAYER;
        }
    GET_PLAYER_TYPE_IMPL(client, fd, offset, length);
}

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              const sp<IStreamSource> &source) {
    return NU_PLAYER; //just for WFD modified by ht
//    GET_PLAYER_TYPE_IMPL(client, source);
}

#undef GET_PLAYER_TYPE_IMPL

sp<MediaPlayerBase> MediaPlayerFactory::createPlayer(
        player_type playerType,
        void* cookie,
        notify_callback_f notifyFunc) {
    sp<MediaPlayerBase> p;
    IFactory* factory;
    status_t init_result;
    
    ALOGD("createPlayer:playerType=%d",playerType);
    Mutex::Autolock lock_(&sLock);

    if (sFactoryMap.indexOfKey(playerType) < 0) {
        ALOGE("Failed to create player object of type %d, no registered"
              " factory", playerType);
        return p;
    }

    factory = sFactoryMap.valueFor(playerType);
    CHECK(NULL != factory);
    p = factory->createPlayer();

    if (p == NULL) {
        ALOGE("Failed to create player object of type %d, create failed",
               playerType);
        return p;
    }

    init_result = p->initCheck();
    if (init_result == NO_ERROR) {
        p->setNotifyCallback(cookie, notifyFunc);
    } else {
        ALOGE("Failed to create player object of type %d, initCheck failed"
              " (res = %d)", playerType, init_result);
        p.clear();
    }

    return p;
}

/*****************************************************************************
 *                                                                           *
 *                     Built-In Factory Implementations                      *
 *                                                                           *
 *****************************************************************************/

class StagefrightPlayerFactory :
    public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               int fd,
                               int64_t offset,
                               int64_t length,
                               float curScore) {
        char buf[20];
        lseek(fd, offset, SEEK_SET);
        read(fd, buf, sizeof(buf));
        lseek(fd, offset, SEEK_SET);

        long ident = *((long*)buf);

        // Ogg vorbis?
        if (ident == 0x5367674f) // 'OggS'
            return 1.0;

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGD(" create StagefrightPlayer");
        return new StagefrightPlayer();
    }
};

extern player_type UrlCheckPlayerType(const char* url,KeyedVector<String8, String8> *headers);

class NuPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               const char* url,KeyedVector<String8, String8> *headers,
                               float curScore) {
        static const float kOurScore = 0.0;

        if (kOurScore <= curScore)
            return 0.0;

       /* if (!strncasecmp("http://", url, 7)
                || !strncasecmp("https://", url, 8)) {
            size_t len = strlen(url);
            if (len >= 5 && !strcasecmp(".m3u8", &url[len - 5])) {
                return kOurScore;
            }

            if (strstr(url,"m3u8")) {
                return kOurScore;
            }
        }

        if (!strncasecmp("rtsp://", url, 7)) {
            return kOurScore;
        }*/
      /* if(UrlCheckPlayerType(url,headers)== NU_PLAYER){
			return kOurScore;
		}*/

        return 0.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               const sp<IStreamSource> &source,
                               float curScore) {
        return 1.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGV(" create NuPlayer");
        return new NuPlayerDriver;
    }
};

class SonivoxPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               const char* url,KeyedVector<String8, String8> *headers,
                               float curScore) {
        static const float kOurScore = 0.4;
        static const char* const FILE_EXTS[] = { ".mid",
                                                 ".midi",
                                                 ".smf",
                                                 ".xmf",
                                                 ".mxmf",
                                                 ".imy",
                                                 ".rtttl",
                                                 ".rtx",
                                                 ".ota" };
        if (kOurScore <= curScore)
            return 0.0;

        // use MidiFile for MIDI extensions
        int lenURL = strlen(url);
        for (int i = 0; i < NELEM(FILE_EXTS); ++i) {
            int len = strlen(FILE_EXTS[i]);
            int start = lenURL - len;
            if (start > 0) {
                if (!strncasecmp(url + start, FILE_EXTS[i], len)) {
                    return kOurScore;
                }
            }
        }

        return 0.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               int fd,
                               int64_t offset,
                               int64_t length,
                               float curScore) {
        static const float kOurScore = 0.8;

        if (kOurScore <= curScore)
            return 0.0;

        // Some kind of MIDI?
        EAS_DATA_HANDLE easdata;
        if (EAS_Init(&easdata) == EAS_SUCCESS) {
            EAS_FILE locator;
            locator.path = NULL;
            locator.fd = fd;
            locator.offset = offset;
            locator.length = length;
            EAS_HANDLE  eashandle;
            if (EAS_OpenFile(easdata, &locator, &eashandle) == EAS_SUCCESS) {
                EAS_CloseFile(easdata, eashandle);
                EAS_Shutdown(easdata);
                return kOurScore;
            }
            EAS_Shutdown(easdata);
        }

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGV(" create MidiFile");
        return new MidiFile();
    }
};

class TestPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               const char* url,KeyedVector<String8, String8> *headers,
                               float curScore) {
        if (TestPlayerStub::canBeUsed(url)) {
            return 1.0;
        }

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGV("Create Test Player stub");
        return new TestPlayerStub();
    }
};

class ApePlayerFactory :
    public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               int fd,
                               int64_t offset,
                               int64_t length,
                               float curScore) {
        char buf[20];
        lseek(fd, offset, SEEK_SET);
        read(fd, buf, sizeof(buf));
        lseek(fd, offset, SEEK_SET);

		if(!memcmp("ID3", buf, 3))
		{
			size_t len =
					((buf[6] & 0x7f) << 21)
					| ((buf[7] & 0x7f) << 14)
					| ((buf[8] & 0x7f) << 7)
					| (buf[9] & 0x7f);

			len += 10;
			lseek(fd, offset+len, SEEK_SET);
			read(fd, buf, sizeof(buf));
			lseek(fd, offset, SEEK_SET);
		}

		long ident = *((long*)buf);

		//Ape?
		if (ident == 0x2043414D)
            return 1.0;

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGV(" create ApePlayer");
        return new ApePlayer();
    }
};
class FFPlayerFactory :
    public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& client,
                               int fd,
                               int64_t offset,
                               int64_t length,
                               float curScore) {

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer() {
        ALOGI(" create FFPlayer");
        return new FFPlayer();
    }
};


void MediaPlayerFactory::registerBuiltinFactories() {
    Mutex::Autolock lock_(&sLock);

    if (sInitComplete)
        return;

    registerFactory_l(new StagefrightPlayerFactory(), STAGEFRIGHT_PLAYER);
    registerFactory_l(new NuPlayerFactory(), NU_PLAYER);
    registerFactory_l(new SonivoxPlayerFactory(), SONIVOX_PLAYER);
    registerFactory_l(new TestPlayerFactory(), TEST_PLAYER);
	registerFactory_l(new ApePlayerFactory(),APE_PLAYER);
	registerFactory_l(new FFPlayerFactory(),FF_PLAYER);
    sInitComplete = true;
}

}  // namespace android

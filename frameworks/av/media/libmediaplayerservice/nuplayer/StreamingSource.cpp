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

//#define LOG_NDEBUG 0
#define LOG_TAG "StreamingSource"
#include <utils/Log.h>

#include "StreamingSource.h"

#include "ATSParser.h"
#include "AnotherPacketSource.h"
#include "NuPlayerStreamListener.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

namespace android {

NuPlayer::StreamingSource::StreamingSource(const sp<IStreamSource> &source)
    : mSource(source),
      mFinalResult(OK) {
	  sys_time_base = streaming_audio_start_timeUs = streaming_sys_start_timeUs = StreamingSource_Sign = 0;
}

NuPlayer::StreamingSource::~StreamingSource() {
}

void NuPlayer::StreamingSource::start() {
    mStreamListener = new NuPlayerStreamListener(mSource, 0);

    uint32_t sourceFlags = mSource->flags();

    uint32_t parserFlags = ATSParser::TS_TIMESTAMPS_ARE_ABSOLUTE;
    if ((sourceFlags & 0x0000ffff) & IStreamSource::kFlagAlignedVideoData) {
        parserFlags |= ATSParser::ALIGNED_VIDEO_DATA;
    }
	if((sourceFlags>> 16 )== 0x1234)
		StreamingSource_Sign = 1;
	ALOGD("NuPlayer::StreamingSource::start sourceFlags %x",sourceFlags);
    mTSParser = new ATSParser(parserFlags);

    mStreamListener->start();
}

status_t NuPlayer::StreamingSource::feedMoreTSData() {
	ssize_t n;
    if (mFinalResult != OK) {
        return mFinalResult;
    }

	do
    {
        char buffer[188];
        sp<AMessage> extra;
        n = mStreamListener->read(buffer, sizeof(buffer), &extra);

        if (n == 0) {
            ALOGI("input data EOS reached.");
            mTSParser->signalEOS(ERROR_END_OF_STREAM);
            mFinalResult = ERROR_END_OF_STREAM;
            break;
        } else if (n == INFO_DISCONTINUITY) {
            int32_t type = ATSParser::DISCONTINUITY_SEEK;

            int32_t mask;
			int64_t sys_time = 0ll;
			int64_t timeUs = 0ll;
			int	temp;
            if (extra != NULL
                    && extra->findInt32(
                        IStreamListener::kKeyDiscontinuityMask, &mask)) {
                if (mask == 0) {
                    ALOGE("Client specified an illegal discontinuity type.");
                    return ERROR_UNSUPPORTED;
                }

                type = mask;
            }
			extra->findInt64(
                        "timeUs", &timeUs);
			if(!extra->findInt64(
                        "wifidisplay_sys_timeUs", &sys_time)  )
			{
            mTSParser->signalDiscontinuity(
                    (ATSParser::DiscontinuityType)type, extra);
			}
			if(extra->findInt32(
                        "first_packet", &temp)  )
			{
				mTSParser->set_player_type(3);// jmj for wfd
				ALOGD("first_packet set ATsparser type 3");
			}
			if(StreamingSource_Sign == 1)
			{
				streaming_sys_start_timeUs 		= sys_time;
				streaming_audio_start_timeUs	= 	timeUs ;
			}
        } else if (n < 0) {
            CHECK_EQ(n, -EWOULDBLOCK);
            break;
        } else {
            if (buffer[0] == 0x00) {
                // XXX legacy

                if (extra == NULL) {
                    extra = new AMessage;
                }

                uint8_t type = buffer[1];

                if (type & 2) {
                    int64_t mediaTimeUs;
                    memcpy(&mediaTimeUs, &buffer[2], sizeof(mediaTimeUs));

                    extra->setInt64(IStreamListener::kKeyMediaTimeUs, mediaTimeUs);
                }

                mTSParser->signalDiscontinuity(
                        ((type & 1) == 0)
                            ? ATSParser::DISCONTINUITY_SEEK
                            : ATSParser::DISCONTINUITY_FORMATCHANGE,
                        extra);
            } else {
                status_t err = mTSParser->feedTSPacket(buffer, sizeof(buffer));

                if (err != OK) {
                    ALOGE("TS Parser returned error %d", err);

                    mTSParser->signalEOS(err);
                    mFinalResult = err;
                    break;
                }
            }
        }
    }while(n>0);

    return OK;
}

sp<MetaData> NuPlayer::StreamingSource::getFormatMeta(bool audio) {
    ATSParser::SourceType type =
        audio ? ATSParser::AUDIO : ATSParser::VIDEO;

    sp<AnotherPacketSource> source =
        static_cast<AnotherPacketSource *>(mTSParser->getSource(type).get());
    if (source == NULL) {
        return NULL;
    }

    return source->getFormat();
}

status_t NuPlayer::StreamingSource::dequeueAccessUnit(
        bool audio, sp<ABuffer> *accessUnit) {
    ATSParser::SourceType type =
        audio ? ATSParser::AUDIO : ATSParser::VIDEO;

    sp<AnotherPacketSource> source =
        static_cast<AnotherPacketSource *>(mTSParser->getSource(type).get());

    if (source == NULL) {
        return -EWOULDBLOCK;
    }

    status_t finalResult;
    if (!source->hasBufferAvailable(&finalResult)) {
        return finalResult == OK ? -EWOULDBLOCK : finalResult;
    }

    status_t err = source->dequeueAccessUnit(accessUnit);

#if !defined(LOG_NDEBUG) || LOG_NDEBUG == 0
    if (err == OK) {
        int64_t timeUs;
        CHECK((*accessUnit)->meta()->findInt64("timeUs", &timeUs));
        ALOGV("dequeueAccessUnit timeUs=%lld us", timeUs);
    }
#endif

    return err;
}
uint32_t NuPlayer::StreamingSource::flags() const {
     return 0;
}
int	NuPlayer::StreamingSource::getwifidisplay_info(int *info)
{
	return (StreamingSource_Sign != 0);
};
int	NuPlayer::StreamingSource::Wifidisplay_get_TimeInfo(int64_t *start_time,int64_t *audio_start_time)
{

	if(1 == StreamingSource_Sign)
	{
		if(streaming_sys_start_timeUs !=0 && streaming_audio_start_timeUs !=0 )
		{
			*start_time 		=	streaming_sys_start_timeUs;
			*audio_start_time	=	streaming_audio_start_timeUs;
		}
	}
	ALOGV("StreamingSource_Sign %d start_time %lld %lld",StreamingSource_Sign,*start_time,*audio_start_time );
	return (1 == StreamingSource_Sign) ? 0 : -1;
}

}  // namespace android


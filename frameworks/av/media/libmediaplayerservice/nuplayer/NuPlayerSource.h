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

#ifndef NUPLAYER_SOURCE_H_

#define NUPLAYER_SOURCE_H_

#include "NuPlayer.h"

namespace android {

struct ABuffer;

struct NuPlayer::Source : public RefBase {
    enum Flags {
        FLAG_SEEKABLE           = 1,
        FLAG_DYNAMIC_DURATION   = 2,
    };

    Source() {}

    virtual void start() = 0;
    virtual void stop() {}

    // Returns OK iff more data was available,
    // an error or ERROR_END_OF_STREAM if not.
    virtual status_t feedMoreTSData() = 0;

    virtual sp<AMessage> getFormat(bool audio);

    virtual status_t dequeueAccessUnit(
            bool audio, sp<ABuffer> *accessUnit) = 0;

    virtual status_t getDuration(int64_t *durationUs) {
        return INVALID_OPERATION;
    }
	virtual int	getwifidisplay_info(int *info){return 0;};
	
	virtual int Wifidisplay_get_TimeInfo(int64_t *start_time,int64_t *audio_start_time){return 0;};
    virtual status_t seekTo(int64_t seekTimeUs) {
        return INVALID_OPERATION;
    }
	
	virtual bool isSeekable() {
        return false;
    }
	
    virtual uint32_t flags() const = 0;

    virtual void reset() {
        return;
    }
protected:
    virtual ~Source() {}

    virtual sp<MetaData> getFormatMeta(bool audio) { return NULL; }

private:
    DISALLOW_EVIL_CONSTRUCTORS(Source);
};

}  // namespace android

#endif  // NUPLAYER_SOURCE_H_


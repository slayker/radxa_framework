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
#define LOG_TAG "NuPlayerRenderer"
#include <utils/Log.h>

#include "NuPlayerRenderer.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

// static
const int64_t NuPlayer::Renderer::kMinPositionUpdateDelayUs = 100000ll;
extern FILE *omx_rs_txt;
int video_num;
int audio_num;
int video_num1;
int audio_num1;
FILE* display_yuv;
int64_t start_fill_timeus ;
int64_t last_fill_timeus ;
int post_num;
FILE* yuv_data_render;
int yuv_sign_render;
NuPlayer::Renderer::Renderer(
        const sp<MediaPlayerBase::AudioSink> &sink,
        const sp<AMessage> &notify)
    : mAudioSink(sink),
      mNotify(notify),
      mNumFramesWritten(0),
      mDrainAudioQueuePending(false),
      mDrainVideoQueuePending(false),
      mAudioQueueGeneration(0),
      mVideoQueueGeneration(0),
      mAnchorTimeMediaUs(-1),
      mAnchorTimeRealUs(-1),
      mFlushingAudio(false),
      mFlushingVideo(false),
      mHasAudio(false),
      mHasVideo(false),
      mSyncQueues(false),
      mPaused(false),
      mVideoRenderingStarted(false),
      mLastPositionUpdateUs(-1ll),
      mVideoLateByUs(0ll) ,
      wifidisplay_flag(0){
      audio_latency_time = 0;
	video_num = audio_num = 0;
	video_num1 = audio_num1 = 0;
	display_yuv = NULL;
	start_fill_timeus = 0;
	post_num = 0;
	yuv_data_render = NULL;
 yuv_sign_render = 0;
	sys_start_time = 0;
	audio_start_timeUs = 0;
	last_adujst_time = 0;
	last_timeUs = 0;    
}

NuPlayer::Renderer::~Renderer() {
}

void NuPlayer::Renderer::queueBuffer(
        bool audio,
        const sp<ABuffer> &buffer,
        const sp<AMessage> &notifyConsumed) {
    sp<AMessage> msg = new AMessage(kWhatQueueBuffer, id());
    msg->setInt32("audio", static_cast<int32_t>(audio));
    msg->setBuffer("buffer", buffer);
    msg->setMessage("notifyConsumed", notifyConsumed);
    msg->post();
}

void NuPlayer::Renderer::queueEOS(bool audio, status_t finalResult) {
    CHECK_NE(finalResult, (status_t)OK);

    sp<AMessage> msg = new AMessage(kWhatQueueEOS, id());
    msg->setInt32("audio", static_cast<int32_t>(audio));
    msg->setInt32("finalResult", finalResult);
    msg->post();
}

void NuPlayer::Renderer::flush(bool audio) {
    {
        Mutex::Autolock autoLock(mFlushLock);
        if (audio) {
            if(!mFlushingAudio)
            mFlushingAudio = true;
            else
                return;
        } else {
            if(!mFlushingVideo)
            mFlushingVideo = true;
            else
                return;
        }
    }

    sp<AMessage> msg = new AMessage(kWhatFlush, id());
    msg->setInt32("audio", static_cast<int32_t>(audio));
    msg->post();
}

void NuPlayer::Renderer::signalTimeDiscontinuity() {
    if(!mAudioQueue.empty()){
        mAudioQueue.clear();
    }
    if(!mVideoQueue.empty()){
        mVideoQueue.clear();
    }
    mAnchorTimeMediaUs = -1;
    mAnchorTimeRealUs = -1;
    mSyncQueues = mHasAudio && mHasVideo;
}

void NuPlayer::Renderer::pause() {
    (new AMessage(kWhatPause, id()))->post();
}

void NuPlayer::Renderer::resume() {
    (new AMessage(kWhatResume, id()))->post();
}

void NuPlayer::Renderer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatDrainAudioQueue:
        {
            int32_t generation;
            CHECK(msg->findInt32("generation", &generation));
            if (generation != mAudioQueueGeneration) {
                break;
            }

            mDrainAudioQueuePending = false;

            if (onDrainAudioQueue()) {
                uint32_t numFramesPlayed;
                CHECK_EQ(mAudioSink->getPosition(&numFramesPlayed),
                         (status_t)OK);

                uint32_t numFramesPendingPlayout =
                    mNumFramesWritten - numFramesPlayed;

                // This is how long the audio sink will have data to
                // play back.
                int64_t delayUs =
                    mAudioSink->msecsPerFrame()
                        * numFramesPendingPlayout * 1000ll;

                // Let's give it more data after about half that time
                // has elapsed.
                if(wifidisplay_flag)
                postDrainAudioQueue(5000ll);//delayUs / 2);//);//delayUs / 2);
                else
					 postDrainAudioQueue(delayUs / 2);
            }
            break;
        }

        case kWhatDrainVideoQueue:
        {
            int32_t generation;
            CHECK(msg->findInt32("generation", &generation));
            if (generation != mVideoQueueGeneration) {
                break;
            }

            mDrainVideoQueuePending = false;

            onDrainVideoQueue();

            postDrainVideoQueue();
            break;
        }

        case kWhatQueueBuffer:
        {
            onQueueBuffer(msg);
            break;
        }

        case kWhatQueueEOS:
        {
            onQueueEOS(msg);
            break;
        }

        case kWhatFlush:
        {
            onFlush(msg);
            break;
        }

        case kWhatAudioSinkChanged:
        {
            onAudioSinkChanged();
            break;
        }

        case kWhatPause:
        {
            onPause();
            break;
        }

        case kWhatResume:
        {
            onResume();
            break;
        }

        default:
            TRESPASS();
            break;
    }
}

void NuPlayer::Renderer::postDrainAudioQueue(int64_t delayUs) {
    if (mDrainAudioQueuePending || mSyncQueues || mPaused) {
        return;
    }

    if (mAudioQueue.empty()) {
        return;
    }

    mDrainAudioQueuePending = true;
    sp<AMessage> msg = new AMessage(kWhatDrainAudioQueue, id());
    msg->setInt32("generation", mAudioQueueGeneration);
    msg->post(delayUs);
}

void NuPlayer::Renderer::signalAudioSinkChanged() {
    (new AMessage(kWhatAudioSinkChanged, id()))->post();
}
int	NuPlayer::Renderer::Wifidisplay_set_TimeInfo(int64_t start_time,int64_t audio_start_time)
{
	if(start_time != 0 && audio_start_time != 0)
	{
		sys_start_time = start_time;
		audio_start_timeUs = audio_start_time;
		return 0;
	}
	return 1;
	
}

bool NuPlayer::Renderer::onDrainAudioQueue() {
    uint32_t numFramesPlayed;
    if (mAudioSink->getPosition(&numFramesPlayed) != OK) {
        return false;
    }

    ssize_t numFramesAvailableToWrite =
        mAudioSink->frameCount() - (mNumFramesWritten - numFramesPlayed);

#if 0
    if (numFramesAvailableToWrite == mAudioSink->frameCount()) {
        ALOGI("audio sink underrun");
    } else {
        ALOGV("audio queue has %d frames left to play",
             mAudioSink->frameCount() - numFramesAvailableToWrite);
    }
#endif

    size_t numBytesAvailableToWrite =
        numFramesAvailableToWrite * mAudioSink->frameSize();

    while (numBytesAvailableToWrite > 0 && !mAudioQueue.empty()) {
        QueueEntry *entry = &*mAudioQueue.begin();

        if (entry->mBuffer == NULL) {
            // EOS

            notifyEOS(true /* audio */, entry->mFinalResult);

            mAudioQueue.erase(mAudioQueue.begin());
            entry = NULL;
            return false;
        }

        if (entry->mOffset == 0) {
            int64_t mediaTimeUs;
			int64_t sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
            CHECK(entry->mBuffer->meta()->findInt64("timeUs", &mediaTimeUs));
			uint32_t numFramesPlayed;
            CHECK_EQ(mAudioSink->getPosition(&numFramesPlayed), (status_t)OK);

            uint32_t numFramesPendingPlayout =
                mNumFramesWritten - numFramesPlayed;
			if(wifidisplay_flag)
			{
				int set_flag =0;
				if(sys_start_time == 0)
				{
					sys_start_time =sys_time;		
					ALOGD("sys_start_time == 0 %lld",sys_start_time);
				}
				if(audio_start_timeUs == 0)
				{
					audio_start_timeUs = mediaTimeUs;
					ALOGD("audio_start_timeUs == 0 %lld",audio_start_timeUs);
				}
				if(last_adujst_time == 0)
					last_adujst_time = sys_time;
				int64_t pending_time =  numFramesPendingPlayout    * mAudioSink->msecsPerFrame() * 1000ll;
				if(sys_start_time + (mediaTimeUs - audio_start_timeUs)  - pending_time < sys_time - 100000ll )
				{	
					if(last_timeUs < mediaTimeUs )//loop tntil the real mediaTimeUs catch up with the old setted one  , if there is no data,the old setted is also faster than the real mediaTimeUs.so it's okay
					{
						if(sys_start_time + (mediaTimeUs - audio_start_timeUs) - pending_time< sys_time - 300000ll || (sys_time - last_adujst_time > 20000000ll && 
						sys_start_time + (mediaTimeUs - audio_start_timeUs) - pending_time< sys_time - 100000ll  ))//recalculate the mediaTimeUs.
						{
							int retrtptxt;
							if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
							{
								if(omx_rs_txt == NULL)
						  			omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
								if(omx_rs_txt != NULL)
					
								{
									if(sys_time - last_adujst_time > 20000000ll && 
							sys_start_time + (mediaTimeUs - audio_start_timeUs) - pending_time < sys_time - 100000ll    )
									fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainAudioQueue adjust start   %lld  %lld sys %lld %lld mediaTimeUs %lld last %lld delta %lld  %lld %lld\n"
									,sys_start_time,audio_start_timeUs,last_adujst_time,sys_time,mediaTimeUs,last_timeUs,sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs) + pending_time,
									sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs),sys_time-last_adujst_time);
									else
									fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainAudioQueue before delay 300ms start   %lld  %lld sys %lld %lld mediaTimeUs %lld last %lld delta %lld  %lld %lld\n"
									,sys_start_time,audio_start_timeUs,last_adujst_time,sys_time,mediaTimeUs,last_timeUs,sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs) + pending_time,
									sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs),sys_time-last_adujst_time);
									fflush(omx_rs_txt); 		
													
								}
							}
							mediaTimeUs +=((sys_time - sys_start_time - (mediaTimeUs - audio_start_timeUs) ) / 11) *11;					
							set_flag = 1;
							last_adujst_time = sys_time;
						}
						
						else
						{
							int retrtptxt;
							if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
							{
								if(omx_rs_txt == NULL)
						  			omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
								if(omx_rs_txt != NULL)
					
								{
								
									fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainAudioQueue before dec delay 100-300 ms start   %lld  %lld sys %lld %lld mediaTimeUs %lld last %lld delta %lld  %lld %lld\n"
									,sys_start_time,audio_start_timeUs,last_adujst_time,sys_time,mediaTimeUs,last_timeUs,sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs) + pending_time,
									sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs),sys_time-last_adujst_time);
									fflush(omx_rs_txt); 		
													
								}
							}
						}
						
					
					}
					else
					{
						int retrtptxt;
						if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
						{
							if(omx_rs_txt == NULL)
					  			omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
							if(omx_rs_txt != NULL)
					
							{
							
								fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainAudioQueue before drop start %lld    %lld sys %lld %lld mediaTimeUs %lld last %lld delta %lld   %lld %lld\n"
									,sys_start_time,audio_start_timeUs,last_adujst_time,sys_time,mediaTimeUs,last_timeUs,sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs) + pending_time,
									sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs),sys_time-last_adujst_time);
								fflush(omx_rs_txt);			
							}	
						}
						{
							entry->mNotifyConsumed->post();
							mAudioQueue.erase(mAudioQueue.begin());
							entry = NULL;
						}
						continue;
					}
				}
				else
				{
					int retrtptxt;
					if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
					{
						if(omx_rs_txt == NULL)
				  			omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
						if(omx_rs_txt != NULL)
				
						{
						
							fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainAudioQueue before less than 100ms start   %lld  %lld sys %lld %lld mediaTimeUs %lld last %lld delta %lld   %lld %lld\n"
								,sys_start_time,audio_start_timeUs,last_adujst_time,sys_time,mediaTimeUs,last_timeUs,sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs) + pending_time,
								sys_time-sys_start_time-(mediaTimeUs - audio_start_timeUs),sys_time-last_adujst_time);
							fflush(omx_rs_txt);			
											
						}	
					}
				}  
				last_timeUs = mediaTimeUs;
	        	if(sys_time - last_adujst_time > 20000000ll)
					last_adujst_time = sys_time;
				if(set_flag)
				memset(entry->mBuffer->data(),0,entry->mBuffer->size());
			}
            ALOGV("rendering audio at media time %.2f secs", mediaTimeUs / 1E6);

            mAnchorTimeMediaUs = mediaTimeUs;

            
            int64_t realTimeOffsetUs =
                (mAudioSink->latency() / 2  /* XXX */
                    + numFramesPendingPlayout
                        * mAudioSink->msecsPerFrame()) * 1000ll;

            // ALOGI("realTimeOffsetUs = %lld us", realTimeOffsetUs);
			
            mAnchorTimeRealUs =
                ALooper::GetNowUs() + realTimeOffsetUs;
			if(wifidisplay_flag == 1)
				audio_latency_time = realTimeOffsetUs;
			
        }

        size_t copy = entry->mBuffer->size() - entry->mOffset;
        if (copy > numBytesAvailableToWrite) {
            copy = numBytesAvailableToWrite;
        }

        CHECK_EQ(mAudioSink->write(
                    entry->mBuffer->data() + entry->mOffset, copy),
                 (ssize_t)copy);

        entry->mOffset += copy;
        if (entry->mOffset == entry->mBuffer->size()) {
            entry->mNotifyConsumed->post();
            mAudioQueue.erase(mAudioQueue.begin());

            entry = NULL;
        }

        numBytesAvailableToWrite -= copy;
        size_t copiedFrames = copy / mAudioSink->frameSize();
        mNumFramesWritten += copiedFrames;
    }

    notifyPosition();

    return !mAudioQueue.empty();
}

void NuPlayer::Renderer::postDrainVideoQueue() {
    if (mDrainVideoQueuePending || mSyncQueues || mPaused) {
        return;
    }

    if (mVideoQueue.empty()) {
        return;
    }

    QueueEntry &entry = *mVideoQueue.begin();

    sp<AMessage> msg = new AMessage(kWhatDrainVideoQueue, id());
    msg->setInt32("generation", mVideoQueueGeneration);

    int64_t delayUs;

    if (entry.mBuffer == NULL) {
        // EOS doesn't carry a timestamp.
        delayUs = 0;
    } else {
        int64_t mediaTimeUs;
        CHECK(entry.mBuffer->meta()->findInt64("timeUs", &mediaTimeUs));

        if (mAnchorTimeMediaUs < 0) {
            delayUs = 0;

            if (!mHasAudio) {
                mAnchorTimeMediaUs = mediaTimeUs;
                mAnchorTimeRealUs = ALooper::GetNowUs();
            }
        } else {
            int64_t realTimeUs =
                (mediaTimeUs - mAnchorTimeMediaUs) + mAnchorTimeRealUs;

            delayUs = realTimeUs - ALooper::GetNowUs();
        }
    }
	if(wifidisplay_flag)
	{
		if(delayUs  > 80000){
		//delayUs = 35000;
	    }
			onDrainVideoQueue();
	    msg->post(1000);//delayUs);
	}
	else
	{
		if(delayUs > 80000){
			delayUs = 35000;
		}
		msg->post(delayUs);
		
	}

    mDrainVideoQueuePending = true;
}

void NuPlayer::Renderer::onDrainVideoQueue() {
    if (mVideoQueue.empty()) {
        return;
    }

    QueueEntry *entry = &*mVideoQueue.begin();

    if (entry->mBuffer == NULL) {
        // EOS

        notifyEOS(false /* audio */, entry->mFinalResult);

        mVideoQueue.erase(mVideoQueue.begin());
        entry = NULL;

        mVideoLateByUs = 0ll;

        notifyPosition();
        return;
    }

    int64_t mediaTimeUs;
    CHECK(entry->mBuffer->meta()->findInt64("timeUs", &mediaTimeUs));

    int64_t realTimeUs = mediaTimeUs - mAnchorTimeMediaUs + mAnchorTimeRealUs;
    mVideoLateByUs = ALooper::GetNowUs() - realTimeUs;

    bool tooLate = (mVideoLateByUs > 40000);
    if(mVideoLateByUs + (audio_latency_time - 120000) < -50000&& wifidisplay_flag == 1){
	    ALOGV("video is early");
	    return; 
    }
	if(mVideoLateByUs  < -50000 && wifidisplay_flag == 0){
	    ALOGV("video is early");
		return;
    }
	if(wifidisplay_flag==1)
	{
	  int retrtptxt;
	  int64_t sys_time;
	  static int64_t last_time_us = 0;
	  static int64_t last_sys_time = 0;
	  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		

      if(sys_start_time == 0)
      {
      	sys_start_time =sys_time;		
      	ALOGD("sys_start_time == 0 %lld",sys_start_time);
      }
      if(audio_start_timeUs == 0)
      {
      	audio_start_timeUs = mediaTimeUs;
      	ALOGD("audio_start_timeUs == 0 %lld",audio_start_timeUs);
      }
	 
	  if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)//test_file!=NULL)
	  {
		  
		  if(omx_rs_txt == NULL)
			  omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
		  if(omx_rs_txt != NULL)
		  {
		  	
		fprintf(omx_rs_txt,"NuPlayer::Renderer::onDrainVideoQueue Video sys1 time %lld start %lld  %lld timeus %lld delta %lld  %lld %lld mAnchorTimeMediaUs %lld mAnchorTimeRealUs %lld mVideoLateByUs %lld audio_pending_time %lld mVideoQueue size %d  tooLate %d\n",
			sys_time,sys_start_time,audio_start_timeUs,mediaTimeUs,sys_time - sys_start_time - (mediaTimeUs - audio_start_timeUs),
			sys_time - last_sys_time,mediaTimeUs -  last_time_us,mAnchorTimeMediaUs,mAnchorTimeRealUs,mVideoLateByUs,
			audio_latency_time,mVideoQueue.size(),tooLate);
		fflush(omx_rs_txt);
		  }
	  }
	  static int video_render_time = 0;

	  video_render_time++;
	  last_time_us = mediaTimeUs;
	  last_sys_time = sys_time;
	}
	
    if (tooLate) {
        ALOGV("video late by %lld us (%.2f secs)",
             mVideoLateByUs, mVideoLateByUs / 1E6);
    } else {
        ALOGV("rendering video at media time %.2f secs", mediaTimeUs / 1E6);
    }
	if(wifidisplay_flag)
		entry->mNotifyConsumed->setInt32("render", 1);

	else
    entry->mNotifyConsumed->setInt32("render", !tooLate);
    entry->mNotifyConsumed->post();
    mVideoQueue.erase(mVideoQueue.begin());
    entry = NULL;

    if (!mVideoRenderingStarted) {
        mVideoRenderingStarted = true;
        notifyVideoRenderingStart();
    }

    notifyPosition();
}

void NuPlayer::Renderer::notifyVideoRenderingStart() {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatVideoRenderingStart);
    notify->post();
}

void NuPlayer::Renderer::notifyEOS(bool audio, status_t finalResult) {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatEOS);
    notify->setInt32("audio", static_cast<int32_t>(audio));
    notify->setInt32("finalResult", finalResult);
    notify->post();
}
void NuPlayer::Renderer::rendervideobuffer(
        bool audio,
        const sp<ABuffer> &buffer,
        const sp<AMessage> &notifyConsumed) {

	if (audio) {
        mHasAudio = true;
    } else {
        mHasVideo = true;
    }
	{
		bool flushing = false;

	    {
	        Mutex::Autolock autoLock(mFlushLock);
	        if (audio) {
	            flushing = mFlushingAudio;
	        } else {
	            flushing = mFlushingVideo;
	        }
	    }

	    if (flushing) {
		notifyConsumed->post();

        return;
			}
	}
	


    QueueEntry entry;
    entry.mBuffer = buffer;
    entry.mNotifyConsumed = notifyConsumed;
    entry.mOffset = 0;
    entry.mFinalResult = OK;


	
	if (audio) {
		   mAudioQueue.push_back(entry);
		   ALOGV("onQueueBuffer postDrainAudioQueue");
		   postDrainAudioQueue();
	} else {
		   mVideoQueue.push_back(entry);
		   postDrainVideoQueue();
		//   onDrainVideoQueue();
	
		   
	}
	if (!mSyncQueues || mAudioQueue.empty() || mVideoQueue.empty()) {
        return;
    }

    sp<ABuffer> firstAudioBuffer = (*mAudioQueue.begin()).mBuffer;
    sp<ABuffer> firstVideoBuffer = (*mVideoQueue.begin()).mBuffer;

    if (firstAudioBuffer == NULL || firstVideoBuffer == NULL) {
        // EOS signalled on either queue.
        syncQueuesDone();
        return;
    }

    int64_t firstAudioTimeUs;
    int64_t firstVideoTimeUs;
    CHECK(firstAudioBuffer->meta()
            ->findInt64("timeUs", &firstAudioTimeUs));
    CHECK(firstVideoBuffer->meta()
            ->findInt64("timeUs", &firstVideoTimeUs));

    int64_t diff = firstVideoTimeUs - firstAudioTimeUs;

    ALOGV("queueDiff = %.2f secs", diff / 1E6);

    if (diff > 100000ll) {
        // Audio data starts More than 0.1 secs before video.
        // Drop some audio.

        (*mAudioQueue.begin()).mNotifyConsumed->post();
        mAudioQueue.erase(mAudioQueue.begin());
        return;
    }

    syncQueuesDone();
}

void NuPlayer::Renderer::onQueueBuffer(const sp<AMessage> &msg) {
    int32_t audio;
    CHECK(msg->findInt32("audio", &audio));

    if (audio) {
        mHasAudio = true;
    } else {
        mHasVideo = true;
    }

    if (dropBufferWhileFlushing(audio, msg)) {
        return;
    }

    sp<ABuffer> buffer;
    CHECK(msg->findBuffer("buffer", &buffer));

    sp<AMessage> notifyConsumed;
    CHECK(msg->findMessage("notifyConsumed", &notifyConsumed));

    QueueEntry entry;
    entry.mBuffer = buffer;
    entry.mNotifyConsumed = notifyConsumed;
    entry.mOffset = 0;
    entry.mFinalResult = OK;

    if (audio) {
        mAudioQueue.push_back(entry);
        postDrainAudioQueue();
    } else {
        mVideoQueue.push_back(entry);
        postDrainVideoQueue();
    }

    if (!mSyncQueues || mAudioQueue.empty() || mVideoQueue.empty()) {
        return;
    }

    sp<ABuffer> firstAudioBuffer = (*mAudioQueue.begin()).mBuffer;
    sp<ABuffer> firstVideoBuffer = (*mVideoQueue.begin()).mBuffer;

    if (firstAudioBuffer == NULL || firstVideoBuffer == NULL) {
        // EOS signalled on either queue.
        syncQueuesDone();
        return;
    }

    int64_t firstAudioTimeUs;
    int64_t firstVideoTimeUs;
    CHECK(firstAudioBuffer->meta()
            ->findInt64("timeUs", &firstAudioTimeUs));
    CHECK(firstVideoBuffer->meta()
            ->findInt64("timeUs", &firstVideoTimeUs));

    int64_t diff = firstVideoTimeUs - firstAudioTimeUs;

    ALOGV("queueDiff = %.2f secs", diff / 1E6);

    if (diff > 100000ll) {
        // Audio data starts More than 0.1 secs before video.
        // Drop some audio.

        (*mAudioQueue.begin()).mNotifyConsumed->post();
        mAudioQueue.erase(mAudioQueue.begin());
        return;
    }

    syncQueuesDone();
}

void NuPlayer::Renderer::syncQueuesDone() {
    if (!mSyncQueues) {
        return;
    }

    mSyncQueues = false;

    if (!mAudioQueue.empty()) {
        postDrainAudioQueue();
    }

    if (!mVideoQueue.empty()) {
        postDrainVideoQueue();
    }
}

void NuPlayer::Renderer::onQueueEOS(const sp<AMessage> &msg) {
    int32_t audio;
    CHECK(msg->findInt32("audio", &audio));

    if (dropBufferWhileFlushing(audio, msg)) {
        return;
    }

    int32_t finalResult;
    CHECK(msg->findInt32("finalResult", &finalResult));

    QueueEntry entry;
    entry.mOffset = 0;
    entry.mFinalResult = finalResult;

    if (audio) {
        mAudioQueue.push_back(entry);
        postDrainAudioQueue();
    } else {
        mVideoQueue.push_back(entry);
        postDrainVideoQueue();
    }
}

void NuPlayer::Renderer::onFlush(const sp<AMessage> &msg) {
    int32_t audio;
    CHECK(msg->findInt32("audio", &audio));

    // If we're currently syncing the queues, i.e. dropping audio while
    // aligning the first audio/video buffer times and only one of the
    // two queues has data, we may starve that queue by not requesting
    // more buffers from the decoder. If the other source then encounters
    // a discontinuity that leads to flushing, we'll never find the
    // corresponding discontinuity on the other queue.
    // Therefore we'll stop syncing the queues if at least one of them
    // is flushed.
    syncQueuesDone();

    if (audio) {
        flushQueue(&mAudioQueue);

        Mutex::Autolock autoLock(mFlushLock);
        mFlushingAudio = false;

        mDrainAudioQueuePending = false;
        ++mAudioQueueGeneration;
    } else {
        flushQueue(&mVideoQueue);

        Mutex::Autolock autoLock(mFlushLock);
        mFlushingVideo = false;

        mDrainVideoQueuePending = false;
        ++mVideoQueueGeneration;
    }

    notifyFlushComplete(audio);
}

void NuPlayer::Renderer::flushQueue(List<QueueEntry> *queue) {
    while (!queue->empty()) {
        QueueEntry *entry = &*queue->begin();

        if (entry->mBuffer != NULL) {
            entry->mNotifyConsumed->post();
        }

        queue->erase(queue->begin());
        entry = NULL;
    }
}

void NuPlayer::Renderer::notifyFlushComplete(bool audio) {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatFlushComplete);
    notify->setInt32("audio", static_cast<int32_t>(audio));
    notify->post();
}

bool NuPlayer::Renderer::dropBufferWhileFlushing(
        bool audio, const sp<AMessage> &msg) {
    bool flushing = false;

    {
        Mutex::Autolock autoLock(mFlushLock);
        if (audio) {
            flushing = mFlushingAudio;
        } else {
            flushing = mFlushingVideo;
        }
    }

    if (!flushing) {
        return false;
    }

    sp<AMessage> notifyConsumed;
    if (msg->findMessage("notifyConsumed", &notifyConsumed)) {
        notifyConsumed->post();
    }

    return true;
}

void NuPlayer::Renderer::onAudioSinkChanged() {
    CHECK(!mDrainAudioQueuePending);
    mNumFramesWritten = 0;
    uint32_t written;
    if (mAudioSink->getFramesWritten(&written) == OK) {
        mNumFramesWritten = written;
    }
}

void NuPlayer::Renderer::notifyPosition() {
    if (mAnchorTimeRealUs < 0 || mAnchorTimeMediaUs < 0) {
        return;
    }

    int64_t nowUs = ALooper::GetNowUs();

    if (mLastPositionUpdateUs >= 0
            && nowUs < mLastPositionUpdateUs + kMinPositionUpdateDelayUs) {
        return;
    }
    mLastPositionUpdateUs = nowUs;

    int64_t positionUs = (nowUs - mAnchorTimeRealUs) + mAnchorTimeMediaUs;

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatPosition);
    notify->setInt64("positionUs", positionUs);
    notify->setInt64("videoLateByUs", mVideoLateByUs);
    notify->post();
}

void NuPlayer::Renderer::onPause() {
    if(mPaused)
    {
        return;
    }
    mDrainAudioQueuePending = false;
    ++mAudioQueueGeneration;

    mDrainVideoQueuePending = false;
    ++mVideoQueueGeneration;

    if (mHasAudio) {
        mAudioSink->pause();
    }

    ALOGV("now paused audio queue has %d entries, video has %d entries",
          mAudioQueue.size(), mVideoQueue.size());

    mPaused = true;
}

void NuPlayer::Renderer::onResume() {
    if (!mPaused) {
        return;
    }

    if (mHasAudio) {
        mAudioSink->start();
    }

    mPaused = false;

    if (!mAudioQueue.empty()) {
        postDrainAudioQueue();
    }

    if (!mVideoQueue.empty()) {
        postDrainVideoQueue();
    }
}

}  // namespace android


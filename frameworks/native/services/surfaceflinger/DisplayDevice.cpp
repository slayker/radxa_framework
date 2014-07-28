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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cutils/properties.h>

#include <utils/RefBase.h>
#include <utils/Log.h>

#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>

#include <gui/SurfaceTextureClient.h>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <hardware/gralloc.h>

#include "DisplayHardware/FramebufferSurface.h"
#include "DisplayHardware/HWComposer.h"

#include "clz.h"
#include "DisplayDevice.h"
#include "GLExtensions.h"
#include "SurfaceFlinger.h"
#include "LayerBase.h"

#define FAKEWIDTH	1280
#define FAKEHEIGHT	720

// ----------------------------------------------------------------------------
using namespace android;
// ----------------------------------------------------------------------------

static __attribute__((noinline))
void checkGLErrors()
{
    do {
        // there could be more than one error flag
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        ALOGE("GL error 0x%04x", int(error));
    } while(true);
}

// ----------------------------------------------------------------------------

/*
 * Initialize the display to the specified values.
 *
 */

DisplayDevice::DisplayDevice(
        const sp<SurfaceFlinger>& flinger,
        DisplayType type,
        bool isSecure,
        const wp<IBinder>& displayToken,
        const sp<ANativeWindow>& nativeWindow,
        const sp<FramebufferSurface>& framebufferSurface,
        EGLConfig config,
        int hardwareOrientation)
    : mFlinger(flinger),
      mType(type), mHwcDisplayId(-1),
      mNativeWindow(nativeWindow),
      mFramebufferSurface(framebufferSurface),
      mDisplay(EGL_NO_DISPLAY),
      mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT),
      mDisplayWidth(), mDisplayHeight(), mFormat(),
      mFlags(),
      mPageFlipCount(),
      mIsSecure(isSecure),
      mSecureLayerVisible(false),
      mScreenAcquired(false),
      mLayerStack(0),
      mOrientation(),
      mHardwareOrientation(hardwareOrientation)
{
    init(config);
}

DisplayDevice::~DisplayDevice() {
    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }
}

bool DisplayDevice::isValid() const {
    return mFlinger != NULL;
}

int DisplayDevice::getWidth() const {
	if(mScaled == true)
		return FAKEWIDTH;
	else
    	return mDisplayWidth;
}

int DisplayDevice::getHeight() const {
	if(mScaled == true)
		return FAKEHEIGHT;
	else
  		return mDisplayHeight;
}

PixelFormat DisplayDevice::getFormat() const {
    return mFormat;
}

EGLSurface DisplayDevice::getEGLSurface() const {
    return mSurface;
}

void DisplayDevice::init(EGLConfig config)
{
    ANativeWindow* const window = mNativeWindow.get();

    int format;
    window->query(window, NATIVE_WINDOW_FORMAT, &format);
	char property[PROPERTY_VALUE_MAX];
	if (property_get("video.use.overlay", property, "0") && atoi(property) > 0)
		mScaled = true;
	else
		mScaled = false;
    /*
     * Create our display's surface
     */

    EGLSurface surface;
    EGLint w, h;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    surface = eglCreateWindowSurface(display, config, window, NULL);
    eglQuerySurface(display, surface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &mDisplayHeight);

    mDisplay = display;
    mSurface = surface;
    mFormat  = format;
    mPageFlipCount = 0;
    mViewport.makeInvalid();
    mFrame.makeInvalid();

    mViewport.set(bounds());
    mFrame.set(bounds());

    // external displays are always considered enabled
    mScreenAcquired = (mType >= DisplayDevice::NUM_DISPLAY_TYPES);

    // get an h/w composer ID
    mHwcDisplayId = mFlinger->allocateHwcDisplayId(mType);

    // Name the display.  The name will be replaced shortly if the display
    // was created with createDisplay().
    switch (mType) {
        case DISPLAY_PRIMARY:
            mDisplayName = "Built-in Screen";
            break;
        case DISPLAY_EXTERNAL:
            mDisplayName = "HDMI Screen";
            break;
        default:
            mDisplayName = "Virtual Screen";    // e.g. Overlay #n
            break;
    }

    // initialize the display orientation transform.
    setProjection(DisplayState::eOrientationDefault, mViewport, mFrame, mFlinger->mUseLcdcComposer);
}

void DisplayDevice::setDisplayName(const String8& displayName) {
    if (!displayName.isEmpty()) {
        // never override the name with an empty name
        mDisplayName = displayName;
    }
}

uint32_t DisplayDevice::getPageFlipCount() const {
    return mPageFlipCount;
}

status_t DisplayDevice::compositionComplete() const {
    if (mFramebufferSurface == NULL) {
        return NO_ERROR;
    }
    return mFramebufferSurface->compositionComplete();
}

void DisplayDevice::flip(const Region& dirty) const
{
    checkGLErrors();

    EGLDisplay dpy = mDisplay;
    EGLSurface surface = mSurface;

#ifdef EGL_ANDROID_swap_rectangle
    if (mFlags & SWAP_RECTANGLE) {
        const Region newDirty(dirty.intersect(bounds()));
        const Rect b(newDirty.getBounds());
        eglSetSwapRectangleANDROID(dpy, surface,
                b.left, b.top, b.width(), b.height());
    }
#endif

    mPageFlipCount++;
}

void DisplayDevice::swapBuffers(HWComposer& hwc) const {
    EGLBoolean success = EGL_TRUE;
    if (hwc.initCheck() != NO_ERROR) {
        // no HWC, we call eglSwapBuffers()
        success = eglSwapBuffers(mDisplay, mSurface);
    } else {
        // We have a valid HWC, but not all displays can use it, in particular
        // the virtual displays are on their own.
        // TODO: HWC 1.2 will allow virtual displays
        if (mType >= DisplayDevice::DISPLAY_VIRTUAL) {
            // always call eglSwapBuffers() for virtual displays
            success = eglSwapBuffers(mDisplay, mSurface);
        } else if (hwc.supportsFramebufferTarget()) {
            // as of hwc 1.1 we always call eglSwapBuffers if we have some
            // GLES layers
            if (hwc.hasGlesComposition(mType)) {
                success = eglSwapBuffers(mDisplay, mSurface);
            }
        } else {
            // HWC doesn't have the framebuffer target, we don't call
            // eglSwapBuffers(), since this is handled by HWComposer::commit().
        }
    }

    if (!success) {
        EGLint error = eglGetError();
        if (error == EGL_CONTEXT_LOST ||
                mType == DisplayDevice::DISPLAY_PRIMARY) {
            LOG_ALWAYS_FATAL("eglSwapBuffers(%p, %p) failed with 0x%08x",
                    mDisplay, mSurface, error);
        }
    }
}

void DisplayDevice::onSwapBuffersCompleted(HWComposer& hwc) const {
    if (hwc.initCheck() == NO_ERROR) {
        if (hwc.supportsFramebufferTarget()) {
            int fd = hwc.getAndResetReleaseFenceFd(mType);
            mFramebufferSurface->setReleaseFenceFd(fd);
        }
    }
}

#ifdef TARGET_RK30

//void DisplayDevice::RenderVPUBuffToLayerBuff(  hwc_layer_1_t* Layer) const
void DisplayDevice::RenderVPUBuffToLayerBuff(   struct private_handle_t* srcandle) const

{


	struct tVPU_FRAME *pFrame  = NULL;
   	struct rga_req  Rga_Request;
  	struct timeval tpend1, tpend2 ;
	long usec1 = 0;



  	//if(LastBuffAddr == srcandle->base)
  		//return;

#ifdef TARGET_BOARD_PLATFORM_RK30XXB
	pFrame = (tVPU_FRAME *)srcandle->iBase;
#else
	pFrame = (tVPU_FRAME *)srcandle->base;
#endif
	memset(&Rga_Request,0x0,sizeof(Rga_Request));

	ALOGV("videopFrame addr=%x,FrameWidth=%d,FrameHeight=%d",pFrame->FrameBusAddr[0],pFrame->FrameWidth,pFrame->FrameHeight);



    if(fd_rga < 0)
		fd_rga = open("/dev/rga",O_RDWR,0);
	if(fd_rga < 0)
	{
        ALOGE(" rga open err");
		return;
	}

    Rga_Request.src.yrgb_addr =  (int)pFrame->FrameBusAddr[0] + 0x60000000;

    Rga_Request.src.uv_addr  = Rga_Request.src.yrgb_addr + (( pFrame->FrameWidth + 15) & ~15) * ((pFrame->FrameHeight + 15) & ~15);
    Rga_Request.src.v_addr   =  Rga_Request.src.uv_addr;
    Rga_Request.src.vir_w =  ((pFrame->FrameWidth + 15) & ~15);
    Rga_Request.src.vir_h = ((pFrame->FrameHeight + 15) & ~15);
    Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;

  	Rga_Request.src.act_w = pFrame->FrameWidth;//Rga_Request.src.vir_w;
    Rga_Request.src.act_h = pFrame->FrameHeight;//Rga_Request.src.vir_h;
    Rga_Request.src.x_offset = 0;
    Rga_Request.src.y_offset = 0;

 	ALOGV("src info: yrgb_addr=%x, uv_addr=%x,v_addr=%x,"
         "vir_w=%d,vir_h=%d,format=%d,"
         "act_x_y_w_h [%d,%d,%d,%d] ",
			Rga_Request.src.yrgb_addr, Rga_Request.src.uv_addr ,Rga_Request.src.v_addr,
			Rga_Request.src.vir_w ,Rga_Request.src.vir_h ,Rga_Request.src.format ,
			Rga_Request.src.x_offset ,
			Rga_Request.src.y_offset,
			Rga_Request.src.act_w ,
			Rga_Request.src.act_h

        );



#ifdef TARGET_BOARD_PLATFORM_RK30XXB
 	Rga_Request.dst.yrgb_addr = srcandle->iBase; //dsthandle->base;//(int)(fixInfo.smem_start + dsthandle->offset);
   	Rga_Request.dst.vir_w =   (srcandle->iWidth + 31) & ~31;//((srcandle->iWidth*2 + (8-1)) & ~(8-1))/2 ;  /* 2:RK_FORMAT_RGB_565 ,8:????*///srcandle->width;
        Rga_Request.dst.vir_h =  srcandle->iHeight;
	Rga_Request.dst.act_w = srcandle->iWidth;//Rga_Request.dst.vir_w;
	Rga_Request.dst.act_h = srcandle->iHeight;//Rga_Request.dst.vir_h;

#else
	Rga_Request.dst.yrgb_addr = srcandle->base; //dsthandle->base;//(int)(fixInfo.smem_start + dsthandle->offset);
   	Rga_Request.dst.vir_w =   ((srcandle->width*2 + (8-1)) & ~(8-1))/2 ;  /* 2:RK_FORMAT_RGB_565 ,8:????*///srcandle->width;
    Rga_Request.dst.vir_h = srcandle->height;
	Rga_Request.dst.act_w = srcandle->width;//Rga_Request.dst.vir_w;
	Rga_Request.dst.act_h = srcandle->height;//Rga_Request.dst.vir_h;

#endif
    Rga_Request.dst.uv_addr  = 0;//Rga_Request.dst.yrgb_addr + (( srcandle->width + 15) & ~15) * ((srcandle->height + 15) & ~15);
    Rga_Request.dst.v_addr   = Rga_Request.dst.uv_addr;
    //Rga_Request.dst.format = RK_FORMAT_RGB_565;
    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.xmax = Rga_Request.dst.vir_w - 1;
    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax = Rga_Request.dst.vir_h - 1;
	Rga_Request.dst.x_offset = 0;
	Rga_Request.dst.y_offset = 0;

	Rga_Request.sina = 0;
	Rga_Request.cosa = 0x10000;

	char property[PROPERTY_VALUE_MAX];
	int gpuformat = HAL_PIXEL_FORMAT_RGB_565;
	if (property_get("sys.yuv.rgb.format", property, NULL) > 0) {
	    gpuformat = atoi(property);
	}
	if(gpuformat == HAL_PIXEL_FORMAT_RGBA_8888){
    	Rga_Request.dst.format = RK_FORMAT_RGBA_8888;//RK_FORMAT_RGB_565;
	}
    else if(gpuformat == HAL_PIXEL_FORMAT_RGBX_8888)
    {
    	Rga_Request.dst.format = RK_FORMAT_RGBX_8888;
    }
    else if(gpuformat == HAL_PIXEL_FORMAT_RGB_565)
    {
    	Rga_Request.dst.format = RK_FORMAT_RGB_565;
    }
 	ALOGV("dst info: yrgb_addr=%x, uv_addr=%x,v_addr=%x,"
         "vir_w=%d,vir_h=%d,format=%d,"
         "clip[%d,%d,%d,%d], "
         "act_x_y_w_h [%d,%d,%d,%d] ",

			Rga_Request.dst.yrgb_addr, Rga_Request.dst.uv_addr ,Rga_Request.dst.v_addr,
			Rga_Request.dst.vir_w ,Rga_Request.dst.vir_h ,Rga_Request.dst.format,
			Rga_Request.clip.xmin,
			Rga_Request.clip.xmax,
			Rga_Request.clip.ymin,
			Rga_Request.clip.ymax,
			Rga_Request.dst.x_offset ,
			Rga_Request.dst.y_offset,
			Rga_Request.dst.act_w ,
			Rga_Request.dst.act_h

        );

    if(Rga_Request.src.act_w != Rga_Request.dst.act_w
        || Rga_Request.src.act_h != Rga_Request.dst.act_h)
    {
	Rga_Request.scale_mode = 1;
	Rga_Request.rotate_mode = 1;
    }
    //Rga_Request.render_mode = pre_scaling_mode;
    Rga_Request.alpha_rop_flag |= (1 << 5);

   	Rga_Request.mmu_info.mmu_en    = 1;
   	Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;


	//gettimeofday(&tpend1,NULL);
	if(ioctl(fd_rga, RGA_BLIT_SYNC, &Rga_Request) != 0)
	{

		ALOGE("%s(%d):  RGA_BLIT_ASYNC Failed ", __FUNCTION__, __LINE__);
	 	ALOGE("src info: yrgb_addr=%x, uv_addr=%x,v_addr=%x,"
	         "vir_w=%d,vir_h=%d,format=%d,"
	         "act_x_y_w_h [%d,%d,%d,%d] ",
				Rga_Request.src.yrgb_addr, Rga_Request.src.uv_addr ,Rga_Request.src.v_addr,
				Rga_Request.src.vir_w ,Rga_Request.src.vir_h ,Rga_Request.src.format ,
				Rga_Request.src.x_offset ,
				Rga_Request.src.y_offset,
				Rga_Request.src.act_w ,
				Rga_Request.src.act_h

	        );

	 	ALOGE("dst info: yrgb_addr=%x, uv_addr=%x,v_addr=%x,"
	         "vir_w=%d,vir_h=%d,format=%d,"
	         "clip[%d,%d,%d,%d], "
	         "act_x_y_w_h [%d,%d,%d,%d] ",

				Rga_Request.dst.yrgb_addr, Rga_Request.dst.uv_addr ,Rga_Request.dst.v_addr,
				Rga_Request.dst.vir_w ,Rga_Request.dst.vir_h ,Rga_Request.dst.format,
				Rga_Request.clip.xmin,
				Rga_Request.clip.xmax,
				Rga_Request.clip.ymin,
				Rga_Request.clip.ymax,
				Rga_Request.dst.x_offset ,
				Rga_Request.dst.y_offset,
				Rga_Request.dst.act_w ,
				Rga_Request.dst.act_h

	        );


	}
	//else
	//{
		//LastBuffAddr = srcandle->base;
	//}


	//gettimeofday(&tpend2,NULL);
	//usec1 = 1000*(tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec- tpend1.tv_usec)/1000;
	//LOGD("yuv to rga use time=%ld ms",usec1);


#if 0
	static int blitcount = 0;
	char pro_value[16];
	property_get("sys.dump",pro_value,0);

	if(!strcmp(pro_value,"true") && blitcount < 5)
	{
		FILE * pfile = NULL;
		char layername[100] ;
		blitcount ++ ;

		//mkdir( "/data/",777);
		sprintf(layername,"/data/dumpvo%d.bin",blitcount);
		pfile = fopen(layername,"wb");
		if(pfile)
		{
			fwrite((const void *)srcandle->base,(size_t)(2 * Rga_Request.dst.vir_w  * Rga_Request.dst.vir_h),1,pfile);
			fclose(pfile);
		}

   	}
#endif

}
void DisplayDevice::SetLastBuffAddr( int value) const
{
	LastBuffAddr = value;
}
#endif

uint32_t DisplayDevice::getFlags() const
{
    return mFlags;
}

EGLBoolean DisplayDevice::makeCurrent(EGLDisplay dpy,
        const sp<const DisplayDevice>& hw, EGLContext ctx) {
    EGLBoolean result = EGL_TRUE;
    EGLSurface sur = eglGetCurrentSurface(EGL_DRAW);
    if (sur != hw->mSurface) {
        result = eglMakeCurrent(dpy, hw->mSurface, hw->mSurface, ctx);
        if (result == EGL_TRUE) {
            setViewportAndProjection(hw);
        }
    }
    return result;
}

void DisplayDevice::setViewportAndProjection(const sp<const DisplayDevice>& hw) {
    GLsizei w = hw->mDisplayWidth;
    GLsizei h = hw->mDisplayHeight;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // put the origin in the left-bottom corner
    glOrthof(0, w, 0, h, 0, 1); // l=0, r=w ; b=0, t=h
    glMatrixMode(GL_MODELVIEW);
}

// ----------------------------------------------------------------------------

void DisplayDevice::setVisibleLayersSortedByZ(const Vector< sp<LayerBase> >& layers) {
    mVisibleLayersSortedByZ = layers;
    mSecureLayerVisible = false;
    size_t count = layers.size();
    for (size_t i=0 ; i<count ; i++) {
        if (layers[i]->isSecure()) {
            mSecureLayerVisible = true;
        }
    }
}

const Vector< sp<LayerBase> >& DisplayDevice::getVisibleLayersSortedByZ() const {
    return mVisibleLayersSortedByZ;
}

bool DisplayDevice::getSecureLayerVisible() const {
    return mSecureLayerVisible;
}

Region DisplayDevice::getDirtyRegion(bool repaintEverything) const {
    Region dirty;
    if (repaintEverything) {
        dirty.set(getBounds());
    } else {
        const Transform& planeTransform(mGlobalTransform);
        dirty = planeTransform.transform(this->dirtyRegion);
        dirty.andSelf(getBounds());
    }
    return dirty;
}

// ----------------------------------------------------------------------------

bool DisplayDevice::canDraw() const {
    return mScreenAcquired;
}

void DisplayDevice::releaseScreen() const {
    mScreenAcquired = false;
}

void DisplayDevice::acquireScreen() const {
    mScreenAcquired = true;
}

bool DisplayDevice::isScreenAcquired() const {
    return mScreenAcquired;
}

// ----------------------------------------------------------------------------

void DisplayDevice::setLayerStack(uint32_t stack) {
    mLayerStack = stack;
    dirtyRegion.set(bounds());
}

// ----------------------------------------------------------------------------

status_t DisplayDevice::orientationToTransfrom(
        int orientation, int w, int h, Transform* tr, bool shouldTransform, bool scaled)
{
    uint32_t flags = 0;
    switch (orientation) {
    case DisplayState::eOrientationDefault:
        flags = Transform::ROT_0;
        break;
    case DisplayState::eOrientation90:
        flags = Transform::ROT_90;
        break;
    case DisplayState::eOrientation180:
        flags = Transform::ROT_180;
        break;
    case DisplayState::eOrientation270:
        flags = Transform::ROT_270;
        if(scaled == true)	h = FAKEHEIGHT;
        break;
    default:
        return BAD_VALUE;
    }
    if(shouldTransform) {
        flags = Transform::ROT_0;
    }
    tr->set(flags, w, h);
    return NO_ERROR;
}

void DisplayDevice::setProjection(int orientation,
        const Rect& viewport, const Rect& frame, bool shouldTransform) {
    mOrientation = orientation;
    mViewport = viewport;
    mFrame = frame;

    #if FORCE_SCALE_FULLSCREEN
    ALOGV("name =%s",getDisplayName().string());
    ALOGV(" viewport [%d %d]",mViewport.getWidth(),mViewport.getHeight());
    ALOGV(" frame [%d %d]", mFrame.getWidth(),mFrame.getHeight());
    ALOGV(" hw [%d %d]", getWidth(),getHeight());
    if(strcmp(getDisplayName().string(),"Built-in Screen")
        && mViewport.getWidth() > mViewport.getHeight())
    {
        mFrame = Rect(0,0,getWidth(),getHeight());

    }
    #endif

    if (mType == DisplayDevice::DISPLAY_PRIMARY) {
        mOrientation = (mHardwareOrientation +orientation) % 4;
        mClientOrientation = orientation;
    }

    updateGeometryTransform(shouldTransform);
}

void DisplayDevice::updateGeometryTransform(bool shouldTransform) {
    int w = mDisplayWidth;
    int h = mDisplayHeight;
    Transform TL, TP, R, S, realR, TA;
    if (DisplayDevice::orientationToTransfrom(
            mOrientation, w, h, &R, shouldTransform, mScaled) == NO_ERROR) {
        dirtyRegion.set(bounds());

        Rect viewport(mViewport);
        Rect frame(mFrame);

        if (!frame.isValid()) {
            // the destination frame can be invalid if it has never been set,
            // in that case we assume the whole display frame.
            frame = Rect(w, h);
        }

        if (viewport.isEmpty()) {
            // viewport can be invalid if it has never been set, in that case
            // we assume the whole display size.
            // it's also invalid to have an empty viewport, so we handle that
            // case in the same way.
            viewport = Rect(w, h);
            if (R.getOrientation() & Transform::ROT_90) {
                // viewport is always specified in the logical orientation
                // of the display (ie: post-rotation).
                swap(viewport.right, viewport.bottom);
            }
        }

        float src_width  = viewport.width();
        float src_height = viewport.height();
        float dst_width  = frame.width();
        float dst_height = frame.height();
        if (src_width != dst_width || src_height != dst_height) {
            float sx = dst_width  / src_width;
            float sy = dst_height / src_height;
            S.set(sx, 0, 0, sy);
        }

        float src_x = viewport.left;
        float src_y = viewport.top;
        float dst_x = frame.left;
        float dst_y = frame.top;
        TL.set(-src_x, -src_y);
        TP.set(dst_x, dst_y);

        // The viewport and frame are both in the logical orientation.
        // Apply the logical translation, scale to physical size, apply the
        // physical translation and finally rotate to the physical orientation.
        mGlobalTransform = R * TP * S * TL;

        if (DisplayDevice::orientationToTransfrom(
                mOrientation, w, h, &realR, false, mScaled) == NO_ERROR) {
            mRealGlobalTransform = realR * TP * S * TL;
        }

        if (mFlinger->mUseLcdcComposer && mType == 2) {
            sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
            switch (hw->getHardwareRotation()) {
                case DisplayState::eOrientation270:
                    TA.set(Transform::ROT_90, src_width, 0);
                    break;
                case DisplayState::eOrientation90:
                    TA.set(Transform::ROT_270, 0, src_height);
                    break;
                case DisplayState::eOrientation180:
                    TA.set(Transform::ROT_180, src_width, src_height);
                    break;
            }
            mGlobalTransform = mGlobalTransform * TA;
        }
        const uint8_t type = mGlobalTransform.getType();
        mNeedsFiltering = (!mGlobalTransform.preserveRects() ||
                (type >= Transform::SCALE));
    }
}

void DisplayDevice::dump(String8& result, char* buffer, size_t SIZE) const {
    const Transform& tr(mGlobalTransform);
    const Transform& realTR(mRealGlobalTransform);
    snprintf(buffer, SIZE,
        "+ DisplayDevice: %s\n"
        "   type=%x, layerStack=%u, (%4dx%4d), ANativeWindow=%p, orient=%2d clienOrient=%2d (type=%08x), "
        "flips=%u, isSecure=%d, secureVis=%d, acquired=%d, numLayers=%u\n"
        "   v:[%d,%d,%d,%d], f:[%d,%d,%d,%d], "
        "transform:[[%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f]]\n"
        "   real transform:[[%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f]]\n",
        mDisplayName.string(), mType,
        mLayerStack, mDisplayWidth, mDisplayHeight, mNativeWindow.get(),
        mOrientation, mClientOrientation, tr.getType(), getPageFlipCount(),
        mIsSecure, mSecureLayerVisible, mScreenAcquired, mVisibleLayersSortedByZ.size(),
        mViewport.left, mViewport.top, mViewport.right, mViewport.bottom,
        mFrame.left, mFrame.top, mFrame.right, mFrame.bottom,
        tr[0][0], tr[1][0], tr[2][0],
        tr[0][1], tr[1][1], tr[2][1],
        tr[0][2], tr[1][2], tr[2][2],
        realTR[0][0], realTR[1][0], realTR[2][0],
        realTR[0][1], realTR[1][1], realTR[2][1],
        realTR[0][2], realTR[1][2], realTR[2][2]);

    result.append(buffer);

    String8 fbtargetDump;
    if (mFramebufferSurface != NULL) {
        mFramebufferSurface->dump(fbtargetDump);
        result.append(fbtargetDump);
    }
}

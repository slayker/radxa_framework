//$_FOR_ROCKCHIP_RBOX_$
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

#define LOG_TAG "Sprites"

//#define LOG_NDEBUG 0

#include "SpriteController.h"

#include <cutils/log.h>
#include <utils/String8.h>

#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkColor.h>
#include <SkPaint.h>
#include <SkXfermode.h>
//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
#include <linux/fb.h>
#include <fcntl.h>
#include "cutils/properties.h"

#define FBIOPUT_SET_CURSOR_EN    0x4609
#define FBIOPUT_SET_CURSOR_IMG    0x460a
#define FBIOPUT_SET_CURSOR_POS    0x460b
#define FBIOPUT_SET_CURSOR_CMAP    0x460c
#define FBIOPUT_GET_CURSOR_RESOLUTION    0x460d
#define FBIOPUT_GET_CURSOR_EN    0x460e
//$_rbox_$_modify_$_end

namespace android {

// --- SpriteController ---

SpriteController::SpriteController(const sp<Looper>& looper, int32_t overlayLayer) :
        mLooper(looper), mOverlayLayer(overlayLayer) {
    mHandler = new WeakMessageHandler(this);

    mLocked.transactionNestingCount = 0;
    mLocked.deferredSpriteUpdate = false;
//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
    mFbHandle = 0;
    mUseHwCursor = false;
    mUseSwCursor = true;
    setHwCursor();
//$_rbox_$_modify_$_end
}

SpriteController::~SpriteController() {
    mLooper->removeMessages(mHandler);

    if (mSurfaceComposerClient != NULL) {
        mSurfaceComposerClient->dispose();
        mSurfaceComposerClient.clear();
    }
}

//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
void SpriteController::setHwCursor()
{
    bool useHwCursor = mUseHwCursor;
    char propVal[PROPERTY_VALUE_MAX];
    property_get("cursor.hw", propVal, "false");
//    ALOGD("cursor.hw=%s", propVal);
    mUseHwCursor = (strcmp(propVal, "true")==0) ? true : false;
//    ALOGD("mUseHwCursor=0x%x", mUseHwCursor);

    property_get("cursor.sw", propVal, "true");
//    ALOGD("cursor.sw=%s", propVal);
    mUseSwCursor = (strcmp(propVal, "true")==0) ? true : false;
//    ALOGD("mUseSwCursor=0x%x", mUseSwCursor);

    if (!mUseHwCursor)
	return;

    if (useHwCursor == mUseHwCursor)
	return;

    if (!mFbHandle) {
        mFbHandle = open("/dev/graphics/fb0", O_RDWR, 0);
    }
    if(mFbHandle < 0) {
        ALOGE("open fb fail");
    } else {
	char cursorImg[128] = {
	//big mouse with tail
	0x80, 0x00, 0x00, 0x00,
	0xE0, 0x00, 0x00, 0x00,
	0x78, 0x00, 0x00, 0x00,
	0x7E, 0x00, 0x00, 0x00,
	0x3F, 0x80, 0x00, 0x00,
	0x3F, 0xE0, 0x00, 0x00,
	0x1F, 0xF8, 0x00, 0x00,
	0x1F, 0xFE, 0x00, 0x00,
	0x0F, 0xFF, 0x80, 0x00,
	0x0F, 0xFF, 0xE0, 0x00,
	0x07, 0xFF, 0xF8, 0x00,
	0x07, 0xFF, 0xFE, 0x00,
	0x03, 0xFF, 0xFF, 0x80,
	0x03, 0xFF, 0xFF, 0xE0,
	0x01, 0xFF, 0xFF, 0xF8,
	0x01, 0xFF, 0xFF, 0xFC,
	0x00, 0xFF, 0xFF, 0xF8,
	0x00, 0xFF, 0xFF, 0xF0,
	0x00, 0x7F, 0xFF, 0xE0,
	0x00, 0x7F, 0xFF, 0xC0,
	0x00, 0x3F, 0xFF, 0x80,
	0x00, 0x3F, 0xFF, 0x80,
	0x00, 0x1F, 0xFF, 0xC0,
	0x00, 0x1F, 0xFF, 0xE0,
	0x00, 0x0F, 0xFF, 0xF0,
	0x00, 0x0F, 0xF3, 0xF8,
	0x00, 0x07, 0xE1, 0xFC,
	0x00, 0x07, 0xC0, 0xFE,
	0x00, 0x03, 0x80, 0x7F,
	0x00, 0x03, 0x00, 0x3F,
	0x00, 0x00, 0x00, 0x1E,
	0x00, 0x00, 0x00, 0x0C,
	};
        struct fb_image img;
        int cursor_en = 1;
	int red;
	int green;
	int blue;
	property_get("cursor.hw.colour.red", propVal, "255");
	red = atoi(propVal);
	property_get("cursor.hw.colour.green", propVal, "0");
	green = atoi(propVal);
	property_get("cursor.hw.colour.blue", propVal, "0");
	blue = atoi(propVal);

        img.bg_color = 0x000000ff;
        img.fg_color = ((red<<16) + (green<<8) + blue);//0x00ff7f00;

	ALOGD("red=%d, green=%d, blue=%d, img.fg_color=0x%x", red, green, blue, img.fg_color);
        ioctl(mFbHandle, FBIOPUT_SET_CURSOR_CMAP, &img);
        ioctl(mFbHandle, FBIOPUT_SET_CURSOR_IMG, cursorImg);
        ioctl(mFbHandle, FBIOPUT_SET_CURSOR_EN, &cursor_en);
    }
}
//$_rbox_$_modify_$_end

sp<Sprite> SpriteController::createSprite() {
    return new SpriteImpl(this);
}

void SpriteController::openTransaction() {
    AutoMutex _l(mLock);

    mLocked.transactionNestingCount += 1;
}

void SpriteController::closeTransaction() {
    AutoMutex _l(mLock);

    LOG_ALWAYS_FATAL_IF(mLocked.transactionNestingCount == 0,
            "Sprite closeTransaction() called but there is no open sprite transaction");

    mLocked.transactionNestingCount -= 1;
    if (mLocked.transactionNestingCount == 0 && mLocked.deferredSpriteUpdate) {
        mLocked.deferredSpriteUpdate = false;
        mLooper->sendMessage(mHandler, Message(MSG_UPDATE_SPRITES));
    }
}

void SpriteController::invalidateSpriteLocked(const sp<SpriteImpl>& sprite) {
    bool wasEmpty = mLocked.invalidatedSprites.isEmpty();
    mLocked.invalidatedSprites.push(sprite);
    if (wasEmpty) {
        if (mLocked.transactionNestingCount != 0) {
            mLocked.deferredSpriteUpdate = true;
        } else {
            mLooper->sendMessage(mHandler, Message(MSG_UPDATE_SPRITES));
        }
    }
}

void SpriteController::disposeSurfaceLocked(const sp<SurfaceControl>& surfaceControl) {
    bool wasEmpty = mLocked.disposedSurfaces.isEmpty();
    mLocked.disposedSurfaces.push(surfaceControl);
    if (wasEmpty) {
        mLooper->sendMessage(mHandler, Message(MSG_DISPOSE_SURFACES));
    }
}

void SpriteController::handleMessage(const Message& message) {
    switch (message.what) {
    case MSG_UPDATE_SPRITES:
        doUpdateSprites();
        break;
    case MSG_DISPOSE_SURFACES:
        doDisposeSurfaces();
        break;
    }
}

void SpriteController::doUpdateSprites() {
    // Collect information about sprite updates.
    // Each sprite update record includes a reference to its associated sprite so we can
    // be certain the sprites will not be deleted while this function runs.  Sprites
    // may invalidate themselves again during this time but we will handle those changes
    // in the next iteration.
    Vector<SpriteUpdate> updates;
    size_t numSprites;

//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
    if(!mUseSwCursor) {

        return;
    }
//$_rbox_$_modify_$_end

    { // acquire lock
        AutoMutex _l(mLock);

        numSprites = mLocked.invalidatedSprites.size();
        for (size_t i = 0; i < numSprites; i++) {
            const sp<SpriteImpl>& sprite = mLocked.invalidatedSprites.itemAt(i);

            updates.push(SpriteUpdate(sprite, sprite->getStateLocked()));
            sprite->resetDirtyLocked();
        }
        mLocked.invalidatedSprites.clear();
    } // release lock

    // Create missing surfaces.
    bool surfaceChanged = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);

        if (update.state.surfaceControl == NULL && update.state.wantSurfaceVisible()) {
            update.state.surfaceWidth = update.state.icon.bitmap.width();
            update.state.surfaceHeight = update.state.icon.bitmap.height();
            update.state.surfaceDrawn = false;
            update.state.surfaceVisible = false;
            update.state.surfaceControl = obtainSurface(
                    update.state.surfaceWidth, update.state.surfaceHeight);
            if (update.state.surfaceControl != NULL) {
                update.surfaceChanged = surfaceChanged = true;
            }
        }
    }

    // Resize sprites if needed, inside a global transaction.
    bool haveGlobalTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);

        if (update.state.surfaceControl != NULL && update.state.wantSurfaceVisible()) {
            int32_t desiredWidth = update.state.icon.bitmap.width();
            int32_t desiredHeight = update.state.icon.bitmap.height();
            if (update.state.surfaceWidth < desiredWidth
                    || update.state.surfaceHeight < desiredHeight) {
                if (!haveGlobalTransaction) {
                    SurfaceComposerClient::openGlobalTransaction();
                    haveGlobalTransaction = true;
                }

                status_t status = update.state.surfaceControl->setSize(desiredWidth, desiredHeight);
                if (status) {
                    ALOGE("Error %d resizing sprite surface from %dx%d to %dx%d",
                            status, update.state.surfaceWidth, update.state.surfaceHeight,
                            desiredWidth, desiredHeight);
                } else {
                    update.state.surfaceWidth = desiredWidth;
                    update.state.surfaceHeight = desiredHeight;
                    update.state.surfaceDrawn = false;
                    update.surfaceChanged = surfaceChanged = true;

                    if (update.state.surfaceVisible) {
                        status = update.state.surfaceControl->hide();
                        if (status) {
                            ALOGE("Error %d hiding sprite surface after resize.", status);
                        } else {
                            update.state.surfaceVisible = false;
                        }
                    }
                }
            }
        }
    }
    if (haveGlobalTransaction) {
        SurfaceComposerClient::closeGlobalTransaction();
    }

    // Redraw sprites if needed.
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);

        if ((update.state.dirty & DIRTY_BITMAP) && update.state.surfaceDrawn) {
            update.state.surfaceDrawn = false;
            update.surfaceChanged = surfaceChanged = true;
        }

        if (update.state.surfaceControl != NULL && !update.state.surfaceDrawn
                && update.state.wantSurfaceVisible()) {
            sp<Surface> surface = update.state.surfaceControl->getSurface();
            Surface::SurfaceInfo surfaceInfo;
            status_t status = surface->lock(&surfaceInfo);
            if (status) {
                ALOGE("Error %d locking sprite surface before drawing.", status);
            } else {
                SkBitmap surfaceBitmap;
                ssize_t bpr = surfaceInfo.s * bytesPerPixel(surfaceInfo.format);
                surfaceBitmap.setConfig(SkBitmap::kARGB_8888_Config,
                        surfaceInfo.w, surfaceInfo.h, bpr);
                surfaceBitmap.setPixels(surfaceInfo.bits);

                SkCanvas surfaceCanvas;
                surfaceCanvas.setBitmapDevice(surfaceBitmap);

                SkPaint paint;
                paint.setXfermodeMode(SkXfermode::kSrc_Mode);
                surfaceCanvas.drawBitmap(update.state.icon.bitmap, 0, 0, &paint);

                if (surfaceInfo.w > uint32_t(update.state.icon.bitmap.width())) {
                    paint.setColor(0); // transparent fill color
                    surfaceCanvas.drawRectCoords(update.state.icon.bitmap.width(), 0,
                            surfaceInfo.w, update.state.icon.bitmap.height(), paint);
                }
                if (surfaceInfo.h > uint32_t(update.state.icon.bitmap.height())) {
                    paint.setColor(0); // transparent fill color
                    surfaceCanvas.drawRectCoords(0, update.state.icon.bitmap.height(),
                            surfaceInfo.w, surfaceInfo.h, paint);
                }

                status = surface->unlockAndPost();
                if (status) {
                    ALOGE("Error %d unlocking and posting sprite surface after drawing.", status);
                } else {
                    update.state.surfaceDrawn = true;
                    update.surfaceChanged = surfaceChanged = true;
                }
            }
        }
    }

    // Set sprite surface properties and make them visible.
    bool haveTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);

        bool wantSurfaceVisibleAndDrawn = update.state.wantSurfaceVisible()
                && update.state.surfaceDrawn;
        bool becomingVisible = wantSurfaceVisibleAndDrawn && !update.state.surfaceVisible;
        bool becomingHidden = !wantSurfaceVisibleAndDrawn && update.state.surfaceVisible;
        if (update.state.surfaceControl != NULL && (becomingVisible || becomingHidden
                || (wantSurfaceVisibleAndDrawn && (update.state.dirty & (DIRTY_ALPHA
                        | DIRTY_POSITION | DIRTY_TRANSFORMATION_MATRIX | DIRTY_LAYER
                        | DIRTY_VISIBILITY | DIRTY_HOTSPOT))))) {
            status_t status;
            if (!haveTransaction) {
                SurfaceComposerClient::openGlobalTransaction();
                haveTransaction = true;
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & DIRTY_ALPHA))) {
                status = update.state.surfaceControl->setAlpha(update.state.alpha);
                if (status) {
                    ALOGE("Error %d setting sprite surface alpha.", status);
                }
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & (DIRTY_POSITION
                            | DIRTY_HOTSPOT)))) {
                status = update.state.surfaceControl->setPosition(
                        update.state.positionX - update.state.icon.hotSpotX,
                        update.state.positionY - update.state.icon.hotSpotY);
                if (status) {
                    ALOGE("Error %d setting sprite surface position.", status);
                }
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible
                            || (update.state.dirty & DIRTY_TRANSFORMATION_MATRIX))) {
                status = update.state.surfaceControl->setMatrix(
                        update.state.transformationMatrix.dsdx,
                        update.state.transformationMatrix.dtdx,
                        update.state.transformationMatrix.dsdy,
                        update.state.transformationMatrix.dtdy);
                if (status) {
                    ALOGE("Error %d setting sprite surface transformation matrix.", status);
                }
            }

            int32_t surfaceLayer = mOverlayLayer + update.state.layer;
            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & DIRTY_LAYER))) {
                status = update.state.surfaceControl->setLayer(surfaceLayer);
                if (status) {
                    ALOGE("Error %d setting sprite surface layer.", status);
                }
            }

            if (becomingVisible) {
                status = update.state.surfaceControl->show();
                if (status) {
                    ALOGE("Error %d showing sprite surface.", status);
                } else {
                    update.state.surfaceVisible = true;
                    update.surfaceChanged = surfaceChanged = true;
                }
            } else if (becomingHidden) {
                status = update.state.surfaceControl->hide();
                if (status) {
                    ALOGE("Error %d hiding sprite surface.", status);
                } else {
                    update.state.surfaceVisible = false;
                    update.surfaceChanged = surfaceChanged = true;
                }
            }
        }
    }

    if (haveTransaction) {
        SurfaceComposerClient::closeGlobalTransaction();
    }

    // If any surfaces were changed, write back the new surface properties to the sprites.
    if (surfaceChanged) { // acquire lock
        AutoMutex _l(mLock);

        for (size_t i = 0; i < numSprites; i++) {
            const SpriteUpdate& update = updates.itemAt(i);

            if (update.surfaceChanged) {
                update.sprite->setSurfaceLocked(update.state.surfaceControl,
                        update.state.surfaceWidth, update.state.surfaceHeight,
                        update.state.surfaceDrawn, update.state.surfaceVisible);
            }
        }
    } // release lock

    // Clear the sprite update vector outside the lock.  It is very important that
    // we do not clear sprite references inside the lock since we could be releasing
    // the last remaining reference to the sprite here which would result in the
    // sprite being deleted and the lock being reacquired by the sprite destructor
    // while already held.
    updates.clear();
}

void SpriteController::doDisposeSurfaces() {
    // Collect disposed surfaces.
    Vector<sp<SurfaceControl> > disposedSurfaces;
    { // acquire lock
        AutoMutex _l(mLock);

        disposedSurfaces = mLocked.disposedSurfaces;
        mLocked.disposedSurfaces.clear();
    } // release lock

    // Release the last reference to each surface outside of the lock.
    // We don't want the surfaces to be deleted while we are holding our lock.
    disposedSurfaces.clear();
}

void SpriteController::ensureSurfaceComposerClient() {
    if (mSurfaceComposerClient == NULL) {
        mSurfaceComposerClient = new SurfaceComposerClient();
    }
}

sp<SurfaceControl> SpriteController::obtainSurface(int32_t width, int32_t height) {
    ensureSurfaceComposerClient();

    sp<SurfaceControl> surfaceControl = mSurfaceComposerClient->createSurface(
            String8("Sprite"), width, height, PIXEL_FORMAT_RGBA_8888,
            ISurfaceComposerClient::eHidden);
    if (surfaceControl == NULL || !surfaceControl->isValid()
            || !surfaceControl->getSurface()->isValid()) {
        ALOGE("Error creating sprite surface.");
        return NULL;
    }
    return surfaceControl;
}


// --- SpriteController::SpriteImpl ---

SpriteController::SpriteImpl::SpriteImpl(const sp<SpriteController> controller) :
        mController(controller) {
}

SpriteController::SpriteImpl::~SpriteImpl() {
    AutoMutex _m(mController->mLock);

    // Let the controller take care of deleting the last reference to sprite
    // surfaces so that we do not block the caller on an IPC here.
    if (mLocked.state.surfaceControl != NULL) {
        mController->disposeSurfaceLocked(mLocked.state.surfaceControl);
        mLocked.state.surfaceControl.clear();
    }
}

void SpriteController::SpriteImpl::setIcon(const SpriteIcon& icon) {
    AutoMutex _l(mController->mLock);

    uint32_t dirty;
    if (icon.isValid()) {
        icon.bitmap.copyTo(&mLocked.state.icon.bitmap, SkBitmap::kARGB_8888_Config);

        if (!mLocked.state.icon.isValid()
                || mLocked.state.icon.hotSpotX != icon.hotSpotX
                || mLocked.state.icon.hotSpotY != icon.hotSpotY) {
            mLocked.state.icon.hotSpotX = icon.hotSpotX;
            mLocked.state.icon.hotSpotY = icon.hotSpotY;
            dirty = DIRTY_BITMAP | DIRTY_HOTSPOT;
        } else {
            dirty = DIRTY_BITMAP;
        }
    } else if (mLocked.state.icon.isValid()) {
        mLocked.state.icon.bitmap.reset();
        dirty = DIRTY_BITMAP | DIRTY_HOTSPOT;
    } else {
        return; // setting to invalid icon and already invalid so nothing to do
    }

    invalidateLocked(dirty);
}

void SpriteController::SpriteImpl::setVisible(bool visible) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.visible != visible) {
        mLocked.state.visible = visible;
//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
	if(visible) {
	    mController->setHwCursor();
	}

        if (mController->mUseHwCursor) {
            int cursor_en = (visible == true) ? 1 : 0;
            ioctl(mController->mFbHandle, FBIOPUT_SET_CURSOR_EN, &cursor_en);
        }
//$_rbox_$_modify_$_end
        invalidateLocked(DIRTY_VISIBILITY);
    }
}

void SpriteController::SpriteImpl::setPosition(float x, float y) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.positionX != x || mLocked.state.positionY != y) {
        mLocked.state.positionX = x;
        mLocked.state.positionY = y;
//$_rbox_$_modify_$_chenzhi_20120628
//$_rbox_$_modify_$_begn
    if(mController->mUseHwCursor) {
        struct fbcurpos cursor_pos = {0,0};
        cursor_pos.x = x;
        cursor_pos.y = y;
        ioctl(mController->mFbHandle, FBIOPUT_SET_CURSOR_POS, &cursor_pos);
    }
//$_rbox_$_modify_$_end
        invalidateLocked(DIRTY_POSITION);
    }
}

void SpriteController::SpriteImpl::setLayer(int32_t layer) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.layer != layer) {
        mLocked.state.layer = layer;
        invalidateLocked(DIRTY_LAYER);
    }
}

void SpriteController::SpriteImpl::setAlpha(float alpha) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.alpha != alpha) {
        mLocked.state.alpha = alpha;
        invalidateLocked(DIRTY_ALPHA);
    }
}

void SpriteController::SpriteImpl::setTransformationMatrix(
        const SpriteTransformationMatrix& matrix) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.transformationMatrix != matrix) {
        mLocked.state.transformationMatrix = matrix;
        invalidateLocked(DIRTY_TRANSFORMATION_MATRIX);
    }
}

void SpriteController::SpriteImpl::invalidateLocked(uint32_t dirty) {
    bool wasDirty = mLocked.state.dirty;
    mLocked.state.dirty |= dirty;

    if (!wasDirty) {
        mController->invalidateSpriteLocked(this);
    }
}

} // namespace android

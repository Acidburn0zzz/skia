/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "picture_utils.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkBitmap.h"
#include "SkPicture.h"
#include "SkRefCnt.h"
#include "SkString.h"
#include "SkStream.h"

namespace sk_tools {
    void force_all_opaque(const SkBitmap& bitmap) {
        SkASSERT(NULL == bitmap.getTexture());
        SkASSERT(SkBitmap::kARGB_8888_Config == bitmap.config());
        if (NULL != bitmap.getTexture() || SkBitmap::kARGB_8888_Config == bitmap.config()) {
            return;
        }

        SkAutoLockPixels lock(bitmap);
        for (int y = 0; y < bitmap.height(); y++) {
            for (int x = 0; x < bitmap.width(); x++) {
                *bitmap.getAddr32(x, y) |= (SK_A32_MASK << SK_A32_SHIFT);
            }
        }
    }

    void make_filepath(SkString* path, const SkString& dir, const SkString& name) {
        size_t len = dir.size();
        path->set(dir);
        if (0 < len  && '/' != dir[len - 1]) {
            path->append("/");
        }
        path->append(name);
    }

    namespace {
        bool is_path_seperator(const char chr) {
#if defined(SK_BUILD_FOR_WIN)
            return chr == '\\' || chr == '/';
#else
            return chr == '/';
#endif
        }
    }

    void get_basename(SkString* basename, const SkString& path) {
        if (path.size() == 0) {
            basename->reset();
            return;
        }

        size_t end = path.size() - 1;

        // Paths pointing to directories often have a trailing slash,
        // we remove it so the name is not empty
        if (is_path_seperator(path[end])) {
            if (end == 0) {
                basename->reset();
                return;
            }

            end -= 1;
        }

        size_t i = end;
        do {
            --i;
            if (is_path_seperator(path[i])) {
                  const char* basenameStart = path.c_str() + i + 1;
                  size_t basenameLength = end - i;
                  basename->set(basenameStart, basenameLength);
                  return;
            }
        } while (i > 0);

        basename->set(path.c_str(), end + 1);
    }

    bool is_percentage(const char* const string) {
        SkString skString(string);
        return skString.endsWith("%");
    }

    void setup_bitmap(SkBitmap* bitmap, int width, int height) {
        bitmap->setConfig(SkBitmap::kARGB_8888_Config, width, height);
        bitmap->allocPixels();
        bitmap->eraseColor(0);
    }

    bool area_too_big(int w, int h, SkISize* newSize) {
        // just a guess, based on what seems to fail on smaller android devices
        static const int64_t kMaxAreaForMemory = 16 * 1024 * 1024;

        if ((int64_t)w * h > kMaxAreaForMemory) {
            do {
                w >>= 1;
                h >>= 1;
            } while ((int64_t)w * h > kMaxAreaForMemory);
            if (0 == w) {
                w = 1;
            }
            if (0 == h) {
                h = 1;
            }
            newSize->set(w, h);
            return true;
        }
        return false;
    }

    void resize_if_needed(SkAutoTUnref<SkPicture>* aur) {
        SkISize newSize;
        SkPicture* picture = aur->get();
        if (area_too_big(picture->width(), picture->height(), &newSize)) {
            SkPicture* pic = SkNEW(SkPicture);
            picture->ref();
            aur->reset(pic);

            SkCanvas* canvas = pic->beginRecording(newSize.width(),
                                                   newSize.height());
            SkScalar scale = SkIntToScalar(newSize.width()) / picture->width();
            canvas->scale(scale, scale);
            canvas->drawPicture(*picture);
            pic->endRecording();

            SkDebugf(
                "... rescaling to [%d %d] to avoid overly large allocations\n",
                newSize.width(), newSize.height());
            picture->unref();
        }
    }
}

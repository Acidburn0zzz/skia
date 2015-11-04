
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
//#include <GL/glx.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>

#include "SkWindow.h"

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColor.h"
#include "SkEvent.h"
#include "SkKey.h"
#include "SkWindow.h"
#include "XkeysToSkKeys.h"
extern "C" {
    #include "keysym2ucs.h"
}

const int WIDTH = 500;
const int HEIGHT = 500;

// Determine which events to listen for.
const long EVENT_MASK = StructureNotifyMask|ButtonPressMask|ButtonReleaseMask
        |ExposureMask|PointerMotionMask|KeyPressMask|KeyReleaseMask;

static Display* getDisplay()
{
    static Display* display = NULL;
    if (!display)
        display = XOpenDisplay(NULL);
    return display;
}
 
static EGLDisplay getEGLDisplay()
{
    static EGLDisplay display = EGL_NO_DISPLAY;
    if (display == EGL_NO_DISPLAY)
        display = eglGetDisplay((EGLNativeDisplayType) getDisplay());
    return display;
}


SkOSWindow::SkOSWindow(void*)
    //: fVi(NULL)
    : fMSAASampleCount(0) {
    fUnixWindow.fDisplay = NULL;
    fUnixWindow.fGLContext = NULL;
    this->initWindow(0, NULL);
    this->resize(WIDTH, HEIGHT);
}

SkOSWindow::~SkOSWindow() {
    this->closeWindow();
}

void SkOSWindow::closeWindow() {
    if (fUnixWindow.fDisplay) {
        this->detach();
        SkASSERT(fUnixWindow.fGc);
        XFreeGC(fUnixWindow.fDisplay, fUnixWindow.fGc);
        fUnixWindow.fGc = NULL;
        XDestroyWindow(fUnixWindow.fDisplay, fUnixWindow.fWin);
        //fVi = NULL;
        XCloseDisplay(fUnixWindow.fDisplay);
        fUnixWindow.fDisplay = NULL;
        fMSAASampleCount = 0;
    }
}

void SkOSWindow::initWindow(int requestedMSAASampleCount, AttachmentInfo* info) {
    EGLConfig configs;
    EGLint numConfigs;
    EGLint num;
    EGLDisplay display;
    if (fMSAASampleCount != requestedMSAASampleCount) {
        this->closeWindow();
    }
    // presence of fDisplay means we already have a window
    if (fUnixWindow.fDisplay) {
        if (info) {
            if (1/*fVi*/) {
                eglGetConfigs(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), &configs, 1, &numConfigs);
                eglGetConfigAttrib(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), configs, EGL_SAMPLES, &info->fSampleCount);  
                eglGetConfigAttrib(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), configs, EGL_STENCIL_SIZE, &info->fStencilBits);
                //glXGetConfig(fUnixWindow.fDisplay, fVi, GLX_SAMPLES_ARB, &info->fSampleCount);
                //glXGetConfig(fUnixWindow.fDisplay, fVi, GLX_STENCIL_SIZE, &info->fStencilBits);
            } else {
                info->fSampleCount = 0;
                info->fStencilBits = 0;
            }
        }
        return;
    }
    fUnixWindow.fDisplay = XOpenDisplay(NULL);
    Display* dsp = fUnixWindow.fDisplay;
    if (NULL == dsp) {
        SkDebugf("Could not open an X Display");
        return;
    }
    // Attempt to create a window that supports GL
    /*GLint att[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_STENCIL_SIZE, 8,
        None
    };*/
    EGLint att[] = { 
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_STENCIL_SIZE, 1,
        /*EGL_SAMPLES, 0,
        EGL_SAMPLE_BUFFERS,0,*/
        EGL_NONE 
    };  

    display = eglGetDisplay ((EGLNativeDisplayType) dsp);
    if (display == EGL_NO_DISPLAY) {
        printf ("Cannot get egl display\n");
        exit (-1);
    }

    EGLint major, minor;

    if (! eglInitialize (display, &major, &minor)) {
        printf ("Cannot initialize egl\n");
        exit (-1);
    }

    if (! eglBindAPI (EGL_OPENGL_ES_API)) {
        printf ("Cannot bind egl to gles2 API\n");
        exit (-1);
    }

    //SkASSERT(NULL == fVi);
    if (requestedMSAASampleCount > 0) {
        static const GLint kAttCount = SK_ARRAY_COUNT(att);
        GLint msaaAtt[kAttCount + 4];
        memcpy(msaaAtt, att, sizeof(att));
        SkASSERT(None == msaaAtt[kAttCount - 1]);
        //msaaAtt[kAttCount - 1] = GLX_SAMPLE_BUFFERS_ARB;
        msaaAtt[kAttCount - 1] = EGL_SAMPLE_BUFFERS;
        msaaAtt[kAttCount + 0] = 1;
        msaaAtt[kAttCount + 1] = EGL_SAMPLES;
        msaaAtt[kAttCount + 2] = requestedMSAASampleCount;
        msaaAtt[kAttCount + 3] = None;
        if (! eglChooseConfig (display, msaaAtt, &fUnixWindow.eglConfig, 1, &num)) {
            printf ("cannot get egl configuration\n");
            exit (-1);
        }

        //fVi = glXChooseVisual(dsp, DefaultScreen(dsp), msaaAtt);
        fMSAASampleCount = requestedMSAASampleCount;
    } else if (requestedMSAASampleCount == 0) {
    //if (NULL == fVi) {
        if (! eglChooseConfig (display, att, &fUnixWindow.eglConfig, 1, &num)) {
            printf ("cannot get egl configuration\n");
            exit (-1);
        }
        //fVi = glXChooseVisual(dsp, DefaultScreen(dsp), att);
        fMSAASampleCount = 0;
    }

    if (1/*fVi*/) {
        if (info) {
              eglGetConfigs(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), &configs, 1, &numConfigs);
              eglGetConfigAttrib(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), configs, EGL_SAMPLES, &info->fSampleCount);  
              eglGetConfigAttrib(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), configs, EGL_STENCIL_SIZE, &info->fStencilBits);
              //glXGetConfig(dsp, fVi, GLX_SAMPLES_ARB, &info->fSampleCount);
              //glXGetConfig(dsp, fVi, GLX_STENCIL_SIZE, &info->fStencilBits);
        }
        fUnixWindow.fWin = XCreateSimpleWindow(dsp,
                                               DefaultRootWindow(dsp),
                                               0, 0,  // x, y
                                               WIDTH, HEIGHT,
                                               0,     // border width
                                               0,     // border value
                                               0);    // background value
        /*Colormap colorMap = XCreateColormap(dsp,
                                            RootWindow(dsp, fVi->screen),
                                            fVi->visual,
                                             AllocNone);
        XSetWindowAttributes swa;
        swa.colormap = colorMap;
        swa.event_mask = EVENT_MASK;
        fUnixWindow.fWin = XCreateWindow(dsp,
                                         RootWindow(dsp, fVi->screen),
                                         0, 0, // x, y
                                         WIDTH, HEIGHT,
                                         0, // border width
                                         fVi->depth,
                                         InputOutput,
                                         fVi->visual,
                                         CWEventMask | CWColormap,
                                         &swa);*/
    } else {
        if (info) {
            info->fSampleCount = 0;
            info->fStencilBits = 0;
        }
        // Create a simple window instead.  We will not be able to show GL
        fUnixWindow.fWin = XCreateSimpleWindow(dsp,
                                               DefaultRootWindow(dsp),
                                               0, 0,  // x, y
                                               WIDTH, HEIGHT,
                                               0,     // border width
                                               0,     // border value
                                               0);    // background value
    }
    this->mapWindowAndWait();
    fUnixWindow.fGc = XCreateGC(dsp, fUnixWindow.fWin, 0, NULL);
}

static unsigned getModi(const XEvent& evt) {
    static const struct {
        unsigned    fXMask;
        unsigned    fSkMask;
    } gModi[] = {
        // X values found by experiment. Is there a better way?
        { 1,    kShift_SkModifierKey },
        { 4,    kControl_SkModifierKey },
        { 8,    kOption_SkModifierKey },
    };

    unsigned modi = 0;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gModi); ++i) {
        if (evt.xkey.state & gModi[i].fXMask) {
            modi |= gModi[i].fSkMask;
        }
    }
    return modi;
}

static SkMSec gTimerDelay;

static bool MyXNextEventWithDelay(Display* dsp, XEvent* evt) {
    // Check for pending events before entering the select loop. There might
    // be events in the in-memory queue but not processed yet.
    if (XPending(dsp)) {
        XNextEvent(dsp, evt);
        return true;
    }

    SkMSec ms = gTimerDelay;
    if (ms > 0) {
        int x11_fd = ConnectionNumber(dsp);
        fd_set input_fds;
        FD_ZERO(&input_fds);
        FD_SET(x11_fd, &input_fds);

        timeval tv;
        tv.tv_sec = ms / 1000;              // seconds
        tv.tv_usec = (ms % 1000) * 1000;    // microseconds

        if (!select(x11_fd + 1, &input_fds, NULL, NULL, &tv)) {
            if (!XPending(dsp)) {
                return false;
            }
        }
    }
    XNextEvent(dsp, evt);
    return true;
}

static Atom wm_delete_window_message;

SkOSWindow::NextXEventResult SkOSWindow::nextXEvent() {
    XEvent evt;
    Display* dsp = fUnixWindow.fDisplay;

    if (!MyXNextEventWithDelay(dsp, &evt)) {
        return kContinue_NextXEventResult;
    }

    switch (evt.type) {
        case Expose:
            if (0 == evt.xexpose.count) {
                return kPaintRequest_NextXEventResult;
            }
            break;
        case ConfigureNotify:
            this->resize(evt.xconfigure.width, evt.xconfigure.height);
            break;
        case ButtonPress:
            if (evt.xbutton.button == Button1)
                this->handleClick(evt.xbutton.x, evt.xbutton.y,
                            SkView::Click::kDown_State, NULL, getModi(evt));
            break;
        case ButtonRelease:
            if (evt.xbutton.button == Button1)
                this->handleClick(evt.xbutton.x, evt.xbutton.y,
                              SkView::Click::kUp_State, NULL, getModi(evt));
            break;
        case MotionNotify:
            this->handleClick(evt.xmotion.x, evt.xmotion.y,
                           SkView::Click::kMoved_State, NULL, getModi(evt));
            break;
        case KeyPress: {
            int shiftLevel = (evt.xkey.state & ShiftMask) ? 1 : 0;
            KeySym keysym = XkbKeycodeToKeysym(dsp, evt.xkey.keycode,
                                               0, shiftLevel);
            if (keysym == XK_Escape) {
                return kQuitRequest_NextXEventResult;
            }
            this->handleKey(XKeyToSkKey(keysym));
            long uni = keysym2ucs(keysym);
            if (uni != -1) {
                this->handleChar((SkUnichar) uni);
            }
            break;
        }
        case KeyRelease:
            this->handleKeyUp(XKeyToSkKey(XkbKeycodeToKeysym(dsp, evt.xkey.keycode, 0, 0)));
            break;
        case ClientMessage:
            if ((Atom)evt.xclient.data.l[0] == wm_delete_window_message) {
                return kQuitRequest_NextXEventResult;
            }
            // fallthrough
        default:
            // Do nothing for other events
            break;
    }
    return kContinue_NextXEventResult;
}

void SkOSWindow::loop() {
    Display* dsp = fUnixWindow.fDisplay;
    if (NULL == dsp) {
        return;
    }
    Window win = fUnixWindow.fWin;

    wm_delete_window_message = XInternAtom(dsp, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dsp, win, &wm_delete_window_message, 1);

    XSelectInput(dsp, win, EVENT_MASK);

    bool sentExposeEvent = false;

    for (;;) {
        SkEvent::ServiceQueueTimer();

        bool moreToDo = SkEvent::ProcessEvent();

        if (this->isDirty() && !sentExposeEvent) {
            sentExposeEvent = true;

            XEvent evt;
            sk_bzero(&evt, sizeof(evt));
            evt.type = Expose;
            evt.xexpose.display = dsp;
            XSendEvent(dsp, win, false, ExposureMask, &evt);
        }

        if (XPending(dsp) || !moreToDo) {
            switch (this->nextXEvent()) {
                case kContinue_NextXEventResult:
                    break;
                case kPaintRequest_NextXEventResult:
                    sentExposeEvent = false;
                    if (this->isDirty()) {
                        this->update(NULL);
                    }
                    this->doPaint();
                    break;
                case kQuitRequest_NextXEventResult:
                    return;
            }
        }
    }
}

void SkOSWindow::mapWindowAndWait() {
    SkASSERT(fUnixWindow.fDisplay);
    Display* dsp = fUnixWindow.fDisplay;
    Window win = fUnixWindow.fWin;
    XMapWindow(dsp, win);

    long eventMask = StructureNotifyMask;
    XSelectInput(dsp, win, eventMask);

    // Wait until screen is ready.
    XEvent evt;
    do {
        XNextEvent(dsp, &evt);
    } while(evt.type != MapNotify);

}

bool SkOSWindow::attach(SkBackEndTypes, int msaaSampleCount, AttachmentInfo* info) {
    this->initWindow(msaaSampleCount, info);

    if (NULL == fUnixWindow.fDisplay) {
        return false;
    }
    if (NULL == fUnixWindow.fGLContext) {
        //SkASSERT(fVi);

        EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

        //fUnixWindow.fGLContext = eglCreateContext(getEGLDisplay(), fUnixWindow.eglConfig, NULL, contextAttributes);
        fUnixWindow.fGLContext = eglCreateContext(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.eglConfig, NULL, contextAttributes);


        /*fUnixWindow.fGLContext = glXCreateContext(fUnixWindow.fDisplay,
                                                    fVi,
                                                    NULL,
                                                    GL_TRUE);*/
        if (NULL == fUnixWindow.fGLContext) {
            return false;
        }
        //fUnixWindow.fGLSurface = eglCreateWindowSurface(getEGLDisplay(), fUnixWindow.eglConfig, (NativeWindowType) fUnixWindow.fWin, NULL);
        fUnixWindow.fGLSurface = eglCreateWindowSurface(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.eglConfig, (NativeWindowType) fUnixWindow.fWin, NULL);
        if (fUnixWindow.fGLSurface == EGL_NO_SURFACE) {
            printf("Cannot create egl window surface\n");
            return false;
        }
    }
 
    eglMakeCurrent(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.fGLSurface, fUnixWindow.fGLSurface, fUnixWindow.fGLContext);

    /*glXMakeCurrent(fUnixWindow.fDisplay,
                   fUnixWindow.fWin,
                   fUnixWindow.fGLContext);*/
    glViewport(0, 0,
               SkScalarRoundToInt(this->width()),
               SkScalarRoundToInt(this->height()));
    glClearColor(0, 0, 0, 0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    return true;
}

void SkOSWindow::detach() {
    if (NULL == fUnixWindow.fDisplay || NULL == fUnixWindow.fGLContext) {
        return;
    }
    eglMakeCurrent (eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext (eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.fGLContext);
    eglDestroySurface (eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.fGLSurface);
    fUnixWindow.fGLContext = EGL_NO_CONTEXT;
    fUnixWindow.fGLSurface = EGL_NO_SURFACE;
    eglTerminate (getEGLDisplay());

    //glXMakeCurrent(fUnixWindow.fDisplay, None, NULL);
    //glXDestroyContext(fUnixWindow.fDisplay, fUnixWindow.fGLContext);
    fUnixWindow.fGLContext = NULL;
}

void SkOSWindow::present() {
    if (fUnixWindow.fDisplay && fUnixWindow.fGLContext) {
        //glXSwapBuffers(fUnixWindow.fDisplay, fUnixWindow.fWin);
        eglSwapBuffers(eglGetDisplay ((EGLNativeDisplayType) fUnixWindow.fDisplay), fUnixWindow.fGLSurface);
    }
}

void SkOSWindow::onSetTitle(const char title[]) {
    if (NULL == fUnixWindow.fDisplay) {
        return;
    }
    XTextProperty textProp;
    textProp.value = (unsigned char*)title;
    textProp.format = 8;
    textProp.nitems = strlen((char*)textProp.value);
    textProp.encoding = XA_STRING;
    XSetWMName(fUnixWindow.fDisplay, fUnixWindow.fWin, &textProp);
}

static bool convertBitmapToXImage(XImage& image, const SkBitmap& bitmap) {
    sk_bzero(&image, sizeof(image));

    int bitsPerPixel = bitmap.bytesPerPixel() * 8;
    image.width = bitmap.width();
    image.height = bitmap.height();
    image.format = ZPixmap;
    image.data = (char*) bitmap.getPixels();
    image.byte_order = LSBFirst;
    image.bitmap_unit = bitsPerPixel;
    image.bitmap_bit_order = LSBFirst;
    image.bitmap_pad = bitsPerPixel;
    image.depth = 24;
    image.bytes_per_line = bitmap.rowBytes() - bitmap.width() * 4;
    image.bits_per_pixel = bitsPerPixel;
    return XInitImage(&image);
}

void SkOSWindow::doPaint() {
    if (NULL == fUnixWindow.fDisplay) {
        return;
    }
    // If we are drawing with GL, we don't need XPutImage.
    if (fUnixWindow.fGLContext) {
        return;
    }
    // Draw the bitmap to the screen.
    const SkBitmap& bitmap = getBitmap();
    int width = bitmap.width();
    int height = bitmap.height();

    XImage image;
    if (!convertBitmapToXImage(image, bitmap)) {
        return;
    }

    XPutImage(fUnixWindow.fDisplay,
              fUnixWindow.fWin,
              fUnixWindow.fGc,
              &image,
              0, 0,     // src x,y
              0, 0,     // dst x,y
              width, height);
}

///////////////////////////////////////////////////////////////////////////////

void SkEvent::SignalNonEmptyQueue() {
    // nothing to do, since we spin on our event-queue, polling for XPending
}

void SkEvent::SignalQueueTimer(SkMSec delay) {
    // just need to record the delay time. We handle waking up for it in
    // MyXNextEventWithDelay()
    gTimerDelay = delay;
}

#pragma once

/* Microscopic platform abstraction to enable showing a
 * borderless hidden window, necessary for scanning VST plug-ins.
 *
 * Based on knowledge from https://github.com/zserge/fenster  
 *
 * MIT License
 * 
 * Copyright (c) 2022 Serge Zaitsev
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
// macOS: -framework CoreFoundation
// X11: -lX11
// Win32: -luser32 -lgdi32

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <objc/NSObjCRuntime.h>

#include <unistd.h>

#include <objc/objc-runtime.h>
#define msg(r, o, s) ((r(*)(id, SEL))objc_msgSend)(o, sel_getUid(s))
#define msg1(r, o, s, A, a) ((r(*)(id, SEL, A))objc_msgSend)(o, sel_getUid(s), a)
#define msg4(r, o, s, A, a, B, b, C, c, D, d) \
  ((r(*)(id, SEL, A, B, C, D))objc_msgSend)(o, sel_getUid(s), a, b, c, d)

#define cls(x) ((id)objc_getClass(x))

extern id const NSDefaultRunLoopMode;
extern id const NSApp;

#elif defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(UNICODE)
#define UNICODE 1
#endif
#if !defined(_UNICODE)
#define _UNICODE 1
#endif
#include <windows.h>
#elif __has_include(<X11/Xlib.h>)
#define _DEFAULT_SOURCE 1
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <dlfcn.h>
#include <unistd.h>

#include <ctime>
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>

struct invisible_window
{
#if defined(__APPLE__)
  id wnd{};
#elif defined(_WIN32)
  HWND hwnd{};
#elif __has_include(<X11/Xlib.h>)
  void* x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
  Display* dpy{};
  Window w{};
  GC gc{};
#endif

  invisible_window()
  {
#if defined(__APPLE__)
    auto pool = msg(id, cls("NSAutoreleasePool"), "alloc");
    msg(void, pool, "init");

    msg(id, cls("NSApplication"), "sharedApplication");

    assert(NSApp);
    // Prevent activation
    msg1(
        void, NSApp, "setActivationPolicy:", NSInteger,
        2 /* NSApplicationActivationPolicyProhibited */);

    // Create the window
    wnd = msg4(
        id, msg(id, cls("NSWindow"), "alloc"),
        "initWithContentRect:styleMask:backing:defer:", CGRect, CGRectMake(0, 0, 1, 1),
        NSUInteger, 3, NSUInteger, 2, BOOL, NO);
    assert(wnd);

    // Make the window transparent
    msg1(void, wnd, "setOpaque:", NSInteger, NO);
    id clearColor = msg(id, cls("NSColor"), "clearColor");
    msg1(void, wnd, "setBackgroundColor:", id, clearColor);

#elif defined(_WIN32)
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
      switch(msg)
      {
        case WM_CLOSE:
          DestroyWindow(hwnd);
          break;
        case WM_DESTROY:
          PostQuitMessage(0);
          break;
        default:
          return DefWindowProc(hwnd, msg, wParam, lParam);
      }
      return 0;
    };
    wc.hInstance = hInstance;
    wc.lpszClassName = "window";
    RegisterClassExA(&wc);

    // Create the window
    hwnd = CreateWindowExA(
        WS_EX_TRANSPARENT, "window", "window", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
        hInstance, nullptr);

    if(hwnd)
      UpdateWindow(hwnd);

#elif __has_include(<X11/Xlib.h>)
    if(!x11)
      return;

    if(auto sym = reinterpret_cast<int (*)()>(dlsym(x11, "XInitThreads")))
      sym();

    decltype(XOpenDisplay)* d_XOpenDisplay
        = (decltype(XOpenDisplay)*)dlsym(x11, "XOpenDisplay");
    decltype(XCreateWindow)* d_XCreateWindow
        = (decltype(XCreateWindow)*)dlsym(x11, "XCreateWindow");
    decltype(XCreateGC)* d_XCreateGC = (decltype(XCreateGC)*)dlsym(x11, "XCreateGC");
    decltype(XSelectInput)* d_XSelectInput
        = (decltype(XSelectInput)*)dlsym(x11, "XSelectInput");
    decltype(XMapWindow)* d_XMapWindow = (decltype(XMapWindow)*)dlsym(x11, "XMapWindow");
    decltype(XSync)* d_XSync = (decltype(XSync)*)dlsym(x11, "XSync");
    decltype(XInternAtom)* d_XInternAtom
        = (decltype(XInternAtom)*)dlsym(x11, "XInternAtom");
    decltype(XChangeProperty)* d_XChangeProperty
        = (decltype(XChangeProperty)*)dlsym(x11, "XChangeProperty");

    dpy = d_XOpenDisplay(NULL);
    if(!dpy)
      throw;

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;

    w = d_XCreateWindow(
        dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 1, 1, 0, CopyFromParent,
        InputOutput, CopyFromParent, CWOverrideRedirect, &attrs);

    gc = d_XCreateGC(dpy, w, 0, 0);
    d_XSelectInput(
        dpy, w,
        ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask
            | ButtonReleaseMask | PointerMotionMask);
    d_XMapWindow(dpy, w);
    d_XSync(dpy, w);

    // Disable decorations
    struct
    {
      unsigned long flags = 2;
      unsigned long functions = 0;
      unsigned long decorations = 0;
      long inputMode = 0;
      unsigned long status = 0;
    } hints{};
    auto property = d_XInternAtom(dpy, "_MOTIF_WM_HINTS", true);
    d_XChangeProperty(
        dpy, w, property, property, 32, PropModeReplace, (unsigned char*)&hints, 5);
    d_XMapWindow(dpy, w);
#endif
  }

  int loop()
  {
#if defined(__APPLE__)
    msg1(void, msg(id, wnd, "contentView"), "setNeedsDisplay:", BOOL, YES);
    id ev = msg4(
        id, NSApp, "nextEventMatchingMask:untilDate:inMode:dequeue:", NSUInteger,
        NSUIntegerMax, id, NULL, id, NSDefaultRunLoopMode, BOOL, YES);
    if(!ev)
      return 0;
    msg1(void, NSApp, "sendEvent:", id, ev);
    return 0;
#elif defined(_WIN32)
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if(msg.message == WM_QUIT)
        return -1;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    InvalidateRect(hwnd, NULL, TRUE);

#elif __has_include(<X11/Xlib.h>)
    if(x11)
    {
      if(decltype(XFlush)* d_XFlush = (decltype(XFlush)*)dlsym(x11, "XFlush"))
        d_XFlush(dpy);
    }
#endif
    return 0;
  }

  ~invisible_window()
  {
#if defined(__APPLE__)
    msg(void, wnd, "close");
    msg1(void, NSApp, "terminate:", id, NSApp);

#elif defined(_WIN32)
    // nothing to do

#elif __has_include(<X11/Xlib.h>)
    if(x11)
    {
      if(decltype(XCloseDisplay)* d_XCloseDisplay
         = (decltype(XCloseDisplay)*)dlsym(x11, "XCloseDisplay"))
        d_XCloseDisplay(dpy);
    }
#endif
  }
};

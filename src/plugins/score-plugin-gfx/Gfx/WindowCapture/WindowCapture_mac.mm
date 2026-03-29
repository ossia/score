#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#if defined(__APPLE__)
#include <QDebug>

#include <CoreGraphics/CoreGraphics.h>

// ScreenCaptureKit requires macOS 13+
// We weak-link it and check availability at runtime.
#if __has_include(<ScreenCaptureKit/ScreenCaptureKit.h>)
#define HAS_SCREENCAPTUREKIT 1
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurface.h>

#include <mutex>

// -----------------------------------------------------------------
// Objective-C delegate that receives frames from SCStream
// -----------------------------------------------------------------
API_AVAILABLE(macos(13.0))
@interface WindowCaptureDelegate : NSObject <SCStreamOutput>
{
  std::mutex _mutex;
  IOSurfaceRef _latestSurface;
  int _surfaceWidth;
  int _surfaceHeight;
}

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type;

// Returns a retained + read-locked IOSurface snapshot.
// The caller owns it and must call IOSurfaceUnlock + IOSurfaceDecrementUseCount
// + CFRelease when done.
- (IOSurfaceRef)copyAndLockSurface:(int *)outWidth height:(int *)outHeight;
- (void)invalidate;

@end

@implementation WindowCaptureDelegate

- (instancetype)init
{
  self = [super init];
  if(self)
  {
    _latestSurface = nullptr;
    _surfaceWidth = 0;
    _surfaceHeight = 0;
  }
  return self;
}

- (void)dealloc
{
  [self invalidate];
  [super dealloc];
}

- (void)invalidate
{
  std::lock_guard<std::mutex> lock(_mutex);
  if(_latestSurface)
  {
    IOSurfaceDecrementUseCount(_latestSurface);
    CFRelease(_latestSurface);
    _latestSurface = nullptr;
  }
  _surfaceWidth = 0;
  _surfaceHeight = 0;
}

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
  if(type != SCStreamOutputTypeScreen)
    return;

  CVPixelBufferRef pixbuf = CMSampleBufferGetImageBuffer(sampleBuffer);
  if(!pixbuf)
    return;

  IOSurfaceRef newSurface = CVPixelBufferGetIOSurface(pixbuf);
  if(!newSurface)
    return;

  // Retain the new surface before taking the lock
  CFRetain(newSurface);
  IOSurfaceIncrementUseCount(newSurface);

  int w = (int)IOSurfaceGetWidth(newSurface);
  int h = (int)IOSurfaceGetHeight(newSurface);

  std::lock_guard<std::mutex> lock(_mutex);

  // Release the old surface
  IOSurfaceRef old = _latestSurface;
  _latestSurface = newSurface;
  _surfaceWidth = w;
  _surfaceHeight = h;

  if(old)
  {
    IOSurfaceDecrementUseCount(old);
    CFRelease(old);
  }
}

// Returns a snapshot of the latest surface. The returned surface is
// independently retained (CFRetain + IOSurfaceIncrementUseCount) and
// read-locked. The caller owns it and must:
//   1. Read the pixel data
//   2. IOSurfaceUnlock(surf, kIOSurfaceLockReadOnly, nullptr)
//   3. IOSurfaceDecrementUseCount(surf)
//   4. CFRelease(surf)
// This avoids the race where the delegate replaces _latestSurface
// between lock and unlock.
- (IOSurfaceRef)copyAndLockSurface:(int *)outWidth height:(int *)outHeight
{
  std::lock_guard<std::mutex> lock(_mutex);
  if(!_latestSurface)
    return nullptr;

  // Take our own retain so the surface stays alive independent of
  // any replacement that may happen on the capture queue.
  CFRetain(_latestSurface);
  IOSurfaceIncrementUseCount(_latestSurface);

  IOSurfaceLock(_latestSurface, kIOSurfaceLockReadOnly, nullptr);
  *outWidth = _surfaceWidth;
  *outHeight = _surfaceHeight;
  return _latestSurface;
}

@end

#endif // HAS_SCREENCAPTUREKIT

namespace Gfx::WindowCapture
{

class MacWindowCaptureBackend final : public WindowCaptureBackend
{
public:
  MacWindowCaptureBackend() = default;

  ~MacWindowCaptureBackend() override
  {
    stop();
  }

  bool available() const override
  {
#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
      return true;
#endif
    return false;
  }

  std::vector<CapturableWindow> enumerate() override
  {
    std::vector<CapturableWindow> result;

#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      dispatch_semaphore_t sem = dispatch_semaphore_create(0);
      __block std::vector<CapturableWindow> windows;

      [SCShareableContent
          getShareableContentExcludingDesktopWindows:YES
                                onScreenWindowsOnly:YES
                                  completionHandler:^(
                                      SCShareableContent* content, NSError* error) {
                                    if(error)
                                    {
                                      qWarning()
                                          << "ScreenCaptureKit: enumeration error:"
                                          << error.localizedDescription.UTF8String;
                                      qWarning()
                                          << "Ensure screen recording permission is "
                                             "granted in System Settings > Privacy "
                                             "& Security > Screen Recording.";
                                    }
                                    if(!error && content)
                                    {
                                      for(SCWindow* win in content.windows)
                                      {
                                        if(win.title.length == 0)
                                          continue;
                                        std::string title
                                            = [win.title UTF8String] ?: "";
                                        if(win.owningApplication
                                               .applicationName.length
                                           > 0)
                                        {
                                          title += " (";
                                          title += [win.owningApplication
                                                        .applicationName
                                              UTF8String];
                                          title += ")";
                                        }
                                        windows.push_back(
                                            {std::move(title), win.windowID});
                                      }
                                    }
                                    dispatch_semaphore_signal(sem);
                                  }];

      dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
      result = std::move(windows);
    }
#endif

    return result;
  }

  bool start(uint64_t windowId) override
  {
#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      // Stop any existing capture first
      stop();

      m_windowId = windowId;

      // Enumerate shareable content to find the matching SCWindow
      dispatch_semaphore_t sem = dispatch_semaphore_create(0);
      __block SCWindow* targetWindow = nil;

      [SCShareableContent
          getShareableContentExcludingDesktopWindows:YES
                                onScreenWindowsOnly:NO
                                  completionHandler:^(
                                      SCShareableContent* content, NSError* error) {
                                    if(!error && content)
                                    {
                                      for(SCWindow* win in content.windows)
                                      {
                                        if(win.windowID == windowId)
                                        {
                                          targetWindow = [win retain];
                                          break;
                                        }
                                      }
                                    }
                                    else if(error)
                                    {
                                      qWarning()
                                          << "ScreenCaptureKit: cannot get shareable "
                                             "content:"
                                          << error.localizedDescription.UTF8String;
                                    }
                                    dispatch_semaphore_signal(sem);
                                  }];

      dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

      if(!targetWindow)
      {
        qWarning() << "ScreenCaptureKit: window" << windowId << "not found";
        return false;
      }

      // Create content filter for the specific window
      SCContentFilter* filter = [[SCContentFilter alloc]
          initWithDesktopIndependentWindow:targetWindow];

      // Get window dimensions for the stream configuration
      CGRect frame = targetWindow.frame;
      int captureWidth = (int)CGRectGetWidth(frame);
      int captureHeight = (int)CGRectGetHeight(frame);
      if(captureWidth <= 0)
        captureWidth = 1920;
      if(captureHeight <= 0)
        captureHeight = 1080;

      // Create stream configuration
      SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
      config.width = captureWidth;
      config.height = captureHeight;
      config.pixelFormat = kCVPixelFormatType_32BGRA;
      config.showsCursor = YES;
      config.minimumFrameInterval = CMTimeMake(1, 60); // 60 fps max

      // Disable audio capture
      if(@available(macOS 13.0, *))
      {
        // capturesAudio is available from macOS 13
        config.capturesAudio = NO;
      }

      // Create the delegate and dispatch queue
      m_delegate = [[WindowCaptureDelegate alloc] init];
      m_captureQueue = dispatch_queue_create(
          "org.ossia.score.windowcapture", DISPATCH_QUEUE_SERIAL);

      // Create the stream
      m_stream = [[SCStream alloc] initWithFilter:filter
                                    configuration:config
                                         delegate:nil];

      // Add ourselves as output
      NSError* addOutputError = nil;
      BOOL added = [m_stream addStreamOutput:m_delegate
                                        type:SCStreamOutputTypeScreen
                              sampleHandlerQueue:m_captureQueue
                                       error:&addOutputError];
      if(!added || addOutputError)
      {
        qWarning() << "ScreenCaptureKit: failed to add stream output:"
                   << (addOutputError
                           ? addOutputError.localizedDescription.UTF8String
                           : "unknown error");
        [filter release];
        [config release];
        [targetWindow release];
        cleanup();
        return false;
      }

      // Start capture (synchronous wait for the async completion)
      dispatch_semaphore_t startSem = dispatch_semaphore_create(0);
      __block BOOL startSuccess = NO;
      __block NSError* startError = nil;

      [m_stream startCaptureWithCompletionHandler:^(NSError* error) {
        if(error)
        {
          startError = [error retain];
          startSuccess = NO;
        }
        else
        {
          startSuccess = YES;
        }
        dispatch_semaphore_signal(startSem);
      }];

      dispatch_semaphore_wait(
          startSem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

      [filter release];
      [config release];
      [targetWindow release];

      if(!startSuccess)
      {
        qWarning() << "ScreenCaptureKit: failed to start capture:"
                   << (startError
                           ? startError.localizedDescription.UTF8String
                           : "timed out");
        if(startError)
          [startError release];
        cleanup();
        return false;
      }

      m_capturing = true;
      return true;
    }
#endif
    return false;
  }

  void stop() override
  {
#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      if(!m_capturing)
        return;

      if(m_stream)
      {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [m_stream stopCaptureWithCompletionHandler:^(NSError* error) {
          if(error)
          {
            qWarning() << "ScreenCaptureKit: error stopping capture:"
                       << error.localizedDescription.UTF8String;
          }
          dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(
            sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
      }

      cleanup();
      m_capturing = false;
    }
#endif
  }

  CapturedFrame grab() override
  {
    CapturedFrame frame{};

#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      if(!m_delegate || !m_capturing)
        return frame;

      // Release the surface from the previous grab() call.
      // The renderer has already consumed the pixel data by now.
      releasePreviousSurface();

      int w = 0, h = 0;
      IOSurfaceRef surface = [m_delegate copyAndLockSurface:&w height:&h];
      if(!surface || w <= 0 || h <= 0)
      {
        if(surface)
        {
          IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
          IOSurfaceDecrementUseCount(surface);
          CFRelease(surface);
        }
        return frame;
      }

      // CPU path: read pixels directly from the locked IOSurface
      const uint8_t* baseAddr
          = (const uint8_t*)IOSurfaceGetBaseAddress(surface);
      int stride = (int)IOSurfaceGetBytesPerRow(surface);

      if(baseAddr && stride > 0)
      {
        frame.type = CapturedFrame::CPU_BGRA;
        frame.data = baseAddr;
        frame.stride = stride;
        frame.width = w;
        frame.height = h;

        // Also expose the IOSurface for potential Metal zero-copy path
        frame.nativeHandle = (void*)surface;

        // IMPORTANT: The surface is left locked (read-only) here.
        // The caller (the renderer) uses the data synchronously in
        // update(), which uploads it to a QRhiTexture via
        // QRhiResourceUpdateBatch::uploadTexture(). We store the
        // surface so we can unlock+release it on the next grab() call.
        m_grabbedSurface = surface;
      }
      else
      {
        IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
        IOSurfaceDecrementUseCount(surface);
        CFRelease(surface);
      }
    }
#endif

    return frame;
  }

private:
  void cleanup()
  {
#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      // Release any surface that is still held from a previous grab()
      releasePreviousSurface();

      if(m_delegate)
      {
        [m_delegate invalidate];
        [m_delegate release];
        m_delegate = nil;
      }
      if(m_stream)
      {
        [m_stream release];
        m_stream = nil;
      }
      m_captureQueue = nil;
    }
#endif
  }

  void releasePreviousSurface()
  {
#if HAS_SCREENCAPTUREKIT
    if(m_grabbedSurface)
    {
      IOSurfaceUnlock(m_grabbedSurface, kIOSurfaceLockReadOnly, nullptr);
      IOSurfaceDecrementUseCount(m_grabbedSurface);
      CFRelease(m_grabbedSurface);
      m_grabbedSurface = nullptr;
    }
#endif
  }

  uint64_t m_windowId{};
  bool m_capturing{false};

#if HAS_SCREENCAPTUREKIT
  SCStream* m_stream API_AVAILABLE(macos(13.0)) = nil;
  WindowCaptureDelegate* m_delegate API_AVAILABLE(macos(13.0)) = nil;
  dispatch_queue_t m_captureQueue = nil;
  IOSurfaceRef m_grabbedSurface = nullptr;
#endif
};

std::unique_ptr<WindowCaptureBackend> createWindowCaptureBackend()
{
  auto backend = std::make_unique<MacWindowCaptureBackend>();
  if(backend->available())
    return backend;
  return nullptr;
}

}

#endif

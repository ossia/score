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

  bool supportsMode(CaptureMode mode) const override
  {
    switch(mode)
    {
      case CaptureMode::Window:
      case CaptureMode::SingleScreen:
      case CaptureMode::Region:
        return true;
      case CaptureMode::AllScreens:
        return false;
    }
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

  std::vector<CapturableScreen> enumerateScreens() override
  {
    std::vector<CapturableScreen> result;

#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      dispatch_semaphore_t sem = dispatch_semaphore_create(0);
      __block std::vector<CapturableScreen> screens;

      [SCShareableContent
          getShareableContentExcludingDesktopWindows:NO
                                onScreenWindowsOnly:NO
                                  completionHandler:^(
                                      SCShareableContent* content, NSError* error) {
                                    if(!error && content)
                                    {
                                      for(SCDisplay* display in content.displays)
                                      {
                                        CGDirectDisplayID displayID = display.displayID;
                                        CGRect bounds = CGDisplayBounds(displayID);

                                        // Try to get a meaningful display name
                                        std::string name = "Display "
                                            + std::to_string(displayID);
                                        if(CGDisplayIsMain(displayID))
                                          name += " (Main)";

                                        CapturableScreen screen;
                                        screen.name = std::move(name);
                                        screen.id = displayID;
                                        screen.x = (int)bounds.origin.x;
                                        screen.y = (int)bounds.origin.y;
                                        screen.width = (int)bounds.size.width;
                                        screen.height = (int)bounds.size.height;
                                        screens.push_back(std::move(screen));
                                      }
                                    }
                                    dispatch_semaphore_signal(sem);
                                  }];

      dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
      result = std::move(screens);
    }
#endif

    return result;
  }

  bool start(const CaptureTarget& target) override
  {
#if HAS_SCREENCAPTUREKIT
    if(@available(macOS 13.0, *))
    {
      // Stop any existing capture first
      stop();

      switch(target.mode)
      {
        case CaptureMode::Window:
          return startWindow(target.windowId);
        case CaptureMode::SingleScreen:
          return startScreen(target.screenId, CGRectNull);
        case CaptureMode::Region:
        {
          // Find the display containing the region and set sourceRect
          CGRect region = CGRectMake(
              target.regionX, target.regionY,
              target.regionW, target.regionH);

          // Find which display contains the region's origin
          CGDirectDisplayID displayID = CGMainDisplayID();
          uint32_t displayCount = 0;
          CGDirectDisplayID matchingDisplays[16];
          if(CGGetDisplaysWithRect(region, 16, matchingDisplays, &displayCount)
                 == kCGErrorSuccess
             && displayCount > 0)
          {
            displayID = matchingDisplays[0];
          }

          // Convert to display-local coordinates
          CGRect displayBounds = CGDisplayBounds(displayID);
          CGRect sourceRect = CGRectMake(
              target.regionX - displayBounds.origin.x,
              target.regionY - displayBounds.origin.y,
              target.regionW, target.regionH);

          return startScreen(displayID, sourceRect);
        }
        case CaptureMode::AllScreens:
          return false;
      }
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
#if HAS_SCREENCAPTUREKIT
  bool startWindow(uint64_t windowId) API_AVAILABLE(macos(13.0))
  {
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

    bool ok = startCapture(filter, captureWidth, captureHeight, CGRectNull);

    [filter release];
    [targetWindow release];
    return ok;
  }

  bool startScreen(uint64_t displayID, CGRect sourceRect) API_AVAILABLE(macos(13.0))
  {
    // Enumerate shareable content to find the matching SCDisplay
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block SCDisplay* targetDisplay = nil;

    [SCShareableContent
        getShareableContentExcludingDesktopWindows:NO
                              onScreenWindowsOnly:NO
                                completionHandler:^(
                                    SCShareableContent* content, NSError* error) {
                                  if(!error && content)
                                  {
                                    for(SCDisplay* display in content.displays)
                                    {
                                      if(display.displayID == (CGDirectDisplayID)displayID)
                                      {
                                        targetDisplay = [display retain];
                                        break;
                                      }
                                    }
                                  }
                                  dispatch_semaphore_signal(sem);
                                }];

    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

    if(!targetDisplay)
    {
      qWarning() << "ScreenCaptureKit: display" << displayID << "not found";
      return false;
    }

    // Create content filter for the display (excluding no windows)
    SCContentFilter* filter = [[SCContentFilter alloc]
        initWithDisplay:targetDisplay
        excludingWindows:@[]];

    int captureWidth, captureHeight;
    if(!CGRectIsNull(sourceRect))
    {
      captureWidth = (int)sourceRect.size.width;
      captureHeight = (int)sourceRect.size.height;
    }
    else
    {
      CGRect displayBounds = CGDisplayBounds(targetDisplay.displayID);
      captureWidth = (int)displayBounds.size.width;
      captureHeight = (int)displayBounds.size.height;
    }

    if(captureWidth <= 0) captureWidth = 1920;
    if(captureHeight <= 0) captureHeight = 1080;

    bool ok = startCapture(filter, captureWidth, captureHeight, sourceRect);

    [filter release];
    [targetDisplay release];
    return ok;
  }

  bool startCapture(
      SCContentFilter* filter, int captureWidth, int captureHeight,
      CGRect sourceRect) API_AVAILABLE(macos(13.0))
  {
    // Create stream configuration
    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    config.width = captureWidth;
    config.height = captureHeight;
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = YES;
    config.minimumFrameInterval = CMTimeMake(1, 60); // 60 fps max

    // Set source rect for region capture
    if(!CGRectIsNull(sourceRect))
    {
      config.sourceRect = sourceRect;
      config.destinationRect = CGRectMake(0, 0, captureWidth, captureHeight);
      config.scalesToFit = YES;
    }

    // Disable audio capture
    if(@available(macOS 13.0, *))
    {
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
      [config release];
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

    [config release];

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

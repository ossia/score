#include "HWVideoToolbox_metal.hpp"

#if defined(__APPLE__)

#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>

namespace score::gfx
{

void* createMetalTextureCache(void* mtlDevice)
{
  if(!mtlDevice)
    return nullptr;

  id<MTLDevice> dev = (__bridge id<MTLDevice>)mtlDevice;
  CVMetalTextureCacheRef cache = nullptr;
  CVReturn ret = CVMetalTextureCacheCreate(
      kCFAllocatorDefault, nullptr, dev, nullptr, &cache);
  if(ret != kCVReturnSuccess || !cache)
    return nullptr;

  return (void*)cache; // caller manages lifetime
}

void releaseMetalTextureCache(void* cache)
{
  if(cache)
    CFRelease((CVMetalTextureCacheRef)cache);
}

MetalTextureFromPixelBuffer createMetalTextureFromPixelBuffer(
    void* cache, CVPixelBufferRef pixbuf, unsigned planeIndex,
    unsigned pixelFormat)
{
  MetalTextureFromPixelBuffer result{};
  if(!cache || !pixbuf)
    return result;

  // For non-planar buffers (e.g. AYUV), plane functions return 0.
  // Use the full buffer dimensions with planeIndex=0 instead.
  size_t planeCount = CVPixelBufferGetPlaneCount(pixbuf);
  size_t w, h;
  if(planeCount == 0)
  {
    w = CVPixelBufferGetWidth(pixbuf);
    h = CVPixelBufferGetHeight(pixbuf);
  }
  else
  {
    w = CVPixelBufferGetWidthOfPlane(pixbuf, planeIndex);
    h = CVPixelBufferGetHeightOfPlane(pixbuf, planeIndex);
  }
  if(w == 0 || h == 0)
    return result;

  CVMetalTextureRef cvTex = nullptr;
  CVReturn ret = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault,
      (CVMetalTextureCacheRef)cache,
      pixbuf,
      nullptr, // texture attributes
      (MTLPixelFormat)pixelFormat,
      w, h,
      planeIndex,
      &cvTex);

  if(ret != kCVReturnSuccess || !cvTex)
    return result;

  id<MTLTexture> mtlTex = CVMetalTextureGetTexture(cvTex);
  if(!mtlTex)
  {
    CFRelease(cvTex);
    return result;
  }

  result.cvMetalTexture = (void*)cvTex; // retained
  result.mtlTexture = (__bridge void*)mtlTex; // borrowed, valid while cvTex alive
  result.width = w;
  result.height = h;
  return result;
}

void releaseMetalTextureRef(void* cvMetalTexture)
{
  if(cvMetalTexture)
    CFRelease((CVMetalTextureRef)cvMetalTexture);
}

size_t getPixelBufferPlaneCount(CVPixelBufferRef pixbuf)
{
  if(!pixbuf)
    return 0;
  return CVPixelBufferGetPlaneCount(pixbuf);
}

} // namespace score::gfx

#endif // __APPLE__

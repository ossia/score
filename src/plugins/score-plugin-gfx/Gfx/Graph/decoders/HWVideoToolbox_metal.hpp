#pragma once
#if defined(__APPLE__)

#include <cstddef>
#include <cstdint>

typedef struct __CVBuffer* CVPixelBufferRef;

namespace score::gfx
{

/// Opaque handle for CVMetalTextureCache (id<MTLDevice> → cache).
void* createMetalTextureCache(void* mtlDevice);
void releaseMetalTextureCache(void* cache);

/// Result of creating a Metal texture from a CVPixelBuffer plane.
struct MetalTextureFromPixelBuffer
{
  void* cvMetalTexture; // CVMetalTextureRef, retained. Must release via releaseMetalTextureRef().
  void* mtlTexture;     // id<MTLTexture>, borrowed from cvMetalTexture. Valid while cvMetalTexture is alive.
  size_t width;
  size_t height;
};

/// Create a Metal texture from a CVPixelBuffer plane via CVMetalTextureCache.
/// cache: CVMetalTextureCacheRef. pixbuf: CVPixelBufferRef.
/// pixelFormat: MTLPixelFormat for this plane.
/// planeIndex: 0 for Y, 1 for UV.
/// Returns null mtlTexture on failure.
MetalTextureFromPixelBuffer createMetalTextureFromPixelBuffer(
    void* cache, CVPixelBufferRef pixbuf, unsigned planeIndex,
    unsigned pixelFormat);

/// Release a CVMetalTextureRef obtained from createMetalTextureFromPixelBuffer.
void releaseMetalTextureRef(void* cvMetalTexture);

/// Get plane count from CVPixelBuffer.
size_t getPixelBufferPlaneCount(CVPixelBufferRef pixbuf);

} // namespace score::gfx

#endif // __APPLE__

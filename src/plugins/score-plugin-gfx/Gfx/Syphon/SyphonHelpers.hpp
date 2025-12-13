#pragma once
#include <QObject>
#include <QOpenGLContext>

#include <AppKit/AppKit.h>

#if __has_include(<Metal/Metal.h>)
#include <Metal/Metal.h>
#endif

class QRhi;
class QRhiCommandBuffer;

namespace Gfx
{
// OpenGL helpers
CGLContextObj nativeContext(QRhi& rhi);

// Metal helpers
#if __has_include(<Metal/Metal.h>)
id<MTLDevice> nativeMetalDevice(QRhi& rhi);
id<MTLCommandBuffer> nativeMetalCommandBuffer(QRhiCommandBuffer& cb);
#endif
}

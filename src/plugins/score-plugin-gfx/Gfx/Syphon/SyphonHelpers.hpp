#pragma once
#include <QObject>
#include <QOpenGLContext>

#include <AppKit/AppKit.h>
class QRhi;
namespace Gfx
{
CGLContextObj nativeContext(QRhi& rhi);
}

#pragma once
#include <QObject>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtPlatformHeaders/QCocoaNativeContext>
#else
#include <QOpenGLContext>
#include <AppKit/AppKit.h>
#endif
class QRhi;
namespace Gfx
{
CGLContextObj nativeContext(QRhi& rhi);
}


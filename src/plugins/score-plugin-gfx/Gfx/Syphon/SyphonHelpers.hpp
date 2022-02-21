#pragma once
#include <QtPlatformHeaders/QCocoaNativeContext>
class QRhi;
namespace Gfx
{
CGLContextObj nativeContext(QRhi& rhi);
}


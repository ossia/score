#pragma once
#include <qobjectdefs.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <score/tools/DeleteAll.hpp>
#include <deque>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qshader_p.h>
#include <QtGui/private/qrhinull_p.h>

#ifndef QT_NO_OPENGL
#include <QtGui/private/qrhigles2_p.h>
#endif

#if QT_CONFIG(vulkan)
#include <QtGui/private/qrhivulkan_p.h>
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif

#ifdef Q_OS_DARWIN
#include <QtGui/private/qrhimetal_p.h>
#endif

#define createOrResize buildOrResize
#define destroy release
#define create build
#define deleteLater releaseAndDestroyLater
#endif

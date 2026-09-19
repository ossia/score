#include "Platforms.hpp"

#include <QCoreApplication>

#include <algorithm>

namespace score
{
QString currentPlatform() noexcept
{
#if defined(__EMSCRIPTEN__)
  return QStringLiteral("web");
#elif defined(_WIN32)
  return QStringLiteral("windows");
#elif defined(__APPLE__)
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
  return QStringLiteral("ios");
#else
  return QStringLiteral("macos");
#endif
#elif defined(__ANDROID__)
  return QStringLiteral("android");
#elif defined(__linux__)
  return QStringLiteral("linux");
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  return QStringLiteral("bsd");
#else
  return {};
#endif
}

const QStringList& knownPlatforms() noexcept
{
  static const QStringList platforms{"windows", "macos", "linux",
                                     "bsd",     "android", "ios", "web"};
  return platforms;
}

QString platformName(const QString& token) noexcept
{
  if(token == QLatin1String{"windows"})
    return QCoreApplication::translate("Platforms", "Windows");
  if(token == QLatin1String{"macos"})
    return QCoreApplication::translate("Platforms", "macOS");
  if(token == QLatin1String{"linux"})
    return QCoreApplication::translate("Platforms", "Linux");
  if(token == QLatin1String{"bsd"})
    return QCoreApplication::translate("Platforms", "BSD");
  if(token == QLatin1String{"android"})
    return QCoreApplication::translate("Platforms", "Android");
  if(token == QLatin1String{"ios"})
    return QCoreApplication::translate("Platforms", "iOS");
  if(token == QLatin1String{"web"})
    return QCoreApplication::translate("Platforms", "Web");
  return token;
}

bool runsOnThisPlatform(const QString& platforms) noexcept
{
  const auto list = platforms.split(' ', Qt::SkipEmptyParts);
  if(list.isEmpty())
    return true;

  const auto self = currentPlatform();
  if(self.isEmpty())
    return true;

  return std::any_of(list.begin(), list.end(), [&](const QString& p) {
    return p.compare(self, Qt::CaseInsensitive) == 0;
  });
}
}

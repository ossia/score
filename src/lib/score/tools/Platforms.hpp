#pragma once
#include <QString>
#include <QStringList>

#include <score_lib_base_export.h>

namespace score
{
//! "windows", "macos", "linux", "bsd", "android", "ios" or "web".
SCORE_LIB_BASE_EXPORT
QString currentPlatform() noexcept;

SCORE_LIB_BASE_EXPORT
const QStringList& knownPlatforms() noexcept;

SCORE_LIB_BASE_EXPORT
QString platformName(const QString& token) noexcept;

//! @p platforms is a space-separated whitelist; empty means everywhere.
SCORE_LIB_BASE_EXPORT
bool runsOnThisPlatform(const QString& platforms) noexcept;
}

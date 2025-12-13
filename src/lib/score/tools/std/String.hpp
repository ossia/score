#pragma once
#include <QDebug>
#include <QString>

#include <score_lib_base_export.h>

#include <string>

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
#include <string_view>
SCORE_LIB_BASE_EXPORT
QDebug operator<<(QDebug debug, const std::string& obj);
SCORE_LIB_BASE_EXPORT
QDebug operator<<(QDebug debug, std::string_view obj);
#endif

inline QStringList splitWithoutEmptyParts(const QString& src, const QString& sep)
{
  return src.split(sep, Qt::SkipEmptyParts);
}

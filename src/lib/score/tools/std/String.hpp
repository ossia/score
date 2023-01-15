#pragma once
#include <QDebug>
#include <QString>

#include <score_lib_base_export.h>

#include <string>

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
SCORE_LIB_BASE_EXPORT
QDebug operator<<(QDebug debug, const std::string& obj);
#endif

inline QStringList splitWithoutEmptyParts(const QString& src, const QString& sep)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  return src.split(sep, Qt::SkipEmptyParts);
#else
  QStringList str = src.split(sep);
  str.removeAll(QString{});
  return str;
#endif
}

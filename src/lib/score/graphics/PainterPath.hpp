#pragma once
#include <QPainterPath>

#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
inline void clearPainterPath(QPainterPath& p) noexcept { p = QPainterPath{}; }
#else
inline void clearPainterPath(QPainterPath& p) noexcept { p.clear(); }
#endif

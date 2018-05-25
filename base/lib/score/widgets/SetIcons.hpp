#pragma once
#include <QAction>
#include <QIcon>
#include <score_lib_base_export.h>

SCORE_LIB_BASE_EXPORT void
setIcons(QAction* action, const QString& iconOn, const QString& iconOff, bool enableHover = true);

SCORE_LIB_BASE_EXPORT QIcon
makeIcons(const QString& iconOn, const QString& iconOff);

SCORE_LIB_BASE_EXPORT QIcon
genIconFromPixmaps(const QString& iconOn, const QString& iconOff);

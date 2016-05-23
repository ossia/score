#pragma once
#include <QAction>
#include <QIcon>
#include <iscore_lib_base_export.h>

ISCORE_LIB_BASE_EXPORT void setIcons(QAction* action,
              const QString& iconOn,
              const QString& iconOff);

ISCORE_LIB_BASE_EXPORT void makeIcons(QIcon* icon,
                                      const QString &iconOn,
                                      const QString &iconOff);

ISCORE_LIB_BASE_EXPORT QIcon genIconFromPixmaps(const QString &iconOn,
                                      const QString &iconOff);

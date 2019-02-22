#pragma once
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QPixmap>
#include <QScreen>

#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
QImage get_image(QString str);
SCORE_LIB_BASE_EXPORT
QPixmap get_pixmap(QString str);
}

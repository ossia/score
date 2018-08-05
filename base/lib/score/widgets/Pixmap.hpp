#pragma once
#include <QApplication>
#include <QPixmap>
#include <QFile>
#include <QScreen>
#include <QDebug>
#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
QImage get_image(QString str);
SCORE_LIB_BASE_EXPORT
QPixmap get_pixmap(QString str);
}

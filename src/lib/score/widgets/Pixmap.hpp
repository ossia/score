#pragma once
#include <QPixmap>

#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
QImage get_image(QString str, QString svg = {});
SCORE_LIB_BASE_EXPORT
QPixmap get_pixmap(QString str, QString svg = {});
SCORE_LIB_BASE_EXPORT
QCursor get_cursor(QString str, double hotspot_x, double hostpot_y);
}

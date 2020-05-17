#pragma once

#include <score_lib_base_export.h>

namespace score
{

SCORE_LIB_BASE_EXPORT int question(
    QWidget* parent,
    const QString& title,
    const QString& text);

SCORE_LIB_BASE_EXPORT int information(
    QWidget* parent,
    const QString& title,
    const QString& text);

SCORE_LIB_BASE_EXPORT int warning(
    QWidget* parent,
    const QString& title,
    const QString& text);

}

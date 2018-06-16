#pragma once
#include <QDebug>
#include <string>
#include <score_lib_base_export.h>

SCORE_LIB_BASE_EXPORT
QDebug operator<<(QDebug debug, const std::string& obj);


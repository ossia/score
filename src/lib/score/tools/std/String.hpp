#pragma once
#include <QDebug>

#include <score_lib_base_export.h>

#include <string>

SCORE_LIB_BASE_EXPORT
QDebug operator<<(QDebug debug, const std::string& obj);

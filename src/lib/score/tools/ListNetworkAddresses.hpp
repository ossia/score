#pragma once
#include <QStringList>

#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
QStringList list_ipv4_for_connecting() noexcept;
SCORE_LIB_BASE_EXPORT
QStringList list_ipv4_for_listening() noexcept;
}

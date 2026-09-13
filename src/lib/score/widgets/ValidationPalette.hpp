#pragma once
#include <score_lib_base_export.h>

class QWidget;

namespace score
{
enum class InputValidity
{
  Valid,   //!< No tint: the widget inherits the application palette.
  Unknown, //!< Well-formed but does not resolve to anything.
  Invalid
};

/**
 * @brief Tint a widget to flag what it holds, and keep the tint skin-aware.
 *
 * Sets only the roles the tint needs, so the widget keeps inheriting the rest.
 */
SCORE_LIB_BASE_EXPORT void setInputValidity(QWidget& w, InputValidity v);
}

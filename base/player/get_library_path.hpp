#pragma once
#include <ossia/detail/string_view.hpp>
#include <iscore_player_export.h>
namespace iscore
{
/**
 * \brief Returns the path to a library whose name contains name_part
 *
 * e.g. the path to "i-score.pd_linux" for "i-score.p"
 *
 * Currently implemented only for linux and macOS
 */
ISCORE_PLAYER_EXPORT std::string get_library_path(ossia::string_view name_part);
}

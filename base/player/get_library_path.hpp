#pragma once
#include <ossia/detail/string_view.hpp>

#include <score_player_export.h>

#include <string>
namespace score
{
/**
 * \brief Returns the path to a library whose name contains name_part
 *
 * e.g. the path to "score.pd_linux" for "score.p"
 *
 * Currently implemented only for linux and macOS
 */
SCORE_PLAYER_EXPORT std::string get_library_path(ossia::string_view name_part);
}

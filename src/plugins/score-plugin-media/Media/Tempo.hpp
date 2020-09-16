#pragma once
#include <Process/Process.hpp>
#include <score_plugin_media_export.h>
#include <optional>

namespace Media
{
SCORE_PLUGIN_MEDIA_EXPORT
double tempoAtStartDate(const Process::ProcessModel& m) noexcept;
}

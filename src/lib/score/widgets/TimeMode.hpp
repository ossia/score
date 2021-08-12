#pragma once
#include <score_lib_base_export.h>
namespace score
{

enum TimeMode
{
  Bars,
  Seconds,
  Flicks
};

SCORE_LIB_BASE_EXPORT
void setGlobalTimeMode(TimeMode);
}

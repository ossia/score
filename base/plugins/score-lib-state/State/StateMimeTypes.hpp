#pragma once

// This file defines the mime types that are used for remote communication.
// The actual data is JSON as encoded by the score visitor classes.
namespace score
{
namespace mime
{
inline constexpr const char* state()
{
  return "application/x-score-state";
}
}
}

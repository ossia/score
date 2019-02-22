#pragma once

namespace score
{
namespace mime
{
// TODO give a definition of what's expected to be in each mime type somewhere.
inline constexpr const char* device()
{
  return "application/x-score-device";
}
inline constexpr const char* xml_namespace()
{
  return "application/x-score-xml-namespace";
}
inline constexpr const char* address()
{
  return "application/x-score-address";
}
}
}

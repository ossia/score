#include <score/tools/std/String.hpp>

QDebug operator<<(QDebug debug, const std::string& obj)
{
  debug << obj.c_str();
  return debug;
}

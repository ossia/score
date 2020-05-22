#pragma once
#include <ossia/detail/optional.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct OSCQuerySpecificSettings
{
  QString host;
  std::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::OSCQuerySpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCQuerySpecificSettings)

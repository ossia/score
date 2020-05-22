#pragma once
#include <ossia/detail/optional.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct OSCSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  std::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCSpecificSettings)

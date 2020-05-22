#pragma once
#include <ossia/detail/optional.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct MinuitSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  QString localName;
  std::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::MinuitSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MinuitSpecificSettings)

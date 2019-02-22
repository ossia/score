#pragma once
#include <QMetaType>
#include <QString>
#include <ossia/detail/optional.hpp>
#include <wobjectdefs.h>

namespace Protocols
{
struct OSCSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  ossia::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCSpecificSettings)

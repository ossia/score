#pragma once
#include <QMetaType>
#include <QString>
#include <ossia/detail/optional.hpp>
#include <wobjectdefs.h>


namespace Protocols
{
struct MinuitSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  QString localName;
  ossia::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::MinuitSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MinuitSpecificSettings)

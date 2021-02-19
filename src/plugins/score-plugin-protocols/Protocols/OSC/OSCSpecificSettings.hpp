#pragma once
#include <ossia/detail/optional.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct OSCSpecificSettings
{
  int deviceListeningPort{};
  int scoreListeningPort{};
  QString host;
  std::optional<int> rate{};

  // Note: this one is not saved, it is only used
  // to allow loading a .json file as an OSC device
  QByteArray jsonToLoad;
};
}
Q_DECLARE_METATYPE(Protocols::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCSpecificSettings)

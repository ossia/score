#pragma once

#include <Protocols/Bitfocus/BitfocusProtocolFactory.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

namespace Protocols
{
class BitfocusEnumerator final : public SubfolderDeviceEnumerator
{
public:
  explicit BitfocusEnumerator(const score::DocumentContext& ctx);

  std::pair<QString, QVariant> loadSettings(const QString& path);
  ~BitfocusEnumerator();
};
}

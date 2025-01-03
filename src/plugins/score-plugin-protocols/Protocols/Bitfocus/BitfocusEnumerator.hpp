#pragma once

#include <Protocols/Bitfocus/BitfocusProtocolFactory.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

namespace Protocols
{
class BitfocusEnumerator final : public SubfolderDeviceEnumerator
{
public:
  explicit BitfocusEnumerator(QString path, const score::DocumentContext& ctx);

  ret_type loadSettings(const QString& path);
  ~BitfocusEnumerator();
};
}

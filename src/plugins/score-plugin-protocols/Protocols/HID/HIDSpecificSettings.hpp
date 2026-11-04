#pragma once
#include <QString>

#include <verdigris>

namespace Protocols
{
struct HIDSpecificSettings
{
  QString path;
  QString serial;
  QString product;
  int vendorId{};
  int productId{};
  int interfaceNumber{-1};
};
}

Q_DECLARE_METATYPE(Protocols::HIDSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::HIDSpecificSettings)

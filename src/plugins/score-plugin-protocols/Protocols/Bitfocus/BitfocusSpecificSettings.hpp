#pragma once
#include <ossia/detail/optional.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct BitfocusSpecificSettings
{
  QString path;
  QString id;
  QString name;
};
}
Q_DECLARE_METATYPE(Protocols::BitfocusSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::BitfocusSpecificSettings)

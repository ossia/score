#pragma once
#include <ossia/detail/optional.hpp>
#include <ossia/network/value/value.hpp>

#include <QString>

#include <memory>
#include <verdigris>

namespace bitfocus
{
struct module_handler;
}

namespace Protocols
{
struct BitfocusSpecificSettings
{
  QString path;
  QString entrypoint;
  QString id;
  QString name;
  QString brand;
  QString product;
  QString nodeVersion;
  QString apiVersion;

  std::vector<std::pair<QString, ossia::value>> configuration;

  QString description;
  std::shared_ptr<bitfocus::module_handler> handler;

  //! Keeps the first entry for each key.
  void deduplicateConfiguration();

  QString enumeratorLabel() const;
};
}
Q_DECLARE_METATYPE(Protocols::BitfocusSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::BitfocusSpecificSettings)

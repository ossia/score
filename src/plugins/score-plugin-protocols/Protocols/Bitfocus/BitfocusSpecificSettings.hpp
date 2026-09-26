#pragma once
#include <ossia/detail/optional.hpp>
#include <ossia/network/value/value.hpp>

#include <QString>
#include <QVariant>

#include <map>
#include <memory>
#include <optional>
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
  //! Last upgrade script the configuration went through
  std::optional<int> upgradeIndex;
  //! The keys of the configuration which go to the module as secrets
  std::optional<std::vector<QString>> secretKeys;

  QString description;
  std::shared_ptr<bitfocus::module_handler> handler;

  //! Keeps the first entry for each key.
  void deduplicateConfiguration();

  QString enumeratorLabel() const;

  //! The configuration as sent to the module
  std::map<QString, QVariant> moduleConfiguration() const;

  //! Starts the module process
  std::shared_ptr<bitfocus::module_handler> makeHandler(const QString& label) const;
};
}
Q_DECLARE_METATYPE(Protocols::BitfocusSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::BitfocusSpecificSettings)

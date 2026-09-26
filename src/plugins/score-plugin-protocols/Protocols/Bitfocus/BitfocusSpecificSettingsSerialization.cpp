// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusSpecificSettings.hpp"

#include "BitfocusContext.hpp"

#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Protocols/NetworkWidgets/Serialization.hpp>

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/FilePath.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <set>
#include <vector>

QString Protocols::BitfocusSpecificSettings::enumeratorLabel() const
{
  const QString& prod = product.isEmpty() ? name : product;
  return brand.isEmpty() ? prod : brand + ": " + prod;
}

void Protocols::BitfocusSpecificSettings::deduplicateConfiguration()
{
  std::set<QString> seen;
  std::erase_if(configuration, [&seen](const auto& kv) {
    return !seen.insert(kv.first).second;
  });
}

std::map<QString, QVariant>
Protocols::BitfocusSpecificSettings::moduleConfiguration() const
{
  std::map<QString, QVariant> conf;
  if(!product.isEmpty())
    conf["product"] = product;
  for(auto& [k, v] : configuration)
    conf[k] = bitfocus::widenFloat(v.apply(ossia::qt::ossia_to_qvariant{}));
  return conf;
}

std::shared_ptr<bitfocus::module_handler>
Protocols::BitfocusSpecificSettings::makeHandler(const QString& label) const
{
  return std::make_shared<bitfocus::module_handler>(
      path, entrypoint, nodeVersion, apiVersion, moduleConfiguration(), label,
      configuration.empty(), upgradeIndex);
}

template <>
void DataStreamReader::read(const Protocols::BitfocusSpecificSettings& n)
{
  m_stream << score::relativizeFilePath(n.path) << n.entrypoint << n.id << n.name
           << n.brand << n.product << n.nodeVersion << n.apiVersion << n.configuration
           << n.description << n.upgradeIndex.has_value() << n.upgradeIndex.value_or(-1);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::BitfocusSpecificSettings& n)
{
  m_stream >> n.path >> n.entrypoint >> n.id >> n.name >> n.brand >> n.product
      >> n.nodeVersion >> n.apiVersion >> n.configuration >> n.description;
  bool hasUpgradeIndex{};
  int upgradeIndex{};
  m_stream >> hasUpgradeIndex >> upgradeIndex;
  if(hasUpgradeIndex)
    n.upgradeIndex = upgradeIndex;
  n.path = score::locateFilePath(n.path);
  n.deduplicateConfiguration();
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::BitfocusSpecificSettings& n)
{
  obj["Path"] = score::relativizeFilePath(n.path);
  obj["Entrypoint"] = n.entrypoint;
  obj["Identifier"] = n.id;
  obj["Name"] = n.name;
  obj["Brand"] = n.brand;
  obj["Product"] = n.product;
  obj["NodeVersion"] = n.nodeVersion;
  obj["APIVersion"] = n.apiVersion;
  obj["Configuration"] = n.configuration;
  obj["Description"] = n.description;
  if(n.upgradeIndex)
    obj["UpgradeIndex"] = *n.upgradeIndex;
}

template <>
void JSONWriter::write(Protocols::BitfocusSpecificSettings& n)
{
  n.path <<= obj["Path"];
  n.path = score::locateFilePath(n.path);
  n.entrypoint <<= obj["Entrypoint"];
  n.id <<= obj["Identifier"];
  n.name <<= obj["Name"];
  n.brand <<= obj["Brand"];
  n.product <<= obj["Product"];
  n.nodeVersion <<= obj["NodeVersion"];
  n.apiVersion <<= obj["APIVersion"];
  n.configuration <<= obj["Configuration"];
  n.description <<= obj["Description"];
  if(auto idx = obj.tryGet("UpgradeIndex"))
    n.upgradeIndex = idx->toInt();
  n.deduplicateConfiguration();
}

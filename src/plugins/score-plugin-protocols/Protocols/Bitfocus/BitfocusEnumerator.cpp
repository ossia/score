#include "BitfocusEnumerator.hpp"

#include <Library/LibrarySettings.hpp>

#include <ossia/detail/json.hpp>
namespace Protocols
{
BitfocusEnumerator::BitfocusEnumerator(const score::DocumentContext& ctx)
    : SubfolderDeviceEnumerator{
          ctx.app.settings<Library::Settings::Model>().getPackagesPath()
              + "/default/Devices/Bitfocus",
          BitfocusProtocolFactory::static_concreteKey(),
          [this](const QString& path) { return loadSettings(path); }, ctx}
{
}

static inline QString json_to_qstr(const rapidjson::Value& other)
{
  auto str = other.GetString();
  auto n = other.GetStringLength();
  return QString::fromUtf8(str, n);
}

std::pair<QString, QVariant> BitfocusEnumerator::loadSettings(const QString& path)
{
  QFile f{path + "/companion/manifest.json"};
  if(f.open(QIODevice::ReadOnly))
  {
    auto n = f.size();
    auto ptr = f.map(0, n);
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(ptr), n);
    if(!doc.HasParseError() && doc.IsObject())
    {
      BitfocusSpecificSettings set{};
      if(auto m_it = doc.FindMember("id");
         m_it != doc.MemberEnd() && m_it->value.IsString())
      {
        set.id = json_to_qstr(m_it->value);
      }
      if(auto m_name = doc.FindMember("shortname");
         m_name != doc.MemberEnd() && m_name->value.IsString())
      {
        set.name = json_to_qstr(m_name->value);
      }
      if(!set.id.isEmpty() && !set.name.isEmpty())
      {
        set.path = path;
        return {set.name, QVariant::fromValue(set)};
      }
    }
  }

  return {};
}

BitfocusEnumerator::~BitfocusEnumerator() { }
}

#include "BitfocusEnumerator.hpp"

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/json.hpp>
namespace Protocols
{
BitfocusEnumerator::BitfocusEnumerator(QString path, const score::DocumentContext& ctx)
    : SubfolderDeviceEnumerator{
          {path, path + "/_legacy"},
          BitfocusProtocolFactory::static_concreteKey(),
          [this](const QString& path) { return loadSettings(path); },
          ctx}
{
}

static inline QString json_to_qstr(const rapidjson::Value& other)
{
  auto str = other.GetString();
  auto n = other.GetStringLength();
  return QString::fromUtf8(str, n);
}

auto BitfocusEnumerator::loadSettings(const QString& path) -> ret_type
{
  using namespace std::string_view_literals;
  ret_type devices;
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
      set.path = path;
      QString name, brand;
      std::vector<QString> products;

      if(auto m_it = doc.FindMember("id");
         m_it != doc.MemberEnd() && m_it->value.IsString())
      {
        set.id = json_to_qstr(m_it->value);
      }
      else
      {
        return {};
      }

      if(auto m_runtime = doc.FindMember("runtime");
         m_runtime != doc.MemberEnd() && m_runtime->value.IsObject())
      {
        auto runtime = m_runtime->value.GetObject();
        if(auto ipc = runtime.FindMember("api");
           ipc != runtime.MemberEnd() && ipc->value.IsString())
        {
          if(ipc->value.GetString() != "nodejs-ipc"sv)
            return {};
        }
        if(auto t = runtime.FindMember("type");
           t != runtime.MemberEnd() && t->value.IsString())
        {
          if(t->value.GetString() == "node18"sv)
            set.nodeVersion = "node18";
          else if(t->value.GetString() == "node22"sv)
            set.nodeVersion = "node22";
          else
            set.nodeVersion = "node22";
        }
        if(auto ver = runtime.FindMember("apiVersion");
           ver != runtime.MemberEnd() && ver->value.IsString())
        {
          set.apiVersion = json_to_qstr(ver->value);
        }
        if(auto ep = runtime.FindMember("entrypoint");
           ep != runtime.MemberEnd() && ep->value.IsString())
        {
          set.entrypoint = json_to_qstr(ep->value);
          set.entrypoint.remove("../");
        }
      }
      else
      {
        return {};
      }
      if(auto m_name = doc.FindMember("shortname");
         m_name != doc.MemberEnd() && m_name->value.IsString())
      {
        name = json_to_qstr(m_name->value);
        set.name = name;
      }
      if(auto m_brand = doc.FindMember("manufacturer");
         m_brand != doc.MemberEnd() && m_brand->value.IsString())
      {
        brand = json_to_qstr(m_brand->value);
        set.brand = brand;
      }
      if(auto m_products = doc.FindMember("products");
         m_products != doc.MemberEnd() && m_products->value.IsArray())
      {
        for(const auto& prod : m_products->value.GetArray())
        {
          if(prod.IsString())
          {
            products.push_back(json_to_qstr(prod));
          }
        }
      }

      if(!brand.isEmpty() && !products.empty())
      {
        for(auto& prod : products)
        {
          QString label = brand + ": " + prod;
          set.product = prod;
          devices.push_back({label, QVariant::fromValue(set)});
        }
      }
      else if(brand.isEmpty() && !products.empty())
      {
        for(auto& prod : products)
        {
          set.product = prod;
          devices.push_back({prod, QVariant::fromValue(set)});
        }
      }
      else if(!brand.isEmpty() && products.empty())
      {
        QString label = brand + ": " + set.name;
        devices.push_back({label, QVariant::fromValue(set)});
      }
      else if(brand.isEmpty() && products.empty())
      {
        devices.push_back({set.name, QVariant::fromValue(set)});
      }
    }
  }

  return devices;
}

BitfocusEnumerator::~BitfocusEnumerator() { }
}

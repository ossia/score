// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TouchOSCDeviceLoader.hpp"

#include <score/tools/File.hpp>
#include <score/tools/std/String.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QDebug>
#include <QDomAttr>
#include <QDomDocument>
#include <QDomEntity>
#include <QFile>

#include <zipdownloader.hpp>

namespace Device
{
namespace
{

struct TouchOscControl
{
  QString type;
  QString text;
  double min{0.};
  double max{0.01};
};

using TouchOSCControlMap = ossia::flat_map<QString, TouchOscControl>;

void handleTouchOSC(TouchOSCControlMap& layout, Device::Node& node)
{
  for(auto& [osc_addr, ctl] : layout)
  {
    auto splitted = ::splitWithoutEmptyParts(osc_addr, "/");
    Device::Node* cur = &node;
    for(int i = 0; i < splitted.size(); i++)
    {
      if(auto it = ossia::find_if(
             node.children(),
             [&](const Device::Node& o) { return o.displayName() == splitted[i]; });
         it != node.children().end())
      {
        cur = const_cast<Device::Node*>(&*it);
      }
      else
      {
        Device::AddressSettings a;
        a.ioType = ossia::access_mode::BI;
        a.name = splitted[i];
        if(ctl.text.isEmpty())
          ossia::net::set_description(a.extendedAttributes, ctl.text.toStdString());
        cur = &cur->emplace_back(a, cur);
      }
    }

    if(ctl.type == "push")
    {
      auto& a = cur->get<Device::AddressSettings>();
      a.value = ossia::impulse{};
    }
    else if(ctl.type == "toggle")
    {
      auto& a = cur->get<Device::AddressSettings>();
      a.value = bool{};
    }
    else if(ctl.type.startsWith("fader"))
    {
      auto& a = cur->get<Device::AddressSettings>();
      a.value = float{0.f};
      a.domain = ossia::make_domain(float(ctl.min), float(ctl.max));
    }
    else if(ctl.type.startsWith("rotary"))
    {
      auto& a = cur->get<Device::AddressSettings>();
      a.value = float{0.f};
      a.domain = ossia::make_domain(float(ctl.min), float(ctl.max));
    }
    else if(ctl.type.startsWith("xy"))
    {
      auto& a = cur->get<Device::AddressSettings>();
      a.value = ossia::vec2f{0.f};
      a.domain = ossia::make_domain(float(ctl.min), float(ctl.max));
    }
    else
    {
      qDebug() << "unhandled" << ctl.type;
    }
  }
}

void addTouchOSCControl(QDomElement& e, TouchOSCControlMap& node)
{
  QString type = e.attribute("type");
  if(type.isEmpty() || type.contains("label"))
    return;

  if(!e.childNodes().isEmpty())
  {
    if(e.childNodes().at(0).nodeName() == "midi")
      return;
  }

  QString osc_cs = [&] {
    QByteArray osc_base = e.attribute("osc_cs").toUtf8();
    QString name = QByteArray::fromBase64(e.attribute("name").toUtf8());
    QString osc_cs
        = osc_base.startsWith("/") ? osc_base : QByteArray::fromBase64(osc_base);

    QString osc_address = osc_cs;
    if(osc_address.isEmpty())
    {
      auto parent = e.parentNode();
      if(parent.nodeName() == "tabpage")
      {
        auto parent_e = parent.toElement();
        auto parent_name = QByteArray::fromBase64(parent_e.attribute("name").toUtf8());

        osc_address = "/" + parent_name + "/" + name;
      }
    }
    for(const QChar& c : osc_address)
    {
      if(c == '/')
        continue;
      else if(!ossia::net::is_valid_character_for_name(c))
        return QString{};
    }
    return osc_address;
  }();

  if(osc_cs.isEmpty())
    return;

  QString text = QByteArray::fromBase64(e.attribute("text").toUtf8());

  QString scalef = e.attribute("scalef");
  QString scalet = e.attribute("scalet");

  auto it = node.find(osc_cs);
  if(it != node.end() && it->second.type == "push")
    it->second.type = "faderv";
  else
    it = node.insert({osc_cs, {type, text, 0., 0.01}}).first;

  if(node[osc_cs].min > scalef.toDouble())
    node[osc_cs].min = scalef.toDouble();
  if(node[osc_cs].max < scalet.toDouble())
    node[osc_cs].max = scalet.toDouble();
}
}

bool loadDeviceFromTouchOSC(const QString& filePath, Device::Node& node)
{
  // ouverture d'un xml
  QFile doc_xml(filePath);
  if(!doc_xml.open(QIODevice::ReadOnly))
    return false;

  auto files = zdl::unzip_all_files_to_memory(score::mapAsByteArray(doc_xml));

  auto it = ossia::find_if(files, [](const std::pair<QString, QByteArray>& f) {
    return f.first.endsWith("index.xml", Qt::CaseInsensitive);
  });
  if(it == files.end())
    return false;

  QDomDocument domDoc;
  if(!domDoc.setContent(it->second))
    return false;

  QDomElement doc = domDoc.documentElement();

  TouchOSCControlMap ctlmap;
  for(int k = 0; k < doc.childNodes().size(); k++)
  {
    auto cld = doc.childNodes().at(k).childNodes();
    for(int i = 0; i < cld.size(); i++)
    {
      auto n = cld.at(i);
      if(n.nodeName() == "control" && n.nodeType() == QDomNode::ElementNode)
      {
        auto e = n.toElement();
        addTouchOSCControl(e, ctlmap);
      }
    }
  }
  handleTouchOSC(ctlmap, node);

  return true;
}
}

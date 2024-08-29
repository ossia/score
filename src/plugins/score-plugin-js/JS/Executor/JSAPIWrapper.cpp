// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSAPIWrapper.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>

#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/dataflow.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/midi_port.hpp>
#include <ossia/dataflow/typed_value.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/network/value/value.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::ExecStateWrapper)
namespace JS
{

ExecStateWrapper::~ExecStateWrapper() { }

const ossia::destination_t& ExecStateWrapper::find_address(const QString& str)
{
  // OPTIMIZEME this function can be optimized a lot
  // c.f. MapperDevice.cpp:find_parameter
  auto it = m_address_cache.find(str);
  if(it != m_address_cache.end())
  {
    return it->second;
  }

  auto d = str.indexOf(':');
  if(d == -1)
  {
    // Address looks like '/foo/bar'
    // Try to find automatically in all devices
    auto node = find_node(devices, str.toStdString());
    if(node)
    {
      if(auto addr = node->get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }
  }

  // Split in devices
  auto dev = ossia::find_if(
      devices, [devname = str.mid(0, d).toStdString()](const auto& dev) {
    return dev->get_name() == devname;
  });

  if(dev != devices.end())
  {
    if(d == str.size() - 1)
    {
      if(auto addr = (*dev)->get_root_node().get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }

    auto node
        = ossia::net::find_node((*dev)->get_root_node(), str.mid(d + 1).toStdString());
    if(node)
    {
      if(auto addr = node->get_parameter())
      {
        auto [it, b] = m_address_cache.insert({str, addr});
        return it->second;
      }
    }
  }

  if(auto p = ossia::traversal::make_path(str.toStdString()))
  {
    auto [it, b] = m_address_cache.insert({str, *p});
    return it->second;
  }

  static const ossia::destination_t bad_dest;
  return bad_dest;
}

QVariant ExecStateWrapper::read(const QString& address)
{
  if(auto addr = find_address(address))
  {
    QVariant var;
    QVariantMap mv;

    bool unique = ossia::apply_to_destination(
        addr, devices, [&](ossia::net::parameter_base* addr, bool unique) {
      if(unique)
      {
        var = addr->value().apply(ossia::qt::ossia_to_qvariant{});
      }
      else
      {
        mv[QString::fromStdString(addr->get_node().osc_address())]
            = addr->value().apply(ossia::qt::ossia_to_qvariant{});
      }
    }, ossia::do_nothing_for_nodes{});
    if(unique)
      return var;
    else
      return mv;
  }
  return {};
}

void ExecStateWrapper::write(const QString& address, const QVariant& value)
{
  if(const auto& addr = find_address(address))
  {
    m_port_cache.get_data().clear();
    m_port_cache.get_data().emplace_back(ossia::qt::qt_to_ossia{}(value));

    ossia::apply_to_destination(
        addr, devices, [&](ossia::net::parameter_base* addr, bool unique) {
      on_push(*addr, m_port_cache);
    }, ossia::do_nothing_for_nodes{});
  }
}

ossia::net::node_base*
ExecStateWrapper::find_node(DeviceCache& devices, std::string_view name)
{
  for(auto dev : devices)
  {
    if(auto res = ossia::net::find_node(dev->get_root_node(), name))
      return res;
  }
  return nullptr;
}

QVariant ExecStateWrapper::asColor(QVariant v) const noexcept
{
  switch(v.userType())
  {
    case QMetaType::QVector3D: {
      auto val = v.value<QVector3D>();
      return QVariant::fromValue(QColor::fromRgbF(val.x(), val.y(), val.z()));
    }
    case QMetaType::QVector4D: {
      auto val = v.value<QVector4D>();
      return QVariant::fromValue(QColor::fromRgbF(val.x(), val.y(), val.z(), val.w()));
    }
    default:
      return v;
  }
}

QVariant ExecStateWrapper::asVec2(QVariant v) const noexcept
{
  switch(v.userType())
  {
    default:
      return v;
  }
}

QVariant ExecStateWrapper::asVec3(QVariant v) const noexcept
{
  switch(v.userType())
  {
    default:
      return v;
  }
}

QVariant ExecStateWrapper::asVec4(QVariant v) const noexcept
{
  switch(v.userType())
  {
    default:
      return v;
  }
}

QVariant ExecStateWrapper::asArray(QVariant v) const noexcept
{
  switch(v.userType())
  {
    case QMetaType::Bool:
    case QMetaType::QTime:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::Char:
    case QMetaType::QString:
    case QMetaType::QByteArray:
    case QMetaType::Double:
    case QMetaType::QColor: {
      auto val = v.value<QColor>();
      return QVariant::fromValue(
          QList<double>{val.redF(), val.greenF(), val.blueF(), val.alphaF()});
    }
    case QMetaType::QPoint: {
      auto val = v.value<QPoint>();
      return QVariant::fromValue(QList<int>{val.x(), val.y()});
    }
    case QMetaType::QPointF: {
      auto val = v.value<QPointF>();
      return QVariant::fromValue(QList<double>{val.x(), val.y()});
    }
    case QMetaType::QVector2D: {
      auto val = v.value<QVector2D>();
      return QVariant::fromValue(QList<double>{val.x(), val.y()});
    }
    case QMetaType::QSize: {
      auto val = v.value<QSize>();
      return QVariant::fromValue(QList<int>{val.width(), val.height()});
    }
    case QMetaType::QSizeF: {
      auto val = v.value<QSizeF>();
      return QVariant::fromValue(QList<double>{val.width(), val.height()});
    }
    case QMetaType::QVector3D: {
      auto val = v.value<QVector3D>();
      return QVariant::fromValue(QList<double>{val.x(), val.y(), val.z()});
    }
    case QMetaType::QVector4D: {
      auto val = v.value<QVector4D>();
      return QVariant::fromValue(QList<double>{val.x(), val.y(), val.z(), val.w()});
    }
    case QMetaType::QQuaternion: {
      auto val = v.value<QQuaternion>();
      return QVariant::fromValue(QList<double>{val.x(), val.y(), val.z(), val.scalar()});
    }
    case QMetaType::QLine: {
      auto val = v.value<QLine>();
      return QVariant::fromValue(
          QList<int>{val.p1().x(), val.p1().y(), val.p2().x(), val.p2().y()});
    }
    case QMetaType::QLineF: {
      auto val = v.value<QLineF>();
      return QVariant::fromValue(
          QList<double>{val.p1().x(), val.p1().y(), val.p2().x(), val.p2().y()});
    }
    case QMetaType::QRect: {
      auto val = v.value<QRect>();
      return QVariant::fromValue(
          QList<int>{val.x(), val.y(), val.width(), val.height()});
    }
    case QMetaType::QRectF: {
      auto val = v.value<QRectF>();
      return QVariant::fromValue(
          QList<double>{val.x(), val.y(), val.width(), val.height()});
    }
    case QMetaType::QVariantList:
    case QMetaType::QStringList:
    case QMetaType::QDate:
    default:
      break;
  }

  return v;
}
}

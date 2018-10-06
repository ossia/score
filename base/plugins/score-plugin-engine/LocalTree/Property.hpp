#pragma once
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <LocalTree/BaseCallbackWrapper.hpp>
#include <LocalTree/TypeConversion.hpp>

#include <ossia/network/base/node.hpp>
#include <QTimer>
namespace LocalTree
{
template <typename T>
using qt_property_converter_T = ossia::qt_property_converter<
    std::remove_const_t<std::remove_reference_t<T>>>;

template <typename Property>
struct PropertyWrapper final : public BaseCallbackWrapper
{
  using model_t = typename Property::model_type;
  using param_t = typename Property::param_type;
  model_t& m_model;
  using converter_t
      = ossia::qt_property_converter<typename Property::param_type>;
  PropertyWrapper(
      ossia::net::parameter_base& param_addr, model_t& obj, QObject* context)
      : BaseCallbackWrapper{param_addr}, m_model{obj}
  {
    callbackIt = addr.add_callback([=,m=QPointer<model_t>{&m_model}](const ossia::value& v) {
        QTimer::singleShot(0, [m, v] {
          if(m) ((*m).*Property::set())(::State::convert::value<param_t>(v));
      });
    });

    QObject::connect(
        &m_model, Property::notify(), context,
        [=] {
          auto newVal = converter_t::convert((m_model.*Property::get())());
          try
          {
            auto res = addr.value();

            if (newVal != res)
            {
              addr.push_value(newVal);
            }
          }
          catch (...)
          {
          }
        },
        Qt::QueuedConnection);

    addr.set_value(converter_t::convert((m_model.*Property::get())()));
  }
};

template <typename Property, typename Object>
auto add_property(ossia::net::node_base& n, Object& obj, QObject* context)
{
  constexpr const auto t
      = ossia::qt_property_converter<typename Property::param_type>::val;
  auto node = n.create_child(Property::name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::BI);
  return std::make_unique<PropertyWrapper<Property>>(*addr, obj, context);
}

template <typename Property, typename Object>
auto add_property(ossia::net::node_base& n, Object& obj, const std::string& name, QObject* context)
{
  constexpr const auto t
      = ossia::qt_property_converter<typename Property::param_type>::val;
  auto node = n.create_child(name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::BI);
  return std::make_unique<PropertyWrapper<Property>>(*addr, obj, context);
}
}

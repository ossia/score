#pragma once
#include <Process/TypeConversion.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/tools/Debug.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/network/base/node.hpp>

#include <LocalTree/BaseCallbackWrapper.hpp>
namespace LocalTree
{
template <typename T>
using qt_property_converter_T
    = ossia::qt_property_converter<std::remove_const_t<std::remove_reference_t<T>>>;

template <typename Property>
struct PropertyWrapper final : public BaseCallbackWrapper
{
  using model_t = typename Property::model_type;
  using param_t = typename Property::param_type;
  model_t& m_model;
  using converter_t = ossia::qt_property_converter<typename Property::param_type>;
  PropertyWrapper(ossia::net::parameter_base& param_addr, model_t& obj, QObject* context)
      : BaseCallbackWrapper{param_addr}, m_model{obj}
  {
    QObject::connect(
        &m_model,
        Property::notify,
        context,
        [=] {
          auto newVal = converter_t::convert((m_model.*Property::get)());
          try
          {
            auto res = addr.value();

            if (newVal != res)
            {
              addr.set_value_quiet(newVal);
              addr.push_value();
            }
          }
          catch (...)
          {
          }
        },
        Qt::QueuedConnection);

    addr.set_value(converter_t::convert((m_model.*Property::get)()));
    callbackIt = addr.add_callback([=, m = QPointer<model_t>{&m_model}](const ossia::value& v) {
      score::invoke([m, v] {
        if (m)
          ((*m).*Property::set)(::State::convert::value<param_t>(v));
      });
    });
  }
};

template <typename Property, typename Object>
auto add_property(ossia::net::node_base& n, Object& obj, QObject* context)
{
  constexpr const auto t = ossia::qt_property_converter<typename Property::param_type>::val;
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
  constexpr const auto t = ossia::qt_property_converter<typename Property::param_type>::val;
  auto node = n.create_child(name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::BI);
  return std::make_unique<PropertyWrapper<Property>>(*addr, obj, context);
}
}

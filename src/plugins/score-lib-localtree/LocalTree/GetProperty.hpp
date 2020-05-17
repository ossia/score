#pragma once
#include <Process/TypeConversion.hpp>

#include <ossia/network/base/node.hpp>

#include <QObject>

#include <LocalTree/BaseCallbackWrapper.hpp>

namespace LocalTree
{

template <typename Property>
struct GetPropertyWrapper final : public BaseProperty
{
  using model_t = typename Property::model_type;
  using param_t = typename Property::param_type;
  model_t& m_model;
  using converter_t = ossia::qt_property_converter<typename Property::param_type>;

  GetPropertyWrapper(ossia::net::parameter_base& param_addr, model_t& obj, QObject* context)
      : BaseProperty{param_addr}, m_model{obj}
  {
    QObject::connect(
        &m_model,
        Property::notify,
        context,
        [&m = m_model, &addr = addr] {
          auto newVal = converter_t::convert((m.*Property::get)());
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

    addr.set_value(converter_t::convert((m_model.*Property::get)()));
  }
};

template <typename Property, typename Object>
auto add_getProperty(ossia::net::node_base& n, Object& obj, QObject* context)
{
  constexpr const auto t = ossia::qt_property_converter<typename Property::param_type>::val;
  auto node = n.create_child(Property::name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::GET);

  return std::make_unique<GetPropertyWrapper<Property>>(*addr, obj, context);
}
}

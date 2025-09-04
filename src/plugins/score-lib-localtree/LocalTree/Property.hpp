#pragma once
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <Process/TypeConversion.hpp>

#include <LocalTree/BaseCallbackWrapper.hpp>

#include <score/tools/Debug.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/network/base/node.hpp>

#include <QApplication>
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
  struct shared_state
  {
    bool active{};
    bool in_push_from_qt{};
    bool in_push_from_ossia{};
    auto push_from_qt() noexcept
    {
      struct res
      {
        shared_state& self;
        res(shared_state& self)
            : self{self}
        {
          self.in_push_from_qt = true;
        }
        ~res() { self.in_push_from_qt = false; }
      };
      return res{*this};
    }
    auto push_from_ossia() noexcept
    {
      struct res
      {
        shared_state& self;
        res(shared_state& self)
            : self{self}
        {
          self.in_push_from_ossia = true;
        }
        ~res() { self.in_push_from_ossia = false; }
      };
      return res{*this};
    }
  };

  std::shared_ptr<shared_state> state
      = std::make_shared<shared_state>(shared_state{.active = true});
  using converter_t = ossia::qt_property_converter<typename Property::param_type>;
  PropertyWrapper(ossia::net::parameter_base& param_addr, model_t& obj, QObject* context)
      : BaseCallbackWrapper{param_addr}
      , m_model{obj}
  {
    qtCallback
        = QObject::connect(&m_model, Property::notify, context, [this, state = state] {
      if(!state->active)
        return;
      if(state->in_push_from_qt)
        return;
      if(state->in_push_from_ossia)
        return;
      auto push = state->push_from_qt();

      auto newVal = converter_t::convert((m_model.*Property::get)());
      try
      {
        auto res = addr.value();

        if(newVal != res)
        {
          addr.push_value(newVal);
        }
      }
      catch(...)
      {
      }
    }, Qt::QueuedConnection);

    addr.set_value(converter_t::convert((m_model.*Property::get)()));
    callbackIt = addr.add_callback(
        [=, m = QPointer<model_t>{&m_model}, state = state](const ossia::value& v) {
      if(!state->active)
        return;
      if(state->in_push_from_qt)
        return;
      if(state->in_push_from_ossia)
        return;
      auto push = state->push_from_ossia();
      ossia::qt::run_async(qApp, [m, v] {
        if(m)
          ((*m).*Property::set)(::State::convert::value<param_t>(v));
      });
    });
  }

  ~PropertyWrapper()
  {
    this->clear();
    state->active = false;
    auto& node = this->addr.get_node();
    auto par = node.get_parent();
    if(par)
      par->remove_child(node);
  }
};

template <typename Property, typename Object>
auto add_property(ossia::net::node_base& n, Object& obj, QObject* context)
{
  SCORE_ASSERT(!std::string_view(Property::name).empty());
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
auto add_property(
    ossia::net::node_base& n, Object& obj, const std::string& name, QObject* context)
{
  SCORE_ASSERT(!name.empty());
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

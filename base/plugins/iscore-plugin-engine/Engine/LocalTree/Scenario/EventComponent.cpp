// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventComponent.hpp"
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>

namespace Engine
{
namespace LocalTree
{
Event::Event(
    ossia::net::node_base& parent,
    const Id<iscore::Component>& id,
    Scenario::EventModel& event,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, event.metadata(), doc,
                      id,     "EventComponent", parent_comp}
{
  auto exp_n = node().create_child("expression");
  auto exp_a = exp_n->create_address(ossia::val_type::STRING);
  exp_a->set_access(ossia::access_mode::BI);

  exp_a->add_callback([&](const ossia::value& v) {
    auto expr = v.target<std::string>();
    if(expr)
    {
      auto expr_p = ::State::parseExpression(*expr);
      if(expr_p)
        event.setCondition(*std::move(expr_p));
    }
  });

  QObject::connect(&event, &Scenario::EventModel::conditionChanged, this,
      [=] (const ::State::Expression& cond) {
        // TODO try to simplify the other get / set properties like this
        ossia::value newVal = cond.toString().toStdString();
        try
        {
          auto res = exp_a->value();

          if (newVal != res)
          {
            exp_a->push_value(std::move(newVal));
          }
        }
        catch (...)
        {
        }
      },
      Qt::QueuedConnection);

  exp_a->set_value(event.condition().toString().toStdString());

}
}
}

#include <ossia/dataflow/nodes/percentage.hpp>
#include <ossia/editor/automation/curve_value_visitor.hpp>
#include <ossia/editor/curve/behavior.hpp>

#include <Execution/Automation/InterpStateComponent.hpp>
namespace ossia::nodes
{
class state_interpolation final : public ossia::graph_node
{
public:
  using drives_vector = std::vector<std::pair<ossia::destination, ossia::behavior>>;
  state_interpolation() { }

  ~state_interpolation() override { }

  std::string label() const noexcept override { return "state_interpolation"; }

  void set_behaviors(const drives_vector& b) { m_drives = b; }

  void reset_drive()
  {
    for (auto& [_, drive] : m_drives)
      drive.reset();
  }

private:
  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    for (auto& [dest, drive] : m_drives)
    {
      auto val = ossia::apply(
          ossia::detail::compute_value_visitor{t.position(), dest.value.get().get_value_type()},
          drive);
      e.insert(dest.address(), ossia::typed_value{std::move(val)});
    }
  }

  drives_vector m_drives;
};
}
namespace InterpState
{

ExecComponent::ExecComponent(
    InterpState::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{element, ctx, id, "Executor::InterpStateComponent", parent}
{
  // - When a state (start / end) changes
  //   -> value changed
  //   -> value added
  //   -> value removed
  // - When the curve changes

  /*
  node = std::make_shared<ossia::nodes::automation>();
  m_ossia_process = std::make_shared<ossia::nodes::automation_process>(node);

  con(element, &Automation::ProcessModel::minChanged, this,
      [this](const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::maxChanged, this,
      [this](const auto&) { this->recompute(); });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Automation::ProcessModel::tweenChanged, this,
      [this](const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::curveChanged, this,
      [this]() { this->recompute(); });
*/
  recompute();
}

ExecComponent::~ExecComponent() { }

void ExecComponent::recompute()
{ /*
   auto dest = Engine::score_to_ossia::makeDestination(
       system().execState, process().address());

   if (dest)
   {
     auto& d = *dest;
     auto addressType = d.address().get_value_type();

     auto curve = process().tween() ? on_curveChanged(addressType, d)
                                    : on_curveChanged(addressType, {});

     if (curve)
     {
       in_exec([proc = std::dynamic_pointer_cast<ossia::nodes::automation>(
                    OSSIAProcess().node),
                curve, d_ = d] { proc->set_behavior(curve); });
       return;
     }
   }
   else
   {
     auto curve = on_curveChanged_impl<float>({});

     if (curve)
     {
       in_exec([proc = std::dynamic_pointer_cast<ossia::nodes::automation>(
                    OSSIAProcess().node),
                curve] { proc->set_behavior(curve); });
       return;
     }
   }*/
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract>
ExecComponent::on_curveChanged_impl(const optional<ossia::destination>& d)
{ /*
   using namespace ossia;

   const double min = process().min();
   const double max = process().max();

   auto scale_x = [](double val) -> double { return val; };
   auto scale_y = [=](double val) -> Y_T { return val * (max - min) + min; };

   auto segt_data = process().curve().sortedSegments();
   if (segt_data.size() != 0)
   {
     return Engine::score_to_ossia::curve<double, Y_T>(
         scale_x, scale_y, segt_data, d);
   }
   else*/
  {
    return {};
  }
}

std::shared_ptr<ossia::curve_abstract>
ExecComponent::on_curveChanged(ossia::val_type type, const optional<ossia::destination>& d)
{ /*
   switch (type)
   {
     case ossia::val_type::INT:
       return on_curveChanged_impl<int>(d);
     case ossia::val_type::FLOAT:
       return on_curveChanged_impl<float>(d);
     case ossia::val_type::LIST:
     case ossia::val_type::VEC2F:
     case ossia::val_type::VEC3F:
     case ossia::val_type::VEC4F:
       return on_curveChanged_impl<float>(d);
     default:
       qDebug() << "Unsupported curve type: " << (int)type;
       SCORE_TODO;
   }
 */
  return {};
}
}

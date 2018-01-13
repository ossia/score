#pragma once
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Widgets.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <QVBoxLayout>


namespace Control
{
template<typename Info>
class ControlCommand final : public score::Command
{
    using Command::Command;
    ControlCommand() = default;

    ControlCommand(const ControlProcess<Info>& obj, int control, ossia::value newval)
        : m_path{obj}
        , m_control{control}
        , m_old{obj.control(control)}
        , m_new{newval}
    {
    }

    virtual ~ControlCommand() { }

    void undo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setControl(m_control, m_old);
    }

    void redo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setControl(m_control, m_new);
    }

    void update(unused_t, ossia::value newval)
    {
      m_new = std::move(newval);
    }

  protected:
    void serializeImpl(DataStreamInput& stream) const final override
    {
      stream << m_path << m_control << m_old << m_new;
    }
    void deserializeImpl(DataStreamOutput& stream) final override
    {
      stream >> m_path >> m_control >> m_old >> m_new;
    }

  private:
    Path<ControlProcess<Info>> m_path;
    int m_control{};
    ossia::value m_old, m_new;

};

template<typename Info>
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<ControlProcess<Info>>
{
public:
  explicit InspectorWidget(
      const ControlProcess<Info>& object,
      const score::DocumentContext& doc,
      QWidget* parent)
      : Process::InspectorWidgetDelegate_T<ControlProcess<Info>>{object, parent}
    {
      auto vlay = new QVBoxLayout{this};
      vlay->setSpacing(2);
      vlay->setMargin(2);
      vlay->setContentsMargins(0, 0, 0, 0);

      if constexpr(InfoFunctions<Info>::control_count > 0)
      {
        std::size_t i = 0;
        ossia::for_each_in_tuple(
              get_controls(Info::info),
              [&] (const auto& ctrl) {
          auto inlet = static_cast<Process::ControlInlet*>(object.inlets()[InfoFunctions<Info>::control_start + i]);

          auto lab = new TextLabel{ctrl.name, this};
          vlay->addWidget(lab);

          auto widg = ctrl.make_widget(ctrl, *inlet, doc, this, this);
          vlay->addWidget(widg);

          i++;
        });
      }

      vlay->addStretch();
    }

private:
};

template<typename Info>
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ControlProcess<Info>, InspectorWidget<Info>>
{
  public:
    static Q_DECL_RELAXED_CONSTEXPR Process::InspectorWidgetDelegateFactory::ConcreteKey
    static_concreteKey() noexcept
    {
      return Info::Metadata::uuid;
    }

    Process::InspectorWidgetDelegateFactory::ConcreteKey concreteKey() const noexcept final override
    {
      return static_concreteKey();
    }
};

}

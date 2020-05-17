#pragma once
#include <Media/Merger/Commands.hpp>
#include <Media/Merger/Model.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFormLayout>
#include <QSpinBox>

namespace Media
{
namespace Merger
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Model>
{
public:
  explicit InspectorWidget(const Model& obj, const score::DocumentContext& doc, QWidget* parent)
      : InspectorWidgetDelegate_T{obj, parent}, m_dispatcher{doc.commandStack}, m_count{this}
  {
    m_count.setRange(1, 24);
    m_count.setValue(obj.inCount());

    auto lay = new QFormLayout{this};

    con(process(), &Model::inCountChanged, this, [&] { m_count.setValue(obj.inCount()); });

    con(m_count, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.submit<SetMergeInCount>(obj, m_count.value());
      m_dispatcher.commit();
    });

    lay->addRow(tr("Count"), &m_count);
  }

private:
  OngoingCommandDispatcher m_dispatcher;

  QSpinBox m_count;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("cb41b8ba-7fe6-47ae-891d-90da8cc145c5")
};
}
}

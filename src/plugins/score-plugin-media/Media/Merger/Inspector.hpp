#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Media/Merger/Commands.hpp>
#include <Media/Merger/Model.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

namespace Media
{
namespace Merger
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Model>
{
public:
  explicit InspectorWidget(
      const Model& obj, const score::DocumentContext& doc, QWidget* parent)
      : InspectorWidgetDelegate_T{obj, parent}
      , m_dispatcher{doc.dispatcher}
      , m_count{this}
  {
    m_count.setRange(1, 24);
    m_count.setValue(obj.inCount());

    m_mode.addItem("Stereo");
    m_mode.addItem("Mono");

    auto lay = new QFormLayout{this};

    con(process(), &Model::inCountChanged, this, [&] {
      if(m_count.value() != obj.inCount())
        m_count.setValue(obj.inCount());
    });

    con(m_count, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.submit<SetMergeInCount>(obj, m_count.value());
      m_dispatcher.commit();
    });

    lay->addRow(tr("Count"), &m_mode);
    con(process(), &Model::modeChanged, this, [&] {
      if(m_mode.currentIndex() != (int)obj.mode())
        m_mode.setCurrentIndex((int)obj.mode());
    });

    con(m_mode, &QComboBox::currentIndexChanged, this, [&]() {
      m_dispatcher.submit<SetMergeMode>(obj, (Merger::Model::Mode)m_mode.currentIndex());
      m_dispatcher.commit();
    });

    lay->addRow(tr("Count"), &m_count);
    lay->addRow(tr("Mode"), &m_mode);
  }

private:
  OngoingCommandDispatcher& m_dispatcher;

  QSpinBox m_count;
  QComboBox m_mode;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("cb41b8ba-7fe6-47ae-891d-90da8cc145c5")
};
}
}

#pragma once

#include <Gris/Commands.hpp>
#include <Gris/Model.hpp>

#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QSignalBlocker>

#include <QFormLayout>
#include <QSpinBox>

namespace Gris
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<SpatModel>
{
public:
  explicit InspectorWidget(
      const SpatModel& object, const score::DocumentContext& doc, QWidget* parent)
      : InspectorWidgetDelegate_T{object, parent}
      , m_dispatcher{doc.commandStack}
  {
    auto lay = new QFormLayout{this};

    m_sources = new QSpinBox{this};
    m_sources->setRange(1, SpatModel::maxSourceCount);
    m_sources->setValue(object.sourceCount());
    lay->addRow(tr("Sources"), m_sources);

    con(object, &SpatModel::sourceCountChanged, this, [this](int count) {
      if(m_sources->value() != count)
      {
        QSignalBlocker blocker{m_sources};
        m_sources->setValue(count);
      }
    });

    connect(
        m_sources, &QSpinBox::editingFinished, this,
        [this, &object] {
      if(m_sources->value() != object.sourceCount())
        m_dispatcher.submit<SetSourceCount>(object, m_sources->value());
        });
  }

private:
  CommandDispatcher<> m_dispatcher;
  QSpinBox* m_sources{};
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<SpatModel, InspectorWidget>
{
  SCORE_CONCRETE("7f2a1d63-84c9-4b05-a1de-3e9c07b5d2fa")
};
} // namespace Gris

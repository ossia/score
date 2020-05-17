#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/plugins/Interface.hpp>

#include <QComboBox>

namespace Process
{
class Cable;
}
namespace Dataflow
{
class CableWidget final : public Inspector::InspectorWidgetBase
{
  QComboBox m_cabletype;

public:
  CableWidget(const Process::Cable& cable, const score::DocumentContext& ctx, QWidget* parent);
};

class CableInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("4b1a99aa-016e-440f-8ba6-24b961cff532")
public:
  CableInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}

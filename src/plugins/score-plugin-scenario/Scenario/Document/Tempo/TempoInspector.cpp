#include <Curve/Commands/MovePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>

#include <Scenario/Document/Tempo/TempoInspector.hpp>

#include <Inspector/InspectorLayout.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QDoubleSpinBox>

namespace Scenario
{

TempoPointInspectorFactory::TempoPointInspectorFactory()
    : InspectorWidgetFactory{}
{
}

QWidget* TempoPointInspectorFactory::make(
    const InspectedObjects& sourceElements, const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new TempoPointInspectorWidget{
      safe_cast<const Curve::PointModel&>(*sourceElements.first()), doc, parent};
}

bool TempoPointInspectorFactory::matches(const InspectedObjects& objects) const
{
  auto pt = qobject_cast<const Curve::PointModel*>(objects.first());
  if(pt)
    return qobject_cast<const Scenario::TempoProcess*>(pt->parent()->parent());
  else
    return false;
}

TempoPointInspectorWidget::TempoPointInspectorWidget(
    const Curve::PointModel& model, const score::DocumentContext& doc, QWidget* parent)
    : Curve::PointInspectorWidget{model, doc, parent}
    , m_dispatcher{doc.dispatcher}
{
  auto automModel_base = safe_cast<Scenario::TempoProcess*>(model.parent()->parent());

  auto& automModel = *automModel_base;
  m_Ymin = automModel.min;
  m_yFactor = automModel.max - m_Ymin;
  m_YBox = new QDoubleSpinBox{};
  m_YBox->setRange(automModel.min, automModel.max);
  m_YBox->setSingleStep(m_yFactor / 100);
  m_YBox->setValue(model.pos().y() * m_yFactor + m_Ymin);
  m_YBox->setDecimals(4); // NOTE : settings ?

  connect(
      m_YBox, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
      &TempoPointInspectorWidget::on_pointChanged);

  connect(
      m_YBox, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
      &TempoPointInspectorWidget::on_editFinished);

  m_layout->addRow("Value", m_YBox);
}

void TempoPointInspectorWidget::on_pointChanged(double d)
{
  if(!m_model)
    return;
  auto& model = *m_model;
  Curve::Point pos{m_XBox->value() / m_xFactor, (m_YBox->value() - m_Ymin) / m_yFactor};
  m_dispatcher.submit<Curve::MovePoint>(
      *safe_cast<Curve::Model*>(model.parent()), model.id(), pos);
}

void TempoPointInspectorWidget::on_editFinished()
{
  m_dispatcher.commit();
}
}

// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePointInspectorWidget.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/Commands/MovePoint.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/TimeValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <list>

namespace Curve
{
PointInspectorWidget::PointInspectorWidget(
    const Curve::PointModel& model,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetBase{model, doc, parent, tr("Point")}
    , m_model{model}
{
  setObjectName("CurvePointInspectorWidget");
  setParent(parent);

  auto cm = safe_cast<Curve::Model*>(m_model.parent());
  auto process_base = qobject_cast<Process::ProcessModel*>(cm->parent());
  m_xFactor = process_base->duration().msec();

  auto w = new QWidget;
  m_layout = new Inspector::Layout{w};
  updateAreaLayout({w});

  // x box
  m_XBox = new QDoubleSpinBox{};
  m_XBox->setRange(0., m_xFactor);
  m_XBox->setDecimals(0);

  m_XBox->setValue(m_model.pos().x() * m_xFactor);

  m_XBox->setSingleStep(m_xFactor / 100);
  m_XBox->setEnabled(false);

  m_layout->addRow("Time (ms)", m_XBox);
}

}
namespace Automation
{
PointInspectorWidget::PointInspectorWidget(
    const Curve::PointModel& model,
    const score::DocumentContext& doc,
    QWidget* parent)
    : Curve::PointInspectorWidget{model, doc, parent}
    , m_dispatcher{doc.dispatcher}
{
  /*
  connect(
      m_XBox,
      static_cast<void (QDoubleSpinBox::*)(double)>(
          &QDoubleSpinBox::valueChanged),
      this,
      &PointInspectorWidget::on_pointChanged);

  connect(
      m_XBox,
      static_cast<void (QDoubleSpinBox::*)(double)>(
          &QDoubleSpinBox::valueChanged),
      this,
      &PointInspectorWidget::on_editFinished);
  */


  // y  box
  auto cm = safe_cast<Curve::Model*>(m_model.parent());
  auto automModel_base = safe_cast<Automation::ProcessModel*>(cm->parent());

  auto& automModel = *automModel_base;
  m_Ymin = automModel.min();
  m_yFactor = automModel.max() - m_Ymin;
  m_YBox = new QDoubleSpinBox{};
  m_YBox->setRange(automModel.min(), automModel.max());
  m_YBox->setSingleStep(m_yFactor / 100);
  m_YBox->setValue(m_model.pos().y() * m_yFactor + m_Ymin);
  m_YBox->setDecimals(4); // NOTE : settings ?

  connect(
        m_YBox,
        SignalUtils::QDoubleSpinBox_valueChanged_double(),
        this,
        &PointInspectorWidget::on_pointChanged);

  connect(
        m_YBox,
        SignalUtils::QDoubleSpinBox_valueChanged_double(),
        this,
        &PointInspectorWidget::on_editFinished);

  m_layout->addRow("Value", m_YBox);

  // y en %
  /*    auto widgP = new QWidget;
        auto hlayP = new QHBoxLayout{widgP};
        auto spinP = new QDoubleSpinBox{};
        spinP->setRange(automModel.min(), automModel.max());
        spinP->setSingleStep(m_yFactor/100);
        spinP->setValue(m_model.pos().y());

        hlayP->addWidget(new QLabel{"value %"});
        hlayP->addWidget(spinP);

        vec.push_back(widgP);
    */

}

void PointInspectorWidget::on_pointChanged(double d)
{
  Curve::Point pos{
      m_XBox->value() / m_xFactor, (m_YBox->value() - m_Ymin) / m_yFactor};
  m_dispatcher.submit<Curve::MovePoint>(
      *safe_cast<Curve::Model*>(m_model.parent()), m_model.id(), pos);
}

void PointInspectorWidget::on_editFinished()
{
  m_dispatcher.commit();
}
}

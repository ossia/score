// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePointInspectorWidget.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/Commands/MovePoint.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/TimeValue.hpp>

#include <score/selection/SelectionDispatcher.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QLabel>
#include <QTimer>
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
    , m_model{&model}
{
  setObjectName("CurvePointInspectorWidget");
  setParent(parent);

  auto cm = safe_cast<Curve::Model*>(model.parent());
  auto process_base = qobject_cast<Process::ProcessModel*>(cm->parent());
  m_xFactor = process_base->duration().msec();

  auto w = new QWidget;
  m_layout = new Inspector::Layout{w};
  updateAreaLayout({w});

  // x box
  m_XBox = new QDoubleSpinBox{};
  m_XBox->setRange(0., m_xFactor);
  m_XBox->setDecimals(0);

  m_XBox->setValue(model.pos().x() * m_xFactor);
  m_XBox->setEnabled(false);

  m_XBox->setSingleStep(m_xFactor / 100);

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
    , m_moveState{new Curve::StateBase{}}
    , m_moveX{*qobject_cast<Curve::Model*>(model.parent()), nullptr, doc.commandStack}
    , m_dispatcher{doc.dispatcher}
{
  ;
  ((QObject*)m_moveState)->setParent(this);
  m_moveX.setCurveState(m_moveState);
  connect(
      m_XBox,
      static_cast<void (QDoubleSpinBox::*)(double)>(
          &QDoubleSpinBox::valueChanged),
      this,
      &PointInspectorWidget::on_pointXChanged);

  connect(
      m_XBox,
      &QSpinBox::editingFinished,
      this,
      &PointInspectorWidget::on_editXFinished);


  // y  box
  auto cm = safe_cast<Curve::Model*>(model.parent());
  auto automModel_base = safe_cast<Automation::ProcessModel*>(cm->parent());

  auto& automModel = *automModel_base;
  m_XBox->setEnabled(true);
  m_Ymin = automModel.min();
  m_yFactor = automModel.max() - m_Ymin;
  m_YBox = new QDoubleSpinBox{};
  m_YBox->setRange(automModel.min(), automModel.max());
  m_YBox->setSingleStep(m_yFactor / 100);
  m_YBox->setValue(model.pos().y() * m_yFactor + m_Ymin);
  m_YBox->setDecimals(4); // NOTE : settings ?

  connect(
        m_YBox,
        SignalUtils::QDoubleSpinBox_valueChanged_double(),
        this,
        &PointInspectorWidget::on_pointYChanged);

  connect(
        m_YBox,
        &QSpinBox::editingFinished,
        this,
        &PointInspectorWidget::on_editYFinished);

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

void PointInspectorWidget::on_pointXChanged(double d)
{
  // m_dispatcher.submit<Curve::UpdateCurve>(
  //       *safe_cast<Curve::Model*>(m_model.parent()), m_model.id(), pos);
}

void PointInspectorWidget::on_editXFinished()
{
  if(!m_model)
    return;

  auto& model = *m_model;
  Curve::Point pos{
    m_XBox->value() / m_xFactor, (m_YBox->value() - m_Ymin) / m_yFactor};
  if(std::abs(pos.x() - model.pos().x()) < 0.0001)
    return;


  auto simpleMove = [this] { on_pointYChanged(m_YBox->value()); on_editYFinished(); };

  // Try to handle all the simple cases:
  auto& curve = *qobject_cast<Curve::Model*>(model.parent());
  double new_x = m_XBox->value() / m_xFactor;
  if(!model.previous())
  {
    if(!model.following())
    {
      return simpleMove();
    }
    else
    {
      // Moving in the first segment
      auto& next_seg = curve.segments().at(*model.following());
      if(new_x < next_seg.end().x())
      {
        return simpleMove();
      }
    }
  }

  if(!model.following())
  {
    SCORE_ASSERT(model.previous()); // We know that from the previous if

    auto& prev_seg = curve.segments().at(*model.previous());
    if(new_x > prev_seg.start().x())
    {
      return simpleMove();
    }
  }

  if(model.previous() && model.following())
  {
    auto& prev_seg = curve.segments().at(*model.previous());
    auto& next_seg = curve.segments().at(*model.following());
    if(new_x > prev_seg.start().x() && new_x < next_seg.end().x())
    {
      return simpleMove();
    }
  }

  m_model = nullptr;

  m_moveState->clickedPointId.previous = model.previous();
  m_moveState->clickedPointId.following = model.following();

  m_moveState->currentPoint = pos;

  if(!m_startedEditing)
  {
    m_startedEditing = true;
  }

  m_moveX.press();
  m_moveX.move();
  m_moveX.release();
  m_startedEditing = false;

  QTimer::singleShot(1, [&ctx=this->context(), &curve, pos] {
    // Find the point that is at the specified position and select it
    for(const Curve::PointModel* pt : curve.points())
    {
      if(pt->pos() == pos)
      {
        score::SelectionDispatcher disp{ctx.selectionStack};
        disp.select(*pt);
        return;
      }
    }
  });
}

void PointInspectorWidget::on_pointYChanged(double d)
{
  if(!m_model)
    return;
  Curve::Point pos{
      m_XBox->value() / m_xFactor, (m_YBox->value() - m_Ymin) / m_yFactor};
  m_dispatcher.submit<Curve::MovePoint>(
      *safe_cast<Curve::Model*>(m_model->parent()), m_model->id(), pos);
}

void PointInspectorWidget::on_editYFinished()
{
  m_dispatcher.commit();
}
}

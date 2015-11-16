#include "CurvePointInspectorWidget.hpp"

#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Automation/AutomationModel.hpp>

#include <Curve/Commands/MovePoint.hpp>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDoubleSpinBox>


CurvePointInspectorWidget::CurvePointInspectorWidget(
    const CurvePointModel& model,
    iscore::Document& doc,
    QWidget* parent):
    InspectorWidgetBase{model, doc, parent},
    m_model{model},
    m_dispatcher{commandDispatcher()->stack()}
{
    setObjectName("CurvePointInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;
    auto cm = safe_cast<CurveModel*>(m_model.parent());
    auto& automModel = *safe_cast<AutomationModel*>(cm->parent());
    m_xFactor = automModel.duration().msec();
    m_yFactor = automModel.max() - automModel.min();

    // x box
    auto widgX = new QWidget;
    auto hlayX = new QHBoxLayout{widgX};
    m_XBox = new QDoubleSpinBox{};
    m_XBox->setRange(0., m_xFactor);

    m_XBox->setValue(m_model.pos().x() * m_xFactor);

    vec.push_back(widgX);
    m_XBox->setEnabled(false);

    hlayX->addWidget(new QLabel{"t (ms)"});
    hlayX->addWidget(m_XBox);

    // y  box
    auto widgY = new QWidget;
    auto hlayY = new QHBoxLayout{widgY};
    m_YBox = new QDoubleSpinBox{};
    m_YBox->setRange(automModel.min(), automModel.max());
    m_YBox->setSingleStep(m_yFactor/100);
    m_YBox->setValue(m_model.pos().y() * m_yFactor);

    connect(m_YBox, SIGNAL(valueChanged(double)), this, SLOT(on_pointChanged(double)));

    connect(m_YBox, &QDoubleSpinBox::editingFinished,
            this, &CurvePointInspectorWidget::on_editFinished);
    vec.push_back(widgY);

    hlayY->addWidget(new QLabel{"value"});
    hlayY->addWidget(m_YBox);

    vec.push_back(new QWidget{});

    updateAreaLayout(vec);
}

void CurvePointInspectorWidget::on_pointChanged(double d)
{
    CurvePoint pos{m_XBox->value()/m_xFactor, m_YBox->value()/m_yFactor};
    m_dispatcher.submitCommand<MovePoint>(
                *safe_cast<CurveModel*>(m_model.parent()),
                m_model.id(),
                pos);
}

void CurvePointInspectorWidget::on_editFinished()
{
    m_dispatcher.commit();
}


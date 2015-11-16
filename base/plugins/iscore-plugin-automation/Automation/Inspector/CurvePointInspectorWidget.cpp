#include "CurvePointInspectorWidget.hpp"

#include <Curve/Point/CurvePointModel.hpp>

#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDoubleSpinBox>


CurvePointInspectorWidget::CurvePointInspectorWidget(
    const CurvePointModel& model,
    iscore::Document& doc,
    QWidget* parent):
    InspectorWidgetBase{model, doc, parent},
    m_model{model}
{
    setObjectName("CurvePointInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;

    // x box
    auto widgX = new QWidget;
    auto hlayX = new QHBoxLayout{widgX};
    m_XBox = new QDoubleSpinBox{};
    m_XBox->setValue(m_model.pos().x());
    vec.push_back(widgX);

    connect(m_XBox, &QDoubleSpinBox::editingFinished,
            this, &CurvePointInspectorWidget::on_pointChanged);

    hlayX->addWidget(new QLabel{"x"});
    hlayX->addWidget(m_XBox);

    // y  box
    auto widgY = new QWidget;
    auto hlayY = new QHBoxLayout{widgY};
    m_YBox = new QDoubleSpinBox{};
    m_YBox->setValue(m_model.pos().y());

    connect(m_YBox, &QDoubleSpinBox::editingFinished,
            this, &CurvePointInspectorWidget::on_pointChanged);
    vec.push_back(widgY);

    hlayY->addWidget(new QLabel{"y"});
    hlayY->addWidget(m_YBox);

    vec.push_back(new QWidget{});

    updateAreaLayout(vec);
}

void CurvePointInspectorWidget::on_pointChanged()
{
    CurvePoint pos{m_XBox->value(), m_YBox->value()};
//    m_model.setPos(pos);
}


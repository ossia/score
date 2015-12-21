#include "ProcessInspectorWidget.hpp"
#include "ProcessInspectorWidgetDelegate.hpp"
#include <Process/Process.hpp>
#include <QPushButton>
#include <QHBoxLayout>
ProcessInspectorWidget::ProcessInspectorWidget(
        ProcessInspectorWidgetDelegate* delegate,
        const iscore::DocumentContext& doc,
        QWidget* parent):
    QWidget{parent},
    m_delegate{delegate},
    m_context{doc}
{
    auto lay = new QVBoxLayout;

    lay->addWidget(delegate);

    QPushButton* displayBtn = new QPushButton {
            tr("Display in new Slot"),
            this};
    lay->addWidget(displayBtn);

    connect(displayBtn, &QPushButton::clicked,
            [=] ()
    {
        emit createViewInNewSlot(QString::number(m_delegate->process().id_val()));
    });

    setLayout(lay);
}

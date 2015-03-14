#include "MessageWidget.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <State/Message.hpp>
#include <QToolButton>

MessageWidget::MessageWidget(const Message& mess, QWidget* parent):
    QWidget{parent}
{
    auto lay = new QHBoxLayout {this};
    auto lbl = new QLabel {mess.address + " " + mess.value.toString(), this};
    lay->addWidget(lbl);

    QToolButton* rmBtn = new QToolButton;
    rmBtn->setText("X");
    lay->addWidget(rmBtn);

    connect(rmBtn, &QToolButton::clicked,
            this, &MessageWidget::removeMe);
}

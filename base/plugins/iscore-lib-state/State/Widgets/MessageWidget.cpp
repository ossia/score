#include "MessageWidget.hpp"
#include <State/Message.hpp>

MessageWidget::MessageWidget(const iscore::Message& mess, QWidget* parent):
    QLabel{mess.toString(), parent}
{
}

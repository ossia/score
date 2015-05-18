#pragma once
#include <QLabel>

struct Message;
class MessageWidget : public QLabel
{
    public:
        MessageWidget(const Message& m, QWidget* parent);
};

#pragma once
#include <QLabel>

namespace iscore
{
struct Message;
}
class MessageWidget : public QLabel
{
    public:
        MessageWidget(const iscore::Message& m, QWidget* parent);
};

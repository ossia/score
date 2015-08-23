#pragma once
#include <QPushButton>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace iscore
{
struct Message;
}
class MessageWidget : public QPushButton
{
    public:
        MessageWidget(
                const iscore::Message& m,
                const CommandDispatcher<> &stack,
                QWidget* parent);

    private:
        void on_clicked();

        const iscore::Message& m_message;
};

#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <State/Message.hpp>

namespace iscore
{
struct Message;
}
class MessageWidget : public QPushButton
{
    public:
        MessageWidget(
                iscore::Message& m,
                QWidget* parent);

    private:
        void on_clicked();

        iscore::Message& m_message;
};

class MessageListEditor : public QDialog
{
    public:
        MessageListEditor(
                const iscore::MessageList& m,
                QWidget* parent);

        const auto& messages() const
        { return m_messages; }
    private:
        iscore::MessageList m_messages;
};

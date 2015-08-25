#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <State/Message.hpp>

namespace iscore
{
struct Message;
}

class DeviceExplorerModel;
class MessageWidget : public QPushButton
{
    public:
        MessageWidget(
                iscore::Message& m,
                DeviceExplorerModel* model,
                QWidget* parent);

    private:
        void on_clicked();

        DeviceExplorerModel* m_model{};
        iscore::Message& m_message;
};

class MessageListEditor : public QDialog
{
    public:
        MessageListEditor(
                const iscore::MessageList& m,
                DeviceExplorerModel* model,
                QWidget* parent);

        const auto& messages() const
        { return m_messages; }

    private:
        void addMessage();
        void removeMessage(int);

        void updateLayout();

        DeviceExplorerModel* m_model{};

        QGridLayout* m_messageListLayout{};
        iscore::MessageList m_messages;
};

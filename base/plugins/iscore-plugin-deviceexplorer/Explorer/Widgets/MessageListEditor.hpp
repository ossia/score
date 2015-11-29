#pragma once
#include <State/Message.hpp>
#include <qdialog.h>

class DeviceExplorerModel;
class QGridLayout;
class QWidget;

/**
 * @brief The MessageListEditor class
 *
 * This dialog allows editing of a MessageList.
 * It uses a DeviceExplorer for the completion of addresses.
 *
 * After editing, when the dialog is accepted,
 * the modified messages are in m_messages and can be
 * used for a Command for instance.
 */
class MessageListEditor final : public QDialog
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

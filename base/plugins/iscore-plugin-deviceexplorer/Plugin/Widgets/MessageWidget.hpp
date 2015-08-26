#pragma once
#include <QPushButton>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <State/Message.hpp>

namespace iscore
{
struct Message;
}

class DeviceExplorerModel;
/**
 * @brief The MessageWidget class
 *
 * A button with a message. When clicked, opens a MessageEditDialog
 * that allows editing of the message.
 */
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

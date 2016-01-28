#pragma once

#include <QPushButton>

class QWidget;

namespace State
{
struct Message;
}

namespace DeviceExplorer
{
class DeviceExplorerModel;

/**
 * @brief The MessageWidget class
 *
 * A button with a message. When clicked, opens a MessageEditDialog
 * that allows editing of the message.
 */
class MessageWidget final : public QPushButton
{
    public:
        MessageWidget(
                State::Message& m,
                DeviceExplorerModel* model,
                QWidget* parent);

    private:
        void on_clicked();

        DeviceExplorerModel* m_model{};
        State::Message& m_message;
};
}

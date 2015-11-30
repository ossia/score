#include <iscore/widgets/ClearLayout.hpp>
#include <QDialogButtonBox>
#include <QFlags>
#include <QGridLayout>

#include <QPushButton>
#include <QString>

#include "MessageEditDialog.hpp"
#include "MessageListEditor.hpp"
#include "MessageWidget.hpp"
#include <State/Message.hpp>

class QWidget;

MessageListEditor::MessageListEditor(
        const iscore::MessageList& m,
        DeviceExplorerModel* model,
        QWidget* parent):
    m_model{model},
    m_messages{m}
{
    auto lay = new QGridLayout;

    m_messageListLayout = new QGridLayout;
    lay->addLayout(m_messageListLayout, 0, 0);
    updateLayout();

    auto addButton = new QPushButton(tr("Add message"));
    lay->addWidget(addButton);
    connect(addButton, &QPushButton::clicked,
            this, &MessageListEditor::addMessage);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok |
                                        QDialogButtonBox::StandardButton::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    lay->addWidget(buttons);

    this->setLayout(lay);
}

void MessageListEditor::addMessage()
{
    MessageEditDialog dial(iscore::Message(), m_model, this);
    int res = dial.exec();

    if(res)
    {
        m_messages.push_back(iscore::Message(dial.address(), dial.value()));
    }

    updateLayout();
}

void MessageListEditor::removeMessage(int i)
{
    m_messages.removeAt(i);
    updateLayout();
}

void MessageListEditor::updateLayout()
{
    iscore::clearLayout(m_messageListLayout);
    int i = 0;
    for(auto& mess : m_messages)
    {
        m_messageListLayout->addWidget(new MessageWidget{mess, m_model, this}, i, 0);

        auto removeButton = new QPushButton(tr("Remove"));
        m_messageListLayout->addWidget(removeButton, i, 1);

        connect(removeButton,&QPushButton::pressed,
                this, [=] { removeMessage(i); });
        i++;
    }
}

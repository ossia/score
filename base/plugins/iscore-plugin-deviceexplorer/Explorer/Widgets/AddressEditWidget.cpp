#include <State/Widgets/AddressLineEdit.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QLineEdit>

#include "AddressEditWidget.hpp"
#include "DeviceCompleter.hpp"
#include "DeviceExplorerMenuButton.hpp"
#include <State/Address.hpp>


namespace DeviceExplorer
{
AddressEditWidget::AddressEditWidget(DeviceExplorerModel* model, QWidget* parent):
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QVBoxLayout>;

    m_lineEdit = new AddressLineEdit{this};

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [&]() {
        m_address = State::Address::fromString(m_lineEdit->text());
        emit addressChanged(m_address);
    });

    lay->addWidget(m_lineEdit);

    if(model)
    {
        // LineEdit completion
        m_lineEdit->setCompleter(new DeviceCompleter {model, this});

        auto pb = new DeviceExplorerMenuButton{model};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const State::Address& addr)
        {
            setAddress(addr);
            emit addressChanged(addr);
        });

        lay->addWidget(pb);
    }

    this->setLayout(lay);
}

void AddressEditWidget::setAddress(const State::Address& addr)
{
    m_address = addr;
    m_lineEdit->setText(m_address.toString());
}
}

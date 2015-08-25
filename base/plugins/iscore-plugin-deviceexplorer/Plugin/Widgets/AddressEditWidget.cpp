#include "AddressEditWidget.hpp"

#include <State/Widgets/AddressLineEdit.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include "DeviceCompleter.hpp"
#include "DeviceExplorerMenuButton.hpp"
AddressEditWidget::AddressEditWidget(DeviceExplorerModel* model, QWidget* parent):
    QWidget{parent}
{
    auto lay = new MarginLess<QVBoxLayout>;

    m_lineEdit = new AddressLineEdit{this};

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [&]() {
        emit addressChanged(iscore::Address::fromString(m_lineEdit->text()));
    });

    lay->addWidget(m_lineEdit);

    if(model)
    {
        // LineEdit completion
        m_lineEdit->setCompleter(new DeviceCompleter {model, this});

        auto pb = new DeviceExplorerMenuButton{model};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const iscore::Address& addr)
        {
            m_lineEdit->setText(addr.toString());
            emit addressChanged(addr);
        });

        lay->addWidget(pb);
    }

    this->setLayout(lay);
}

void AddressEditWidget::setAddress(const iscore::Address& addr)
{
    m_lineEdit->setText(addr.toString());
}

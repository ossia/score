#include <State/Widgets/AddressLineEdit.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QLineEdit>

#include "AddressEditWidget.hpp"
#include "DeviceCompleter.hpp"
#include <State/Address.hpp>
#include <State/MessageListSerialization.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/QMenuView/qmenuview.h>
#include <QContextMenuEvent>

namespace Explorer
{
AddressEditWidget::AddressEditWidget(DeviceExplorerModel* model, QWidget* parent):
    QWidget{parent},
    m_model{model}
{
    setAcceptDrops(true);
    auto lay = new iscore::MarginLess<QHBoxLayout>{this};

    m_lineEdit = new AddressLineEdit{this};

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [&]() {
        m_address = State::Address::fromString(m_lineEdit->text());
        emit addressChanged(m_address);
    });

    m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lineEdit, &QLineEdit::customContextMenuRequested,
            this, &AddressEditWidget::customContextMenuEvent);
    if(model)
    {
        // LineEdit completion
        m_lineEdit->setCompleter(new DeviceCompleter {model, this});
    }

    lay->addWidget(m_lineEdit);
}

void AddressEditWidget::setAddress(const State::Address& addr)
{
    m_address = addr;
    m_lineEdit->setText(m_address.toString());
}

void AddressEditWidget::setAddressString(const QString s)
{
    m_lineEdit->setText(s);
    State::Address addr;
    m_address = addr.fromString(s);
}

void AddressEditWidget::customContextMenuEvent(const QPoint& p)
{
    auto device_menu = new QMenuView{m_lineEdit};
    device_menu->setModel(reinterpret_cast<QAbstractItemModel*>(m_model));
    connect(device_menu, &QMenuView::triggered,
            this, [&](const QModelIndex & m)
    {
        auto addr = Device::address(m_model->nodeFromModelIndex(m));
        setAddress(addr);
        emit addressChanged(addr);
    });

    device_menu->exec(m_lineEdit->mapToGlobal(p));
    delete device_menu;
}

void AddressEditWidget::dropEvent(QDropEvent* ev)
{
    auto mime = ev->mimeData();

    if(mime->formats().contains(iscore::mime::messagelist()))
    {
        Mime<State::MessageList>::Deserializer des{*mime};
        State::MessageList ml = des.deserialize();
        if(ml.size() > 0)
        {
            setAddress(ml[0].address);
        }
    }
}











AddressAccessorEditWidget::AddressAccessorEditWidget(
        DeviceExplorerModel* model,
        QWidget* parent):
    QWidget{parent},
    m_model{model}
{
    setAcceptDrops(true);
    auto lay = new iscore::MarginLess<QHBoxLayout>{this};

    m_lineEdit = new AddressAccessorLineEdit{this};

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [&]() {
        auto res = ::State::parseAddressAccessor(m_lineEdit->text());

        if(res) {
            m_address = *res;
        }
        else {
            m_address = State::AddressAccessor{};
        }

        emit addressChanged(m_address);
    });

    m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lineEdit, &QLineEdit::customContextMenuRequested,
            this, &AddressAccessorEditWidget::customContextMenuEvent);
    if(model)
    {
        // LineEdit completion
        m_lineEdit->setCompleter(new DeviceCompleter {model, this});
    }

    lay->addWidget(m_lineEdit);
}

void AddressAccessorEditWidget::setAddress(const State::Address& addr)
{
    m_address.address = addr;
    m_address.accessors.clear();
    m_lineEdit->setText(m_address.toString());
}

void AddressAccessorEditWidget::setAddress(const State::AddressAccessor& addr)
{
    m_address = addr;
    m_lineEdit->setText(m_address.toString());
}


void AddressAccessorEditWidget::customContextMenuEvent(const QPoint& p)
{
    auto device_menu = new QMenuView{m_lineEdit};
    device_menu->setModel(reinterpret_cast<QAbstractItemModel*>(m_model));
    connect(device_menu, &QMenuView::triggered,
            this, [&](const QModelIndex & m)
    {
        auto addr = Device::address(m_model->nodeFromModelIndex(m));
        setAddress(addr);
        emit addressChanged(m_address);
    });

    device_menu->exec(m_lineEdit->mapToGlobal(p));
    delete device_menu;
}

void AddressAccessorEditWidget::dropEvent(QDropEvent* ev)
{
    auto mime = ev->mimeData();

    if(mime->formats().contains(iscore::mime::messagelist()))
    {
        Mime<State::MessageList>::Deserializer des{*mime};
        State::MessageList ml = des.deserialize();
        if(ml.size() > 0)
        {
            setAddress(ml[0].address);
        }
    }
}

}

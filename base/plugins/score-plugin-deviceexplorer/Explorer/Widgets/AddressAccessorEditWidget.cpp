// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressAccessorEditWidget.hpp"
#include "DeviceCompleter.hpp"

#include <State/Widgets/AddressLineEdit.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Device/QMenuView/qmenuview.h>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/widgets/MarginLess.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <QHBoxLayout>
#include <ossia/network/value/value.hpp>

namespace Explorer
{
AddressAccessorEditWidget::AddressAccessorEditWidget(
    DeviceExplorerModel& model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
  setAcceptDrops(true);
  auto lay = new score::MarginLess<QHBoxLayout>{this};

  m_lineEdit = new State::AddressAccessorLineEdit<AddressAccessorEditWidget>{this};

  connect(m_lineEdit, &QLineEdit::editingFinished, [&]() {
    auto res = State::parseAddressAccessor(m_lineEdit->text());
    // TODO Try to find the address to get its min / max.
    // Explorer::makeFullAddressAccessorSettings(
    //   *res,
    //   score::IDocument::documentContext(mapping),
    //   0., 1.)

    m_address = Device::FullAddressAccessorSettings{};

    if(res)
    {
      m_address = Explorer::makeFullAddressAccessorSettings(
            *res, model, 0., 1.);
    }

    addressChanged(m_address);
  });

  m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_lineEdit, &QLineEdit::customContextMenuRequested, this,
      &AddressAccessorEditWidget::customContextMenuEvent);

  m_lineEdit->setCompleter(new DeviceCompleter{model, this});

  lay->addWidget(m_lineEdit);
}

void AddressAccessorEditWidget::setAddress(const State::AddressAccessor& addr)
{
  m_address = Device::FullAddressAccessorSettings{};
  m_address.address = addr;
  m_lineEdit->setText(m_address.address.toString());
}
void AddressAccessorEditWidget::setFullAddress(Device::FullAddressAccessorSettings&& addr)
{
  m_address = std::move(addr);
  m_lineEdit->setText(m_address.address.toString());
}

const Device::FullAddressAccessorSettings&AddressAccessorEditWidget::address() const
{
  return m_address;
}

QString AddressAccessorEditWidget::addressString() const
{
  return m_address.address.toString();
}

void AddressAccessorEditWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::messagelist()) ||
      formats.contains(score::mime::addressettings()))
  {
    event->accept();
  }
}

void AddressAccessorEditWidget::customContextMenuEvent(const QPoint& p)
{
  auto device_menu = new QMenuView{m_lineEdit};
  device_menu->setModel(&m_model);
  connect(device_menu, &QMenuView::triggered, this, [&](const QModelIndex& m)
  {
    setFullAddress(
          makeFullAddressAccessorSettings(
            m_model.nodeFromModelIndex(m), m_model));

    addressChanged(m_address);
  });

  device_menu->exec(m_lineEdit->mapToGlobal(p));
  delete device_menu;
}

void AddressAccessorEditWidget::dropEvent(QDropEvent* ev)
{
  auto& mime = *ev->mimeData();

  // TODO refactor this with AutomationPresenter and AddressLineEdit
  if (mime.formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{mime};
    Device::FullAddressSettings as = des.deserialize();

    if (as.address.path.isEmpty())
      return;

    setFullAddress(Device::FullAddressAccessorSettings{std::move(as)});
    addressChanged(m_address);
  }
  else if (mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if (nl.empty())
      return;

    // We only take the first node.
    const Device::Node& node = nl.front().second;
    // TODO refactor with CreateCurves and AutomationDropHandle
    if (node.is<Device::AddressSettings>())
    {
      const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
      Device::FullAddressSettings as;
      static_cast<Device::AddressSettingsCommon&>(as) = addr;
      as.address = nl.front().first;

      setFullAddress(Device::FullAddressAccessorSettings{std::move(as)});
      addressChanged(m_address);
    }
  }
  else if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if (!ml.empty())
    {
      // TODO if multiple addresses are selected we could instead show a selection dialog.
      setAddress(ml[0].address);
      addressChanged(m_address);
    }
  }
}
}

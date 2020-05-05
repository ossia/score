// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressAccessorEditWidget.hpp"

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <QMenuView/qmenuview.h>
#include <Device/Widgets/DeviceCompleter.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>
#include <State/Widgets/AddressLineEdit.hpp>
#include <State/Widgets/UnitWidget.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/value/value.hpp>


#include <QVBoxLayout>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Device::AddressAccessorEditWidget)
namespace Device
{
AddressAccessorEditWidget::AddressAccessorEditWidget(
    const score::DocumentContext& ctx,
    QWidget* parent)
    : QWidget{parent}
{
  setAcceptDrops(true);
  auto lay = new score::MarginLess<QVBoxLayout>{this};
  m_lineEdit
      = new State::AddressAccessorLineEdit<AddressAccessorEditWidget>{this};

  m_qualifiers = new State::DestinationQualifierWidget{this};
  m_qualifiers->setVisible(false);
  connect(
      m_qualifiers,
      &State::DestinationQualifierWidget::qualifiersChanged,
      this,
      [=](const auto& qual) {
        if (m_address.address.qualifiers != qual)
        {
          m_address.address.qualifiers = qual;
          m_lineEdit->setText(m_address.address.toString_unsafe());
          addressChanged(m_address);
        }
      });

  auto act = new QAction{this};
  act->setIcon(QIcon(":/icons/unit_icon.png"));
  act->setStatusTip(tr("Show the unit selector"));
  m_lineEdit->addAction(act, QLineEdit::TrailingPosition);

  connect(act, &QAction::triggered, [=] {
    if (m_qualifiers->isVisible())
    {
      m_qualifiers->setVisible(false);
    }
    else
    {
      m_qualifiers->setVisible(true);
    }
  });

  {
    auto& plist = ctx.app.interfaces<DeviceModelProviderList>();
    if (auto provider = plist.getBestProvider(ctx))
    {
      m_model = provider->getNodeModel(ctx);
    }
  }

  // find the model
  connect(m_lineEdit, &QLineEdit::editingFinished, [&]() {
    auto res = State::parseAddressAccessor(m_lineEdit->text());
    // TODO Try to find the address to get its min / max.
    // Explorer::makeFullAddressAccessorSettings(
    //   *res,
    //   score::IDocument::documentContext(mapping),
    //   0., 1.)

    m_address = Device::FullAddressAccessorSettings{};

    if (m_model && res)
    {
      m_address
          = Device::makeFullAddressAccessorSettings(*res, *m_model, 0., 1.);
    }
    else if (res)
    {
      m_address.address = *res;
    }
    else
    {
      m_address.address = State::AddressAccessor{};
    }

    addressChanged(m_address);
  });

  m_lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_lineEdit,
      &QLineEdit::customContextMenuRequested,
      this,
      &AddressAccessorEditWidget::customContextMenuEvent);

  if (m_model)
    m_lineEdit->setCompleter(new DeviceCompleter{*m_model, this});

  lay->addWidget(m_lineEdit);
  lay->addWidget(m_qualifiers);
}

void AddressAccessorEditWidget::setAddress(const State::AddressAccessor& addr)
{
  m_address = Device::FullAddressAccessorSettings{};
  m_address.address = addr;
  m_lineEdit->setText(m_address.address.toString_unsafe());
  if (m_qualifiers->qualifiers() != m_address.address.qualifiers)
    m_qualifiers->setQualifiers(m_address.address.qualifiers);
}
void AddressAccessorEditWidget::setFullAddress(
    Device::FullAddressAccessorSettings&& addr)
{
  m_address = std::move(addr);
  m_lineEdit->setText(m_address.address.toString_unsafe());
  if (m_qualifiers->qualifiers() != m_address.address.qualifiers)
    m_qualifiers->setQualifiers(m_address.address.qualifiers);
}

const Device::FullAddressAccessorSettings&
AddressAccessorEditWidget::address() const
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
  if (formats.contains(score::mime::messagelist()))
  {
    event->accept();
  }
}

void AddressAccessorEditWidget::customContextMenuEvent(const QPoint& p)
{
  if (m_model)
  {
    auto device_menu = new QMenuView{m_lineEdit};
    device_menu->setModel(m_model);
    connect(
        device_menu, &QMenuView::triggered, this, [&](const QModelIndex& m) {
          setFullAddress(
              makeFullAddressAccessorSettings(m_model->nodeFromModelIndex(m)));

          addressChanged(m_address);
        });

    device_menu->exec(m_lineEdit->mapToGlobal(p));
    delete device_menu;
  }
}

void AddressAccessorEditWidget::dropEvent(QDropEvent* ev)
{
  auto& mime = *ev->mimeData();

  // TODO refactor this with AutomationPresenter and AddressLineEdit
  if (mime.formats().contains(score::mime::nodelist()))
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
      const Device::AddressSettings& addr
          = node.get<Device::AddressSettings>();
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
      // TODO if multiple addresses are selected we could instead show a
      // selection dialog.
      setAddress(ml[0].address);
      addressChanged(m_address);
    }
  }
}
}

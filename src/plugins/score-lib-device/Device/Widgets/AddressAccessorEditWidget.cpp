// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressAccessorEditWidget.hpp"

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Device/Widgets/DeviceCompleter.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>
#include <State/Widgets/AddressLineEdit.hpp>
#include <State/Widgets/UnitWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/detail/flat_map.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>

#include <QMenuView/qmenuview.h>
#include <QVBoxLayout>

#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
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
        const auto ad = m_address.address.toString_unsafe();
        if (m_lineEdit->text() != ad)
        {
          m_lineEdit->blockSignals(true);
          m_lineEdit->setText(ad);
          m_lineEdit->blockSignals(false);
        }
      });

  auto act = new QAction{tr("Show Unit selector"), this};
  act->setStatusTip(tr("Show the unit selector"));
  setIcons(
      act,
      QStringLiteral(":/icons/port_address_unit_on.png"),
      QStringLiteral(":/icons/port_address_unit.png"),
      QStringLiteral(":/icons/port_address_unit.png"));

  m_lineEdit->addAction(act, QLineEdit::TrailingPosition);

  connect(
      act, &QAction::triggered, [=]() { m_qualifiers->chooseQualifier(); });

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
          = Device::makeFullAddressAccessorSettings(*res, *m_model, 0., 1., 0.5);
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
}
void AddressAccessorEditWidget::setFullAddress(
    Device::FullAddressAccessorSettings&& addr)
{
  m_address = std::move(addr);
  m_lineEdit->setText(m_address.address.toString_unsafe());
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

namespace
{
struct AddressComparator {
  bool operator()(const State::Address& lhs, const State::Address& rhs) const noexcept
{
  return lhs.toString() < rhs.toString();
}
};

class LearnWidget : public QListWidget {
public:
  using map_type = ossia::flat_map<::State::Address, ossia::value, AddressComparator> ;
  Device::NodeBasedItemModel& m_model;
  explicit LearnWidget(Device::NodeBasedItemModel& model, QWidget* parent = nullptr)
      : QListWidget{parent}
      , m_model{model}
  {
    connect(&model, &QAbstractItemModel::dataChanged, this, &LearnWidget::on_dataChanged);
  }

  void on_dataChanged(const QModelIndex& first, const QModelIndex& second)
  {
    auto& node = m_model.nodeFromModelIndex(first);
    auto as = node.target<Device::AddressSettings>();
    if(!as)
      return;
    auto addr = address(node);
    if(addr.address.toString().isEmpty())
    {
      return;
    }

    auto it = messages.find(addr.address);
    if(it == messages.end())
    {
      const auto& [emplaced_it, value] = messages.emplace(addr.address, as->value);
      auto dist = std::distance(messages.begin(), emplaced_it);
      auto new_item = new QListWidgetItem{textFromMessage(addr.address, as->value)};
      this->insertItem(dist, new_item);
      this->scrollToBottom();

      // Select the first message to be learned
      if(messages.size() == 1)
      {
        new_item->setSelected(true);
      }
    }
    else
    {
      auto dist = std::distance(messages.begin(), it);
      it.underlying->second = as->value;
      this->item(dist)->setText(textFromMessage(addr.address, as->value));
    }
  }

  static QString textFromMessage(const ::State::Address& addr, const ossia::value& value)
  {
    return QStringLiteral("%1: %2").arg(addr.toString()).arg(QString::fromStdString(fmt::format("{}", value)));
  }

  map_type messages;
};

class LearnDialog : public QDialog {
public:
  QVBoxLayout m_layout;
  LearnWidget m_learn;
  QDialogButtonBox m_buttons;
  LearnDialog(Device::NodeBasedItemModel& model, QWidget* parent = nullptr)
      : QDialog{parent}
      , m_layout{this}
      , m_learn{model, this}
      , m_buttons{QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel}
  {
    m_layout.addWidget(&m_learn);
    m_layout.addWidget(&m_buttons);
    connect(
        &m_buttons,
        &QDialogButtonBox::accepted,
        this,
        &LearnDialog::on_accept);
    connect(
        &m_buttons,
        &QDialogButtonBox::rejected,
        this,
        &LearnDialog::reject);
  }

  void on_accept()
  {
    auto selected = m_learn.currentRow();
    if(ossia::valid_index(selected, m_learn.messages))
    {
      selectedAddress = &m_learn.messages.container[selected].first;
      accept();
    }
    else
    {
      reject();
    }
  }

  const State::Address* selectedAddress{};
};
}
void AddressAccessorEditWidget::startLearn()
{
  auto dialog = new LearnDialog{*m_model};
  dialog->exec();
  if(dialog->selectedAddress)
  {
    const auto& node = Device::getNodeFromAddress(m_model->rootNode(), *dialog->selectedAddress);
    setFullAddress(makeFullAddressAccessorSettings(node));

    addressChanged(m_address);
  }
}

void AddressAccessorEditWidget::customContextMenuEvent(const QPoint& p)
{
  if (m_model)
  {
    auto m = new QMenu;
    // Allow to learn whenever a value change
    auto act = m->addAction(tr("Learn"));
    connect(act, &QAction::triggered, this, &AddressAccessorEditWidget::startLearn);

    // Show the menu with all the device explorer nodes
    auto device_menu = new QMenuView{m_lineEdit};
    device_menu->setTitle("Devices");
    device_menu->setModel(m_model);

    m->addMenu(device_menu);
    connect(
        device_menu, &QMenuView::triggered, this, [&](const QModelIndex& m) {
          if(m.isValid())
            {
              setFullAddress(
                  makeFullAddressAccessorSettings(m_model->nodeFromModelIndex(m)));

              addressChanged(m_address);
            }
        });

    m->exec(m_lineEdit->mapToGlobal(p));
    m->deleteLater();
    device_menu->deleteLater();
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

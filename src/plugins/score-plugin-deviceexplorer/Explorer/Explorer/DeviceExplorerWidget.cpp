// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerWidget.hpp"

#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerView.hpp"
#include "ExplorationWorkerWrapper.hpp"
#include "QProgressIndicator.h"
#include "Widgets/AddressEditDialog.hpp"
#include "Widgets/DeviceEditDialog.hpp"
#include <Explorer/Panel/DeviceExplorerPanelDelegate.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Loading/JamomaDeviceLoader.hpp>
#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/AddDevice.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/RemoveNodes.hpp>
#include <Explorer/Commands/ReplaceDevice.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/AddressItemModel.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/SearchLineEdit.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QAbstractProxyModel>
#include <QAction>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QPair>
#include <QRegExp>
#include <QSize>
#include <QStackedLayout>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QToolButton>
#include <QTreeView>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <set>
#include <stdexcept>
W_OBJECT_IMPL(Explorer::DeviceExplorerWidget)
namespace Explorer
{
static const Device::DeviceSettings* getDevice(const Device::Node& n)
{
  if (n.is<Device::AddressSettings>())
  {
    const Device::Node* p = &n;
    while ((p = p->parent()))
    {
      if (p->is<Device::DeviceSettings>())
        return &p->get<Device::DeviceSettings>();
    }
  }
  else if (n.is<Device::DeviceSettings>())
  {
    return &n.get<Device::DeviceSettings>();
  }
  return nullptr;
}

struct ExplorerSearchLineEdit final : public score::SearchLineEdit
{
public:
  ExplorerSearchLineEdit(DeviceExplorerWidget& parent)
      : score::SearchLineEdit{&parent}, m_widget{parent}
  {
    connect(this, &QLineEdit::textEdited, this, [=] { search(); });
  }

  void search() override
  {
    auto m = m_widget.proxyModel();
    if (!m)
      return;
    auto v = m_widget.view();
    if (!v)
      return;

    if (text() != m->filterRegExp().pattern())
    {
      m->setFilterRegExp(QRegExp(text(), Qt::CaseInsensitive, QRegExp::FixedString));

      if (text().isEmpty())
        v->collapseAll();
      else
        v->expandAll();
    }
  }

  DeviceExplorerWidget& m_widget;
};

class LearnDialog final : public QDialog
{
public:
  LearnDialog(Device::DeviceInterface& dev, QWidget* w) : QDialog{w}, m_dev{dev}
  {
    this->setWindowTitle(tr("OSC learning"));
    auto lay = new QVBoxLayout{this};

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_list = new QListWidget{this};
    lay->addWidget(m_list);
    lay->addWidget(buttonBox);

    con(dev, &Device::DeviceInterface::pathAdded, this, [=](const State::Address& a) {
      m_list->addItem(a.toString());
    });

    m_dev.setLearning(true);
  }

  ~LearnDialog() { m_dev.setLearning(false); }

  Device::DeviceInterface& m_dev;
  QListWidget* m_list{};
};

DeviceExplorerWidget::DeviceExplorerWidget(const Device::ProtocolFactoryList& pl, QWidget* parent)
    : QWidget(parent), m_protocolList{pl}, m_proxyModel(nullptr), m_deviceDialog(nullptr)
{
  setMinimumWidth(150);
  buildGUI();

  // Set the expansion signals
  connect(
      m_ntView,
      &DeviceExplorerView::created,
      this,
      [&](const QModelIndex& parent, int start, int end) {
        if (m_listeningManager)
        {
          for (int i = start; i <= end; i++)
          {
            Device::Node* node{};
            if (m_ntView->hasProxy())
            {
              node = (Device::Node*)sourceIndex(
                         ((QTreeView*)m_ntView)->model()->index(i, 0, parent))
                         .internalPointer();
            }
            else
            {
              node = ((Device::Node*)model()->index(i, 0, parent).internalPointer());
            }

            m_listeningManager->enableListening(*node);
          }
        }
      });
  connect(m_ntView, &QTreeView::expanded, this, [&](const QModelIndex& idx) {
    if (m_listeningManager)
      m_listeningManager->setListening(idx, true);
  });
  connect(m_ntView, &QTreeView::collapsed, this, [&](const QModelIndex& idx) {
    if (m_listeningManager)
      m_listeningManager->setListening(idx, false);
  });
}

void DeviceExplorerWidget::buildGUI()
{
  m_ntView = new DeviceExplorerView(this);
  m_ntView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

  m_addressModel = new AddressItemModel{this};
  m_addressView = new QTableView{this};
  auto delegate = new AddressItemDelegate{m_addressView};
  m_addressView->setItemDelegate(delegate);
  m_addressView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  m_addressView->setMinimumHeight(100);

  m_addressView->horizontalHeader()->hide();
  m_addressView->verticalHeader()->hide();
  m_addressView->horizontalHeader()->setCascadingSectionResizes(true);
  m_addressView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_addressView->horizontalHeader()->setStretchLastSection(true);
  m_addressView->setAlternatingRowColors(true);
  m_addressView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_addressView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_addressView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  m_addressView->verticalHeader()->setDefaultSectionSize(14);

  m_addressView->setModel(m_addressModel);
  connect(
      m_ntView,
      static_cast<void (DeviceExplorerView::*)()>(&DeviceExplorerView::selectionChanged),
      this,
      [=] {
        updateAddressView();
        updateActions();
      },
      Qt::QueuedConnection);

  m_editAction = new QAction(tr("Edit"), this);
  m_editAction->setStatusTip(tr("Edit the device."));

  m_refreshAction = new QAction(tr("Refresh namespace"), this);
  m_refreshAction->setShortcut(QKeySequence::Refresh);

  m_disconnect = new QAction{tr("Disconnect"), this};
  m_reconnect = new QAction{tr("Reconnect"), this};

  m_refreshValueAction = new QAction(tr("Refresh value"), this);

  m_removeNodeAction = new QAction(tr("Remove"), this);
  m_exportDeviceAction = new QAction{tr("Export device"), this};
  m_learnAction = new QAction{tr("Learn"), this};
  m_findUsageAction = new QAction{tr("Find usage"), this};

#ifdef __APPLE__
  m_removeNodeAction->setShortcut(QKeySequence(tr("Ctrl+Backspace")));
#else
  m_removeNodeAction->setShortcut(QKeySequence::Delete);
#endif

  m_editAction->setEnabled(false);
  m_refreshAction->setEnabled(false);
  m_refreshValueAction->setEnabled(false);
  m_removeNodeAction->setEnabled(false);
  m_disconnect->setEnabled(false);
  m_reconnect->setEnabled(false);
  m_exportDeviceAction->setEnabled(false);
  m_learnAction->setEnabled(false);
  m_findUsageAction->setEnabled(false);

  m_editAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_refreshAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_refreshValueAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_removeNodeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_disconnect->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_reconnect->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_exportDeviceAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_learnAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_findUsageAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  connect(m_editAction, &QAction::triggered, this, &DeviceExplorerWidget::edit);
  connect(m_refreshAction, &QAction::triggered, this, &DeviceExplorerWidget::refresh);
  connect(m_refreshValueAction, &QAction::triggered, this, &DeviceExplorerWidget::refreshValue);
  connect(m_disconnect, &QAction::triggered, this, &DeviceExplorerWidget::disconnect);
  connect(m_reconnect, &QAction::triggered, this, &DeviceExplorerWidget::reconnect);
  connect(m_removeNodeAction, &QAction::triggered, this, &DeviceExplorerWidget::removeNodes);
  connect(m_exportDeviceAction, &QAction::triggered, this, &DeviceExplorerWidget::exportDevice);
  connect(m_learnAction, &QAction::triggered, this, &DeviceExplorerWidget::learn);
  connect(m_findUsageAction, &QAction::triggered, this, &DeviceExplorerWidget::findUsage);

  m_openMenu = new QToolButton(this);
  m_openMenu->setIcon(makeIcons(
      QStringLiteral(":/icons/add_on.png"),
      QStringLiteral(":/icons/add_off.png"),
      QStringLiteral(":/icons/add_disabled.png")));
  m_openMenu->setAutoRaise(true);

  m_addDeviceAction = new QAction(tr("Add device"), this);
  setIcons(
      m_addDeviceAction,
      QStringLiteral(":/icons/add_device_on.png"),
      QStringLiteral(":/icons/add_device_off.png"),
      QStringLiteral(":/icons/add_device_disabled.png"));
  m_addDeviceAction->setShortcut(tr("Ctrl+B"));

  m_addSiblingAction = new QAction(tr("Add sibling"), this);
  setIcons(
      m_addSiblingAction,
      QStringLiteral(":/icons/add_sibling_on.png"),
      QStringLiteral(":/icons/add_sibling_off.png"),
      QStringLiteral(":/icons/add_sibling_disabled.png"));

  m_addChildAction = new QAction(tr("Add child"), this);
  setIcons(
      m_addChildAction,
      QStringLiteral(":/icons/add_child_on.png"),
      QStringLiteral(":/icons/add_child_off.png"),
      QStringLiteral(":/icons/add_child_disabled.png"));
  connect(m_addDeviceAction, &QAction::triggered, this, &DeviceExplorerWidget::addDevice);
  connect(m_addSiblingAction, &QAction::triggered, this, &DeviceExplorerWidget::addSibling);
  connect(m_addChildAction, &QAction::triggered, this, &DeviceExplorerWidget::addChild);

  m_addSiblingAction->setEnabled(false);
  m_addChildAction->setEnabled(false);

  // Setup menus

  QMenu* addMenu = new QMenu(this);
  addMenu->addAction(m_addDeviceAction);
  addMenu->addAction(m_addSiblingAction);
  addMenu->addAction(m_addChildAction);
  addMenu->addAction(m_exportDeviceAction);
  addMenu->addSeparator();
  addMenu->addAction(m_removeNodeAction);

  connect(m_openMenu, &QToolButton::clicked,
          addMenu, [addMenu]() { addMenu->popup(QCursor::pos()); });

  // Add actions to the current widget so that shortcuts work
  {
    this->addAction(m_addDeviceAction);
    this->addAction(m_addSiblingAction);
    this->addAction(m_addChildAction);
    this->addAction(m_exportDeviceAction);

    this->addAction(m_refreshAction);
    this->addAction(m_refreshValueAction);
    this->addAction(m_learnAction);
    this->addAction(m_findUsageAction);

    this->addAction(m_removeNodeAction);
  }

  m_columnCBox = new QComboBox(this);
  m_nameLEdit = new ExplorerSearchLineEdit(*this);

  connect(
      m_columnCBox,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      &DeviceExplorerWidget::filterChanged);
  connect(m_nameLEdit, &QLineEdit::textEdited, this, &DeviceExplorerWidget::filterChanged);

  auto hLayout = new score::MarginLess<QHBoxLayout>;
  hLayout->setSpacing(0);
  hLayout->addWidget(m_openMenu);
  hLayout->addWidget(m_columnCBox);
  hLayout->addWidget(m_nameLEdit);

  QWidget* mainWidg = new QWidget;
  mainWidg->setContentsMargins(0, 0, 0, 2);
  auto vLayout = new score::MarginLess<QVBoxLayout>;
  vLayout->addLayout(hLayout);
  vLayout->addWidget(m_ntView);
  vLayout->addWidget(m_addressView);
  mainWidg->setLayout(vLayout);
  mainWidg->setObjectName("DeviceExplorer");

  m_lay = new QStackedLayout;
  m_lay->addWidget(mainWidg);

  auto refreshParent = new QWidget;
  auto refreshLay = new QGridLayout;
  refreshParent->setLayout(refreshLay);
  m_refreshIndicator = new QProgressIndicator{refreshParent};
  QPalette palette;
  palette.setBrush(QPalette::Window, Qt::transparent);
  m_refreshIndicator->setPalette(palette);

  refreshLay->addWidget(m_refreshIndicator);
  m_lay->addWidget(refreshParent);
  setLayout(m_lay);
}

void DeviceExplorerWidget::blockGUI(bool b)
{
  m_ntView->setDisabled(b);
  m_addressView->setDisabled(b);
  if (b)
  {
    // m_ntView to front
    m_lay->setCurrentIndex(1);
    m_refreshIndicator->startAnimation();
  }
  else
  {
    // progreess widget to front
    m_lay->setCurrentIndex(0);
    m_refreshIndicator->stopAnimation();
  }
}

void DeviceExplorerWidget::setEditable(bool b)
{
  if(m_openMenu) m_openMenu->setEnabled(b);
  m_addressView->setEnabled(b);
}

QModelIndex DeviceExplorerWidget::sourceIndex(QModelIndex index) const
{
  if (m_ntView->hasProxy())
    index = static_cast<const QAbstractProxyModel*>(m_ntView->QTreeView::model())
                ->mapToSource(index);
  return index;
}

QModelIndex DeviceExplorerWidget::proxyIndex(QModelIndex index) const
{
  if (m_ntView->hasProxy())
    index = static_cast<const QAbstractProxyModel*>(m_ntView->QTreeView::model())
                ->mapFromSource(index);
  return index;
}

QSize DeviceExplorerWidget::sizeHint() const
{
  return {200, 800};
}

void DeviceExplorerWidget::contextMenuEvent(QContextMenuEvent* event)
{
  updateActions();
  QMenu* contextMenu = new QMenu{this};

  if (auto m = model())
  {
    if (!m->isEmpty())
    {
      QModelIndexList selection = m_ntView->selectedIndexes();

      if (selection.size() == 1)
      {
        auto& node = m->nodeFromModelIndex(m_ntView->selectedIndex());
        if (node.is<Device::DeviceSettings>())
        {
          auto& lst = m->deviceModel().list();
          auto& dev = lst.device(node.get<Device::DeviceSettings>().name);
          dev.setupContextMenu(*contextMenu);
          contextMenu->addSeparator();
        }
      }
    }
  }

  contextMenu->addAction(m_editAction);
  contextMenu->addAction(m_refreshAction);
  contextMenu->addAction(m_refreshValueAction);

  contextMenu->addAction(m_disconnect);
  contextMenu->addAction(m_reconnect);

  contextMenu->addSeparator();
  contextMenu->addAction(m_addDeviceAction);
  contextMenu->addAction(m_addSiblingAction);
  contextMenu->addAction(m_addChildAction);
  contextMenu->addAction(m_exportDeviceAction);
  contextMenu->addAction(m_learnAction);
  contextMenu->addAction(m_findUsageAction);
  contextMenu->addSeparator();
  contextMenu->addAction(m_removeNodeAction);

  contextMenu->exec(event->globalPos());
  contextMenu->deleteLater();
}

void DeviceExplorerWidget::setModel(DeviceExplorerModel* model)
{
  delete m_proxyModel; //? will also delete previous model ??
  m_proxyModel = nullptr;
  m_listeningManager.reset();
  QObject::disconnect(m_modelCon);
  QObject::disconnect(m_addressCon);

  if (model)
  {
    m_proxyModel = new DeviceExplorerFilterProxyModel(this);
    m_proxyModel->setSourceModel(model);
    m_ntView->setModel(m_proxyModel);
    model->setView(m_ntView);

    m_listeningManager = std::make_unique<ListeningManager>(*model, *this);
    m_cmdDispatcher = std::make_unique<CommandDispatcher<>>(model->commandStack());

    populateColumnCBox();

    updateActions();

    m_modelCon = connect(model, &DeviceExplorerModel::nodeChanged, this, [this](Device::Node* n) {
      bool parent_is_expanded = m_ntView->isExpanded(
          proxyIndex(m_ntView->model()->modelIndexFromNode(*n->parent(), 0)));
      if (parent_is_expanded)
      {
        if (m_listeningManager)
          m_listeningManager->enableListening(*n);
      }
    });

    connect(
        model,
        &DeviceExplorerModel::dataChanged,
        this,
        [this](
            const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QVector<int>& roles) {
          auto indexes = m_ntView->selectedIndexes();

          if (indexes.size() == 1)
          {
            auto selected = sourceIndex(indexes.first());

            if (selected.parent() == topLeft.parent() && selected.row() == topLeft.row())
              updateAddressView();
          }
        });
  }
  else
  {
    m_ntView->setModel((QAbstractItemModel*)nullptr);
  }

  setEnabled(bool(model));
}

void DeviceExplorerWidget::populateColumnCBox()
{
  SCORE_ASSERT(model());
  SCORE_ASSERT(m_columnCBox);

  QStringList columns = model()->getColumns();
  m_columnCBox->clear();
  m_columnCBox->addItems(columns);
}

// The bool indicates if the passed node was a device
std::pair<Device::DeviceCapas, bool> getCapas(Device::Node* p, const Device::DeviceList& lst)
{
  if (p->is<Device::DeviceSettings>())
  {
    return {lst.device(p->get<Device::DeviceSettings>().name).capabilities(), true};
  }
  while (p && !p->is<Device::DeviceSettings>())
  {
    p = p->parent();
  }
  if (!p)
    throw std::runtime_error("Cannot get capabilities of no device");

  return {lst.device(p->get<Device::DeviceSettings>().name).capabilities(), false};
}

void DeviceExplorerWidget::updateActions()
{
  auto m = model();
  if (!m)
    return;

  const bool editable = this->editable();
  m_addDeviceAction->setEnabled(editable);
  m_exportDeviceAction->setEnabled(false);
  m_learnAction->setEnabled(false);
  m_addSiblingAction->setEnabled(false);
  m_addChildAction->setEnabled(false);
  m_editAction->setEnabled(false);
  m_refreshAction->setEnabled(false);
  m_refreshValueAction->setEnabled(false);
  m_removeNodeAction->setEnabled(false);
  m_findUsageAction->setEnabled(false);
  m_reconnect->setEnabled(false);
  m_disconnect->setEnabled(false);

  if (!m->isEmpty())
  {
    // TODO: choice for multi selection

    SCORE_ASSERT(m_ntView);

    QModelIndexList selection = m_ntView->selectedIndexes();
    if (!selection.isEmpty())
    {
      m_findUsageAction->setEnabled(true);
    }

    if (selection.size() == 1)
    {
      const auto [capas, aDeviceIsSelected]
          = getCapas(&m->nodeFromModelIndex(m_ntView->selectedIndex()), m->deviceModel().list());

      if (!aDeviceIsSelected)
      {
        m_refreshValueAction->setEnabled(capas.canRefreshValue && editable);
        m_addSiblingAction->setEnabled(capas.canAddNode && editable);
        m_addChildAction->setEnabled(capas.canAddNode && editable);
        m_removeNodeAction->setEnabled(capas.canRemoveNode && editable);
      }
      else
      {
        m_refreshAction->setEnabled(capas.canRefreshTree && editable);
        m_reconnect->setEnabled(capas.canDisconnect && editable);
        m_disconnect->setEnabled(capas.canDisconnect && editable);
        m_exportDeviceAction->setEnabled(true);
        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(capas.canAddNode && editable);
        m_removeNodeAction->setEnabled(editable);
        m_learnAction->setEnabled(capas.canLearn && editable);
      }
      m_editAction->setEnabled(editable);
    }
  }
  else
  {
    m_editAction->setEnabled(false);
    m_refreshAction->setEnabled(false);
    m_refreshValueAction->setEnabled(false);
    m_removeNodeAction->setEnabled(false);
    m_addSiblingAction->setEnabled(false);
    m_addChildAction->setEnabled(false);
    m_findUsageAction->setEnabled(false);
  }
}

Device::FullAddressSettings make(const Device::Node& node)
{
  SCORE_ASSERT(node.is<Device::AddressSettings>());
  auto& other = node.get<Device::AddressSettings>();

  Device::FullAddressSettings as;
  static_cast<Device::AddressSettingsCommon&>(as) = other;
  as.address = Device::address(node).address;

  return as;
}

void DeviceExplorerWidget::updateAddressView()
{
  auto indexes = m_ntView->selectedIndexes();

  if (indexes.size() != 1)
  {
    m_addressModel->clear();
    return;
  }

  auto& n = model()->nodeFromModelIndex(sourceIndex(indexes.first()));
  if (n.is<Device::AddressSettings>())
  {
    m_addressModel->setState(model(), Device::NodePath(n), make(n));
  }
  else
  {
    m_addressModel->clear();
  }
}

bool DeviceExplorerWidget::editable() const noexcept
{
  return m_openMenu ? m_openMenu->isEnabled() : false;
}

DeviceExplorerModel* DeviceExplorerWidget::model() const
{
  return m_ntView->model();
}

DeviceExplorerView* DeviceExplorerWidget::view() const
{
  return m_ntView;
}

DeviceExplorerFilterProxyModel* DeviceExplorerWidget::proxyModel()
{
  return m_proxyModel;
}

void DeviceExplorerWidget::edit()
{
  if(!editable())
    return;

  const auto& select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
  if (select.is<Device::DeviceSettings>())
  {
    if (!m_deviceDialog)
    {
      m_deviceDialog = new DeviceEditDialog{m_protocolList, this};
    }
    auto set = select.get<Device::DeviceSettings>();
    m_deviceDialog->setSettings(set);

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

    if (code == QDialog::Accepted)
    {
      auto cmd = new Explorer::Command::UpdateDeviceSettings{
          model()->deviceModel(), set.name, m_deviceDialog->getSettings()};

      m_cmdDispatcher->submit(cmd);
    }

    updateActions();
  }
  else
  {
    auto before = select.get<Device::AddressSettings>();

    if (!model())
      return;

    auto dev_s = getDevice(select);
    if (!dev_s)
      return;
    auto proto = m_protocolList.get(dev_s->protocol);
    if (!proto)
      return;
    auto dev = model()->deviceModel().list().findDevice(dev_s->name);
    if (!dev)
      return;

    auto dial =
        proto->makeEditAddressDialog(before, *dev, model()->deviceModel().context(), this);

    if (!dial)
      return;

    connect(dial, &QDialog::accepted,
            this, [this, dial, select, before] {
      auto stgs = dial->getSettings();
      // TODO do like for DeviceSettings
      if (!model()->checkAddressEditable(*select.parent(), before, stgs))
        return;

      auto cmd = new Explorer::Command::UpdateAddressSettings{
          model()->deviceModel(), Device::NodePath(select), stgs};

      m_cmdDispatcher->submit(cmd);
      updateActions();
      dial->deleteLater();
    });
    connect(dial, &QDialog::rejected,
            this, [this, dial] {
      updateActions();
      dial->deleteLater();
    });

    dial->show();
  }
}

void DeviceExplorerWidget::refresh()
{
  if(!editable())
    return;

  auto m = model();
  if (!m)
    return;

  const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
  if (select.is<Device::DeviceSettings>())
  {
    // Create a thread, ask the device, when it is done put a command on the
    // chain.
    auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
    if (!dev.capabilities().canRefreshTree)
      return;

    if (!dev.connected())
      return;
    auto wrkr = make_worker(
        [=](Device::Node&& node) {
          auto cmd = new Explorer::Command::ReplaceDevice{
              m->deviceModel(), m_ntView->selectedIndex().row(), std::move(node)};

          m_cmdDispatcher->submit(cmd);
        },
        *this,
        dev);

    wrkr->start();
  }
}

void DeviceExplorerWidget::refreshValue()
{
  if(!editable())
    return;

  // TODO deprecate this
  QList<QPair<const Device::Node*, ossia::value>> lst;

  auto expl = model();

  const auto& indices = m_ntView->selectedIndexes();
  for (auto index : indices)
  {
    // Model checks
    index = sourceIndex(index);
    Device::Node* node
        = index.isValid() ? static_cast<Device::Node*>(index.internalPointer()) : nullptr;

    if (!node || node->is<Device::DeviceSettings>())
      continue;

    // Device checks
    auto addr = Device::address(*node);
    auto& dev = model()->deviceModel().list().device(addr.address.device);
    if (!dev.capabilities().canRefreshValue)
      return;
    if (!dev.connected())
      return;

    // Getting the new values
    auto val = dev.refresh(addr.address);
    if (val)
    {
      expl->editData(*node, Explorer::Column::Value, *val, Qt::EditRole);
    }
  }
}

void DeviceExplorerWidget::disconnect()
{
  if(!editable())
    return;

  auto m = model();
  if (!m)
    return;

  const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
  if (select.is<Device::DeviceSettings>())
  {
    auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
    dev.disconnect();
  }
}

void DeviceExplorerWidget::reconnect()
{
  if(!editable())
    return;

  auto m = model();
  if (!m)
    return;

  const Device::Node& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
  if (select.is<Device::DeviceSettings>())
  {
    auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
    auto con_handle = std::make_shared<QMetaObject::Connection>();
    *con_handle = con(
        dev,
        &Device::DeviceInterface::deviceChanged,
        this,
        [&dev, con_handle, select](auto oldd, auto newd) {
          if (newd)
          {
            dev.recreate(select);
            QObject::disconnect(*con_handle);
          }
        });
    dev.reconnect();
  }
}

void DeviceExplorerWidget::addDevice()
{
  if(!editable())
    return;

  if (!m_deviceDialog)
  {
    m_deviceDialog = new DeviceEditDialog{m_protocolList, this};
  }

  connect(m_deviceDialog, &QDialog::accepted,
          this, [this] {
    SCORE_ASSERT(model());
    auto node = m_deviceDialog->getDevice();
    auto& deviceSettings = *node.target<Device::DeviceSettings>();
    if (!model()->checkDeviceInstantiatable(deviceSettings))
    {
      if (!model()->tryDeviceInstantiation(deviceSettings, *m_deviceDialog))
      {
        delete m_deviceDialog;
        m_deviceDialog = nullptr;
        return;
      }
    }
    ossia::net::sanitize_device_name(deviceSettings.name);

    blockGUI(true);

    auto& devplug = model()->deviceModel();
    m_cmdDispatcher->submit(new Command::LoadDevice{devplug, std::move(node)});

    blockGUI(false);

    updateActions();
    m_deviceDialog->deleteLater();
    m_deviceDialog = nullptr;
  });

  connect(m_deviceDialog, &QDialog::rejected,
          this, [this] {
    updateActions();
    m_deviceDialog->deleteLater();
    m_deviceDialog = nullptr;
  });

  m_deviceDialog->show();
}

void DeviceExplorerWidget::exportDevice()
{
  auto indexes = m_ntView->selectedIndexes();

  if (indexes.size() != 1)
    return;
  Device::Node& n = model()->nodeFromModelIndex(sourceIndex(indexes.first()));
  if (!n.is<Device::DeviceSettings>())
    return;

  auto fileName = QFileDialog::getSaveFileName(
        this, tr("Device file"), QString{}, tr("Device file (*.device)"));
  if(!fileName.endsWith(".device"))
    fileName.append(".device");

  QFile f{fileName};
  if (f.open(QIODevice::WriteOnly))
  {
    f.write(toJson(n));
  }
}

void DeviceExplorerWidget::addChild()
{
  addAddress(InsertMode::AsChild);
}

void DeviceExplorerWidget::addSibling()
{
  addAddress(InsertMode::AsSibling);
}

void DeviceExplorerWidget::removeNodes()
{
  if(!editable())
    return;

  auto indexes = m_ntView->selectedIndexes();

  Device::NodeList nodes;
  for (auto index : indexes)
  {
    auto& n = model()->nodeFromModelIndex(sourceIndex(index));
    if (!n.is<InvisibleRootNode>())
      nodes.push_back(&n);
  }

  auto cmd = new Command::RemoveNodes;
  const auto& dev_model = model()->deviceModel();

  // If two nodes have the same parent,
  // we should send the commands in reverse order
  // (from the last to the first)
  // so that they are emplaced in correct order afterwards.
  // IMPORTANT ! don't use emplace, only emplace_back in D.E. model
  struct PathComparator
  {
    bool operator()(const Device::NodePath& lhs, const Device::NodePath& rhs) const
    {
      // We iterate on the shorter.
      // The shorter is considered "smaller" : it comes before.
      int l_size = lhs.size();
      int r_size = rhs.size();
      if (l_size < r_size)
      {
        // lhs shorter
        for (int i = 0; i < l_size; i++)
        {
          if (lhs[i] < rhs[i])
            return true;
          else if (lhs[i] == rhs[i])
            continue;
          else if (lhs[i] > rhs[i])
            return false;
        }
        return true;
      }
      else if (l_size == r_size)
      {
        for (int i = 0; i < l_size; i++)
        {
          if (lhs[i] < rhs[i])
            return true;
          else if (lhs[i] == rhs[i])
            continue;
          else if (lhs[i] > rhs[i])
            return false;
        }
        SCORE_ABORT;
      }
      else
      {
        // rhs shorter
        for (int i = 0; i < r_size; i++)
        {
          if (lhs[i] < rhs[i])
            return true;
          else if (lhs[i] == rhs[i])
            continue;
          else if (lhs[i] > rhs[i])
            return false;
        }
        return false;
      }

      SCORE_ABORT;
    }
  };

  std::set<Device::NodePath, PathComparator> paths;
  for (const auto& n : filterUniqueParents(nodes))
  {
    if (n->is<Device::DeviceSettings>())
    {
      cmd->addCommand(new Explorer::Command::Remove{dev_model, *n});
    }
    else
    {
      paths.insert(*n);
    }
  }
  /*
      for(auto path : paths)
      {
          qDebug() << path;
      }
  */
  for (auto it = paths.rbegin(); it != paths.rend(); ++it)
  {
    cmd->addCommand(new Explorer::Command::Remove{dev_model, Device::NodePath{*it}});
  }

  m_cmdDispatcher->submit(cmd);
}

void DeviceExplorerWidget::learn()
{
  if(!editable())
    return;

  // Get the device
  auto indexes = m_ntView->selectedIndexes();

  if (indexes.size() != 1)
    return;

  auto m = model();
  Device::Node& n = m->nodeFromModelIndex(sourceIndex(indexes.first()));
  if (!n.is<Device::DeviceSettings>())
    return;
  auto di = m->deviceModel().list().findDevice(n.get<Device::DeviceSettings>().name);
  if (!di)
    return;

  if (!di->capabilities().canLearn)
    return;

  if (!di->connected())
    return;

  // Make a copy of the node
  Device::Node oldDevice = n;
  // Show a dialog for as long as there is learn status active
  auto d = new LearnDialog{*di, this};

  auto res = d->exec();
  delete d; // Stops learning

  // Create a command and push it if we agree, undo it and don't push it if we
  // refuse
  Device::Node newDevice = n;
  if (res)
  {
    // Create a command with the current state of the device
    auto cmd = new Explorer::Command::ReloadWholeDevice{
        m->deviceModel(), std::move(oldDevice), std::move(newDevice)};

    // Push it without redoing it since the device already has the nodes
    CommandDispatcher<SendStrategy::Quiet> disp{m_cmdDispatcher->stack()};
    disp.submit(cmd);

    // This way we're able to undo the learn operation
  }
  else
  {
    // We still have to rollback the messages that may have been received
    Explorer::Command::ReloadWholeDevice cmd{
        m->deviceModel(), std::move(oldDevice), std::move(newDevice)};

    // No need to push anything
    cmd.undo(m_cmdDispatcher->stack().context());
  }
}

void DeviceExplorerWidget::findUsage()
{

  auto indexes = m_ntView->selectedIndexes();

  QStringList search_txt;
  for (auto index : indexes)
  {
    auto& n = model()->nodeFromModelIndex(sourceIndex(index));

    State::AddressAccessor address = Device::address(n);

    search_txt.push_back(address.address.toString());
  }
  findAddresses(search_txt);
}

void DeviceExplorerWidget::addAddress(InsertMode insert)
{
  if(!editable())
    return;

  SCORE_ASSERT(model());
  QModelIndex index = proxyModel()->mapToSource(m_ntView->currentIndex());

  // If the node is added in sibling mode, we check that no sibling have
  // the same name
  // Else we check that no child of the index has the same name.
  auto& node = model()->nodeFromModelIndex(index);

  // TODO not very elegant.
  if (insert == InsertMode::AsSibling && node.is<Device::DeviceSettings>())
  {
    return;
  }

  auto dev_s = getDevice(node);
  if (!dev_s)
    return;
  auto proto = m_protocolList.get(dev_s->protocol);
  if (!proto)
    return;
  auto dev = model()->deviceModel().list().findDevice(dev_s->name);
  if (!dev)
    return;

  QScopedPointer<Device::AddressDialog> dial{
      proto->makeAddAddressDialog(*dev, model()->deviceModel().context(), this)};

  if (!dial)
    return;

  auto code = static_cast<QDialog::DialogCode>(dial->exec());

  if (code == QDialog::Accepted)
  {
    Device::Node* parent = (insert == InsertMode::AsChild) ? &node : node.parent();

    auto stgs = dial->getSettings();
    if (!model()->checkAddressInstantiatable(*parent, stgs))
      return;

    // TODO checking for expansion should not be necessary anymore
    bool parent_is_expanded
        = m_ntView->isExpanded(proxyIndex(m_ntView->model()->modelIndexFromNode(*parent, 0)));

    m_cmdDispatcher->submit(new Explorer::Command::AddAddress{
        model()->deviceModel(), Device::NodePath{index}, insert, stgs});

    // If the node is going to be visible, we have to start listening to it.
    if (parent_is_expanded)
    {
      auto child_it = ossia::find_if(*parent, [&](const Device::Node& child) {
        return child.get<Device::AddressSettings>().name == stgs.name;
      });
      SCORE_ASSERT(child_it != parent->end());

      if (m_listeningManager)
        m_listeningManager->enableListening(*child_it);
    }
    updateActions();
  }
}

void DeviceExplorerWidget::filterChanged()
{
  SCORE_ASSERT(m_proxyModel);
  SCORE_ASSERT(m_nameLEdit);

  QString pattern = m_nameLEdit->text();
  Qt::CaseSensitivity cs = Qt::CaseSensitive;

  QRegExp::PatternSyntax syntax = QRegExp::WildcardUnix; // RegExp; //Wildcard; //WildcardUnix; //?
  // See http://qt-project.org/doc/qt-5/QRegExptml#PatternSyntax-enum

  QRegExp regExp(pattern, cs, syntax);

  m_proxyModel->setFilterRegExp(regExp);
  m_proxyModel->setColumn((Explorer::Column)m_columnCBox->currentIndex());
}

DeviceExplorerWidget* findDeviceExplorerWidgetInstance(const score::GUIApplicationContext& ctx) noexcept
{
  for (auto& cpt : ctx.panels())
  {
    if (Explorer::PanelDelegate* panel = dynamic_cast<Explorer::PanelDelegate*>(&cpt))
    {
      return static_cast<Explorer::DeviceExplorerWidget*>(panel->widget());
    }
  }
  return nullptr;
}

}

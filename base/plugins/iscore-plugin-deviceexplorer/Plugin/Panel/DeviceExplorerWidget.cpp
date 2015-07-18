#include "DeviceExplorerWidget.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPushButton>
#include <QLineEdit>

#include "Widgets/AddressEditDialog.hpp"
#include "Widgets/DeviceEditDialog.hpp"

#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerView.hpp"
#include "IOTypeDelegate.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>


#include <Commands/Add/AddDevice.hpp>
#include "Commands/Add/AddAddress.hpp"
#include "Commands/Remove.hpp"


DeviceExplorerWidget::DeviceExplorerWidget(QWidget* parent)
    : QWidget(parent),
      m_proxyModel(nullptr),
      m_deviceDialog(nullptr), m_addressDialog(nullptr)

{
    buildGUI();
}

void
DeviceExplorerWidget::buildGUI()
{
    m_ntView = new DeviceExplorerView(this);

    m_ntView->setItemDelegateForColumn((int)DeviceExplorerModel::Column::IOType, new IOTypeDelegate);

    connect(m_ntView, SIGNAL(selectionChanged()), this, SLOT(updateActions()));


    /*
    m_cmdQ = new iscore::CommandQueue(this);
    m_undoAction = m_cmdQ->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcuts(QKeySequence::Undo);
    m_redoAction = m_cmdQ->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcuts(QKeySequence::Redo);*/

    m_editAction = new QAction(tr("Edit"), this);
    m_editAction->setShortcut(QKeySequence(Qt::Key_Return));

    m_refreshAction = new QAction(tr("Refresh namespace"), this);
    m_refreshAction->setShortcut(QKeySequence::Refresh);

    m_copyAction = new QAction(QIcon(":/resources/images/copy.png"), tr("Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_cutAction = new QAction(QIcon(":/resources/images/cut.png"), tr("Cut"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    m_pasteAction = new QAction(QIcon(":/resources/images/paste.png"), tr("Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_removeNodeAction = new QAction(tr("Remove"), this);
    m_removeNodeAction->setShortcut(QKeySequence::Delete);

    m_moveUpAction = new QAction(QIcon(":/resources/images/up.png"), tr("Move up"), this);
    m_moveUpAction->setShortcut(QKeySequence(tr("Alt+Up")));
    m_moveDownAction = new QAction(QIcon(":/resources/images/down.png"), tr("Move down"), this);
    m_moveDownAction->setShortcut(QKeySequence(tr("Alt+Down")));
    m_promoteAction = new QAction(QIcon(":/resources/images/promote.png"), tr("Promote"), this);
    m_promoteAction->setShortcut(QKeySequence(tr("Alt+Left")));
    m_demoteAction = new QAction(QIcon(":/resources/images/demote.png"), tr("Demote"), this);
    m_demoteAction->setShortcut(QKeySequence(tr("Alt+Right")));

    m_editAction->setEnabled(false);
    m_refreshAction->setEnabled(false);
    m_removeNodeAction->setEnabled(false);
    m_copyAction->setEnabled(false);
    m_cutAction->setEnabled(false);
    m_pasteAction->setEnabled(false);
    m_moveUpAction->setEnabled(false);
    m_moveDownAction->setEnabled(false);
    m_promoteAction->setEnabled(false);
    m_demoteAction->setEnabled(false);


    connect(m_editAction, SIGNAL(triggered()), this, SLOT(edit()));
    connect(m_refreshAction, SIGNAL(triggered()), this, SLOT(refresh()));
    connect(m_copyAction, SIGNAL(triggered()), this, SLOT(copy()));
    connect(m_cutAction, SIGNAL(triggered()), this, SLOT(cut()));
    connect(m_pasteAction, SIGNAL(triggered()), this, SLOT(paste()));
    connect(m_moveUpAction, SIGNAL(triggered()), this, SLOT(moveUp()));
    connect(m_moveDownAction, SIGNAL(triggered()), this, SLOT(moveDown()));
    connect(m_promoteAction, SIGNAL(triggered()), this, SLOT(promote()));
    connect(m_demoteAction, SIGNAL(triggered()), this, SLOT(demote()));
    connect(m_removeNodeAction, SIGNAL(triggered()), this, SLOT(removeNode()));

    /*
    QPushButton *addDeviceButton = new QPushButton(this);
    addDeviceButton->setIcon(QIcon(":/resources/images/addANode.png"));
    addDeviceButton->setToolTip(tr("Add a device..."));
    addDeviceButton->setMaximumSize(QSize(64, 64));
    addDeviceButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    QPushButton *addChildButton = new QPushButton(this);
    addChildButton->setIcon(QIcon(":/resources/images/addChild.png"));
    addChildButton->setToolTip(tr("Add as child..."));
    addChildButton->setMaximumSize(QSize(64, 64));

    QPushButton *addSiblingButton = new QPushButton(this);
    addSiblingButton->setIcon(QIcon(":/resources/images/addSibling.png"));
    addSiblingButton->setToolTip(tr("Add as sibling..."));
    addSiblingButton->setMaximumSize(QSize(64, 64));

    connect(addDeviceButton, SIGNAL(clicked(bool)), this, SLOT(addDevice()));
    connect(addChildButton, SIGNAL(clicked(bool)), this, SLOT(addChild()));
    connect(addSiblingButton, SIGNAL(clicked(bool)), this, SLOT(addSibling()));
    */

    QPushButton* addButton = new QPushButton(this);
    addButton->setIcon(QIcon(":/resources/images/add.png"));
    addButton->setMaximumSize(QSize(32, 32));
    addButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    m_addDeviceAction = new QAction(QIcon(":/resources/images/addDevice.png"), tr("Add device"), this);
    m_addDeviceAction->setShortcut(tr("Ctrl+B"));
    m_addSiblingAction = new QAction(QIcon(":/resources/images/addSibling.png"), tr("Add sibling"), this);
    m_addChildAction = new QAction(QIcon(":/resources/images/addChild.png"), tr("Add child"), this);

    connect(m_addDeviceAction, SIGNAL(triggered()), this, SLOT(addDevice()));
    connect(m_addSiblingAction, SIGNAL(triggered()), this, SLOT(addSibling()));
    connect(m_addChildAction, SIGNAL(triggered()), this, SLOT(addChild()));

    m_addSiblingAction->setEnabled(false);
    m_addChildAction->setEnabled(false);


    QMenu* addMenu = new QMenu(this);
    addMenu->addAction(m_addDeviceAction);
    addMenu->addAction(m_addSiblingAction);
    addMenu->addAction(m_addChildAction);
    addMenu->addSeparator();
    addMenu->addAction(m_removeNodeAction);

    addButton->setMenu(addMenu);


    QPushButton* editButton = new QPushButton(this);
    editButton->setIcon(QIcon(":/resources/images/edit.png"));
    editButton->setMaximumSize(QSize(32, 32));
    editButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    QMenu* editMenu = new QMenu(this);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_moveUpAction);
    editMenu->addAction(m_moveDownAction);
    editMenu->addAction(m_promoteAction);
    editMenu->addAction(m_demoteAction);
    editMenu->addSeparator();/*
  editMenu->addAction(m_undoAction);
  editMenu->addAction(m_redoAction);*/

    editButton->setMenu(editMenu);



    m_columnCBox = new QComboBox(this);
    m_nameLEdit = new QLineEdit(this);

    connect(m_columnCBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged()));
    connect(m_nameLEdit, SIGNAL(textEdited(const QString&)), this, SLOT(filterChanged()));

    QHBoxLayout* filterHLayout = new QHBoxLayout;
    filterHLayout->setContentsMargins(0, 0, 0, 0);
    filterHLayout->addWidget(m_columnCBox);
    filterHLayout->addWidget(m_nameLEdit);




    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(addButton);
    hLayout->addWidget(editButton);
    //hLayout->addStretch(0);
    hLayout->addLayout(filterHLayout);
    hLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(m_ntView);
    vLayout->addLayout(hLayout);

    setLayout(vLayout);


    installStyleSheet();
}

void
DeviceExplorerWidget::installStyleSheet()
{
    setStyleSheet(
        "* {"
        // "background-color: #bababa;"
        "}"
    );
}



void
DeviceExplorerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    updateActions();
    QMenu* contextMenu = new QMenu(this);

    connect(contextMenu, &QMenu::triggered,
            [=] () { contextMenu->deleteLater(); });
    contextMenu->addAction(m_editAction);
    contextMenu->addAction(m_refreshAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_addDeviceAction);
    contextMenu->addAction(m_addSiblingAction);
    contextMenu->addAction(m_addChildAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_copyAction);
    contextMenu->addAction(m_cutAction);
    contextMenu->addAction(m_pasteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_moveUpAction);
    contextMenu->addAction(m_moveDownAction);
    contextMenu->addAction(m_promoteAction);
    contextMenu->addAction(m_demoteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_removeNodeAction);/*
  contextMenu->addAction(m_undoAction);
  contextMenu->addAction(m_redoAction);*/

    contextMenu->exec(event->globalPos());
}

void
DeviceExplorerWidget::setModel(DeviceExplorerModel* model)
{
    delete m_proxyModel;    //? will also delete previous model ??
    m_proxyModel = nullptr;
    delete m_cmdDispatcher;
    m_cmdDispatcher = nullptr;

    if (model)
    {
        m_proxyModel = new DeviceExplorerFilterProxyModel(this);
        m_proxyModel->setSourceModel(model);
        m_ntView->setModel(m_proxyModel);
        model->setView(m_ntView);


        m_cmdDispatcher = new CommandDispatcher<SendStrategy::Simple>{
                iscore::IDocument::documentFromObject(model)->commandStack()};

        populateColumnCBox();

        updateActions();
    }
    else
    {
        m_ntView->setModel((QAbstractItemModel*)nullptr);
    }
}

void
DeviceExplorerWidget::populateColumnCBox()
{
    Q_ASSERT(model());
    Q_ASSERT(m_columnCBox);

    QStringList columns = model()->getColumns();
    m_columnCBox->addItems(columns);
}

void
DeviceExplorerWidget::updateActions()
{
    Q_ASSERT(model());

    if(! model()->isEmpty())
    {

        //TODO: choice for multi selection

        Q_ASSERT(m_ntView);

        QModelIndexList selection = m_ntView->selectedIndexes();

        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(false);

        if(selection.isEmpty())
        {
            m_editAction->setEnabled(false);
            m_refreshAction->setEnabled(false);
            m_copyAction->setEnabled(false);
            m_cutAction->setEnabled(false);
            m_promoteAction->setEnabled(false);
            m_demoteAction->setEnabled(false);
            m_moveUpAction->setEnabled(false);
            m_moveDownAction->setEnabled(false);
            m_removeNodeAction->setEnabled(false);
        }
        else
        {
            m_refreshAction->setEnabled(true);
            m_copyAction->setEnabled(true);
            m_cutAction->setEnabled(true);
//            m_moveUpAction->setEnabled(true);
//            m_moveDownAction->setEnabled(true);
        }

        if(selection.size() == 1)
        {
            const bool aDeviceIsSelected = model()->isDevice(m_ntView->selectedIndex());

            if(! aDeviceIsSelected)
            {
                m_addSiblingAction->setEnabled(true);
                m_promoteAction->setEnabled(true);
                m_demoteAction->setEnabled(true);
                m_removeNodeAction->setEnabled(true);
                m_moveUpAction->setEnabled(true);
                m_moveDownAction->setEnabled(true);
            }
            else
            {
                m_addSiblingAction->setEnabled(false);
                m_promoteAction->setEnabled(false);
                m_demoteAction->setEnabled(false);
                m_removeNodeAction->setEnabled(false);
                m_moveUpAction->setEnabled(false);
                m_moveDownAction->setEnabled(false);
            }
            m_editAction->setEnabled(true);
            m_addChildAction->setEnabled(true);
        }

    }
    else
    {
        m_editAction->setEnabled(false);
        m_refreshAction->setEnabled(false);
        m_copyAction->setEnabled(false);
        m_cutAction->setEnabled(false);
        m_promoteAction->setEnabled(false);
        m_demoteAction->setEnabled(false);
        m_moveUpAction->setEnabled(false);
        m_moveDownAction->setEnabled(false);
        m_removeNodeAction->setEnabled(false);
        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(false);
    }


    m_pasteAction->setEnabled(model()->hasCut());
}


DeviceExplorerModel*
DeviceExplorerWidget::model()
{
    return m_ntView->model();
}

DeviceExplorerFilterProxyModel*
DeviceExplorerWidget::proxyModel()
{
    return m_proxyModel;
}

void DeviceExplorerWidget::edit()
{
    // TODO there should be a command here
    iscore::Node* select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if ( model()->isDevice(m_ntView->selectedIndex()))
    {
        if(! m_deviceDialog)
        {
            m_deviceDialog = new DeviceEditDialog(this);
        }
        auto set = select->deviceSettings();
        m_deviceDialog->setSettings(set);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto deviceSettings = m_deviceDialog->getSettings();
            select->setDeviceSettings(deviceSettings);
        }

        updateActions();
    }
    else
    {
        if (! m_addressDialog)
        {
            m_addressDialog = new AddressEditDialog(this);
        }
        auto settings = select->addressSettings();
        m_addressDialog->setSettings(settings);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_addressDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto addressSettings = m_addressDialog->getSettings();
            select->setAddressSettings(addressSettings);
        }
        updateActions();
    }
}

#include "ExplorationWorker.hpp"
#include "Commands/UpdateNamespace.hpp"
#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"
void DeviceExplorerWidget::refresh()
{
    iscore::Node* select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if ( model()->isDevice(m_ntView->selectedIndex()))
    {
        // Create a thread, ask the device, when it is done put a command on the chain.
        auto& dev = model()->deviceModel()->list().device(select->deviceSettings().name);
        if(!dev.canRefresh())
            return;

        auto thread = new QThread;
        auto worker = new ExplorationWorker{dev};

        connect(thread, &QThread::started, worker, &ExplorationWorker::process, Qt::QueuedConnection);
        connect(worker, &ExplorationWorker::finished, this,
                [=] () {
            auto cmd = new DeviceExplorer::Command::ReplaceDevice{
                    iscore::IDocument::path(model()),
                    m_ntView->selectedIndex().row(),
                    std::move(worker->node)};

            m_cmdDispatcher->submitCommand(cmd);

            thread->quit();
            worker->deleteLater();
        }, Qt::QueuedConnection);

        worker->moveToThread(thread);
        thread->start();
    }
}

void
DeviceExplorerWidget::addDevice()
{

    if(! m_deviceDialog)
    {
        m_deviceDialog = new DeviceEditDialog(this);
    }

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

    if(code == QDialog::Accepted)
    {
        auto deviceSettings = m_deviceDialog->getSettings();
        auto path = m_deviceDialog->getPath();
        Q_ASSERT(model());
        m_cmdDispatcher->submitCommand(new AddDevice{iscore::IDocument::path(model()), deviceSettings, path});
        //TODO: we should set the focus on this Node & expand it
        //m_ntView->setCurrentIndex(?)
    }

    //m_deviceDialog->hide();

    updateActions();
}

void
DeviceExplorerWidget::addChild()
{
    addAddress(DeviceExplorerModel::Insert::AsChild);
}

void
DeviceExplorerWidget::addSibling()
{
    addAddress(DeviceExplorerModel::Insert::AsSibling);

    //QModelIndex index = m_ntView->currentIndex();
    //getModel()->addNode(index, DeviceExplorerModel::AsSibling)  ;
}

void DeviceExplorerWidget::removeNode()
{
    iscore::Node* n = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if(! n->isDevice())
        m_cmdDispatcher->submitCommand(new DeviceExplorer::Command::Remove{iscore::IDocument::path(model()), Path(n)});
}

void
DeviceExplorerWidget::addAddress(DeviceExplorerModel::Insert insert)
{
    if(! m_addressDialog)
    {
        m_addressDialog = new AddressEditDialog(this);
    }
    m_addressDialog->setSettings(AddressEditDialog::makeDefaultSettings());

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_addressDialog->exec());

    if(code == QDialog::Accepted)
    {
        const AddressSettings addressSettings = m_addressDialog->getSettings();
        Q_ASSERT(model());
        QModelIndex index = proxyModel()->mapToSource(m_ntView->currentIndex());
        m_cmdDispatcher->submitCommand(new DeviceExplorer::Command::AddAddress{iscore::IDocument::path(model()), Path{index}, insert, addressSettings });
//        model()->addAddress(index, insert, addressSettings);
        //TODO: we should set the focus on this Node & expand it
        //m_ntView->setCurrentIndex(?)
        updateActions();
    }

}

void
DeviceExplorerWidget::copy()
{
    Q_ASSERT(m_ntView);
    m_ntView->copy();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}

void
DeviceExplorerWidget::cut()
{
    Q_ASSERT(m_ntView);
    m_ntView->cut();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}

void
DeviceExplorerWidget::paste()
{
    Q_ASSERT(m_ntView);
    m_ntView->paste();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}



void
DeviceExplorerWidget::moveUp()
{
    Q_ASSERT(m_ntView);
    m_ntView->moveUp();
}

void
DeviceExplorerWidget::moveDown()
{
    Q_ASSERT(m_ntView);
    m_ntView->moveDown();
}

void
DeviceExplorerWidget::promote()
{
    Q_ASSERT(m_ntView);
    m_ntView->promote();
}

void
DeviceExplorerWidget::demote()
{
    Q_ASSERT(m_ntView);
    m_ntView->demote();
}

void
DeviceExplorerWidget::filterChanged()
{
    Q_ASSERT(m_proxyModel);
    Q_ASSERT(m_nameLEdit);

    QString pattern = m_nameLEdit->text();
    Qt::CaseSensitivity cs = Qt::CaseSensitive;

    QRegExp::PatternSyntax syntax = QRegExp::WildcardUnix; //RegExp; //Wildcard; //WildcardUnix; //?
    //See http://qt-project.org/doc/qt-5/qregexp.html#PatternSyntax-enum

    QRegExp regExp(pattern, cs, syntax);

    m_proxyModel->setFilterRegExp(regExp);

}

DeviceExplorerWidget::~DeviceExplorerWidget()
{
    delete m_cmdDispatcher;
}

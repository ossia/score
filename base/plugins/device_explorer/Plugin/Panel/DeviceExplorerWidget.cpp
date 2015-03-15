#include "DeviceExplorerWidget.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPushButton>
#include <QLineEdit>

#include "AddressEditDialog.hpp"
#include "DeviceEditDialog.hpp"
#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerView.hpp"
#include "IOTypeDelegate.hpp"

#include <core/command/CommandStack.hpp>

#include <QDebug>

#include <iostream> //DEBUG


DeviceExplorerWidget::DeviceExplorerWidget(QWidget* parent)
    : QWidget(parent),
      m_proxyModel(nullptr),
      m_deviceDialog(nullptr), m_addressDialog(nullptr)   //,
      //m_cmdQ(nullptr)
{
    buildGUI();

}

void
DeviceExplorerWidget::buildGUI()
{
    m_ntView = new DeviceExplorerView(this);

    m_ntView->setItemDelegateForColumn(m_ntView->getIOTypeColumn(), new IOTypeDelegate);

    connect(m_ntView, SIGNAL(selectionChanged()), this, SLOT(updateActions()));


    /*
    m_cmdQ = new iscore::CommandQueue(this);
    m_undoAction = m_cmdQ->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcuts(QKeySequence::Undo);
    m_redoAction = m_cmdQ->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcuts(QKeySequence::Redo);*/


    m_copyAction = new QAction(QIcon(":/resources/images/copy.png"), tr("Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_cutAction = new QAction(QIcon(":/resources/images/cut.png"), tr("Cut"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    m_pasteAction = new QAction(QIcon(":/resources/images/paste.png"), tr("Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);

    m_moveUpAction = new QAction(QIcon(":/resources/images/up.png"), tr("Move up"), this);
    m_moveUpAction->setShortcut(QKeySequence(tr("Alt+Up")));
    m_moveDownAction = new QAction(QIcon(":/resources/images/down.png"), tr("Move down"), this);
    m_moveDownAction->setShortcut(QKeySequence(tr("Alt+Down")));
    m_promoteAction = new QAction(QIcon(":/resources/images/promote.png"), tr("Promote"), this);
    m_promoteAction->setShortcut(QKeySequence(tr("Alt+Left")));
    m_demoteAction = new QAction(QIcon(":/resources/images/demote.png"), tr("Demote"), this);
    m_demoteAction->setShortcut(QKeySequence(tr("Alt+Right")));

    m_copyAction->setEnabled(false);
    m_cutAction->setEnabled(false);
    m_pasteAction->setEnabled(false);
    m_moveUpAction->setEnabled(false);
    m_moveDownAction->setEnabled(false);
    m_promoteAction->setEnabled(false);
    m_demoteAction->setEnabled(false);

    connect(m_copyAction, SIGNAL(triggered()), this, SLOT(copy()));
    connect(m_cutAction, SIGNAL(triggered()), this, SLOT(cut()));
    connect(m_pasteAction, SIGNAL(triggered()), this, SLOT(paste()));
    connect(m_moveUpAction, SIGNAL(triggered()), this, SLOT(moveUp()));
    connect(m_moveDownAction, SIGNAL(triggered()), this, SLOT(moveDown()));
    connect(m_promoteAction, SIGNAL(triggered()), this, SLOT(promote()));
    connect(m_demoteAction, SIGNAL(triggered()), this, SLOT(demote()));

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
    //TODO: decide which actions to show according to what's possible ?
    QMenu contextMenu(this);
    contextMenu.addAction(m_addDeviceAction);
    contextMenu.addAction(m_addSiblingAction);
    contextMenu.addAction(m_addChildAction);
    contextMenu.addSeparator();
    contextMenu.addAction(m_copyAction);
    contextMenu.addAction(m_cutAction);
    contextMenu.addAction(m_pasteAction);
    contextMenu.addSeparator();
    contextMenu.addAction(m_moveUpAction);
    contextMenu.addAction(m_moveDownAction);
    contextMenu.addAction(m_promoteAction);
    contextMenu.addAction(m_demoteAction);
    contextMenu.addSeparator();/*
  contextMenu.addAction(m_undoAction);
  contextMenu.addAction(m_redoAction);*/

    contextMenu.exec(event->globalPos());
}

void
DeviceExplorerWidget::setModel(DeviceExplorerModel* model)
{
    if(m_proxyModel)
    {
        delete m_proxyModel;    //? will also delete previous model ??
    }

    Q_ASSERT(model);
    m_proxyModel = new DeviceExplorerFilterProxyModel(this);
    m_proxyModel->setSourceModel(model);
    m_ntView->setModel(m_proxyModel);
    model->setView(m_ntView);

    populateColumnCBox();

    updateActions();
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

        //TODO: we should also test/handle multi-selection !?!

        Q_ASSERT(m_ntView);
        QModelIndexList selection = m_ntView->selectedIndexes();

        //std::cerr<<"DeviceExplorerWidget::updateActions() selection.size()="<<selection.size()<<"\n";


        if(selection.size() == 1)
        {

            const bool aDeviceIsSelected = model()->isDevice(selection.at(0));

            if(! aDeviceIsSelected)
            {
                m_addSiblingAction->setEnabled(true);
                m_promoteAction->setEnabled(true);
                m_demoteAction->setEnabled(true);
            }
            else
            {
                m_addSiblingAction->setEnabled(false);
                m_promoteAction->setEnabled(false);
                m_demoteAction->setEnabled(false);
            }

            m_addChildAction->setEnabled(true);
            m_copyAction->setEnabled(true);
            m_cutAction->setEnabled(true);
            m_moveUpAction->setEnabled(true);
            m_moveDownAction->setEnabled(true);
        }
        else
        {
            m_copyAction->setEnabled(false);
            m_cutAction->setEnabled(false);
            m_promoteAction->setEnabled(false);
            m_demoteAction->setEnabled(false);
            m_moveUpAction->setEnabled(false);
            m_moveDownAction->setEnabled(false);
        }

    }
    else
    {
        m_copyAction->setEnabled(false);
        m_cutAction->setEnabled(false);
        m_promoteAction->setEnabled(false);
        m_demoteAction->setEnabled(false);
        m_moveUpAction->setEnabled(false);
        m_moveDownAction->setEnabled(false);
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


bool
DeviceExplorerWidget::loadModel(const QString filename)
{
    Q_ASSERT(m_ntView);
    Q_ASSERT(m_ntView->model());

    const bool loadOk = m_ntView->model()->load(filename);
    //if (loadOk) {
    populateColumnCBox();
    updateActions();
    //}

    return loadOk;

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
        QList<QString> deviceSettings = m_deviceDialog->getSettings();
        Q_ASSERT(model());
        model()->addDevice(deviceSettings);  //TODO: pass a API::Device ???
        //TODO: we should set the focus on this Node & expand it
        //m_ntView->setCurrentIndex(?)
    }

    //m_deviceDialog->hide();

    updateActions();
}

void
DeviceExplorerWidget::addChild()
{
    addAddress(DeviceExplorerModel::AsChild);
}

void
DeviceExplorerWidget::addSibling()
{
    addAddress(DeviceExplorerModel::AsSibling);

    //QModelIndex index = m_ntView->currentIndex();
    //getModel()->addNode(index, DeviceExplorerModel::AsSibling)  ;
}

void
DeviceExplorerWidget::addAddress(int insertType)
{
    DeviceExplorerModel::Insert insert = static_cast<DeviceExplorerModel::Insert>(insertType);


    if(! m_addressDialog)
    {
        m_addressDialog = new AddressEditDialog(this);
    }

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_addressDialog->exec());

    if(code == QDialog::Accepted)
    {
        QList<QString> addressSettings = m_addressDialog->getSettings();
        Q_ASSERT(model());
        QModelIndex index = proxyModel()->mapToSource(m_ntView->currentIndex());
        model()->addAddress(index, insert, addressSettings);  //TODO: pass a API::Device ???
        //TODO: we should set the focus on this Node & expand it
        //m_ntView->setCurrentIndex(?)
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

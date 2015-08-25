#include "GroupTableWidget.hpp"

#include "GroupHeaderItem.hpp"
#include "SessionHeaderItem.hpp"
#include "GroupTableCheckbox.hpp"

#include "DistributedScenario/Group.hpp"
#include "session/Session.hpp"

#include "DistributedScenario/Commands/AddClientToGroup.hpp"
#include "DistributedScenario/Commands/RemoveClientFromGroup.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QLabel>
#include <QGridLayout>

#include "DistributedScenario/GroupManager.hpp"
GroupTableWidget::GroupTableWidget(const GroupManager* mgr, const Session* session, QWidget* parent):
    QWidget{parent},
    m_mgr{mgr},
    m_session{session},
    m_managerPath{iscore::IDocument::path(m_mgr)},
    m_dispatcher{iscore::IDocument::documentFromObject(m_mgr)->commandStack()}
{
    connect(m_mgr, &GroupManager::groupAdded, this, &GroupTableWidget::setup);
    connect(m_mgr, &GroupManager::groupRemoved, this, &GroupTableWidget::setup);
    connect(m_session, &Session::clientsChanged, this, &GroupTableWidget::setup);

    this->setLayout(new QGridLayout);
    this->layout()->addWidget(new QLabel{"Execution table"});

    setup();
}


void GroupTableWidget::setup()
{
    delete m_table;
    m_table = new QTableWidget;
    this->layout()->addWidget(m_table);

    // Groups
    for(unsigned int i = 0; i < m_mgr->groups().size(); i++)
    {
        m_table->insertColumn(i);
        m_table->setHorizontalHeaderItem(i, new GroupHeaderItem{*m_mgr->groups()[i]});
    }

    // Clients
    m_table->insertRow(0); // Local client
    m_table->setVerticalHeaderItem(0, new SessionHeaderItem{m_session->localClient()});

    for(int i = 0; i < m_session->remoteClients().size(); i++)
    {
        m_table->insertRow(i + 1);
        m_table->setVerticalHeaderItem(i + 1, new SessionHeaderItem{*m_session->remoteClients()[i]});
    }

    // Set the data
    using namespace std;
    for(int row = 0; row < m_session->remoteClients().size() + 1; row++)
    {
        for(unsigned int col = 0; col < m_mgr->groups().size(); col++)
        {
            auto cb = new GroupTableCheckbox;
            m_table->setCellWidget(row, col, cb);
            connect(cb, &GroupTableCheckbox::stateChanged,
                    this, [=] (int state)
            { on_checkboxChanged(row, col, state); });
        }
    }

    // Handlers
    for(unsigned int i = 0; i < m_mgr->groups().size(); i++)
    {
        connect(m_mgr->groups()[i], &Group::clientAdded,
                m_table, [=] (id_type<Client> addedClient)
        { findCheckbox(i, addedClient)->setState(Qt::Checked); });

        connect(m_mgr->groups()[i], &Group::clientRemoved,
                m_table, [=] (id_type<Client> removedClient)
        { findCheckbox(i, removedClient)->setState(Qt::Unchecked); });
    }

}

GroupTableCheckbox* GroupTableWidget::findCheckbox(int i,  id_type<Client> theClient) const
{
    if(theClient == m_session->localClient().id())
    {
        auto widg = m_table->cellWidget(0, i);
        return static_cast<GroupTableCheckbox*>(widg);
    }

    for(int j = 0; j < m_session->remoteClients().size(); j++)
    {
        if(static_cast<SessionHeaderItem*>(m_table->verticalHeaderItem(j+1))->client == theClient)
        {
            return static_cast<GroupTableCheckbox*>(m_table->cellWidget(j+1, i));
        }
    }

    ISCORE_ABORT;
    return nullptr;
}

void GroupTableWidget::on_checkboxChanged(int i, int j, int state)
{
    // Lookup id's from the row / column headers
    auto client = static_cast<SessionHeaderItem*>(m_table->verticalHeaderItem(i))->client;
    auto group  = static_cast<GroupHeaderItem*>(m_table->horizontalHeaderItem(j))->group;

    // Find if we have to perform the change.
    auto client_is_in_group = m_mgr->group(group)->clients().contains(client);

    if(state)
    {
        if(client_is_in_group) return;
        auto cmd = new AddClientToGroup(ObjectPath{m_managerPath}, client, group);
        m_dispatcher.submitCommand(cmd);
    }
    else
    {
        if(!client_is_in_group) return;
        auto cmd = new RemoveClientFromGroup(ObjectPath{m_managerPath}, client, group);
        m_dispatcher.submitCommand(cmd);
    }
}

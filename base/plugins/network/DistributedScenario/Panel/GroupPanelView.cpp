#include "GroupPanelView.hpp"
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelViewInterface.hpp>


#include "DistributedScenario/Group.hpp"
#include "Repartition/session/ClientSession.hpp"
#include "Repartition/session/MasterSession.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QTableWidget>
#include "GroupTableCheckbox.hpp"
#include "GroupPanelModel.hpp"

#include "DistributedScenario/Commands/AddClientToGroup.hpp"
#include "DistributedScenario/Commands/RemoveClientFromGroup.hpp"
#include "DistributedScenario/Commands/CreateGroup.hpp"

#include <iscore/command/OngoingCommandManager.hpp>
class GroupHeaderItem : public QTableWidgetItem
{
    public:
        template<typename... Args>
        GroupHeaderItem(const Group& group, Args&&... args):
            QTableWidgetItem{group.name(), std::forward<Args>(args)...},
            group{group.id()}
        {

        }

        const id_type<Group> group;
};

class SessionHeaderItem : public QTableWidgetItem
{
    public:
        template<typename... Args>
        SessionHeaderItem(const Client& client, Args&&... args):
            QTableWidgetItem{client.name(), std::forward<Args>(args)...},
            client{client.id()}
        {

        }

        const id_type<Client> client;
};

class GroupTableWidget : public QWidget
{
        QTableWidget* m_table{new QTableWidget};

    public:
#include "DistributedScenario/Commands/CreateGroup.hpp"
        GroupTableWidget(const GroupManager* mgr, const Session* session, QWidget* parent):
            QWidget{parent},
            m_mgr{mgr},
            m_session{session}
        {
            connect(m_mgr, &GroupManager::groupAdded, this, &GroupTableWidget::setup);
            connect(m_mgr, &GroupManager::groupRemoved, this, &GroupTableWidget::setup);
            connect(m_session, &Session::clientsChanged, this, &GroupTableWidget::setup);

            this->setLayout(new QGridLayout);
            this->layout()->addWidget(new QLabel{"Execution table"});
            this->layout()->addWidget(m_table);

            setup();
        }

        void setup()
        {
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

            //Set the data
            using namespace std;
            for(unsigned int i = 0; i < m_mgr->groups().size(); i++)
            {
                for(int j = 0; j < m_session->remoteClients().size() + 1; j++)
                {
                    auto cb =  new GroupTableCheckbox;
                    m_table->setCellWidget(j, i, cb);
                    connect(cb, &GroupTableCheckbox::stateChanged, this, [=] (int state)
                    {
                        // Lookup id's from the row / column headers
                        auto group  = static_cast<GroupHeaderItem*>(m_table->horizontalHeaderItem(i))->group;
                        auto client = static_cast<SessionHeaderItem*>(m_table->verticalHeaderItem(j))->client;

                        // Find if we have to perform the change.
                        auto groupclients = m_mgr->group(group)->clients();
                        auto it = std::find(begin(groupclients), end(groupclients), client);

                        if(state)
                        {
                             if(it != end(groupclients)) return;
                             auto cmd = new AddClientToGroup(ObjectPath{m_managerPath}, client, group);
                             m_dispatcher.submitCommand(cmd);
                        }
                        else
                        {
                            if(it == end(groupclients)) return;
                            auto cmd = new RemoveClientFromGroup(ObjectPath{m_managerPath}, client, group);
                            m_dispatcher.submitCommand(cmd);
                        }
                    });

                }
            }

            // Handlers

            delete m_groupConnectionContext;
            m_groupConnectionContext = new QObject;

            auto findCheckbox = [this] (int i, id_type<Client> theClient)
            {
                if(theClient == m_session->localClient().id())
                {
                    return static_cast<GroupTableCheckbox*>(m_table->cellWidget(0, i));
                }

                for(int j = 0; j < m_session->remoteClients().size(); j++)
                {
                    if(static_cast<SessionHeaderItem*>(m_table->verticalHeaderItem(j))->client == theClient)
                    {
                        return static_cast<GroupTableCheckbox*>(m_table->cellWidget(j+1, i));
                    }
                }

                Q_ASSERT(false);
            };

            for(unsigned int i = 0; i < m_mgr->groups().size(); i++)
            {
                connect(m_mgr->groups()[i], &Group::clientAdded,
                        m_groupConnectionContext, [=] (id_type<Client> addedClient)
                {
                    findCheckbox(i, addedClient)->setState(Qt::Checked);
                });

                connect(m_mgr->groups()[i], &Group::clientRemoved,
                        m_groupConnectionContext, [=] (id_type<Client> removedClient)
                {
                    findCheckbox(i, removedClient)->setState(Qt::Unchecked);
                });
            }

        }

    private:
        QObject* m_groupConnectionContext{};
        const GroupManager* m_mgr;
        const Session* m_session;

        ObjectPath m_managerPath{iscore::IDocument::path(m_mgr)};
        CommandDispatcher<> m_dispatcher{iscore::IDocument::documentFromObject(m_mgr)->commandStack(), nullptr};
};

class GroupWidget : public QWidget
{
    public:
        GroupWidget(Group* group, QWidget* parent):
            QWidget{parent},
            m_group{group}
        {
            auto lay = new QHBoxLayout{this};
            lay->addWidget(new QLabel{group->name()});

            auto rename = new QPushButton(QObject::tr("Rename"));
            lay->addWidget(rename);

            auto remove = new QPushButton(QObject::tr("Remove"));
            lay->addWidget(remove);
        }

        auto id() const
        { return m_group->id(); }

    private:
        Group* m_group;
};


class GroupListWidget : public QWidget
{
    public:
        GroupListWidget(const GroupManager* mgr, QWidget* parent):
            QWidget{parent},
            m_mgr{mgr}
        {
            this->setLayout(new QVBoxLayout);
            for(auto& group : m_mgr->groups())
            {
                auto widg = new GroupWidget{group, this};
                this->layout()->addWidget(widg);
                m_widgets.append(widg);
            }

            connect(m_mgr, &GroupManager::groupAdded, this, &GroupListWidget::addGroup);
            connect(m_mgr, &GroupManager::groupRemoved, this, &GroupListWidget::removeGroup);
        }

    private:
        void addGroup(const id_type<Group>& id)
        {
            auto widg = new GroupWidget{m_mgr->group(id), this};
            this->layout()->addWidget(widg);
            m_widgets.append(widg);
        }

        void removeGroup(const id_type<Group>& id)
        {
            using namespace std;
            auto it = find_if(begin(m_widgets),
                              end(m_widgets),
                              [&] (GroupWidget* widg)
            { return widg->id() == id; } );

            m_widgets.removeOne(*it);
            delete *it;

        }

        const GroupManager* m_mgr{};
        QList<GroupWidget*> m_widgets;
};

class GroupPanelView : public iscore::PanelViewInterface
{
    public:
        GroupPanelView(iscore::View* v):
            m_widget{new QWidget}
        {
            setObjectName(tr("Groups"));
            auto lay = new QVBoxLayout;
            m_widget->setLayout(lay);
        }

        QWidget* getWidget() override
        { return m_widget; }

        Qt::DockWidgetArea defaultDock() const override
        { return Qt::LeftDockWidgetArea; }

        void setView(const GroupManager* mgr, const Session* session)
        {
            // Make the containing widget
            delete m_subWidget;
            m_subWidget = new QWidget;

            auto lay = new QVBoxLayout;
            m_subWidget->setLayout(lay);

            m_widget->layout()->addWidget(m_subWidget);

            // The sub-widgets (group data presentation)
            m_widget->layout()->addWidget(new GroupListWidget{mgr, m_subWidget});

            // Add group button
            auto button = new QPushButton{tr("Add group")};
            ObjectPath mgrpath{iscore::IDocument::path(mgr)};
            connect(button, &QPushButton::pressed, this, [=] ( )
            {
                bool ok;
                QString text = QInputDialog::getText(m_widget, tr("New group"),
                                                     tr("Group name:"), QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty())
                {
                    auto cmd = new CreateGroup{ObjectPath{mgrpath}, text};

                    CommandDispatcher<> dispatcher{
                        iscore::IDocument::documentFromObject(mgr)->commandStack(),
                                nullptr};
                    dispatcher.submitCommand(cmd);
                }
            });
            m_subWidget->layout()->addWidget(button);

            // Group table
            m_subWidget->layout()->addWidget(new GroupTableWidget{mgr, session, m_widget});
        }

        void setEmptyView()
        {
            delete m_subWidget;
        }

    private:
        QWidget* m_widget{}, *m_subWidget{};
};


class GroupPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        GroupPanelPresenter(iscore::Presenter* parent_presenter,
                            iscore::PanelViewInterface* view):
            iscore::PanelPresenterInterface{parent_presenter, view}
        {
        }

        QString modelObjectName() const override
        {
            return "GroupPanelModel";
        }

        void on_modelChanged() override
        {
            auto gmodel = static_cast<GroupPanelModel*>(model());
            connect(gmodel, &GroupPanelModel::update, this, &GroupPanelPresenter::on_update);
        }

        void on_update()
        {
            auto gmodel = static_cast<GroupPanelModel*>(model());
            auto gview = static_cast<GroupPanelView*>(view());

            if(gmodel->manager())
            {
                gview->setView(gmodel->manager(), gmodel->session());
            }
            else
            {
                gview->setEmptyView();
            }
        }

};

iscore::PanelViewInterface*GroupPanelFactory::makeView(iscore::View* v)
{
    return new GroupPanelView{v};
}

iscore::PanelPresenterInterface*GroupPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
                                                                 iscore::PanelViewInterface* view)
{
    return new GroupPanelPresenter{parent_presenter, view};
}

iscore::PanelModelInterface*GroupPanelFactory::makeModel(iscore::DocumentModel* m)
{
    return new GroupPanelModel{m};
}

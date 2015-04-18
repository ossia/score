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
#include <QTableWidget>
#include "GroupTableCheckbox.hpp"
#include "GroupPanelModel.hpp"

#include "DistributedScenario/Commands/AddClientToGroup.hpp"
#include "DistributedScenario/Commands/RemoveClientFromGroup.hpp"

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
        GroupTableWidget(const GroupManager* mgr, const Session* session, QWidget* parent):
            QWidget{parent},
            m_mgr{mgr},
            m_session{session}
        {
            connect(m_mgr, &GroupManager::groupsChanged, this, &GroupTableWidget::setup);
            connect(m_session, &Session::clientsChanged, this, &GroupTableWidget::setup);

            this->setLayout(new QGridLayout);
            this->layout()->addWidget(new QLabel{"Execution table"});
            this->layout()->addWidget(m_table);

            setup();
        }

        void setup()
        {

            // Groups
            for(int i = 0; i < m_mgr->groups().size(); i++)
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
            for(int i = 0; i < m_mgr->groups().size(); i++)
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

                        if(state)
                        {
                             auto cmd = new AddClientToGroup(ObjectPath{m_managerPath}, client, group);
                             m_dispatcher.submitCommand(cmd);
                        }
                        else
                        {
                            auto cmd = new RemoveClientFromGroup(ObjectPath{m_managerPath}, client, group);
                            m_dispatcher.submitCommand(cmd);
                        }
                    });

                }
            }

        }

    private:
        const GroupManager* m_mgr;
        const Session* m_session;

        ObjectPath m_managerPath{iscore::IDocument::path(m_mgr)};
        CommandDispatcher<> m_dispatcher{iscore::IDocument::documentFromObject(m_mgr)->commandStack(), nullptr};


};

class GroupWidget : public QWidget
{
    public:
        GroupWidget(Group* group, QWidget* parent):
            QWidget{parent}
        {
            auto lay = new QHBoxLayout{this};
            lay->addWidget(new QLabel{group->name()});

            auto rename = new QPushButton(QObject::tr("Rename"));
            lay->addWidget(rename);

            auto remove = new QPushButton(QObject::tr("Remove"));
            lay->addWidget(remove);
        }
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
            for(auto& group : mgr->groups())
            {
                m_widget->layout()->addWidget(new GroupWidget{group, m_widget});
            }

            m_widget->layout()->addWidget(new GroupTableWidget{mgr, session, m_widget});
        }

        void setEmptyView()
        {

        }

    private:
        QWidget* m_widget{};
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

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
#include <QItemDelegate>

#include "GroupPanelModel.hpp"
class GroupTableWidgetItem : public QItemDelegate
{

};

class GroupTableItemModel : public QAbstractTableModel
{

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
            this->layout()->addWidget(m_table);
        }

        void setup()
        {
            // Headers
            m_table->insertRow(0);
            m_table->insertColumn(0);

            // Groups
            for(int i = 0; i < m_mgr->groups().size(); i++)
            {
                m_table->insertColumn(1);
            }

            // Clients
            m_table->insertRow(1); // Local client

            for(int i = 0; i < m_session->remoteClients().size(); i++)
            {
                m_table->insertRow(2);
            }

            // Set the data
            m_table->item(0, 0)->setText("");
            for(int i = 1; i <= m_mgr->groups().size(); i++)
            {
                m_table->item(0, i)->setText(m_mgr->groups()[i]->name());
            }

            m_table->item(0, 1)->setText("Local");
            for(int i = 2; i <= m_session->remoteClients().size(); i++)
            {
                m_table->item(0, i)->setText(m_mgr->groups()[i]->name());
            }

        }

    private:
        const GroupManager* m_mgr;
        const Session* m_session;
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

#include "GroupListWidget.hpp"
#include "DistributedScenario/GroupManager.hpp"
#include "GroupWidget.hpp"

#include <QVBoxLayout>

GroupListWidget::GroupListWidget(const GroupManager* mgr, QWidget* parent):
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

void GroupListWidget::addGroup(const id_type<Group>& id)
{
    auto widg = new GroupWidget{m_mgr->group(id), this};
    this->layout()->addWidget(widg);
    m_widgets.append(widg);
}

void GroupListWidget::removeGroup(const id_type<Group>& id)
{
    using namespace std;
    auto it = find_if(begin(m_widgets),
                      end(m_widgets),
                      [&] (GroupWidget* widg)
    { return widg->id() == id; } );

    m_widgets.removeOne(*it);
    delete *it;
}

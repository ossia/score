#include <boost/optional/optional.hpp>
#include <qboxlayout.h>
#include <qlayout.h>
#include <algorithm>
#include <iterator>

#include "DistributedScenario/GroupManager.hpp"
#include "GroupListWidget.hpp"
#include "GroupWidget.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

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

void GroupListWidget::addGroup(const Id<Group>& id)
{
    auto widg = new GroupWidget{m_mgr->group(id), this};
    this->layout()->addWidget(widg);
    m_widgets.append(widg);
}

void GroupListWidget::removeGroup(const Id<Group>& id)
{
    using namespace std;
    auto it = find_if(begin(m_widgets),
                      end(m_widgets),
                      [&] (GroupWidget* widg)
    { return widg->id() == id; } );

    m_widgets.removeOne(*it);
    delete *it;
}

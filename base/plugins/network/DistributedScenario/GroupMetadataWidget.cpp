#include "GroupMetadataWidget.hpp"
#include "GroupManager.hpp"

#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include "GroupMetadata.hpp"
#include "Group.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include "Commands/ChangeConstraintGroup.hpp"
#include "Commands/ChangeEventGroup.hpp"
GroupMetadataWidget::GroupMetadataWidget(
        const GroupMetadata& groupmetadata,
        const GroupManager* mgr,
        QWidget* widg):
    QWidget{widg},
    m_groups{mgr}
{
    this->setLayout(new QHBoxLayout);
    this->layout()->addWidget(new QLabel{tr("Groups: ")});

    connect(&groupmetadata, &GroupMetadata::groupChanged,
            this, [=] (const id_type<Group>& grp)
    {
        updateLabel(grp);
    });

    // TODO connect(m_groups, &GroupManager::groupAdded,   this, &GroupMetadataWidget::on_groupAdded);
    // TODO connect(m_groups, &GroupManager::groupRemoved, this, &GroupMetadataWidget::on_groupRemoved);

    updateLabel(groupmetadata.id());
}

void GroupMetadataWidget::on_groupAdded(const id_type<Group>& id)
{

}

void GroupMetadataWidget::on_groupRemoved(const id_type<Group>& id)
{

}

void GroupMetadataWidget::on_indexChanged(int id)
{
    // TODO auto data = m_combo->currentData().value<id_type<Group>>();

}

void GroupMetadataWidget::updateLabel(const id_type<Group>& id)
{
    delete m_combo;
    m_combo = new QComboBox;

    for(auto& group : m_groups->groups())
    {
        // TODO m_combo->addItem(group->name(), QVariant::fromValue(id));
    }

    connect(m_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_indexChanged(int)));

    this->layout()->addWidget(m_combo);
}

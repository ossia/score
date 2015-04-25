#include "GroupMetadataWidget.hpp"
#include "GroupManager.hpp"

#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include "GroupMetadata.hpp"
#include "Group.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include "Commands/ChangeGroup.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"

Q_DECLARE_METATYPE(id_type<Group>)

GroupMetadataWidget::GroupMetadataWidget(
        const GroupMetadata& groupmetadata,
        const GroupManager* mgr,
        QWidget* widg):
    QWidget{widg},
    m_object{groupmetadata},
    m_groups{mgr}
{
    this->setLayout(new QHBoxLayout);
    this->layout()->addWidget(new QLabel{tr("Groups: ")});

    connect(&groupmetadata, &GroupMetadata::groupChanged,
            this, [=] (const id_type<Group>& grp)
    {
        qDebug() << "in lambda" << grp;
        updateLabel(grp);
    });

    connect(m_groups, &GroupManager::groupAdded,   this, &GroupMetadataWidget::on_groupAdded);
    connect(m_groups, &GroupManager::groupRemoved, this, &GroupMetadataWidget::on_groupRemoved);

    updateLabel(groupmetadata.group());
}

void GroupMetadataWidget::on_groupAdded(const id_type<Group>& id)
{
    m_combo->addItem(m_groups->group(id)->name(), QVariant::fromValue(id));
}

void GroupMetadataWidget::on_groupRemoved(const id_type<Group>& id)
{
    int index = m_combo->findData(QVariant::fromValue(id));
    m_combo->removeItem(index);
}

void GroupMetadataWidget::on_indexChanged(int)
{
    auto data = m_combo->currentData().value<id_type<Group>>();
    if(m_object.group() != data)
    {
        auto doc = iscore::IDocument::documentFromObject(m_groups);
        CommandDispatcher<> dispatcher{doc->commandStack(), nullptr};
        dispatcher.submitCommand(
                    new ChangeGroup{
                        iscore::IDocument::path(m_object.element()),
                                                data});
    }
}

void GroupMetadataWidget::updateLabel(const id_type<Group>& currentGroup)
{
    delete m_combo;
    m_combo = new QComboBox;

    for(unsigned int i = 0; i < m_groups->groups().size(); i++)
    {
        m_combo->addItem(m_groups->groups()[i]->name(),
                         QVariant::fromValue(m_groups->groups()[i]->id()));

        if(m_groups->groups()[i]->id() == currentGroup)
        {
            m_combo->setCurrentIndex(i);
        }
    }

    connect(m_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_indexChanged(int)));

    this->layout()->addWidget(m_combo);
}

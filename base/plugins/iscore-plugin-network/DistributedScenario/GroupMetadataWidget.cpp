#include <boost/optional/optional.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLayout>

#include <QString>
#include <QVariant>
#include <vector>

#include "Commands/ChangeGroup.hpp"
#include "Group.hpp"
#include "GroupManager.hpp"
#include "GroupMetadata.hpp"
#include "GroupMetadataWidget.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <core/document/DocumentContext.hpp>


Q_DECLARE_METATYPE(Id<Group>)

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

    con(groupmetadata, &GroupMetadata::groupChanged,
            this, [=] (const Id<Group>& grp)
    {
        updateLabel(grp);
    });

    connect(m_groups, &GroupManager::groupAdded,
            this, &GroupMetadataWidget::on_groupAdded);
    connect(m_groups, &GroupManager::groupRemoved,
            this, &GroupMetadataWidget::on_groupRemoved);

    updateLabel(groupmetadata.group());
}

void GroupMetadataWidget::on_groupAdded(const Id<Group>& id)
{
    m_combo->addItem(m_groups->group(id)->name(), QVariant::fromValue(id));
}

void GroupMetadataWidget::on_groupRemoved(const Id<Group>& id)
{
    int index = m_combo->findData(QVariant::fromValue(id));
    m_combo->removeItem(index);
}

void GroupMetadataWidget::on_indexChanged(int)
{
    auto data = m_combo->currentData().value<Id<Group>>();
    if(m_object.group() != data)
    {
        CommandDispatcher<> dispatcher{iscore::IDocument::documentContext(*m_groups).commandStack};
        dispatcher.submitCommand(
                    new ChangeGroup{
                        iscore::IDocument::unsafe_path(m_object.element()),
                                                data});
    }
}

void GroupMetadataWidget::updateLabel(const Id<Group>& currentGroup)
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

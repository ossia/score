#include "GroupMetadataWidget.hpp"
#include "GroupManager.hpp"

#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include "GroupMetadata.hpp"
#include "Group.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include "Commands/ChangeGroup.hpp"


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
        updateLabel(grp);
    });

    connect(m_groups, &GroupManager::groupAdded,
            this, &GroupMetadataWidget::on_groupAdded);
    connect(m_groups, &GroupManager::groupRemoved,
            this, &GroupMetadataWidget::on_groupRemoved);

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
        CommandDispatcher<> dispatcher{iscore::IDocument::commandStack(*m_groups)};
        dispatcher.submitCommand(
                    new ChangeGroup{
                        iscore::IDocument::unsafe_path(m_object.element()),
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

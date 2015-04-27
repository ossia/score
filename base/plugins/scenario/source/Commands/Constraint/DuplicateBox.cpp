#include "DuplicateBox.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

DuplicateBox::DuplicateBox(ObjectPath&& boxToCopy) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_boxPath {boxToCopy}
{
    auto box = m_boxPath.find<BoxModel>();
    auto constraint = box->constraint();

    m_newBoxId = getStrongId(constraint->boxes());
}

void DuplicateBox::undo()
{
    auto box = m_boxPath.find<BoxModel>();
    auto constraint = box->constraint();

    constraint->removeBox(m_newBoxId);
}

void DuplicateBox::redo()
{
    auto box = m_boxPath.find<BoxModel>();
    auto constraint = box->constraint();
    constraint->addBox(new BoxModel {box,
                                     m_newBoxId,
                                     &DeckModel::copyViewModelsInSameConstraint,
                                     constraint});
}

bool DuplicateBox::mergeWith(const Command* other)
{
    return false;
}

void DuplicateBox::serializeImpl(QDataStream& s) const
{
    s << m_boxPath << m_newBoxId;
}

void DuplicateBox::deserializeImpl(QDataStream& s)
{
    s >> m_boxPath >> m_newBoxId;
}

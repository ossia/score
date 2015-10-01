#include "EditValue.hpp"

#include <iscore/serialization/VisitorCommon.hpp>

EditValue::EditValue(
        Path<iscore::MessageItemModel> &&device_tree,
        const iscore::MessageNodePath& nodePath,
        const QVariant& value):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree},
    m_nodePath{nodePath}
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    auto n = m_nodePath.toNode(&model.rootNode());
    ISCORE_ASSERT(n->parent() && n->parent()->parent());

    // TODO fetch the new values from the processes by asking for it.

    m_oldProcess = n->processValue;
    m_oldUser= n->userValue;
    */
}

void EditValue::undo()
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_old);
    */
}

void EditValue::redo()
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_new);
    */
}

void EditValue::serializeImpl(QDataStream &d) const
{
    ISCORE_TODO;
    /*
    d << m_path
      << m_nodePath
      << m_old
      << m_new;
      */
}

void EditValue::deserializeImpl(QDataStream &d)
{
    /*
    d >> m_path
      >> m_nodePath
      >> m_old
      >> m_new;
      */
}

#include "EditValue.hpp"

#include <iscore/serialization/VisitorCommon.hpp>

EditValue::EditValue(
        Path<iscore::MessageItemModel> &&device_tree,
        const iscore::NodePath& nodePath,
        const QVariant& value):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree},
    m_nodePath{nodePath},
    m_new{value}
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    auto n = m_nodePath.toNode(&model.rootNode());
    ISCORE_ASSERT(n->is<iscore::AddressSettings>());

    m_old = n->get<iscore::AddressSettings>().value.val;
    */
}

void EditValue::undo()
{
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_old);
    */
}

void EditValue::redo()
{
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_new);
    */
}

void EditValue::serializeImpl(QDataStream &d) const
{
    d << m_path
      << m_nodePath
      << m_old
      << m_new;
}

void EditValue::deserializeImpl(QDataStream &d)
{
    d >> m_path
      >> m_nodePath
      >> m_old
      >> m_new;
}

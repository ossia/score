#include "LocalTreeModel.hpp"
#include <QSettings>

namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

const QString Keys::localTree = QStringLiteral("iscore_plugin_ossia/LocalTree");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::localTree))
    {
        setFirstTimeSettings();
    }

    m_tree = s.value(Keys::localTree).toBool();
}

bool Model::getLocalTree() const
{
    return m_tree;
}

void Model::setLocalTree(bool val)
{
    if (m_tree == val)
        return;

    m_tree = val;

    QSettings s;
    s.setValue(Keys::localTree, m_tree);
    emit localTreeChanged(val);
}

void Model::setFirstTimeSettings()
{
    m_tree = false;

    QSettings s;
    s.setValue(Keys::localTree, m_tree);
}

}
}
}

#include "Model.hpp"
#include <QSettings>

namespace Curve
{
namespace Settings
{

const QString Keys::simplificationRatio = QStringLiteral("iscore_plugin_curve/SimplificationRatio");
const QString Keys::simplify = QStringLiteral("iscore_plugin_curve/Simplify");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::simplificationRatio))
    {
        setFirstTimeSettings();
    }

    m_simplificationRatio = s.value(Keys::simplificationRatio).toInt();
    m_simplify = s.value(Keys::simplify).toBool();
}

int Model::getSimplificationRatio() const
{
    return m_simplificationRatio;
}

void Model::setSimplificationRatio(int simplificationRatio)
{
    if (m_simplificationRatio == simplificationRatio)
        return;

    m_simplificationRatio = simplificationRatio;

    QSettings s;
    s.setValue(Keys::simplificationRatio, m_simplificationRatio);
    emit simplificationRatioChanged(simplificationRatio);
}

bool Model::getSimplify() const
{
    return m_simplify;
}

void Model::setSimplify(bool simplify)
{
    if (m_simplify == simplify)
        return;

    m_simplify = simplify;
    emit simplifyChanged(simplify);
}

void Model::setFirstTimeSettings()
{
    QSettings s;
    s.setValue(Keys::simplificationRatio, 10);
    s.setValue(Keys::simplify, true);
    m_simplify = true;
    m_simplificationRatio = 10;
}

}
}

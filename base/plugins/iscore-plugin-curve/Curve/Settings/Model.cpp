#include "Model.hpp"
#include <QSettings>

namespace Curve
{
namespace Settings
{

const QString Keys::simplificationRatio = QStringLiteral("iscore_plugin_curve/SimplificationRatio");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::simplificationRatio))
    {
        setFirstTimeSettings();
    }

    m_simplificationRatio = s.value(Keys::simplificationRatio).toInt();
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

void Model::setFirstTimeSettings()
{
    QSettings s;
    s.setValue(Keys::simplificationRatio, 10);
    m_simplificationRatio = 10;
}

}
}

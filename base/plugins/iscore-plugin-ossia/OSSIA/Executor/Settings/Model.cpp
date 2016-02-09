#include "Model.hpp"
#include <QSettings>

namespace RecreateOnPlay
{
namespace Settings
{

const QString Keys::rate = QStringLiteral("iscore_plugin_ossia/ExecutionRate");


Model::Model()
{
    QSettings s;

    if(!s.contains(Keys::rate))
    {
        setFirstTimeSettings();
    }

    m_rate = s.value(Keys::rate).toInt();
}

int Model::rate() const
{
    return m_rate;
}

void Model::setRate(int rate)
{
    if (m_rate == rate)
        return;

    m_rate = rate;

    QSettings s;
    s.setValue(Keys::rate, m_rate);
    emit rateChanged(rate);
}

void Model::setFirstTimeSettings()
{
    QSettings s;
    s.setValue(Keys::rate, 50);
}

}
}

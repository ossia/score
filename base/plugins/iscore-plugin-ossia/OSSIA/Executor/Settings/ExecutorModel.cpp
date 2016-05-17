#include "ExecutorModel.hpp"
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

int Model::getRate() const
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
    emit RateChanged(rate);
}

void Model::setFirstTimeSettings()
{
    m_rate = 50;

    QSettings s;
    s.setValue(Keys::rate, m_rate);
}

}
}

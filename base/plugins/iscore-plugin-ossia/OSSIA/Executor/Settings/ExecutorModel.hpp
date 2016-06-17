#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <OSSIA/Executor/ClockManager/ClockManagerFactory.hpp>

namespace RecreateOnPlay
{
namespace Settings
{
class Model :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)
        Q_PROPERTY(ClockManagerFactory::ConcreteFactoryKey clock READ getClock WRITE setClock NOTIFY ClockChanged)

        int m_Rate;
        ClockManagerFactory::ConcreteFactoryKey m_Clock;

        const ClockManagerFactoryList& m_clockFactories;

    public:
        Model(QSettings& set, const iscore::ApplicationContext& ctx);

        const ClockManagerFactoryList& clockFactories() const
        { return m_clockFactories; }

        std::unique_ptr<ClockManager> makeClock(const RecreateOnPlay::Context& ctx) const;

        ISCORE_SETTINGS_PARAMETER_HPP(int, Rate)
        ISCORE_SETTINGS_PARAMETER_HPP(ClockManagerFactory::ConcreteFactoryKey, Clock)
};

ISCORE_SETTINGS_PARAMETER(Model, Rate)
ISCORE_SETTINGS_PARAMETER(Model, Clock)

}
}

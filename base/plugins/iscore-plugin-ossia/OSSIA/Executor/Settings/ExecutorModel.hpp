#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>


namespace RecreateOnPlay
{
namespace Settings
{
class Model :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)

        int m_Rate;
    public:
        Model(QSettings& set, const iscore::ApplicationContext& ctx);

        ISCORE_SETTINGS_PARAMETER_HPP(int, Rate)
};

ISCORE_SETTINGS_PARAMETER(Model, Rate)

}
}

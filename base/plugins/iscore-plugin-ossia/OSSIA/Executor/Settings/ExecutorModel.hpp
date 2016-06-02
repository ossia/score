#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>


namespace RecreateOnPlay
{
namespace Settings
{

struct Keys
{
        static const QString rate;
};

class Model :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)

    public:
        Model();

        int getRate() const;
        void setRate(int getRate);

    signals:
        void RateChanged(int getRate);

    private:
        void setFirstTimeSettings() override;
        int m_rate = 50;
};

ISCORE_SETTINGS_PARAMETER(Model, Rate)

}
}

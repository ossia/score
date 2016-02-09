#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>

namespace RecreateOnPlay
{
namespace Settings
{

struct Keys
{
        static const QString rate;
        static constexpr const char * rate_property = "rate";
};

class Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(int rate READ rate WRITE setRate NOTIFY rateChanged)

    public:
        Model();

        int rate() const;
        void setRate(int rate);

    signals:
        void rateChanged(int rate);

    private:
        void setFirstTimeSettings() override;
        int m_rate = 50;
};

}
}

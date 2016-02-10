#pragma once
#include <set>
#include <memory>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QObject>
#include <QChildEvent>
#include <QDebug>
#include <QCoreApplication>
namespace iscore
{
    class SettingsModel final : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsModel()
            {
            }

            void addSettingsModel(SettingsDelegateModelInterface* model)
            {
                model->setParent(this);
                m_settings.push_back(model);
            }

            auto& settings() const
            { return m_settings; }

        protected:
            void childEvent(QChildEvent* ev) override
            {
                QCoreApplication::sendEvent(parent(), ev);
            }

        private:
            std::vector<SettingsDelegateModelInterface*> m_settings;
    };
}

#pragma once
#include <set>
#include <memory>
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <interface/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QObject>
#include <QChildEvent>
#include <QDebug>
#include <QCoreApplication>
namespace iscore
{
    class SettingsModel : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsModel()
            {
            }

            void addSettingsModel (SettingsDelegateModelInterface* model)
            {
                model->setParent (this);
                m_pluginModels.insert (model);
            }

        protected:
            virtual void childEvent (QChildEvent* ev) override
            {
                QCoreApplication::sendEvent (parent(), ev);
            }

        private:
            std::set<SettingsDelegateModelInterface*> m_pluginModels;
    };
}

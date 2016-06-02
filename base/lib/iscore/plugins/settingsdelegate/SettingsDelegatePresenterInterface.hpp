#pragma once
#include <QObject>
#include <iscore/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class SettingsDelegateModel;
    class SettingsDelegateView;
    class SettingsPresenter;

    class ISCORE_LIB_BASE_EXPORT SettingsDelegatePresenter :
            public QObject
    {
        public:
            SettingsDelegatePresenter(
                    SettingsDelegateModel& model,
                    SettingsDelegateView& view,
                    QObject* parent) :
                QObject {parent},
                    m_model {model},
                    m_view {view}
            {}

            virtual ~SettingsDelegatePresenter();
            void on_accept()
            {
                m_disp.commit();
            }

            void on_reject()
            {
                m_disp.rollback();
            }

            virtual QString settingsName() = 0;
            virtual QIcon settingsIcon() = 0;

            template<typename T>
            auto& model(T* self)
            {
                return static_cast<typename T::model_type&>(self->m_model);
            }

            template<typename T>
            auto& view(T* self)
            {
                return static_cast<typename T::view_type&>(self->m_view);
            }

        protected:
            SettingsDelegateModel& m_model;
            SettingsDelegateView& m_view;

            iscore::SettingsCommandDispatcher m_disp;
    };
}

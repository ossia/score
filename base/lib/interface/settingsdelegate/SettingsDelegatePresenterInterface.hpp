#pragma once
#include <QObject>
#include <core/settings/SettingsPresenter.hpp>

namespace iscore
{
    class SettingsDelegateModelInterface;
    class SettingsDelegateViewInterface;
    class SettingsPresenter;

    class SettingsDelegatePresenterInterface : public QObject
    {
        public:
            SettingsDelegatePresenterInterface (SettingsPresenter* parent_presenter,
                                                SettingsDelegateModelInterface* model,
                                                SettingsDelegateViewInterface* view) :
                QObject {parent_presenter},
                    m_model {model},
                    m_view {view},
                    m_parentPresenter {parent_presenter}
            {}

            virtual ~SettingsDelegatePresenterInterface() = default;
            virtual void on_accept() = 0;
            virtual void on_reject() = 0;

            virtual QString settingsName() = 0;
            virtual QIcon settingsIcon() = 0;

        protected:
            SettingsDelegateModelInterface* m_model;
            SettingsDelegateViewInterface* m_view;
            SettingsPresenter* m_parentPresenter;
    };
}

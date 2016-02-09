#pragma once
#include <QWidget>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class SettingsDelegatePresenterInterface;

    class ISCORE_LIB_BASE_EXPORT SettingsDelegateViewInterface : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsDelegateViewInterface();
            virtual void setPresenter(SettingsDelegatePresenterInterface* presenter)
            {
                m_presenter = presenter;
            }

            SettingsDelegatePresenterInterface* getPresenter()
            {
                return m_presenter;
            }

            virtual QWidget* getWidget() = 0; // QML? ownership transfer ? ? ? what about "this" case ?

        protected:
            SettingsDelegatePresenterInterface* m_presenter;
    };
}

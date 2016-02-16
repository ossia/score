#pragma once
#include <QWidget>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class ProjectSettingsPresenter;

    class ISCORE_LIB_BASE_EXPORT ProjectSettingsView :
            public QObject
    {
        public:
            using QObject::QObject;
            virtual ~ProjectSettingsView();
            virtual void setPresenter(ProjectSettingsPresenter* presenter)
            {
                m_presenter = presenter;
            }

            ProjectSettingsPresenter* getPresenter()
            {
                return m_presenter;
            }

            virtual QWidget* getWidget() = 0; // QML? ownership transfer ? ? ? what about "this" case ?

        protected:
            ProjectSettingsPresenter* m_presenter{};
    };
}

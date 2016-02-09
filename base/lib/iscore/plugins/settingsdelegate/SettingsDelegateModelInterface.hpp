#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class SettingsDelegatePresenterInterface;
    class ISCORE_LIB_BASE_EXPORT SettingsDelegateModelInterface : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsDelegateModelInterface();

        virtual void setFirstTimeSettings() = 0;
    };

}

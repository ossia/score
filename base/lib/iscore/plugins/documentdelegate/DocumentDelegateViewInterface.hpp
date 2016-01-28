#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>
class QWidget;

namespace iscore
{
    class ISCORE_LIB_BASE_EXPORT DocumentDelegateViewInterface : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~DocumentDelegateViewInterface();

            virtual QWidget* getWidget() = 0;
    };
}

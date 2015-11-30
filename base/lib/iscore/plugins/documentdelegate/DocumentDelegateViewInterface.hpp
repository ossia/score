#pragma once
#include <QObject>

class QWidget;

namespace iscore
{
    class DocumentDelegateViewInterface : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~DocumentDelegateViewInterface();

            virtual QWidget* getWidget() = 0;
    };
}

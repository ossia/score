#pragma once
#include <tools/NamedObject.hpp>
#include <QJsonObject>
#include <core/interface/selection/SelectionStack.hpp>
namespace iscore
{
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~DocumentDelegateModelInterface() = default;

            virtual QByteArray save() = 0;
            virtual QJsonObject toJson()
            {
                return {};
            }

        signals:
            void selectionChanged(Selection s);
    };
}

#pragma once
#include <tools/IdentifiedObject.hpp>
#include <QJsonObject>
#include <public_interface/selection/SelectionStack.hpp>
namespace iscore
{
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface : public IdentifiedObject<DocumentDelegateModelInterface>
    {
            Q_OBJECT
        public:
            using IdentifiedObject::IdentifiedObject;
            virtual ~DocumentDelegateModelInterface() = default;

            virtual QByteArray save() = 0;
            virtual QJsonObject toJson()
            {
                return {};
            }

        public slots:
            virtual void setNewSelection(const Selection& s) = 0;
    };
}

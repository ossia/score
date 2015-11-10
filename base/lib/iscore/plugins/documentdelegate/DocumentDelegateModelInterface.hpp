#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/SelectionStack.hpp>
namespace iscore
{
    class DocumentDelegateModelInterface : public IdentifiedObject<DocumentDelegateModelInterface>
    {
            Q_OBJECT
        public:
            using IdentifiedObject::IdentifiedObject;
            virtual ~DocumentDelegateModelInterface();

            virtual void serialize(const VisitorVariant&) const = 0;

        public slots:
            virtual void setNewSelection(const Selection& s) = 0;
    };
}

#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QString>

namespace iscore
{
    class DocumentDelegateModelInterface;
    class DocumentDelegateViewInterface;
    class DocumentPresenter;

    class DocumentDelegatePresenterInterface : public NamedObject
    {
        public:
            DocumentDelegatePresenterInterface(DocumentPresenter* parent_presenter,
                                               const QString& object_name,
                                               const DocumentDelegateModelInterface& model,
                                               DocumentDelegateViewInterface& view);

            virtual ~DocumentDelegatePresenterInterface();

        protected:
            const DocumentDelegateModelInterface& m_model;
            DocumentDelegateViewInterface& m_view;
            DocumentPresenter* m_parentPresenter{};
    };
}

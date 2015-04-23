#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
class QByteArray;

namespace iscore
{
    class DocumentDelegateViewInterface;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface;
    class DocumentPresenter;
    class DocumentModel;
    class DocumentView;

    /**
     * @brief The DocumentDelegateFactoryInterface class
     *
     * The interface required to create a custom main document (like MS Word's main page)
     */
    class DocumentDelegateFactoryInterface
    {
        public:
            virtual ~DocumentDelegateFactoryInterface() = default;
            virtual DocumentDelegateViewInterface* makeView(
                    DocumentView* parent) = 0;
            virtual DocumentDelegatePresenterInterface* makePresenter(
                    DocumentPresenter* parent_presenter,
                    DocumentDelegateModelInterface* model,
                    DocumentDelegateViewInterface* view) = 0;

            virtual DocumentDelegateModelInterface* makeModel(
                    DocumentModel* parent) = 0;
            virtual DocumentDelegateModelInterface* makeModel(
                    const VisitorVariant&,
                    DocumentModel* parent) = 0;
    };

}

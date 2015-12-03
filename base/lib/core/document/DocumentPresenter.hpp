#pragma once
#include <iscore/tools/NamedObject.hpp>

class QObject;

namespace iscore
{
    class DocumentDelegateFactoryInterface;
    class DocumentDelegatePresenterInterface;
    class DocumentModel;
    class DocumentView;

    /**
     * @brief The DocumentPresenter class
     *
     * A wrapper between the model and the view.
     */
    class DocumentPresenter final : public NamedObject
    {
            Q_OBJECT
        public:
            DocumentPresenter(DocumentDelegateFactoryInterface*,
                              const DocumentModel&,
                              DocumentView&,
                              QObject* parent);


            DocumentDelegatePresenterInterface* presenterDelegate() const
            { return m_presenter; }


            DocumentView& m_view;
            const DocumentModel& m_model;
            DocumentDelegatePresenterInterface* m_presenter {};
    };
}

#pragma once
#include <memory>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>

namespace iscore
{
    class DocumentModel;
    class DocumentView;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateFactoryInterface;

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

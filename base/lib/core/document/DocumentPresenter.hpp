#pragma once
#include <memory>
#include <tools/NamedObject.hpp>
#include <core/tools/ObjectPath.hpp>

namespace iscore
{
    class DocumentModel;
    class DocumentView;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateFactoryInterface;

    /**
     * @brief The DocumentPresenter class holds the logic for the main document.
     *
     * Its main use is to manage the command queue, since we use the Command pattern,
     * by taking the commands from the document view and applying them on the document model.
     */
    class DocumentPresenter : public NamedObject
    {
            Q_OBJECT
        public:
            DocumentPresenter(DocumentDelegateFactoryInterface*,
                              DocumentModel*,
                              DocumentView*,
                              QObject* parent);


            DocumentDelegatePresenterInterface* presenterDelegate() const
            { return m_presenter; }

        signals:
            // To the network
            void lock(QByteArray);
            void unlock(QByteArray);

        public slots:
            // From the network
            void on_lock(QByteArray);
            void on_unlock(QByteArray);

        private:
            // In the commands
            void lock_impl();
            void unlock_impl();

            ObjectPath m_lockedObject;

            DocumentView* m_view {};
            DocumentModel* m_model {};
            DocumentDelegatePresenterInterface* m_presenter {};

    };
}

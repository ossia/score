#pragma once
#include <tools/NamedObject.hpp>
#include <core/interface/selection/SelectionStack.hpp>
#include <core/presenter/command/CommandQueue.hpp>

namespace iscore
{
    class DocumentModel;
    class DocumentPresenter;
    class DocumentView;
    class DocumentDelegateFactoryInterface;
    class PanelFactoryInterface;
    class PanelPresenterInterface;
    /**
     * @brief The Document class is the central part of the software.
     *
     * It is similar to the opened file in Word for instance, this is the
     * data on which i-score operates, further defined by the plugins.
     */
    class Document : public NamedObject
    {
            Q_OBJECT
        public:
            Document(DocumentDelegateFactoryInterface* type,
                     QWidget* parentview,
                     QObject* parent);

            Document(const QByteArray& data,
                     DocumentDelegateFactoryInterface* type,
                     QWidget* parentview,
                     QObject* parent);

            ~Document();


            CommandStack* commandStack()
            { return &m_commandStack; }

            SelectionStack& selectionStack()
            { return m_selectionStack; }


            DocumentModel* model() const
            {
                return m_model;
            }

            DocumentPresenter* presenter() const
            {
                return m_presenter;
            }

            DocumentView* view() const
            {
                return m_view;
            }

            void setupNewPanel(PanelPresenterInterface* pres,
                               PanelFactoryInterface* factory);
            void bindPanelPresenter(PanelPresenterInterface*);


            QByteArray save();

        private:
            CommandStack m_commandStack;
            SelectionStack m_selectionStack;

            DocumentModel* m_model {};
            DocumentView* m_view {};
            DocumentPresenter* m_presenter {};

    };

}

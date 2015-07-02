#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <core/command/CommandStack.hpp>

#include <QTemporaryFile>

namespace iscore
{
    class DocumentModel;
    class DocumentPresenter;
    class DocumentView;
    class DocumentDelegateFactoryInterface;
    class PanelFactory;
    class PanelPresenter;
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

            Document(const QVariant& data,
                     DocumentDelegateFactoryInterface* type,
                     QWidget* parentview,
                     QObject* parent);

            ~Document();


            CommandStack& commandStack()
            { return m_commandStack; }

            SelectionStack& selectionStack()
            { return m_selectionStack; }

            ObjectLocker& locker()
            { return m_objectLocker; }

            QTemporaryFile& crashDataFile()
            { return m_crashDataFile; }
            QTemporaryFile& crashCommandFile()
            { return m_crashCommandFile; }




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

            void setupNewPanel(PanelPresenter* pres,
                               PanelFactory* factory);
            void bindPanelPresenter(PanelPresenter*);


            QJsonObject saveDocumentModelAsJson();
            QByteArray saveDocumentModelAsByteArray();

            QJsonObject saveAsJson();
            QByteArray saveAsByteArray();

        private:
            void init();

            CommandStack m_commandStack;
            SelectionStack m_selectionStack;
            ObjectLocker m_objectLocker;

            DocumentModel* m_model{};
            DocumentView* m_view{};
            DocumentPresenter* m_presenter{};

            QTemporaryFile m_crashDataFile;
            QTemporaryFile m_crashCommandFile;
    };

}

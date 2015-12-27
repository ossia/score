#pragma once
#include <core/command/CommandStack.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <core/document/DocumentMetadata.hpp>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>

class QObject;
class QWidget;
namespace iscore {
class DocumentBackupManager;
}  // namespace iscore
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class DocumentModel;
class DocumentPresenter;
class DocumentView;
class PanelFactory;
class PanelPresenter;

/**
     * @brief The Document class is the central part of the software.
     *
     * It is similar to the opened file in Word for instance, this is the
     * data on which i-score operates, further defined by the plugins.
     */
class ISCORE_LIB_BASE_EXPORT Document final : public NamedObject
{
        Q_OBJECT
        friend class DocumentBuilder;
        friend struct DocumentContext;
    public:
        DocumentMetadata metadata;

        ~Document();

        const Id<DocumentModel>& id() const;

        CommandStack& commandStack()
        { return m_commandStack; }

        SelectionStack& selectionStack()
        { return m_selectionStack; }

        ObjectLocker& locker()
        { return m_objectLocker; }

        const DocumentContext& context() const
        { return m_context; }

        DocumentModel& model() const
        { return *m_model; }

        DocumentPresenter& presenter() const
        { return *m_presenter; }

        DocumentView& view() const
        { return *m_view; }

        void setupNewPanel(PanelFactory* factory);
        void bindPanelPresenter(PanelPresenter*);


        QJsonObject saveDocumentModelAsJson();
        QByteArray saveDocumentModelAsByteArray();

        QJsonObject saveAsJson();
        QByteArray saveAsByteArray();

        DocumentBackupManager* backupManager() const
        { return m_backupMgr; }

        void setBackupMgr(DocumentBackupManager* backupMgr);

    private:
        // These are to be constructed by DocumentBuilder.
        Document(
                const Id<DocumentModel>& id,
                DocumentDelegateFactoryInterface* type,
                QWidget* parentview,
                QObject* parent);

        Document(
                const QVariant& data,
                DocumentDelegateFactoryInterface* type,
                QWidget* parentview,
                QObject* parent);

        void init();

        CommandStack m_commandStack;
        CommandStackFacade m_commandStackFacade{m_commandStack};

        SelectionStack m_selectionStack;
        ObjectLocker m_objectLocker;

        DocumentModel* m_model{};
        DocumentView* m_view{};
        DocumentPresenter* m_presenter{};

        DocumentBackupManager* m_backupMgr{};

        DocumentContext m_context;
};



}

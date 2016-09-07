#pragma once
#include <core/command/CommandStack.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/selection/FocusManager.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <core/document/DocumentMetadata.hpp>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QTimer>
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
class DocumentDelegateFactory;
class DocumentModel;
class DocumentPresenter;
class DocumentView;

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

        ~Document();

        const DocumentMetadata& metadata() const { return m_metadata; }
        DocumentMetadata& metadata() { return m_metadata; }

        const Id<DocumentModel>& id() const;

        CommandStack& commandStack()
        { return m_commandStack; }

        SelectionStack& selectionStack()
        { return m_selectionStack; }

        FocusManager& focusManager()
        { return m_focus; }


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

        QJsonObject saveDocumentModelAsJson();
        QByteArray saveDocumentModelAsByteArray();

        QJsonObject saveAsJson();
        QByteArray saveAsByteArray();

        DocumentBackupManager* backupManager() const
        { return m_backupMgr; }

        void setBackupMgr(DocumentBackupManager* backupMgr);

        bool virgin() const
        { return m_virgin && !m_commandStack.canUndo() && !m_commandStack.canRedo(); }

    signals:
        void aboutToClose();

    private:
        // These are to be constructed by DocumentBuilder.
        Document(
                const QString& name,
                const Id<DocumentModel>& id,
                DocumentDelegateFactory& type,
                QWidget* parentview,
                QObject* parent);

        Document(
                const QVariant& data,
                DocumentDelegateFactory& type,
                QWidget* parentview,
                QObject* parent);

        void init();

        DocumentMetadata m_metadata;
        CommandStack m_commandStack;

        SelectionStack m_selectionStack;
        ObjectLocker m_objectLocker;
        FocusManager m_focus;
        QTimer m_documentUpdateTimer;

        DocumentModel* m_model{};
        DocumentView* m_view{};
        DocumentPresenter* m_presenter{};

        DocumentBackupManager* m_backupMgr{};

        DocumentContext m_context;

        bool m_virgin{false}; // Used to check if we can safely close it
        // if we want to load a document instead upon opening i-score.
};



}

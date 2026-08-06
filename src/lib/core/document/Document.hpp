#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentRole.hpp>
#include <score/tools/Environment.hpp>
#include <score/locking/ObjectLocker.hpp>
#include <score/selection/FocusManager.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentMetadata.hpp>

#include <ossia/detail/json.hpp>

#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <memory>
#include <verdigris>

class QObject;
class QWidget;
namespace score
{
class DocumentBackupManager;
} // namespace score
#include <score/model/Identifier.hpp>

#include <score_lib_base_export.h>

namespace score
{
using JsonWriter = ossia::json_writer;

class DocumentDelegateFactory;
class DocumentModel;
class DocumentPresenter;
class DocumentView;

/**
 * @brief The Document class is the central part of the software.
 *
 * It is similar to the opened file in Word for instance, this is the
 * data on which score operates, further defined by the plugins.
 *
 * It has ownership on the useful classes for document edition and display :
 * * Selection handling
 * * Command stack
 * * etc...
 */
class SCORE_LIB_BASE_EXPORT Document final : public QObject
{
  W_OBJECT(Document)
  friend class DocumentBuilder;
  friend struct DocumentContext;

public:
  ~Document();

  void blockAllSignals();

  const DocumentMetadata& metadata() const noexcept { return m_metadata; }
  DocumentMetadata& metadata() noexcept { return m_metadata; }

  const Id<DocumentModel>& id() const noexcept;

  CommandStack& commandStack() noexcept { return m_commandStack; }

  SelectionStack& selectionStack() noexcept { return m_selectionStack; }

  FocusManager& focusManager() noexcept { return m_focus; }

  ObjectLocker& locker() noexcept { return m_objectLocker; }

  const DocumentContext& context() const noexcept { return m_context; }

  //! Where this document's files are. Local unless something says otherwise.
  //!
  //! Created on demand rather than in init(): deserialization resolves paths,
  //! and it runs from the constructors before init() would have had a chance.
  score::Environment& environment() const noexcept;

  //! Take the files of this document to be somewhere else -- another machine,
  //! typically, once it is being edited through a session.
  void setEnvironment(std::unique_ptr<score::Environment> env);

  //! Whether this document may drive the hardware of the machine it is open on.
  //!
  //! Fixed at construction: devices connect while their plug-in deserializes,
  //! so a document that must not claim them has to say so before it is read.
  DocumentRole role() const noexcept { return m_role; }

  DocumentModel& model() const noexcept { return *m_model; }

  DocumentPresenter* presenter() const noexcept { return m_presenter; }

  DocumentView* view() const noexcept { return m_view; }

  DocumentBackupManager* backupManager() const noexcept { return m_backupMgr; }

  void saveDocumentModelAsJson(JSONObject::Serializer& writer);
  QByteArray saveDocumentModelAsByteArray();

  void saveAsJson(JSONObject::Serializer& writer);
  QByteArray saveAsByteArray();

  //! Indicates if the document has just been created and can be safely
  //! discarded.
  bool virgin() const
  {
    return m_virgin && !m_commandStack.canUndo() && !m_commandStack.canRedo();
  }

  // Load without creating presenter and view
  Document(const QString& name, DocumentDelegateFactory& type, QObject* parent);

  // Called once all the plug-ins, etc... of the document have been loaded
  void ready();

  // Update the various internal timers rate after settings changes
  void updateTimers();

  bool loaded() const noexcept { return m_loaded; }

private:
  // These are to be constructed by DocumentBuilder.
  Document(
      const QString& name, const Id<DocumentModel>& id, DocumentDelegateFactory& type,
      QWidget* parentview, QObject* parent);

  // Load
  Document(
      const QString& name, DocumentDelegateFactory& type, QWidget* parentview,
      QObject* parent);

  Document(
      const QString& name, const QByteArray& data, SerializationIdentifier format,
      DocumentDelegateFactory& type, QWidget* parentview, QObject* parent,
      DocumentRole role = DocumentRole::Local);

  // Restore
  Document(
      const score::RestorableDocument& data, DocumentDelegateFactory& type,
      QWidget* parentview, QObject* parent);

  void loadModel(const QString& fileName, DocumentDelegateFactory& factory);
  void loadModel(
      const QString& fileName, const QByteArray& data, SerializationIdentifier format,
      DocumentDelegateFactory& factory);
  void restoreModel(const QByteArray& data, DocumentDelegateFactory& factory);
  void init();

  DocumentMetadata m_metadata;
  CommandStack m_commandStack;

  SelectionStack m_selectionStack;
  ObjectLocker m_objectLocker;
  FocusManager m_focus;
  QTimer m_documentCoarseUpdateTimer;
  QTimer m_execTimer;
  CommandStackFacade m_facade{m_commandStack};
  OngoingCommandDispatcher m_disp{m_facade};

  DocumentModel* m_model{};
  DocumentView* m_view{};
  DocumentPresenter* m_presenter{};

  DocumentBackupManager* m_backupMgr{};

  DocumentContext m_context;
  mutable std::unique_ptr<score::Environment> m_environment;
  DocumentRole m_role{DocumentRole::Local};

  std::optional<score::RestorableDocument> m_initialData{};
  bool m_virgin{false}; // Used to check if we can safely close it
  // if we want to load a document instead upon opening score.
  bool m_loaded{false}; // Used to check if the document has finished loading.
  // Can be useful to turn some async operations into sync - creation of dynamic ports,
  // loading of vst plug-ins controls etc.
};
}

Q_DECLARE_METATYPE(score::Document*)
Q_DECLARE_METATYPE(Id<score::DocumentModel>)
Q_DECLARE_METATYPE(rapidjson::Value*)
W_REGISTER_ARGTYPE(score::Document*)
W_REGISTER_ARGTYPE(Id<score::DocumentModel>)

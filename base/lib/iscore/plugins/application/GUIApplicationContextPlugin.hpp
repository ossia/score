#pragma once
#include <QString>
#include <qnamespace.h>
#include <vector>

#include <iscore/actions/Action.hpp>
#include <iscore/application/GUIApplicationContext.hpp>
class QAction;
class QObject;
namespace iscore
{
class Document;
} // namespace iscore
struct VisitorVariant;

namespace iscore
{
class DocumentPlugin;

/**
 * @brief The GUIApplicationContextPlugin class
 *
 * This class's goal is to :
 * * Instantiate some elements that are deeply intertwined with Qt : menus,
 * toolbars
 * * Answer to specific application-level events : creation, loading of a new
 * document, etc.
 *
 * It is instatiated exactly once and is accessible via the application
 * context,
 * iscore::ApplicationContext :
 *
 * @code
 * auto& plugin = context.components.applicationPlugin<MyApplicationPlugin>();
 * @endcode
 *
 * The loading procedure is as follows :
 *
 * - prepareNewDocument is called for all plug-ins.
 *
 * - Creation of a Document.
 * - Creation of a DocumentModel
 * - on_initDocument is called for all plug-ins.
 *
 * * New document case :
 * - The document delegate is created
 * - on_newDocument is called for all plug-ins.
 *
 * * Loaded / restored document case :
 * - The document delegate is loaded
 * - The SerializableDocumentPlugin are loaded (see
 * iscore::DocumentPluginFactory)
 * - on_loadedDocument is called for all plug-ins.
 *
 * Finally, on_createdDocument is called for all plug-ins.
 * If the document becomes visible, on_documentChanged is called for all
 * plug-ins.
 *
 */
class ISCORE_LIB_BASE_EXPORT GUIApplicationContextPlugin
{
public:
  using GUIElements = iscore::GUIElements;
  GUIApplicationContextPlugin(const iscore::GUIApplicationContext& presenter);

  /**
   * @brief initialize
   *
   * This method will be called once every class has been loaded
   * from every plug-in.
   */
  virtual void initialize();

  virtual ~GUIApplicationContextPlugin();

  /**
   * @brief makeGUIElements
   *
   * This function allows a plug-in to provide custom elements
   * to put in toolbars, menus, etc.
   *
   * When returned here, they will afterwards be available through an
   * \ref iscore::ApplicationContext.
   *
   * @see \ref iscore::GUIElements
   */
  virtual GUIElements makeGUIElements();

  const GUIApplicationContext& context;

  /**
   * @brief currentDocument
   * @return Shortcut to get the active (visible) document.
   */
  Document* currentDocument() const;

  /**
   * @brief handleStartup
   * Returns true if the start-up was handled by this plug-in.
   */
  virtual bool handleStartup();

  /**
   * @brief on_initDocument
   * Will be called after the document has been created,
   * for new, load, and restore, and before anything is loaded.
   */
  virtual void on_initDocument(iscore::Document& doc);

  /**
   * @brief on_newDocument
   * Will be called only when a new, empty document is created;
   * classes inheriting from SerializableDocumentPlugin should be
   * instantiated and added to the document at this point.
   */
  virtual void on_newDocument(iscore::Document& doc);

  /**
   * @brief on_loadedDocument
   * Will be called only after a document has been loaded
   */
  virtual void on_loadedDocument(iscore::Document& doc);

  /**
   * @brief on_createdDocument
   * Will be called after either on_newDocument or on_loadedDocument
   * was called on every plug-in.
   */
  virtual void on_createdDocument(iscore::Document& doc);
  /**
   * @brief prepareNewDocument
   * Will be called just before a document switch.
   * It is possible to check if we are in the process
   * of document switching via DocumentManager::preparingNewDocument.
   */
  virtual void prepareNewDocument();

  /**
   * @brief on_documentChanged
   * Will be called after a document switch.
   * If newdoc isn't null, it means that it is currently being
   * focused by i-score.
   */
  virtual void
  on_documentChanged(iscore::Document* olddoc, iscore::Document* newdoc);

  /**
   * @brief on_activeWindowChanged
   * Can be used to check if the focus moves out of the main
   * window.
   */
  virtual void on_activeWindowChanged();
};
}

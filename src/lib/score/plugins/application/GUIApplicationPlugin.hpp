#pragma once
#include <score/actions/Action.hpp>
#include <score/actions/Menu.hpp>
#include <score/actions/Toolbar.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <QKeyEvent>
#include <qnamespace.h>

#include <vector>
class QAction;
class QObject;
namespace score
{
class Document;
} // namespace score
struct VisitorVariant;

namespace score
{
class DocumentPlugin;

struct GUIElements
{
  ActionContainer actions;
  std::vector<Menu> menus;
  std::vector<Toolbar> toolbars;
};

class SCORE_LIB_BASE_EXPORT ApplicationPlugin
{
public:
  ApplicationPlugin(const score::ApplicationContext& ctx);

  /**
   * @brief initialize
   *
   * This method will be called once every class has been loaded
   * from every plug-in.
   */
  virtual void initialize();

  virtual ~ApplicationPlugin();

  const score::ApplicationContext& context;
};

/**
 * @brief Used to extend the software with application-wide data.
 *
 * This class's goal is to :
 * * Instantiate some elements that are deeply intertwined with Qt : menus,
 * toolbars
 * * Answer to specific application-level events : creation, loading of a new
 * document, etc.
 *
 * It is instatiated exactly once and is accessible via the application
 * context,
 * score::ApplicationContext :
 *
 * @code
 * auto& plugin = context.applicationPlugin<MyApplicationPlugin>();
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
 * score::DocumentPluginFactory)
 * - on_loadedDocument is called for all plug-ins.
 *
 * Finally, on_createdDocument is called for all plug-ins.
 * If the document becomes visible, on_documentChanged is called for all
 * plug-ins.
 *
 */
class SCORE_LIB_BASE_EXPORT GUIApplicationPlugin
{
public:
  using GUIElements = score::GUIElements;
  GUIApplicationPlugin(const score::GUIApplicationContext& presenter);

  virtual ~GUIApplicationPlugin();

  /**
   * @brief makeGUIElements
   *
   * This function allows a plug-in to provide custom elements
   * to put in toolbars, menus, etc.
   *
   * When returned here, they will afterwards be available through an
   * \ref score::ApplicationContext.
   *
   * @see \ref score::GUIElements
   */
  virtual GUIElements makeGUIElements();

  const GUIApplicationContext& context;

  /**
   * @brief initialize
   *
   * This method will be called once every class has been loaded
   * from every plug-in.
   */
  virtual void initialize();

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
  virtual void on_initDocument(score::Document& doc);

  /**
   * @brief on_newDocument
   * Will be called only when a new, empty document is created;
   * classes inheriting from SerializableDocumentPlugin should be
   * instantiated and added to the document at this point.
   */
  virtual void on_newDocument(score::Document& doc);

  /**
   * @brief on_loadedDocument
   * Will be called only after a document has been loaded
   */
  virtual void on_loadedDocument(score::Document& doc);

  /**
   * @brief on_createdDocument
   * Will be called after either on_newDocument or on_loadedDocument
   * was called on every plug-in.
   */
  virtual void on_createdDocument(score::Document& doc);
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
   * focused by score.
   */
  virtual void on_documentChanged(score::Document* olddoc, score::Document* newdoc);

  /**
   * @brief on_activeWindowChanged
   * Can be used to check if the focus moves out of the main
   * window.
   */
  virtual void on_activeWindowChanged();

  /**
   * @brief on_keyPressEvent Called with key events in the main document view.
   */
  virtual void on_keyPressEvent(QKeyEvent& event);

  /**
   * @brief on_keyPressEvent Called with key events in the main document view.
   */
  virtual void on_keyReleaseEvent(QKeyEvent& event);
};
}

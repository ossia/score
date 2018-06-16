#pragma once
#include <QObject>
#include <QShortcut>
#include <score/document/DocumentContext.hpp>
#include <score/tools/std/Optional.hpp>
#include <score_lib_base_export.h>
class Selection;
namespace score
{
struct GUIApplicationContext;
struct DocumentContext;
class PanelModel;
class PanelView;

/**
 * @brief The PanelStatus struct
 *
 * Some metadata for the panels.
 *
 */
struct SCORE_LIB_BASE_EXPORT PanelStatus
{
  PanelStatus(
      bool isShown,
      Qt::DockWidgetArea d,
      int prio,
      QString name,
      const QKeySequence& sc);

  const bool shown;              // Controls if it is shown by default.
  const Qt::DockWidgetArea dock; // Which dock.
  const int priority;            // Higher priority will come up first.
  const QString prettyName;      // Used in the header.
  const QKeySequence shortcut; // Keyboard shortcut to show or hide the panel.
};

/**
 * @brief The PanelDelegate class
 *
 * Base class for the panels on the sides of score.
 * A panel is something that may outlive a document.
 * When the document changes, all the panels are updated
 * with the new visible document.
 *
 *
 * \see \ref PanelStatus
 */
class SCORE_LIB_BASE_EXPORT PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);
  virtual ~PanelDelegate();

  const score::GUIApplicationContext& context() const;

  void setModel(const score::DocumentContext& model);
  void setModel(none_t n);

  /**
   * @brief document The optional current document
   * @return The document if there is a current document in score, else
   * nothing.
   */
  MaybeDocument document() const;

  /**
   * @brief widget The widget of the panel.
   * @note The \ref View class takes ownership of it.
   */
  virtual QWidget* widget() = 0;

  /**
   * @brief defaultPanelStatus Metadata of the panel.
   */
  virtual const PanelStatus& defaultPanelStatus() const = 0;

  /**
   * @brief setNewSelection This function will be called if the selected
   * objects
   * change in score
   *
   * @param s The new selection.
   */
  virtual void setNewSelection(const Selection& s);

protected:
  /**
   * @brief on_modelChanged This function is called when the visible
   * document changes.
   * @param oldm The previous (actual) document or nothing if there was none.
   * @param newm The new document (or nothing if the user closed everything).
   */
  virtual void on_modelChanged(MaybeDocument oldm, MaybeDocument newm);

private:
  const score::GUIApplicationContext& m_context;
  MaybeDocument m_model{};
};
}

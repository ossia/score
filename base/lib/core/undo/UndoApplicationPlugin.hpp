#pragma once

#include <QAction>
#include <QList>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score_lib_base_export.h>
#include <vector>

class QObject;
namespace score
{
class Document;
} // namespace score

namespace score
{
/**
 * @brief The UndoApplicationPlugin class
 *
 * Base class for the "fake" undo plugin,
 * which provides a undo panel.
 */
class SCORE_LIB_BASE_EXPORT UndoApplicationPlugin final
    : public score::GUIApplicationPlugin
{
public:
  explicit UndoApplicationPlugin(const score::GUIApplicationContext& app);
  ~UndoApplicationPlugin() override;

private:
  void on_documentChanged(
      score::Document* olddoc, score::Document* newdoc) override;

  GUIElements makeGUIElements() override;

  // Connections to keep for the running document.
  QList<QMetaObject::Connection> m_connections;

  QAction m_undoAction;
  QAction m_redoAction;
};
}

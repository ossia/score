#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_lib_process_export.h>

class QAction;

namespace Process
{

//! Adds the project-consolidation entry to the File menu.
class SCORE_LIB_PROCESS_EXPORT ProjectFilesApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ProjectFilesApplicationPlugin(const score::GUIApplicationContext& ctx);
  ~ProjectFilesApplicationPlugin() override;

  score::GUIElements makeGUIElements() override;

  void on_documentSaveAs(score::Document& doc, const QString& newFileName) override;

private:
  void consolidate();

  QAction* m_action{};
};
}

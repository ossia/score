#pragma once
#include <QDialog>
#include <QMetaObject>

#include <score_plugin_engine_export.h>

class QLabel;
class QPushButton;
class QTreeWidget;
namespace score
{
struct DocumentContext;
struct GUIApplicationContext;
}
namespace LocalTree
{
//! Lists the broken references of a document, to locate or retarget them
class SCORE_PLUGIN_ENGINE_EXPORT ReferencesDialog final : public QDialog
{
public:
  explicit ReferencesDialog(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ReferencesDialog();

  void setDocument(const score::DocumentContext* doc);

private:
  void refresh();
  void locate();
  void rebind();

  QLabel* m_summary{};
  QTreeWidget* m_tree{};
  QPushButton* m_locate{};
  QPushButton* m_rebind{};
  const score::DocumentContext* m_doc{};
  QMetaObject::Connection m_connection;
};
}

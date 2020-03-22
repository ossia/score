#pragma once
#include <QTreeView>
class QFileSystemModel;
class QSortFilterProxyModel;

namespace score
{
struct GUIApplicationContext;
}

namespace Library
{
class SystemLibraryWidget : public QWidget
{
public:
  SystemLibraryWidget(
      const score::GUIApplicationContext& ctx,
      QWidget* parent);
  ~SystemLibraryWidget();

  void setRoot(QString path);

private:
  QFileSystemModel* m_model{};
  QSortFilterProxyModel* m_proxy{};
  QTreeView m_tv;
  QWidget m_preview;
  QWidget* m_previewChild{};
};
}

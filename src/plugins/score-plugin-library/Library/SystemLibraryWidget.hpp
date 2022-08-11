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
class FileSystemModel;
class FileSystemRecursiveFilterProxy;
class SystemLibraryWidget : public QWidget
{
public:
  SystemLibraryWidget(
      const score::GUIApplicationContext& ctx,
      QWidget* parent);
  ~SystemLibraryWidget();

  void setRoot(QString path);

private:
  FileSystemModel* m_model{};
  FileSystemRecursiveFilterProxy* m_proxy{};
  QTreeView m_tv;
  QWidget m_preview;
  QWidget* m_previewChild{};
};
}

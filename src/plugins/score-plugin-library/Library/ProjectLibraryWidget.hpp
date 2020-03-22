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
class ProjectLibraryWidget : public QWidget
{
public:
  ProjectLibraryWidget(
      const score::GUIApplicationContext& ctx,
      QWidget* parent);
  ~ProjectLibraryWidget();

  void setRoot(QString path);

private:
  QFileSystemModel* m_model{};
  QSortFilterProxyModel* m_proxy{};
  QTreeView m_tv;
};
}

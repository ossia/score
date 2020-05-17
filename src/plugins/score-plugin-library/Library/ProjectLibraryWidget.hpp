#pragma once
#include <QTreeView>
class QFileSystemModel;
class QSortFilterProxyModel;

namespace score
{
struct GUIApplicationContext;
struct DocumentMetadata;
}

namespace Library
{
class ProjectLibraryWidget : public QWidget
{
public:
  ProjectLibraryWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProjectLibraryWidget();

  void setRoot(score::DocumentMetadata& meta);
  void unsetRoot();

private:
  QFileSystemModel* m_model{};
  QSortFilterProxyModel* m_proxy{};
  QTreeView m_tv;
  QMetaObject::Connection m_con;
};
}

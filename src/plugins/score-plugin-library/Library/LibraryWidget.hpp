#pragma once
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/std/Optional.hpp>

#include <QTreeView>
class QFileSystemModel;
class QSortFilterProxyModel;
namespace score
{
struct GUIApplicationContext;
}
namespace Library
{
class ProcessTreeView : public QTreeView
{
  W_OBJECT(ProcessTreeView)
public:
  using QTreeView::QTreeView;
  void selected(optional<ProcessData> p) W_SIGNAL(selected, p);

  void selectionChanged(
      const QItemSelection& selected,
      const QItemSelection& deselected) override;
};

class ProcessWidget : public QWidget
{
public:
  ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProcessWidget();

private:
  ProcessesItemModel* m_model{};
  ProcessTreeView m_tv;
};
}

#include <QTreeView>
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

#include <QTreeView>
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

W_REGISTER_ARGTYPE(optional<Library::ProcessData>)
Q_DECLARE_METATYPE(optional<Library::ProcessData>)

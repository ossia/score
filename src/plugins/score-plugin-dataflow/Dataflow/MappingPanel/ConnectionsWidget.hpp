#pragma once
#include <Process/Dataflow/CableData.hpp>

#include <Dataflow/MappingPanel/ConnectionsList.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QPointer>
#include <QWidget>

#include <score_plugin_dataflow_export.h>

#include <nano_observer.hpp>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;
class Selection;
namespace score
{
class Document;
struct DocumentContext;
}

namespace Dataflow
{
//! Lists the cables and address bindings of a document, kept up to date while
//! shown. Edits go through the document's command stack.
class SCORE_PLUGIN_DATAFLOW_EXPORT ConnectionsWidget final
    : public QWidget
    , public Nano::Observer
{
public:
  enum Column
  {
    KindColumn,
    FromColumn,
    ToColumn,
    TypeColumn,
    ModeColumn,
    LocationColumn,
    ColumnCount
  };
  enum Grouping
  {
    NoGrouping,
    ByKind,
    ByFrom,
    ByTo,
    ByDevice
  };
  enum KindFilter
  {
    AllKinds,
    CablesOnly,
    AddressesOnly
  };
  //! Item data role holding the index in rows() of a connection item
  static constexpr int RowRole = Qt::UserRole + 1;

  explicit ConnectionsWidget(QWidget* parent = nullptr);
  ~ConnectionsWidget() override;

  void setDocument(const score::DocumentContext* ctx);
  //! Mirrors a selection made elsewhere in the document
  void setSelection(const Selection& s);

  const std::vector<ConnectionRow>& rows() const noexcept { return m_rows; }
  //! Rebuilds now instead of on the next event loop iteration
  void refresh();
  bool refreshPending() const noexcept { return m_scheduled || m_dirty; }

  QTreeView& view() const noexcept { return *m_view; }
  QLineEdit& searchField() const noexcept { return *m_search; }
  QComboBox& groupingBox() const noexcept { return *m_grouping; }
  QComboBox& kindBox() const noexcept { return *m_kinds; }

  //! Index in rows() of a view index, -1 for a group or nothing
  int rowAt(const QModelIndex& viewIndex) const;
  //! View index of the first column of a row, invalid when filtered out
  QModelIndex viewIndexOf(int row) const;

  //! Rows selected in the view
  std::vector<int> selectedRows() const;
  //! Copies, which stay valid across rebuilds
  std::vector<ConnectionRow> rowsAt(const std::vector<int>& rows) const;
  //! Selects the connections of the rows in the document and reveals the first
  void selectInDocument(const std::vector<int>& rows);
  //! Shows the processes of a row in the scenario or nodal view
  void reveal(const ConnectionRow& row);
  //! Cables are removed, address bindings cleared, in one command
  void removeConnections(const std::vector<ConnectionRow>& rows);
  void setCableType(const std::vector<ConnectionRow>& rows, Process::CableType t);

private:
  void schedule();
  void rebuild();
  void populate();
  //! Keeps selected the rows of these objects, which designate a row across rebuilds
  void populate(const ossia::hash_set<const QObject*>& selected);
  ossia::hash_set<const QObject*> selectedObjects() const;
  void track();
  void watchDocument();
  void unwatchDocument();
  void onViewSelectionChanged();
  void contextMenu(const QPoint& pos);
  void showEvent(QShowEvent* ev) override;
  bool eventFilter(QObject* obj, QEvent* ev) override;

  void on_cableAdded(const Process::Cable&);
  void on_cableRemoved(const Process::Cable&);

  //! The context lives as long as its document, which may go first
  bool alive() const noexcept { return m_ctx && !m_document.isNull(); }
  const score::DocumentContext* m_ctx{};
  QPointer<const score::Document> m_document;
  std::vector<ConnectionRow> m_rows;

  QLineEdit* m_search{};
  QComboBox* m_grouping{};
  QComboBox* m_kinds{};
  QLabel* m_summary{};
  QTreeView* m_view{};
  QStandardItemModel* m_model{};
  QSortFilterProxyModel* m_proxy{};

  //! Parent of the per-object connections, replaced on each rebuild
  QObject* m_tracking{};
  std::vector<QMetaObject::Connection> m_documentConnections;
  QPointer<QObject> m_watchedScenario;

  bool m_scheduled{};
  bool m_dirty{true};
  bool m_syncingSelection{};
};
}

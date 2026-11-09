#pragma once
#include <Process/Dataflow/CableData.hpp>

#include <Dataflow/MappingPanel/ConnectionsList.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QPointer>
#include <QWidget>

#include <score_plugin_dataflow_export.h>

#include <nano_observer.hpp>

#include <vector>

class QComboBox;
class QLineEdit;
class QMenu;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItem;
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
    //! An icon for a cable or for an address, in the color of the port type;
    //! the words, the type and the cable mode are in its tooltip
    KindColumn,
    //! A warning icon when an address is not bound, an information icon for
    //! a pattern, empty otherwise; its tooltip says why
    StatusColumn,
    FromColumn,
    ToColumn,
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
  //! Item data role holding the name of a group item, without its count
  static constexpr int GroupRole = Qt::UserRole + 2;
  //! Item data role holding the text the search matches: what the cell shows,
  //! and for the Kind cell the port type and cable mode of its tooltip
  static constexpr int SearchRole = Qt::UserRole + 3;
  //! Item data role holding the Process::PortType shown by the Kind cell's icon
  static constexpr int PortTypeRole = Qt::UserRole + 4;

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
  //! View index of the Kind column of a row, invalid when filtered out
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
  //! Sets the mode of the cables of the rows in one command
  void setCableType(const std::vector<ConnectionRow>& rows, Process::CableType t);
  //! Builds the context menu of the selection, the clicked row first
  void fillContextMenu(QMenu& menu, int clickedRow);

private:
  void schedule();
  void rebuild();
  void populate();
  //! Keeps selected the rows of these objects, which designate a row across rebuilds
  void populate(const ossia::hash_set<const QObject*>& selected);
  ossia::hash_set<const QObject*> selectedObjects() const;
  //! Connects to the objects of the document not followed yet
  void track();
  void untrack();
  void updateCounts();
  void spanGroups();
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
  //! First-column item of each row in the model, null when filtered by kind
  std::vector<QStandardItem*> m_items;
  //! Rows of each selectable object
  ossia::hash_map<const QObject*, ossia::small_vector<int, 1>> m_rowsOf;
  //! Group items and their names
  std::vector<std::pair<QStandardItem*, QString>> m_groups;
  //! Names of the groups the user collapsed
  ossia::hash_set<QString> m_collapsed;

  QLineEdit* m_search{};
  QComboBox* m_grouping{};
  QComboBox* m_kinds{};
  QTreeView* m_view{};
  QStandardItemModel* m_model{};
  QSortFilterProxyModel* m_proxy{};

  //! Parent of the per-object connections
  QObject* m_tracking{};
  //! Objects of the document whose changes are followed
  ossia::hash_set<QObject*> m_tracked;
  std::vector<QMetaObject::Connection> m_documentConnections;
  QPointer<QObject> m_watchedScenario;

  bool m_scheduled{};
  bool m_dirty{true};
  bool m_syncingSelection{};
  //! A selection made elsewhere while hidden, mirrored when shown
  bool m_selectionPending{};
};
}

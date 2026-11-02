#include "Inspector.hpp"

#include <score/document/DocumentContext.hpp>

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QStandardItemModel>
#include <QTimerEvent>
#include <QTreeView>
#include <QVBoxLayout>

#include <vector>

namespace Gfx::GeometryViewer
{
namespace
{
constexpr int row_role = Qt::UserRole + 1;

QString kindName(SceneRow::Kind k)
{
  using K = SceneRow::Kind;
  switch(k)
  {
    case K::Node:
      return QObject::tr("Node");
    case K::Mesh:
      return QObject::tr("Mesh");
    case K::Material:
      return QObject::tr("Material");
    case K::Light:
      return QObject::tr("Light");
    case K::Camera:
      return QObject::tr("Camera");
    case K::Skeleton:
      return QObject::tr("Skeleton");
    case K::Animation:
      return QObject::tr("Animation");
    case K::Instance:
      return QObject::tr("Instances");
    case K::Data:
      return QObject::tr("Data");
    case K::Splat:
      return QObject::tr("Gaussian splats");
    case K::Voxels:
      return QObject::tr("Voxels");
    case K::PointCloud:
      return QObject::tr("Point cloud");
    case K::Volume:
      return QObject::tr("Volume");
    case K::PrimitiveCloud:
      return QObject::tr("Primitive cloud");
  }
  return {};
}

//! Same objects in the same places: only their details may differ
bool sameStructure(const SceneSummary& a, const SceneSummary& b)
{
  if(a.rows.size() != b.rows.size())
    return false;
  for(std::size_t i = 0; i < a.rows.size(); i++)
    if(a.rows[i].parent != b.rows[i].parent || a.rows[i].kind != b.rows[i].kind
       || a.rows[i].name != b.rows[i].name)
      return false;
  return true;
}
}

InspectorWidget::InspectorWidget(
    const Model& process, const score::DocumentContext& context, QWidget* parent)
    : InspectorWidgetDelegate_T{process, parent}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);

  m_status = new QLabel{this};
  m_status->setWordWrap(true);
  lay->addWidget(m_status);

  m_model = new QStandardItemModel{this};
  m_model->setHorizontalHeaderLabels({tr("Object"), tr("Details")});
  m_tree = new QTreeView{this};
  m_tree->setModel(m_model);
  m_tree->setUniformRowHeights(true);
  m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->header()->setStretchLastSection(true);
  m_tree->setMinimumHeight(180);
  m_tree->setToolTip(tr("Select an object to outline it in the viewer"));
  lay->addWidget(m_tree);
  connect(
      m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
      [this] { select(); });

  connect(&process, &Process::ProcessModel::monitorChanged, this, [this] {
    m_shown.reset();
    watch(isVisible());
    refresh();
  });
  refresh();
}

InspectorWidget::~InspectorWidget()
{
  if(auto* m = monitor())
    m->selected.store(-1, std::memory_order_relaxed);
  watch(false);
}

void InspectorWidget::watch(bool shown)
{
  const auto& current = process().monitor();
  if(shown && m_watched == current)
    return;
  if(m_watched)
    m_watched->viewers.fetch_sub(1, std::memory_order_relaxed);
  m_watched = shown ? current : nullptr;
  if(m_watched)
    m_watched->viewers.fetch_add(1, std::memory_order_relaxed);
}

SceneMonitor* InspectorWidget::monitor() const noexcept
{
  // The monitor of a Geometry viewer is the one its executor makes
  return static_cast<SceneMonitor*>(process().monitor().get());
}

void InspectorWidget::showEvent(QShowEvent* ev)
{
  QWidget::showEvent(ev);
  watch(true);
  if(!m_timer)
    m_timer = startTimer(250);
  refresh();
}

void InspectorWidget::hideEvent(QHideEvent* ev)
{
  QWidget::hideEvent(ev);
  watch(false);
  if(m_timer)
    killTimer(m_timer);
  m_timer = 0;
}

void InspectorWidget::timerEvent(QTimerEvent* ev)
{
  if(ev->timerId() == m_timer)
    refresh();
}

void InspectorWidget::refresh()
{
  auto* m = monitor();
  std::shared_ptr<const SceneSummary> latest;
  if(m)
    latest = m->latest();

  if(!latest)
  {
    m_status->setText(
        m ? tr("No scene on the geometry inlet")
          : tr("The objects of the scene show here while the process plays"));
    if(m_shown)
    {
      m_model->removeRows(0, m_model->rowCount());
      m_shown.reset();
    }
    return;
  }
  if(latest == m_shown)
    return;

  const auto& c = latest->counts;
  m_status->setText(tr("%1 nodes, %2 meshes, %3 primitives, %4 vertices, %5 materials, "
                       "%6 lights, %7 cameras")
                        .arg(c.nodes)
                        .arg(c.meshes)
                        .arg(c.primitives)
                        .arg(c.vertices)
                        .arg(c.materials)
                        .arg(c.lights)
                        .arg(c.cameras));

  if(m_shown && sameStructure(*m_shown, *latest))
  {
    // In place: what is expanded and selected stays so
    const int rows = int(latest->rows.size());
    std::vector<QStandardItem*> stack{m_model->invisibleRootItem()};
    while(!stack.empty())
    {
      auto* parent = stack.back();
      stack.pop_back();
      for(int i = 0; i < parent->rowCount(); i++)
      {
        auto* item = parent->child(i, 0);
        const int row = item->data(row_role).toInt();
        if(row >= 0 && row < rows)
          if(auto* details = parent->child(i, 1))
            details->setText(QString::fromStdString(latest->rows[row].details));
        stack.push_back(item);
      }
    }
    m_shown = latest;
    select();
    return;
  }

  rebuild(*latest);
  m_shown = latest;
  select();
}

void InspectorWidget::rebuild(const SceneSummary& s)
{
  m_model->removeRows(0, m_model->rowCount());
  std::vector<QStandardItem*> items(s.rows.size());
  for(std::size_t i = 0; i < s.rows.size(); i++)
  {
    const auto& r = s.rows[i];
    const QString name = r.kind == SceneRow::Kind::Node
                             ? QString::fromStdString(r.name)
                             : kindName(r.kind);
    auto* item = new QStandardItem{name};
    item->setData(int(i), row_role);
    item->setToolTip(kindName(r.kind));
    auto* details = new QStandardItem{QString::fromStdString(r.details)};
    details->setToolTip(details->text());
    QStandardItem* parent = r.parent >= 0 && r.parent < int(i) ? items[r.parent]
                                                               : m_model->invisibleRootItem();
    parent->appendRow({item, details});
    items[i] = item;
  }
  // The first levels: a large scene stays readable
  m_tree->expandToDepth(1);
  m_tree->resizeColumnToContents(0);
}

void InspectorWidget::select()
{
  auto* m = monitor();
  if(!m || !m_shown)
    return;
  const QModelIndex current = m_tree->currentIndex();
  const QModelIndex first = current.sibling(current.row(), 0);
  const int row = first.isValid() ? first.data(row_role).toInt() : -1;
  m->selected.store(
      SceneMonitor::selection(m_shown->serial, row), std::memory_order_relaxed);
}
}

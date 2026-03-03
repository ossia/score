#include "ClipLauncherView.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <QPainter>

#include <cmath>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/SceneModel.hpp>

W_OBJECT_IMPL(ClipLauncher::ClipLauncherView)

namespace ClipLauncher
{

ClipLauncherView::ClipLauncherView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
  this->setAcceptDrops(true);
}

ClipLauncherView::~ClipLauncherView() { }

void ClipLauncherView::setModel(const ProcessModel* model)
{
  m_model = model;
  update();
}

double ClipLauncherView::cellWidth() const
{
  if(!m_model || m_model->laneCount() == 0)
    return MaxCellWidth;

  double available = width() - SceneHeaderWidth - SceneLaunchButtonWidth;
  double w = (available - CellPadding * (m_model->laneCount() - 1)) / m_model->laneCount();
  return std::clamp(w, MinCellWidth, MaxCellWidth);
}

double ClipLauncherView::cellHeight() const
{
  if(!m_model || m_model->sceneCount() == 0)
    return MaxCellHeight;

  double available = height() - LaneHeaderHeight;
  double h = (available - CellPadding * (m_model->sceneCount() - 1)) / m_model->sceneCount();
  return std::clamp(h, MinCellHeight, MaxCellHeight);
}

void ClipLauncherView::paint_impl(QPainter* painter) const
{
  if(!m_model)
    return;

  painter->setRenderHint(QPainter::Antialiasing, false);

  paintLaneHeaders(painter);
  paintSceneHeaders(painter);
  paintGrid(painter);
}

void ClipLauncherView::paintLaneHeaders(QPainter* painter) const
{
  const double cw = cellWidth();
  painter->setPen(Qt::white);
  painter->setFont(QFont("Ubuntu", 9));

  int laneIdx = 0;
  for(const auto& lane : m_model->lanes)
  {
    double x = SceneHeaderWidth + SceneLaunchButtonWidth + laneIdx * (cw + CellPadding);
    QRectF headerRect(x, 0, cw, LaneHeaderHeight);
    painter->fillRect(headerRect, QColor(60, 60, 70));
    painter->drawText(
        headerRect, Qt::AlignCenter,
        lane.name().isEmpty() ? QString("Lane %1").arg(laneIdx + 1) : lane.name());
    laneIdx++;
  }
}

void ClipLauncherView::paintSceneHeaders(QPainter* painter) const
{
  const double ch = cellHeight();
  painter->setPen(Qt::white);
  painter->setFont(QFont("Ubuntu", 9));

  int sceneIdx = 0;
  for(const auto& scene : m_model->scenes)
  {
    double y = LaneHeaderHeight + sceneIdx * (ch + CellPadding);

    // Scene name
    QRectF headerRect(0, y, SceneHeaderWidth, ch);
    painter->fillRect(headerRect, QColor(50, 50, 60));
    painter->drawText(
        headerRect, Qt::AlignCenter,
        scene.name().isEmpty() ? QString("Scene %1").arg(sceneIdx + 1) : scene.name());

    // Scene launch button
    QRectF launchRect(SceneHeaderWidth, y, SceneLaunchButtonWidth, ch);
    paintSceneLaunchButton(painter, sceneIdx, launchRect);

    sceneIdx++;
  }
}

void ClipLauncherView::paintSceneLaunchButton(
    QPainter* painter, int scene, const QRectF& rect) const
{
  painter->fillRect(rect, QColor(70, 70, 80));
  painter->setPen(QColor(120, 200, 120));

  // Draw a play triangle
  double cx = rect.center().x();
  double cy = rect.center().y();
  double sz = std::min(8.0, rect.height() * 0.2);
  QPolygonF triangle;
  triangle << QPointF(cx - sz * 0.5, cy - sz) << QPointF(cx + sz, cy)
           << QPointF(cx - sz * 0.5, cy + sz);
  painter->setBrush(QColor(120, 200, 120));
  painter->drawPolygon(triangle);
  painter->setBrush(Qt::NoBrush);
}

void ClipLauncherView::paintGrid(QPainter* painter) const
{
  int laneCount = m_model->laneCount();
  int sceneCount = m_model->sceneCount();

  for(int scene = 0; scene < sceneCount; scene++)
  {
    for(int lane = 0; lane < laneCount; lane++)
    {
      QRectF rect = cellRect(lane, scene);
      paintCell(painter, lane, scene, rect);
    }
  }
}

void ClipLauncherView::paintCell(
    QPainter* painter, int lane, int scene, const QRectF& rect) const
{
  auto* cell = m_model->cellAt(lane, scene);

  if(!cell)
  {
    // Empty cell
    painter->fillRect(rect, QColor(40, 40, 45));
    painter->setPen(QColor(60, 60, 65));
    painter->drawRect(rect);
    return;
  }

  // Cell background based on state
  QColor bg;
  switch(cell->cellState())
  {
    case CellState::Stopped:
      bg = QColor(55, 55, 65);
      break;
    case CellState::Queued:
      bg = QColor(120, 100, 30);
      break;
    case CellState::Playing:
      bg = QColor(30, 100, 50);
      break;
    case CellState::Stopping:
      bg = QColor(120, 60, 30);
      break;
    default:
      bg = QColor(40, 40, 45);
      break;
  }

  painter->fillRect(rect, bg);

  // Cell border
  painter->setPen(QColor(80, 80, 90));
  painter->drawRect(rect);

  // Cell name
  painter->setPen(Qt::white);
  painter->setFont(QFont("Ubuntu", 8));
  QString name = cell->interval().metadata().getName();
  if(name.isEmpty())
    name = QString("Clip %1,%2").arg(lane + 1).arg(scene + 1);
  painter->drawText(rect.adjusted(4, 4, -4, -20), Qt::AlignLeft | Qt::AlignTop, name);

  // Process count indicator
  int procCount = cell->interval().processes.size();
  if(procCount > 0)
  {
    painter->setFont(QFont("Ubuntu", 7));
    painter->setPen(QColor(150, 150, 160));
    painter->drawText(
        rect.adjusted(4, 0, -4, -4), Qt::AlignLeft | Qt::AlignBottom,
        QString("%1 process%2").arg(procCount).arg(procCount > 1 ? "es" : ""));
  }

  // Progress bar (use fmod to loop the indicator when progress > 1)
  if(cell->cellState() == CellState::Playing && cell->progress() > 0)
  {
    double p = std::fmod(cell->progress(), 1.0);
    if(p <= 0.)
      p = 1.;
    double pw = rect.width() * p;
    QRectF progressRect(rect.left(), rect.bottom() - 3, pw, 3);
    painter->fillRect(progressRect, QColor(80, 200, 120));
  }

  // Transition rule indicator
  if(!cell->transitionRules().empty())
  {
    painter->setPen(QColor(200, 180, 80));
    painter->setFont(QFont("Ubuntu", 7));
    painter->drawText(
        rect.adjusted(0, 4, -4, 0), Qt::AlignRight | Qt::AlignTop,
        QString::fromUtf8("\xe2\x86\x92")); // →
  }

  // Loop count
  if(cell->loopCount() > 0)
  {
    painter->setPen(QColor(150, 150, 160));
    painter->setFont(QFont("Ubuntu", 7));
    painter->drawText(
        rect.adjusted(0, 0, -4, -4), Qt::AlignRight | Qt::AlignBottom,
        QString("x%1").arg(cell->loopCount()));
  }
}

QRectF ClipLauncherView::cellRect(int lane, int scene) const
{
  const double cw = cellWidth();
  const double ch = cellHeight();
  double x = SceneHeaderWidth + SceneLaunchButtonWidth + lane * (cw + CellPadding);
  double y = LaneHeaderHeight + scene * (ch + CellPadding);
  return QRectF(x, y, cw, ch);
}

std::optional<std::pair<int, int>> ClipLauncherView::cellAtPos(QPointF pos) const
{
  if(!m_model)
    return {};

  const double cw = cellWidth();
  const double ch = cellHeight();

  double x = pos.x() - SceneHeaderWidth - SceneLaunchButtonWidth;
  double y = pos.y() - LaneHeaderHeight;

  if(x < 0 || y < 0)
    return {};

  int lane = static_cast<int>(x / (cw + CellPadding));
  int scene = static_cast<int>(y / (ch + CellPadding));

  if(lane >= m_model->laneCount() || scene >= m_model->sceneCount())
    return {};

  // Check we're within the cell, not in padding
  double cellX = x - lane * (cw + CellPadding);
  double cellY = y - scene * (ch + CellPadding);
  if(cellX > cw || cellY > ch)
    return {};

  return std::make_pair(lane, scene);
}

std::optional<int> ClipLauncherView::sceneLaunchAtPos(QPointF pos) const
{
  if(!m_model)
    return {};

  const double ch = cellHeight();

  double x = pos.x();
  double y = pos.y() - LaneHeaderHeight;

  if(x < SceneHeaderWidth || x > SceneHeaderWidth + SceneLaunchButtonWidth || y < 0)
    return {};

  int scene = static_cast<int>(y / (ch + CellPadding));
  if(scene >= m_model->sceneCount())
    return {};

  return scene;
}

std::optional<int> ClipLauncherView::laneHeaderAtPos(QPointF pos) const
{
  if(!m_model)
    return {};
  if(pos.y() < 0 || pos.y() > LaneHeaderHeight)
    return {};
  const double cw = cellWidth();
  double x = pos.x() - SceneHeaderWidth - SceneLaunchButtonWidth;
  if(x < 0)
    return {};
  int lane = static_cast<int>(x / (cw + CellPadding));
  if(lane >= m_model->laneCount())
    return {};
  double cellX = x - lane * (cw + CellPadding);
  if(cellX > cw)
    return {};
  return lane;
}

std::optional<int> ClipLauncherView::sceneHeaderAtPos(QPointF pos) const
{
  if(!m_model)
    return {};
  if(pos.x() < 0 || pos.x() > SceneHeaderWidth)
    return {};
  const double ch = cellHeight();
  double y = pos.y() - LaneHeaderHeight;
  if(y < 0)
    return {};
  int scene = static_cast<int>(y / (ch + CellPadding));
  if(scene >= m_model->sceneCount())
    return {};
  double cellY = y - scene * (ch + CellPadding);
  if(cellY > ch)
    return {};
  return scene;
}

void ClipLauncherView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    auto pos = event->pos();

    // Check lane header
    if(auto lane = laneHeaderAtPos(pos))
    {
      laneHeaderClicked(*lane);
      event->accept();
      return;
    }

    // Check scene header
    if(auto scene = sceneHeaderAtPos(pos))
    {
      sceneHeaderClicked(*scene);
      event->accept();
      return;
    }

    // Check scene launch button
    if(auto scene = sceneLaunchAtPos(pos))
    {
      sceneLaunchClicked(*scene);
      event->accept();
      return;
    }

    // Check cell
    if(auto cell = cellAtPos(pos))
    {
      cellClicked(cell->first, cell->second);
      event->accept();
      return;
    }
  }
  event->ignore();
}

void ClipLauncherView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(auto cell = cellAtPos(event->pos()))
  {
    cellDoubleClicked(cell->first, cell->second);
    event->accept();
    return;
  }
  event->ignore();
}

void ClipLauncherView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  // Forward to base which triggers askContextMenu -> fillContextMenu on presenter
  Process::LayerView::contextMenuEvent(event);
}

void ClipLauncherView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void ClipLauncherView::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void ClipLauncherView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void ClipLauncherView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  auto pos = event->pos();
  if(auto cell = cellAtPos(pos))
  {
    dropOnCell(cell->first, cell->second, *event->mimeData());
    event->accept();
    return;
  }
  event->ignore();
}

} // namespace ClipLauncher

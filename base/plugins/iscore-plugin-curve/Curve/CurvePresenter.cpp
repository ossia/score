#include <Curve/Commands/UpdateCurve.hpp>

#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/operators.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <set>

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QtAlgorithms>
#include <iscore/widgets/GraphicsItem.hpp>
#include <qnamespace.h>

#include <QApplication>
#include <QPointer>
#include <QSet>
#include <QSize>
#include <QString>
#include <QVariant>
#include <utility>
#include <vector>

#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

#include "CurveModel.hpp"
#include "CurvePresenter.hpp"
#include "CurveView.hpp"
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/IdentifiedObjectAbstract.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

namespace Curve
{
struct Style;

static QPointF myscale(QPointF first, QSizeF second)
{
  return {first.x() * second.width(), (1. - first.y()) * second.height()};
}

Presenter::Presenter(
    const iscore::DocumentContext& context,
    const Curve::Style& style,
    const Model& model,
    View* view,
    QObject* parent)
    : QObject{parent}
    , m_curveSegments{context.app.interfaces<SegmentList>()}
    , m_model{model}
    , m_view{view}
    , m_commandDispatcher{context.commandStack}
    , m_style{style}
{
  // For each segment in the model, create a segment and relevant points in the
  // view.
  // If the segment is linked to another, the point is shared.
  setupView();
  setupSignals();

  connect(
      m_view, &View::contextMenuRequested, this,
      &Presenter::contextMenuRequested);
}

Presenter::~Presenter()
{
}

void Presenter::setRect(const QRectF& rect)
{
  m_localRect = rect;
  // Positions
  for (auto& curve_pt : m_points)
  {
    setPos(curve_pt);
  }

  for (auto& curve_segt : m_segments)
  {
    setPos(curve_segt);
  }
}

void Presenter::setPos(PointView& point)
{
  point.setPosition(myscale(point.model().pos(), m_localRect.size()));
}

void Presenter::setPos(SegmentView& segment)
{
  // Pos is the top-left corner of the segment
  // Width is from begin to end
  // Height is the height of the curve since the segment can do anything
  // in-between.
  double startx, endx;
  startx = segment.model().start().x() * m_localRect.width();
  endx = segment.model().end().x() * m_localRect.width();
  segment.setPosition({startx, 0});
  segment.setRect({0., 0., endx - startx, m_localRect.height()});
}

void Presenter::setupSignals()
{
  con(m_model, &Model::segmentAdded, this, [&](const SegmentModel& segment) {
    addSegment(new SegmentView{&segment, m_style, m_view});
  });

  con(m_model, &Model::pointAdded, this, [&](const PointModel& point) {
    addPoint(new PointView{&point, m_style, m_view});
  });

  con(m_model, &Model::pointRemoved, this, [&](const Id<PointModel>& m) {
    auto& map = m_points.get();
    auto it = map.find(m);
    if (it != map.end()) // TODO should never happen ?
    {
      delete *it;
      map.erase(it);
    }
  });

  con(m_model, &Model::segmentRemoved, this, [&](const Id<SegmentModel>& m) {
    auto& map = m_segments.get();
    auto it = map.find(m);
    if (it != map.end()) // TODO should never happen ?
    {
      delete *it;
      map.erase(it);
    }
  });

  con(m_model, &Model::cleared, this, [&]() {
    qDeleteAll(m_points.get());
    qDeleteAll(m_segments.get());
    m_points.clear();
    m_segments.clear();
  });

  con(m_model, &Model::curveReset, this, &Presenter::modelReset);
}

void Presenter::setupView()
{
  // Initialize the elements
  for (const auto& segment : m_model.segments())
  {
    addSegment(new SegmentView{&segment, m_style, m_view});
  }

  for (PointModel* pt : m_model.points())
  {
    addPoint(new PointView{pt, m_style, m_view});
  }

  // Setup the actions
  m_actions = new QActionGroup{this};
  m_actions->setExclusive(true);

  auto shiftact = new QAction{m_actions};
  shiftact->setCheckable(true);
  shiftact->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_actions->addAction(shiftact);

  auto ctrlact = new QAction{m_actions};
  ctrlact->setCheckable(true);
  ctrlact->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_actions->addAction(ctrlact);

  auto altact = new QAction{m_actions};
  altact->setCheckable(true);
  altact->setChecked(false);
  altact->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_actions->addAction(altact);

  m_actions->setEnabled(true);

  connect(altact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::CreatePen);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });
  connect(shiftact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::SetSegment);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });
  connect(ctrlact, &QAction::toggled, this, [&](bool b) {
    if (b)
    {
      editionSettings().setTool(Curve::Tool::Create);
    }
    else
    {
      editionSettings().setTool(Curve::Tool::Select);
    }
  });

  connect(m_view, &View::keyPressed, this, [=](int key) {
    if (key == Qt::Key_Shift)
    {
      shiftact->setChecked(true);
    }
    if (key == Qt::Key_Control)
    {
      ctrlact->setChecked(true);
    }
    if (key == Qt::Key_Alt)
    {
      altact->setChecked(!altact->isChecked());
    }
  });

  connect(m_view, &View::keyReleased, this, [=](int key) {
    if (key == Qt::Key_Shift)
    {
      shiftact->setChecked(false);
    }
    if (key == Qt::Key_Control)
    {
      ctrlact->setChecked(false);
    }
    if (key == Qt::Key_Alt)
    {
      altact->setChecked(!altact->isChecked());
    }
  });
}

void Presenter::fillContextMenu(
    QMenu& menu, const QPoint& pos, const QPointF& scenepos)
{
  menu.addSeparator();

  auto removeAct = new QAction{tr("Remove"), this};
  connect(removeAct, &QAction::triggered, [&]() { removeSelection(); });

  auto typeMenu = menu.addMenu(tr("Type"));
  for (const auto& seg : m_curveSegments)
  {
    auto act = typeMenu->addAction(seg.prettyName());
    connect(act, &QAction::triggered, this, [
      this, key = seg.concreteKey()
    ]() { updateSegmentsType(key); });
  }

  auto lockAction = new QAction{tr("Lock between points"), this};
  connect(lockAction, &QAction::toggled, this, [&](bool b) {
    m_editionSettings.setLockBetweenPoints(b);
  });
  lockAction->setCheckable(true);
  lockAction->setChecked(m_editionSettings.lockBetweenPoints());

  auto suppressAction = new QAction{tr("Suppress on overlap"), this};
  connect(suppressAction, &QAction::toggled, this, [&](bool b) {
    m_editionSettings.setSuppressOnOverlap(b);
  });

  suppressAction->setCheckable(true);
  suppressAction->setChecked(m_editionSettings.suppressOnOverlap());

  menu.addAction(removeAct);
  menu.addAction(lockAction);
  menu.addAction(suppressAction);
}

void Presenter::addPoint(PointView* pt_view)
{
  setupPointConnections(pt_view);
  addPoint_impl(pt_view);
}

void Presenter::addSegment(SegmentView* seg_view)
{
  setupSegmentConnections(seg_view);
  addSegment_impl(seg_view);
}

void Presenter::addPoint_impl(PointView* pt_view)
{
  m_points.insert(pt_view);
  setPos(*pt_view);

  m_enabled ? pt_view->enable() : pt_view->disable();
}

void Presenter::addSegment_impl(SegmentView* seg_view)
{
  m_segments.insert(seg_view);
  setPos(*seg_view);

  m_enabled ? seg_view->enable() : seg_view->disable();
}

void Presenter::setupPointConnections(PointView* pt_view)
{
  connect(
      pt_view, &PointView::contextMenuRequested, m_view,
      &View::contextMenuRequested);
  con(pt_view->model(), &PointModel::posChanged, this,
      [=]() { setPos(*pt_view); });
}

void Presenter::setupSegmentConnections(SegmentView* seg_view)
{
  connect(
      seg_view, &SegmentView::contextMenuRequested, m_view,
        &View::contextMenuRequested);
}

void Presenter::modelReset()
{
  // 1. We put our current elements in our pool.
  std::vector<PointView*> points(m_points.get().begin(), m_points.get().end());
  std::vector<SegmentView*> segments(
      m_segments.get().begin(), m_segments.get().end());

  std::vector<PointView*> newPoints;
  std::vector<SegmentView*> newSegments;

  // 2. We add / remove new elements if necessary
  {
    int diff_points = m_model.points().size() - points.size();
    if (diff_points > 0)
    {
      points.reserve(points.size() + diff_points);
      for (; diff_points-- > 0;)
      {
        auto pt = new PointView{nullptr, m_style, m_view};
        points.push_back(pt);
        newPoints.push_back(pt);
      }
    }
    else if (diff_points < 0)
    {
      int inv_diff_points = -diff_points;
      for (; inv_diff_points-- > 0;)
      {
        deleteGraphicsItem(points[points.size() - inv_diff_points - 1]);
      }
      points.resize(points.size() + diff_points);
    }
  }

  // Same for segments
  {
    int diff_segts = m_model.segments().size() - segments.size();
    if (diff_segts > 0)
    {
      segments.reserve(segments.size() + diff_segts);
      for (; diff_segts-- > 0;)
      {
        auto seg = new SegmentView{nullptr, m_style, m_view};
        segments.push_back(seg);
        newSegments.push_back(seg);
      }
    }
    else if (diff_segts < 0)
    {
      int inv_diff_segts = -diff_segts;
      for (; inv_diff_segts-- > 0;)
      {
        deleteGraphicsItem(segments[segments.size() - inv_diff_segts - 1]);
      }
      segments.resize(segments.size() + diff_segts);
    }
  }

  ISCORE_ASSERT(points.size() == m_model.points().size());
  ISCORE_ASSERT(segments.size() == m_model.segments().size());

  // 3. We set the data
  { // Points
    int i = 0;
    for (auto point : m_model.points())
    {
      points[i]->setModel(point);
      i++;
    }
  }
  { // Segments
    int i = 0;
    for (const auto& segment : m_model.segments())
    {
      segments[i]->setModel(&segment);
      i++;
    }
  }

  for (auto seg : newSegments)
    setupSegmentConnections(seg);
  for (auto pt : newPoints)
    setupPointConnections(pt);

  // Now the ones that have a new model
  // 4. We put them all back in our maps.
  m_points.clear();
  m_segments.clear();

  for (auto pt_view : points)
  {
    addPoint_impl(pt_view);
  }
  for (auto seg_view : segments)
  {
    addSegment_impl(seg_view);
  }
}

void Presenter::enableActions(bool b)
{
  m_actions->setEnabled(b);
//  m_view->setFocus();
}

void Presenter::enable()
{
  for (auto& segment : m_segments)
  {
    segment.enable();
  }
  for (auto& point : m_points)
  {
    point.enable();
  }

  m_enabled = true;
}

void Presenter::disable()
{
  for (auto& segment : m_segments)
  {
    segment.disable();
  }
  for (auto& point : m_points)
  {
    point.disable();
  }

  m_enabled = false;
}

// TESTME
void Presenter::removeSelection()
{
  // We remove all that is selected,
  // And set the bounds correctly
  std::set<Id<SegmentModel>> segmentsToDelete;

  // First find the segments that will be deleted.
  // If a point is selected, the segments linked to that point
  // will be deleted, too.
  for (const auto& elt : m_model.selectedChildren())
  {
    if (auto point = dynamic_cast<const PointModel*>(elt.data()))
    {
      if (point->previous() && point->following())
      {
        if (point->previous())
          segmentsToDelete.insert(*point->previous());
        if (point->following())
          segmentsToDelete.insert(*point->following());
      }
    }

    /*
    if(auto segmt = dynamic_cast<const SegmentModel*>(elt.data()))
    {
        segmentsToDelete.insert(segmt->id());
    }
    */
  }

  if (segmentsToDelete.empty())
    return;

  double y0 = 0;
  double y1 = 1;
  bool firstRemoved = false;
  bool lastRemoved = false;
  // Then remove
  auto newSegments = model().toCurveData();
  {
    auto it = newSegments.begin();
    while (it != newSegments.end())
    {
      if (ossia::contains(segmentsToDelete, it->id))
      {
        if (it->start.x() == 0)
        {
          firstRemoved = true;
          y0 = it->start.y();
        }
        else if (it->end.x() == 1)
        {
          lastRemoved = true;
          y1 = it->end.y();
        }
        it = newSegments.erase(it);
        continue;
      }

      if (it->previous && ossia::contains(segmentsToDelete, it->previous))
        it->previous = OptionalId<SegmentModel>{};
      if (it->following && ossia::contains(segmentsToDelete, it->following))
        it->following = OptionalId<SegmentModel>{};

      it++;
    }
  }

  // Recreate if appropriate
  if (editionSettings().removePointBehaviour()
      == RemovePointBehaviour::RemoveAndAddSegment)
  {
    // Find the "holes" in the new segment list.
    ossia::sort(newSegments, [](const SegmentData& s1, const SegmentData& s2) {
      return s1.x() < s2.x();
    });

    // First if there is no segments, we recreate one.
    if (newSegments.empty())
    {
      SegmentData d;
      d.start = QPointF{0, y0};
      d.end = QPointF{1, y1};
      d.id = getSegmentId(newSegments);
      d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
      d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
      newSegments.push_back(d);
    }
    else
    {
      if (firstRemoved)
      {
        // Recreate a segment from x = 0 to the beginning of the first segment.
        auto it = newSegments.begin();

        // Create a new segment
        SegmentData d;
        d.start = QPointF{0, y0};
        d.end = it->start;
        d.following = it->id;
        d.id = getSegmentId(newSegments);
        d.type
            = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        it->previous = d.id;

        newSegments.insert(it, d);
      }

      if (lastRemoved)
      {
        // Recreate a segment from x = 0 to the beginning of the first segment.
        auto it = newSegments.rbegin();

        // Create a new segment
        SegmentData d;
        d.end = QPointF{1, y1};
        d.start = it->end;
        d.previous = it->id;
        d.id = getSegmentId(newSegments);
        d.type
            = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        it->following = d.id;

        newSegments.insert(newSegments.end(), d);
      }
    }

    // Then try to fill the holes
    auto it = newSegments.begin();
    for (; it != newSegments.end();)
    {
      // Check if it's the last segment
      auto next = it + 1;
      if (next == newSegments.end())
        break;

      if (it->following)
      {
        it = next;
      }
      else
      {
        // Create a new segment
        SegmentData d;
        d.start = it->end;
        d.end = next->start;
        d.previous = it->id;
        d.following = next->id;
        d.id = getSegmentId(newSegments);
        d.type
            = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        it->following = d.id;
        next->previous = d.id;

        it = newSegments.insert(it, d);
        // it is now at the position of the new segment.

        ++it;
        // it is now at the position of next
      }
    }
  }

  // Apply the changes.
  m_commandDispatcher.submitCommand(
      new UpdateCurve{m_model, std::move(newSegments)});
}

void Presenter::updateSegmentsType(
    const UuidKey<Curve::SegmentFactory>& segment)
{
  // They keep their start / end and previous / following but change type.
  auto factory = m_curveSegments.get(segment);
  auto this_type_base_data = factory->makeCurveSegmentData();
  auto newSegments = model().toCurveData();

  for (auto& seg_data : newSegments)
  {
    if (model().segments().at(seg_data.id).selection.get())
    {
      seg_data.type = segment;
      seg_data.specificSegmentData = this_type_base_data;
    }
  }

  m_commandDispatcher.submitCommand(
      new UpdateCurve{m_model, std::move(newSegments)});
}

} // namespace Curve

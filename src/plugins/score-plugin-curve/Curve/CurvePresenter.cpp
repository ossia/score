// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePresenter.hpp"

#include "CurveModel.hpp"
#include "CurveView.hpp"

#include <Curve/ApplicationPlugin.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <QAction>
#include <QMenu>
#include <QSize>
#include <QString>
#include <QVariant>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <set>
#include <utility>
#include <vector>
W_OBJECT_IMPL(Curve::Presenter)
namespace Curve
{
struct Style;

static QPointF myscale(QPointF first, QSizeF second)
{
  return {first.x() * second.width(), (1. - first.y()) * second.height()};
}

Presenter::Presenter(
    const score::DocumentContext& context, const Curve::Style& style, const Model& model,
    View* view, QObject* parent)
    : QObject{parent}
    , m_curveSegments{context.app.interfaces<SegmentList>()}
    , m_model{model}
    , m_view{view}
    , m_commandDispatcher{context.commandStack}
    , m_style{style}
    , m_editionSettings{
          context.app.guiApplicationPlugin<Curve::ApplicationPlugin>().editionSettings()}
{
  // For each segment in the model, create a segment and relevant points in the
  // view.
  // If the segment is linked to another, the point is shared.
  setupView();
  setupSignals();

  connect(m_view, &View::contextMenuRequested, this, &Presenter::contextMenuRequested);
}

Presenter::~Presenter() { }

void Presenter::setRect(const QRectF& rect)
{
  m_localRect = rect;
  // Positions
  for(auto& curve_pt : m_points)
  {
    setPos(curve_pt);
  }

  for(auto& curve_segt : m_segments)
  {
    setPos(curve_segt);
  }
}

void Presenter::setPos(PointView& point)
{
  point.setPos(myscale(point.model().pos(), m_localRect.size()));
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
  segment.setPos({startx, 0});
  segment.setRect({0., 0., endx - startx, m_localRect.height()});
}

static constexpr int direct_draw_cutoff = 3000;
void Presenter::setupSignals()
{
  con(m_model, &Model::segmentAdded, this, [&](const SegmentModel* segment) {
    if(m_model.points().size() > direct_draw_cutoff)
    {
      m_view->setDirectDraw(true);
      return;
    }

    if(auto pa = qobject_cast<const PointArraySegment*>(segment))
    {
      // FIXME
      addSegment(new SegmentView{segment, m_style, m_view});
    }
    else
    {

      addSegment(new SegmentView{segment, m_style, m_view});
    }
  });

  con(m_model, &Model::pointAdded, this, [&](const PointModel* point) {
    if(m_model.points().size() > direct_draw_cutoff)
    {
      m_view->setDirectDraw(true);
      return;
    }
    addPoint(new PointView{point, m_style, m_view});
  });

  con(m_model, &Model::pointRemoved, this,
      [&](const Id<PointModel>& m) { m_points.erase(m); });

  con(m_model, &Model::segmentRemoved, this,
      [&](const Id<SegmentModel>& m) { m_segments.erase(m); });

  con(m_model, &Model::cleared, this, [&]() {
    m_view->setDirectDraw(false);
    m_points.remove_all();
    m_segments.remove_all();
  });

  con(m_model, &Model::curveReset, this, &Presenter::modelReset);
}

void Presenter::setupView()
{
  // Initialize the elements
  m_view->setModel(this, &m_model);
  if(m_model.points().size() < 3000)
  {
    for(const auto& segment : m_model.segments())
    {
      addSegment(new SegmentView{&segment, m_style, m_view});
    }

    for(PointModel* pt : m_model.points())
    {
      addPoint(new PointView{pt, m_style, m_view});
    }
    m_view->setDirectDraw(false);
  }
  else
  {
    m_view->setDirectDraw(true);
  }
}

void Presenter::fillContextMenu(QMenu& menu, const QPoint& pos, const QPointF& scenepos)
{
  menu.addSeparator();

  auto removeAct = new QAction{tr("Remove"), this};
  score::setHelp(removeAct, tr("Remove the selection"));
  connect(removeAct, &QAction::triggered, [&]() { removeSelection(); });

  auto typeMenu = menu.addMenu(tr("Type"));
  QMap<QString, QMenu*> menus;
  for(const auto& seg : m_curveSegments)
  {
    auto text = seg.category();
    QMenu* menuToAdd{};
    if(text.isEmpty())
    {
      menuToAdd = typeMenu;
    }
    else if(text == "hidden")
    {
      continue;
    }
    else
    {
      auto it = menus.find(text);
      if(it != menus.end())
      {
        menuToAdd = it.value();
      }
      else
      {
        menuToAdd = typeMenu->addMenu(text);
        menus.insert(text, menuToAdd);
      }
    }

    auto act = menuToAdd->addAction(seg.prettyName());
    connect(act, &QAction::triggered, this, [this, key = seg.concreteKey()]() {
      updateSegmentsType(key);
    });
  }

  auto lockAction = new QAction{tr("Lock between points"), this};
  score::setHelp(lockAction, 
      tr("Prevent the moved point from moving before its previous point or "
         "after its following point."));
  connect(lockAction, &QAction::toggled, this, [&](bool b) {
    m_editionSettings.setLockBetweenPoints(b);
  });
  lockAction->setCheckable(true);
  lockAction->setChecked(m_editionSettings.lockBetweenPoints());

  auto suppressAction = new QAction{tr("Suppress on overlap"), this};
  score::setHelp(suppressAction, 
      tr("When moving past another point, remove the other point."));
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
      pt_view, &PointView::contextMenuRequested, m_view, &View::contextMenuRequested);
  con(pt_view->model(), &PointModel::posChanged, this,
      [this, pt_view]() { setPos(*pt_view); });
}

void Presenter::setupSegmentConnections(SegmentView* seg_view)
{
  connect(
      seg_view, &SegmentView::contextMenuRequested, m_view, &View::contextMenuRequested);
}

void Presenter::modelReset()
{
  const int64_t model_points_n = m_model.points().size();
  if(model_points_n > direct_draw_cutoff)
  {
    m_points.remove_all();
    m_segments.remove_all();
    m_view->setDirectDraw(true);
    return;
  }
  else
  {
    m_view->setDirectDraw(false);
  }

  // 1. We put our current elements in our pool.
  std::vector<PointView*> points = m_points.as_vec();
  std::vector<SegmentView*> segments = m_segments.as_vec();

  std::vector<PointView*> newPoints;
  std::vector<SegmentView*> newSegments;

  // 2. We add / remove new elements if necessary
  {
    const int64_t model_points_n = m_model.points().size();
    const int64_t points_n = points.size();
    int64_t diff_points = model_points_n - points_n;
    if(diff_points > 0)
    {
      points.reserve(points_n + diff_points);
      newPoints.reserve(diff_points);
      for(; diff_points-- > 0;)
      {
        auto pt = new PointView{nullptr, m_style, m_view};
        points.push_back(pt);
        newPoints.push_back(pt);
      }
    }
    else if(diff_points < 0)
    {
      if(points_n + diff_points < 0)
      {
        for(auto p : points)
          deleteGraphicsItem(p);
        points.clear();
      }
      else
      {
        int64_t inv_diff_points = -diff_points;
        for(; inv_diff_points-- > 0;)
        {
          deleteGraphicsItem(points[points_n - inv_diff_points - 1]);
        }
        points.resize(points_n + diff_points);
      }
    }
  }

  // Same for segments
  {
    const int64_t model_segts_n = m_model.segments().size();
    const int64_t segts_n = segments.size();
    int64_t diff_segts = model_segts_n - segts_n;
    if(diff_segts > 0)
    {
      segments.reserve(segts_n + diff_segts);
      newSegments.reserve(diff_segts);
      for(; diff_segts-- > 0;)
      {
        auto seg = new SegmentView{nullptr, m_style, m_view};
        segments.push_back(seg);
        newSegments.push_back(seg);
      }
    }
    else if(diff_segts < 0)
    {
      if(segts_n + diff_segts < 0)
      {
        for(auto s : segments)
          deleteGraphicsItem(s);
        segments.clear();
      }
      else
      {
        int64_t inv_diff_segts = -diff_segts;
        for(; inv_diff_segts-- > 0;)
        {
          deleteGraphicsItem(segments[segts_n - inv_diff_segts - 1]);
        }
        segments.resize(segts_n + diff_segts);
      }
    }
  }

  SCORE_ASSERT(points.size() == m_model.points().size());
  SCORE_ASSERT(segments.size() == m_model.segments().size());

  // 3. We set the data
  { // Points
    std::size_t i = 0;
    for(auto point : m_model.points())
    {
      points[i]->setModel(point);
      i++;
    }
  }
  { // Segments
    std::size_t i = 0;
    for(const auto& segment : m_model.segments())
    {
      segments[i]->setModel(&segment);
      i++;
    }
  }

  for(auto seg : newSegments)
    setupSegmentConnections(seg);
  for(auto pt : newPoints)
    setupPointConnections(pt);

  // Now the ones that have a new model
  // 4. We put them all back in our maps.
  m_points.m_map.clear();
  m_segments.m_map.clear();

  for(auto pt_view : points)
  {
    addPoint_impl(pt_view);
  }
  for(auto seg_view : segments)
  {
    addSegment_impl(seg_view);
  }
}

void Presenter::enableActions(bool b)
{
  m_view->setFocus();
}

void Presenter::enable()
{
  for(auto& segment : m_segments)
  {
    segment.enable();
  }
  for(auto& point : m_points)
  {
    point.enable();
  }

  m_enabled = true;
}

void Presenter::disable()
{
  for(auto& segment : m_segments)
  {
    segment.disable();
  }
  for(auto& point : m_points)
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
  ossia::hash_set<Id<SegmentModel>> segmentsToDelete;

  // First find the segments that will be deleted.
  // If a point is selected, the segments linked to that point
  // will be deleted, too.
  const auto& c = m_model.selectedChildren();
  for(const auto& elt : c)
  {
    if(auto point = qobject_cast<const PointModel*>(elt.data()))
    {
      if(point->previous() && point->following())
      {
        segmentsToDelete.insert(*point->previous());
        segmentsToDelete.insert(*point->following());
      }
    }

    /*
    if(auto segmt = qobject_cast<const SegmentModel*>(elt.data()))
    {
        segmentsToDelete.insert(segmt->id());
    }
    */
  }

  if(segmentsToDelete.empty())
    return;

  double x0 = 0;
  double y0 = 0;
  double x1 = 1;
  double y1 = 1;
  bool firstRemoved = false;
  bool lastRemoved = false;
  // Then remove
  auto newSegments = model().toCurveData();
  {
    // First look for the start and end segments
    {
      for(auto& seg : newSegments)
      {
        if(ossia::contains(segmentsToDelete, seg.id))
        {
          if(!seg.previous)
          {
            firstRemoved = true;
            x0 = seg.start.x();
            y0 = seg.start.y();
          }
          if(!seg.following)
          {
            lastRemoved = true;
            x1 = seg.end.x();
            y1 = seg.end.y();
          }
        }
      }
    }

    // Then set the others
    auto it = newSegments.begin();
    while(it != newSegments.end())
    {
      if(ossia::contains(segmentsToDelete, it->id))
      {
        if(it->previous)
        {
          auto prev_it = ossia::find_if(
              newSegments, [&](const SegmentData& d) { return d.id == *it->previous; });
          if(prev_it != newSegments.end())
            prev_it->following = OptionalId<SegmentModel>{};
        }
        if(it->following)
        {
          auto next_it = ossia::find_if(
              newSegments, [&](const SegmentData& d) { return d.id == *it->following; });
          if(next_it != newSegments.end())
            next_it->previous = OptionalId<SegmentModel>{};
        }
        it = newSegments.erase(it);
        continue;
      }

      if(it->previous && ossia::contains(segmentsToDelete, it->previous))
        it->previous = OptionalId<SegmentModel>{};
      if(it->following && ossia::contains(segmentsToDelete, it->following))
        it->following = OptionalId<SegmentModel>{};

      it++;
    }
  }

  // Recreate if appropriate
  if(editionSettings().removePointBehaviour()
     == RemovePointBehaviour::RemoveAndAddSegment)
  {
    // Find the "holes" in the new segment list.
    ossia::sort(newSegments, [](const SegmentData& s1, const SegmentData& s2) {
      return s1.x() < s2.x();
    });

    // First if there is no segments, we recreate one.
    if(newSegments.empty())
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
      if(firstRemoved)
      {
        // Recreate a segment from x = 0 to the beginning of the first segment.
        auto it = newSegments.begin();

        // Create a new segment
        SegmentData d;
        d.start = QPointF{x0, y0};
        d.end = it->start;
        d.following = it->id;
        d.id = getSegmentId(newSegments);
        d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        it->previous = d.id;

        newSegments.insert(it, d);
      }

      if(lastRemoved)
      {
        // Recreate a segment from x = 0 to the end of the last segment.
        auto it = newSegments.rbegin();

        // Create a new segment
        SegmentData d;
        d.end = QPointF{x1, y1};
        d.start = it->end;
        d.previous = it->id;
        d.id = getSegmentId(newSegments);
        d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        it->following = d.id;

        newSegments.insert(newSegments.end(), d);
      }
    }

    // Then try to fill the holes
    auto it = newSegments.begin();
    for(; it != newSegments.end();)
    {
      // Check if it's the last segment
      auto next = it + 1;
      if(next == newSegments.end())
        break;

      if(it->following)
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
        d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
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
  m_commandDispatcher.submit(new UpdateCurve{m_model, std::move(newSegments)});
}

void Presenter::updateSegmentsType(const UuidKey<Curve::SegmentFactory>& segment)
{
  // They keep their start / end and previous / following but change type.
  auto factory = m_curveSegments.get(segment);
  auto this_type_base_data = factory->makeCurveSegmentData();
  auto newSegments = model().toCurveData();

  for(auto& seg_data : newSegments)
  {
    if(model().segments().at(seg_data.id).selection.get())
    {
      seg_data.type = segment;
      seg_data.specificSegmentData = this_type_base_data;
    }
  }

  m_commandDispatcher.submit(new UpdateCurve{m_model, std::move(newSegments)});
}

} // namespace Curve

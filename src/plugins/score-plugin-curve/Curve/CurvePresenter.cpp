// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePresenter.hpp"

#include "CurveModel.hpp"
#include "CurveView.hpp"

#include <Curve/ApplicationPlugin.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
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
#include <Curve/Settings/CurveSettingsModel.hpp>

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
    addSegment(new SegmentView{segment, m_style, m_view});
  });

  con(m_model, &Model::segmentRemoved, this,
      [&](const Id<SegmentModel>& m) { m_segments.erase(m); });

  con(m_model, &Model::cleared, this, [&]() {
    m_view->setDirectDraw(false);
    m_points.remove_all();
    m_segments.remove_all();
  });

  con(m_model, &Model::curveReset, this, &Presenter::modelReset);

  // Without views, the curve is drawn from the model.
  con(m_model, &Model::changed, this, [this] {
    if(m_view->directDraw())
      m_view->update();
  });
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

  const auto sampled = Metadata<ConcreteKey_k, PointArraySegment>::get();
  if(ossia::any_of(m_model.segments(), [&](const SegmentModel& s) {
       return s.concreteKey() == sampled;
     }))
  {
    auto convertAct = new QAction{tr("Convert samples to editable points"), this};
    score::setHelp(
        convertAct, tr("Replace sampled data, such as an imported file, by points "
                       "that follow it and can be edited one by one."));
    connect(convertAct, &QAction::triggered, this, [this] { convertSamplesToPoints(); });
    menu.addAction(convertAct);
  }

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
  con(pt_view->model(), &PointModel::posChanged, pt_view,
      [this, pt_view]() { setPos(*pt_view); });
}

void Presenter::setupSegmentConnections(SegmentView* seg_view)
{
  connect(
      seg_view, &SegmentView::contextMenuRequested, m_view, &View::contextMenuRequested);
  if(auto m = seg_view->modelPtr())
    con(*m, &SegmentModel::dataChanged, seg_view, [this, seg_view] { setPos(*seg_view); });
}

void Presenter::modelReset()
{
  const int64_t model_points_n = m_model.points().size();
  if(model_points_n > direct_draw_cutoff)
  {
    m_points.remove_all();
    m_segments.remove_all();
    m_view->setDirectDraw(true);
    m_view->update();
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
      auto view = points[i];
      if(auto old = view->modelPtr(); old != point)
      {
        if(old)
          QObject::disconnect(old, &PointModel::posChanged, view, nullptr);
        view->setModel(point);
        con(*point, &PointModel::posChanged, view, [this, view] { setPos(*view); });
      }
      i++;
    }
  }
  { // Segments
    std::size_t i = 0;
    for(const auto& segment : m_model.segments())
    {
      auto view = segments[i];
      if(auto old = view->modelPtr(); old != &segment)
      {
        if(old)
          QObject::disconnect(old, &SegmentModel::dataChanged, view, nullptr);
        view->setModel(&segment);
        con(segment, &SegmentModel::dataChanged, view, [this, view] { setPos(*view); });
      }
      i++;
    }
  }

  for(auto seg : newSegments)
    connect(seg, &SegmentView::contextMenuRequested, m_view, &View::contextMenuRequested);
  for(auto pt : newPoints)
    connect(pt, &PointView::contextMenuRequested, m_view, &View::contextMenuRequested);

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

void Presenter::removeSelection()
{
  // A selected point takes its two segments with it; the first and last points
  // of a chain are not removed.
  ossia::hash_set<int32_t> segmentsToDelete;
  for(const PointModel* point : m_model.points())
  {
    if(point->selection.get() && point->previous() && point->following())
    {
      segmentsToDelete.insert(point->previous()->val());
      segmentsToDelete.insert(point->following()->val());
    }
  }

  if(segmentsToDelete.empty())
    return;

  auto newSegments = removeSegments(
      m_model, segmentsToDelete,
      editionSettings().removePointBehaviour()
          == RemovePointBehaviour::RemoveAndAddSegment);
  m_commandDispatcher.submit(new UpdateCurve{m_model, newSegments});
}

void Presenter::convertSamplesToPoints()
{
  const auto sampled = Metadata<ConcreteKey_k, PointArraySegment>::get();
  const bool selectedOnly = ossia::any_of(m_model.segments(), [&](const SegmentModel& s) {
    return s.concreteKey() == sampled && s.selection.get();
  });

  auto segs = m_model.toCurveData();
  SegmentIdAllocator ids{segs};
  const auto& set = score::AppContext().settings<Settings::Model>();
  const double tolerance = 1. / std::max(set.getSimplificationRatio(), 100);

  // Converted segment -> the first and last of the segments replacing it.
  ossia::hash_map<int32_t, std::pair<Id<SegmentModel>, Id<SegmentModel>>> replaced;
  std::vector<SegmentData> out;
  out.reserve(segs.size());
  for(auto& s : segs)
  {
    const bool convert
        = s.type == sampled
          && (!selectedOnly || m_model.segments().at(s.id).selection.get());
    auto chain = convert ? editableSegments(s, tolerance, ids) : std::vector<SegmentData>{};
    if(chain.empty())
    {
      out.push_back(std::move(s));
      continue;
    }
    replaced.emplace(s.id.val(), std::pair{chain.front().id, chain.back().id});
    std::move(chain.begin(), chain.end(), std::back_inserter(out));
  }
  if(replaced.empty())
    return;

  for(auto& s : out)
  {
    if(s.previous)
      if(auto it = replaced.find(s.previous->val()); it != replaced.end())
        s.previous = it->second.second;
    if(s.following)
      if(auto it = replaced.find(s.following->val()); it != replaced.end())
        s.following = it->second.first;
  }

  m_commandDispatcher.submit(new UpdateCurve{m_model, out});
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

  m_commandDispatcher.submit(new UpdateCurve{m_model, newSegments});
}

} // namespace Curve

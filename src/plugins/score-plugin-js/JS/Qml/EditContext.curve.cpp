#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <JS/Qml/EditContext.hpp>
#if SCORE_PLUGIN_MEDIA
#include <Media/Step/Commands.hpp>
#include <Media/Step/Model.hpp>
#endif
#if SCORE_PLUGIN_SPLINE
#include <Spline/Commands.hpp>
#include <Spline/Model.hpp>
#endif
#if SCORE_PLUGIN_SPLINE3D
#include <Spline3D/Commands.hpp>
#include <Spline3D/Model.hpp>
#endif
#include <cmath>

namespace JS
{

template <std::size_t N>
static bool finitePoints(const QVector<QVariantList>& points)
{
  for(auto& pt : points)
  {
    if(pt.size() < qsizetype(N))
      return false;
    for(std::size_t i = 0; i < N; i++)
    {
      bool ok{};
      const double v = pt[i].toDouble(&ok);
      if(!ok || !std::isfinite(v))
        return false;
    }
  }
  return true;
}

void EditJsContext::setCurvePoints(QObject* process, QVector<QVariantList> points)
{
  if(points.size() < 2)
    return;

  auto doc = ctx();
  if(!doc)
    return;

  auto proc = qobject_cast<Process::ProcessModel*>(process);
  if(!proc)
    return;

#if SCORE_PLUGIN_SPLINE3D
  if(auto spline = qobject_cast<Spline3D::ProcessModel*>(proc))
  {
    if(!finitePoints<3>(points))
      return;
    ossia::spline3d_data data;
    data.points.reserve(points.size());
    for(auto& pt : points)
      data.points.push_back({pt[0].toDouble(), pt[1].toDouble(), pt[2].toDouble()});
    auto [m, _] = macro(*doc);
    submit(*m, new Spline3D::ChangeSpline{*spline, data});
    return;
  }
#endif
#if SCORE_PLUGIN_SPLINE
  if(auto spline = qobject_cast<Spline::ProcessModel*>(proc))
  {
    if(!finitePoints<2>(points))
      return;
    ossia::spline_data data;
    data.points.reserve(points.size());
    for(auto& pt : points)
      data.points.push_back({pt[0].toDouble(), pt[1].toDouble()});
    auto [m, _] = macro(*doc);
    submit(*m, new Spline::ChangeSpline{*spline, data});
    return;
  }
#endif

  auto curve = proc->findChild<Curve::Model*>();
  if(!curve)
    return;

  // A NaN or infinite coordinate is not merely a bad curve: the segments are
  // ordered by x downstream, and a non-finite key makes that comparison
  // non-transitive, which is undefined behaviour in std::sort rather than a
  // wrong result. Refuse the whole call, as the shape checks above do.
  for(auto& pt : points)
  {
    if(pt.size() < 2)
      return;
    if(!std::isfinite(pt[0].toDouble()) || !std::isfinite(pt[1].toDouble()))
      return;
  }

  int current_id = 0;
  std::vector<Curve::SegmentData> segt;

  double cur_x = points[0][0].toDouble();
  double cur_y = points[0][1].toDouble();

  for(int i = 1, N = std::ssize(points); i < N; i++)
  {
    const auto& pt = points[i];
    auto x = pt[0].toDouble();
    auto y = pt[1].toDouble();
    Curve::SegmentData dat;
    dat.id = Id<Curve::SegmentModel>{current_id};
    dat.start.rx() = cur_x;
    dat.start.ry() = cur_y;
    dat.end.rx() = x;
    dat.end.ry() = y;
    cur_x = x;
    cur_y = y;
    dat.previous = Id<Curve::SegmentModel>{current_id - 1};
    dat.following = Id<Curve::SegmentModel>{current_id + 1};
    dat.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    dat.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});

    segt.push_back(dat);
    current_id++;
  }
  segt.front().previous = std::nullopt;
  segt.back().following = std::nullopt;

  auto [m, _] = macro(*doc);
  submit(*m, new Curve::UpdateCurve{*curve, std::move(segt)});
}

void EditJsContext::setSteps(QObject* process, QVector<double> points)
{
#if SCORE_PLUGIN_MEDIA
  if(points.empty())
    return;

  auto doc = ctx();
  if(!doc)
    return;

  auto proc = qobject_cast<Media::Step::Model*>(process);
  if(!proc)
    return;

  auto [m, _] = macro(*doc);
  submit(
      *m,
      new Media::ChangeSteps{*proc, ossia::float_vector{points.begin(), points.end()}});
#endif
}

}

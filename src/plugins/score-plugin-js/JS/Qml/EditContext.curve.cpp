#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <JS/Qml/EditContext.hpp>
#include <Media/Step/Commands.hpp>
#include <Media/Step/Model.hpp>

namespace JS
{

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

  auto curve = proc->findChild<Curve::Model*>();
  if(!curve)
    return;

  for(auto& pt : points)
  {
    if(pt.size() < 2)
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
}

}

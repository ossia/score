#include <Media/Tempo.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Curve/CurveModel.hpp>
#include <ossia/detail/flicks.hpp>
namespace Media
{

static double getRootTempo(const QObject& obj) noexcept {
  auto& ctx = score::IDocument::documentContext(obj);
  auto& root_itv = ctx.model<Scenario::ScenarioDocumentModel>().baseInterval();
  double speed = root_itv.duration.speed();
  return speed * ossia::root_tempo;
}

static constexpr double tempoCurveToTempo(double t) noexcept
{
  return Scenario::TempoProcess::min + t * (Scenario::TempoProcess::max - Scenario::TempoProcess::min);
}

double tempoAtStartDate(Process::ProcessModel& m) noexcept
{
  double tempo = [&] {
    auto parent = m.parent();
    while(parent)
    {
      if(auto itv = qobject_cast<Scenario::IntervalModel*>(parent))
      {
        auto [tempo_itv, delta] = Scenario::closestParentWithTempo(itv);
        if(tempo_itv)
        {
          using namespace ossia;
          Curve::Model& tempo_curve = tempo_itv->tempoCurve()->curve();
          if(const auto& dur = tempo_itv->tempoCurve()->duration(); dur > 0_tv)
            if(auto d = tempo_curve.valueAt(double(delta.impl) / dur.impl))
              return tempoCurveToTempo(*d);
            else
              return getRootTempo(*tempo_itv);
          else
            if(auto d = tempo_curve.valueAt(0.))
              return tempoCurveToTempo(*d);
            else
              return getRootTempo(*tempo_itv);
        }
        else
        {
          // return current root tempo
          return getRootTempo(m);
        }
      }
      else
      {
        parent = parent->parent();
      }
    }

    return getRootTempo(m);
  }();

  if(tempo > 0.1)
    return tempo;
  else
    return ossia::root_tempo;
}

}

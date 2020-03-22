#include "PatternPresenter.hpp"
#include "PatternView.hpp"
#include <Patternist/Commands/PatternProperties.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
namespace Patternist
{

Presenter::Presenter(
    const Patternist::ProcessModel& layer,
    View* view,
    const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
{
  putToFront();

  connect(m_view, &View::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(m_view, &View::toggled, this, [&](int lane, int index) {
    auto cur = layer.patterns()[layer.currentPattern()];
    bool b = cur.lanes[lane].pattern[index];

    cur.lanes[lane].pattern[index] = !b;

    CommandDispatcher<> disp{m_context.context.commandStack};
    disp.submit<UpdatePattern>(layer, layer.currentPattern(), cur);
  });
  connect(m_view, &View::noteChanged, this, [&](int lane, int note) {
    auto cur = layer.patterns()[layer.currentPattern()];
    cur.lanes[lane].note = note;

    auto& disp = m_context.context.dispatcher;
    disp.submit<UpdatePattern>(layer, layer.currentPattern(), cur);
  });
  connect(m_view, &View::noteChangeFinished, this, [&]{
    auto& disp = m_context.context.dispatcher;
    disp.commit();
  });
}

Presenter::~Presenter()
{
}
void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio zr)
{
}

void Presenter::parentGeometryChanged() {}

}

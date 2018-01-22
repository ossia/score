#pragma once
#include <Process/LayerPresenter.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Device/Address/AddressSettings.hpp>

namespace Spline
{
class ChangeSpline final : public score::Command
{
    SCORE_COMMAND_DECL(Automation::CommandFactoryName(), ChangeSpline, "ChangeSpline")
    public:
      ChangeSpline(
        const ProcessModel& autom, const ossia::spline_data& newval)
    : m_path{autom}
    , m_old{autom.spline()}
    , m_new{newval}
{

}

public:
void undo(const score::DocumentContext& ctx) const override
{
  m_path.find(ctx).setSpline(m_old);
}
void redo(const score::DocumentContext& ctx) const override
{
  m_path.find(ctx).setSpline(m_new);
}

protected:
void serializeImpl(DataStreamInput& s) const override
{
  s << m_path << m_old << m_new;
}
void deserializeImpl(DataStreamOutput& s) override
{
  s >> m_path >> m_old >> m_new;
}

private:
Path<ProcessModel> m_path;
ossia::spline_data m_old, m_new;
};

class View;
class Presenter final : public Process::LayerPresenter
{
  public:
    explicit Presenter(
        const Spline::ProcessModel& model,
        Spline::View* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent);

    void setWidth(qreal width) override;
    void setHeight(qreal height) override;

    void putToFront() override;
    void putBehind() override;

    void on_zoomRatioChanged(ZoomRatio) override;

    void parentGeometryChanged() override;

    const Spline::ProcessModel& model() const override;
    const Id<Process::ProcessModel>& modelId() const override;

  private:
    const Spline::ProcessModel& m_layer;
    View* m_view{};
    ZoomRatio m_zoomRatio{};
};

}

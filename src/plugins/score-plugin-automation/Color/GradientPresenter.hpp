#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Process/LayerPresenter.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/MapSerialization.hpp>

#include <Color/GradientModel.hpp>



namespace Gradient
{

class ChangeGradient final : public score::Command
{
  SCORE_COMMAND_DECL(
      Automation::CommandFactoryName(),
      ChangeGradient,
      "ChangeGradient")
public:
  ChangeGradient(
      const ProcessModel& autom,
      const ProcessModel::gradient_colors& newval)
      : m_path{autom}, m_old{autom.gradient()}, m_new{newval}
  {
  }

public:
  void undo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setGradient(m_old);
  }
  void redo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setGradient(m_new);
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
  ProcessModel::gradient_colors m_old, m_new;
};

class View;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Gradient::ProcessModel& model,
      Gradient::View* view,
      const Process::Context& ctx,
      QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

private:
  View* m_view{};
  ZoomRatio m_zoomRatio{};
};
}

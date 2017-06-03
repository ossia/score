#pragma once
#include <Process/LayerPresenter.hpp>
#include <Automation/Color/GradientAutomModel.hpp>
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <Device/Address/AddressSettings.hpp>

template <typename T, typename U>
struct TSerializer<DataStream, boost::container::flat_map<T, U>>
{
    using type = boost::container::flat_map<T, U>;
    static void readFrom(DataStream::Serializer& s, const type& obj)
    {
      s.stream() << (int32_t)obj.size();
      for(const auto& e : obj)
        s.stream() << e.first << e.second;
    }

    static void writeTo(DataStream::Deserializer& s, type& obj)
    {
      int32_t n;
      s.stream() >> n;
      for(; n --> 0;)
      {
        T k;
        U v;
        s.stream() >> k >> v;
        obj.insert(std::make_pair(std::move(k), std::move(v)));
      }
    }
};


namespace Gradient
{

class ChangeGradient final : public iscore::Command
{
    ISCORE_COMMAND_DECL(Automation::CommandFactoryName(), ChangeGradient, "ChangeGradient")
    public:
      ChangeGradient(
        const ProcessModel& autom, const ProcessModel::gradient_colors& newval)
    : m_path{autom}
    , m_old{autom.gradient()}
    , m_new{newval}
{

}

public:
void undo() const override
{
  m_path.find().setGradient(m_old);
}
void redo() const override
{
  m_path.find().setGradient(m_new);
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
class Presenter : public Process::LayerPresenter
{
  public:
    explicit Presenter(
        const Gradient::ProcessModel& model,
        Gradient::View* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent);

    void setWidth(qreal width) override;
    void setHeight(qreal height) override;

    void putToFront() override;
    void putBehind() override;

    void on_zoomRatioChanged(ZoomRatio) override;

    void parentGeometryChanged() override;

    const Gradient::ProcessModel& model() const override;
    const Id<Process::ProcessModel>& modelId() const override;

  private:
    const Gradient::ProcessModel& m_layer;
    View* m_view{};
};

}

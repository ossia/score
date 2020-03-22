#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Process/LayerPresenter.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <Color/GradientModel.hpp>

template <>
struct is_custom_serialized<QColor> : public std::true_type
{
};

template <typename T, typename U>
struct TSerializer<DataStream, ossia::flat_map<T, U>>
{
  using type = ossia::flat_map<T, U>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.stream() << (int32_t)obj.size();
    for (const auto& e : obj)
      s.stream() << e.first << e.second;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    int32_t n;
    s.stream() >> n;
    for (; n-- > 0;)
    {
      T k;
      U v;
      s.stream() >> k >> v;
      obj.insert(std::make_pair(std::move(k), std::move(v)));
    }
  }
};

template <>
struct TSerializer<JSONValue, QColor>
{
  static void readFrom(JSONValue::Serializer& s, QColor c)
  {
    if (c.spec() != QColor::Rgb)
      c = c.toRgb();
    s.val = QJsonArray{c.redF(), c.greenF(), c.blueF(), c.alphaF()};
  }

  static void writeTo(JSONValue::Deserializer& s, QColor& c)
  {
    auto arr = s.val.toArray();
    c.setRgbF(
        arr[0].toDouble(),
        arr[1].toDouble(),
        arr[2].toDouble(),
        arr[3].toDouble());
  }
};

template <typename U>
struct TSerializer<JSONValue, ossia::flat_map<double, U>>
{
  using type = ossia::flat_map<double, U>;
  static void readFrom(JSONValue::Serializer& s, const type& obj)
  {
    QJsonArray arr;
    for (const auto& e : obj)
    {
      arr.append(e.first);
      arr.append(toJsonValue(e.second));
    }
    s.val = arr;
  }

  static void writeTo(JSONValue::Deserializer& s, type& obj)
  {
    auto arr = s.val.toArray();
    for (int i = 0; i < arr.size(); i += 2)
    {
      double k = arr[i].toDouble();
      U v = fromJsonValue<U>(arr[i + 1]);
      obj.insert(std::make_pair(std::move(k), std::move(v)));
    }
  }
};

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

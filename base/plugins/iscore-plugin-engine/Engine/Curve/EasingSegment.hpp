#pragma once
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore_plugin_engine_export.h>
namespace Engine
{
namespace EasingCurve
{
struct EasingData
{
};

template <typename T>
class EasingSegment;
}
}
template <typename T>
struct TSerializer<DataStream, Engine::EasingCurve::EasingSegment<T>>
{
  static void readFrom(
      DataStream::Serializer& s,
      const Engine::EasingCurve::EasingSegment<T>& obj);
  static void writeTo(
      DataStream::Deserializer& s, Engine::EasingCurve::EasingSegment<T>& obj);
};

template <typename T>
struct TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>
{
  static void readFrom(
      JSONObject::Serializer& s,
      const Engine::EasingCurve::EasingSegment<T>& obj);
  static void writeTo(
      JSONObject::Deserializer& s, Engine::EasingCurve::EasingSegment<T>& obj);
};

namespace Engine
{
namespace EasingCurve
{
template <typename Easing_T>
class EasingSegment final : public ::Curve::SegmentModel
{
public:
  key_type concreteKey() const final override
  {
    return Metadata<ConcreteKey_k, EasingSegment>::get();
  }

  EasingSegment*
  clone(const id_type& newId, QObject* newParent) const final override
  {
    return new EasingSegment{*this, newId, newParent};
  }

  void serialize_impl(const VisitorVariant& vis) const override
  {
  }

  using data_type = EasingData;
  template <typename... Args>
  EasingSegment(Args&&... args)
      : Curve::SegmentModel{std::forward<Args>(args)...}
  {
  }

  EasingSegment(
      const EasingSegment& other,
      const Curve::SegmentModel::id_type& id,
      QObject* parent)
      : Curve::SegmentModel{other.start(), other.end(), id, parent}
  {
  }

  template <typename Impl, typename = ossia::void_t<decltype(Impl::writeTo)>>
  EasingSegment(Impl& vis, QObject* parent)
      : Curve::SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void on_startChanged() override
  {
    emit dataChanged();
  }
  void on_endChanged() override
  {
    emit dataChanged();
  }

  void updateData(int numInterp) const override
  {
    if (std::size_t(2 * numInterp + 1) != m_data.size())
      m_valid = false;
    if (!m_valid)
    {
      numInterp *= 2;
      m_data.resize(numInterp + 1);

      double start_x = start().x();
      double end_x = end().x();

      double start_y = start().y();
      double end_y = end().y();
      for (int j = 0; j <= numInterp; j++)
      {
        QPointF& pt = m_data[j];
        pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
        pt.setY(
            start_y + (end_y - start_y) * Easing_T{}(double(j) / numInterp));
      }
    }
  }

  double valueAt(double x) const override
  {
    return start().y()
           + (end().y() - start().y()) * (Easing_T{}(x)-start().x())
                 / (end().x() - start().x());
  }

  optional<double> verticalParameter() const override
  {
    return {};
  }
  optional<double> horizontalParameter() const override
  {
    return {};
  }
  void setVerticalParameter(double p) override
  {
  }
  void setHorizontalParameter(double p) override
  {
  }

  QVariant toSegmentSpecificData() const override
  {
    return QVariant::fromValue(EasingData{});
  }

  template <typename Y>
  std::function<Y(double, Y, Y)> makeFunction() const
  {
    return ossia::curve_segment_ease<Y, Easing_T>{};
  }
  std::function<float(double, float, float)> makeFloatFunction() const override
  {
    return makeFunction<float>();
  }
  std::function<int(double, int, int)> makeIntFunction() const override
  {
    return makeFunction<int>();
  }
  std::function<bool(double, bool, bool)> makeBoolFunction() const override
  {
    return makeFunction<bool>();
  }
};
using Segment_backIn = EasingSegment<ossia::easing::backIn>;
using Segment_backOut = EasingSegment<ossia::easing::backOut>;
using Segment_backInOut = EasingSegment<ossia::easing::backInOut>;
using Segment_bounceIn = EasingSegment<ossia::easing::bounceIn>;
using Segment_bounceOut = EasingSegment<ossia::easing::bounceOut>;
using Segment_bounceInOut = EasingSegment<ossia::easing::bounceInOut>;
using Segment_quadraticIn = EasingSegment<ossia::easing::quadraticIn>;
using Segment_quadraticOut = EasingSegment<ossia::easing::quadraticOut>;
using Segment_quadraticInOut = EasingSegment<ossia::easing::quadraticInOut>;
using Segment_cubicIn = EasingSegment<ossia::easing::cubicIn>;
using Segment_cubicOut = EasingSegment<ossia::easing::cubicOut>;
using Segment_cubicInOut = EasingSegment<ossia::easing::cubicInOut>;
using Segment_quarticIn = EasingSegment<ossia::easing::quarticIn>;
using Segment_quarticOut = EasingSegment<ossia::easing::quarticOut>;
using Segment_quarticInOut = EasingSegment<ossia::easing::quarticInOut>;
using Segment_quinticIn = EasingSegment<ossia::easing::quinticIn>;
using Segment_quinticOut = EasingSegment<ossia::easing::quinticOut>;
using Segment_quinticInOut = EasingSegment<ossia::easing::quinticInOut>;
using Segment_sineIn = EasingSegment<ossia::easing::sineIn>;
using Segment_sineOut = EasingSegment<ossia::easing::sineOut>;
using Segment_sineInOut = EasingSegment<ossia::easing::sineInOut>;
using Segment_circularIn = EasingSegment<ossia::easing::circularIn>;
using Segment_circularOut = EasingSegment<ossia::easing::circularOut>;
using Segment_circularInOut = EasingSegment<ossia::easing::circularInOut>;
using Segment_exponentialIn = EasingSegment<ossia::easing::exponentialIn>;
using Segment_exponentialOut = EasingSegment<ossia::easing::exponentialOut>;
using Segment_exponentialInOut
    = EasingSegment<ossia::easing::exponentialInOut>;
using Segment_elasticIn = EasingSegment<ossia::easing::elasticIn>;
using Segment_elasticOut = EasingSegment<ossia::easing::elasticOut>;
using Segment_elasticInOut = EasingSegment<ossia::easing::elasticInOut>;
using Segment_perlinInOut = EasingSegment<ossia::easing::perlinInOut>;
}
}

template <>
inline void DataStreamReader::read(
    const Engine::EasingCurve::EasingData& segmt)
{
}

template <>
inline void
DataStreamWriter::writeTo(Engine::EasingCurve::EasingData& segmt)
{
}

template <>
inline void JSONObjectReader::readFromConcrete(
    const Engine::EasingCurve::EasingData& segmt)
{
}

template <>
inline void
JSONObjectWriter::writeTo(Engine::EasingCurve::EasingData& segmt)
{
}

template <typename T>
void TSerializer<DataStream, Engine::EasingCurve::EasingSegment<T>>::
    readFrom(
        DataStream::Serializer& s,
        const Engine::EasingCurve::EasingSegment<T>& obj)
{
}
template <typename T>
void TSerializer<DataStream, Engine::EasingCurve::EasingSegment<T>>::
    writeTo(
        DataStream::Deserializer& s,
        Engine::EasingCurve::EasingSegment<T>& obj)
{
}

template <typename T>
void TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>::readFrom(
    JSONObject::Serializer& s,
    const Engine::EasingCurve::EasingSegment<T>& obj)
{
}
template <typename T>
void TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>::writeTo(
    JSONObject::Deserializer& s, Engine::EasingCurve::EasingSegment<T>& obj)
{
}

Q_DECLARE_METATYPE(Engine::EasingCurve::EasingData)

// cat easings | xargs -L1 bash -c 'echo $(uuidgen)' | paste - easings | sed
// 's/\t/ /' > easings2
// cat easings2 | awk '{ print
// "CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT,
// Engine::EasingCurve::Segment_" $2 ">, \"" $1 "\", \"" $2 "\", \"" $2 "\")";}
// '

CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backIn,
    "fb5cb6c1-47fd-497c-9d69-7a87adbaf3b3",
    "backIn",
    "backIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backOut,
    "0edbd8f5-67c2-41f2-ae80-f014e5c24aa6",
    "backOut",
    "backOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backInOut,
    "3825c351-698d-4930-9862-28c5f7f51c61",
    "backInOut",
    "backInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceIn,
    "51fafa98-aa8e-48f0-adae-c21c3eeb63ca",
    "bounceIn",
    "bounceIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceOut,
    "75ce6961-22b3-4a9e-b989-3131098bd092",
    "bounceOut",
    "bounceOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceInOut,
    "30d5c3dc-5a8c-44a8-95b4-67ca3ff5088b",
    "bounceInOut",
    "bounceInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticIn,
    "a2f38b24-f2c9-42d7-bb5e-a51a821d2ffd",
    "quadraticIn",
    "quadraticIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticOut,
    "9717560b-fae1-4035-afbf-031f3581d132",
    "quadraticOut",
    "quadraticOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticInOut,
    "fc333d55-064a-4af8-b4a8-23fe16e80ecc",
    "quadraticInOut",
    "quadraticInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicIn,
    "b6bddf8b-b2cb-46d6-8d90-31b3029317e8",
    "cubicIn",
    "cubicIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicOut,
    "ff9a7726-2d8f-43f2-a95c-e395fdbd5aa9",
    "cubicOut",
    "cubicOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicInOut,
    "21556d2a-ac8a-4acf-bf42-5eca9795047c",
    "cubicInOut",
    "cubicInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticIn,
    "72678775-535f-438e-b4bb-1077af3fab99",
    "quarticIn",
    "quarticIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticOut,
    "bd6f4867-2c30-4267-aa97-0ed767883d22",
    "quarticOut",
    "quarticOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticInOut,
    "93cac01a-dedc-4cb4-ad2d-d81b31e6187c",
    "quarticInOut",
    "quarticInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticIn,
    "cd08eabe-51a4-429e-8740-c097c1b34b83",
    "quinticIn",
    "quinticIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticOut,
    "cb6e340b-faee-440c-8ef5-05ec6f7f4791",
    "quinticOut",
    "quinticOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticInOut,
    "b2cce6a7-c651-429f-ae46-d322991e92d4",
    "quinticInOut",
    "quinticInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineIn,
    "e8a44b49-4d91-4066-8e25-7669d8927792",
    "sineIn",
    "sineIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineOut,
    "e5d2dea4-061e-4139-8da2-1d21b0414273",
    "sineOut",
    "sineOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineInOut,
    "61bd1bb4-353b-435f-8b4c-ed61ed43bcf9",
    "sineInOut",
    "sineInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularIn,
    "45b06858-2e16-4cdc-82cc-70f8545bab03",
    "circularIn",
    "circularIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularOut,
    "32a67b2a-61f5-49cd-b1c4-37419350fca8",
    "circularOut",
    "circularOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularInOut,
    "b70a58f2-2fff-4fda-b1fa-cad6c7c11b88",
    "circularInOut",
    "circularInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialIn,
    "8db88491-bed5-4a0d-bcb6-45811c5a7722",
    "exponentialIn",
    "exponentialIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialOut,
    "40f17c1a-9611-4468-b114-c6338fb0fbb7",
    "exponentialOut",
    "exponentialOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialInOut,
    "46224c2f-f60d-4f60-93b8-a0ebe6931d00",
    "exponentialInOut",
    "exponentialInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticIn,
    "6e301164-e079-466b-a518-12fe89048283",
    "elasticIn",
    "elasticIn")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticOut,
    "1f1fddd4-7a23-4c15-a3ec-6e88e9787a97",
    "elasticOut",
    "elasticOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticInOut,
    "8bad1486-b616-4ebe-aa5e-844162545f8b",
    "elasticInOut",
    "elasticInOut")
CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_perlinInOut,
    "66cf32a5-86c5-4747-89d4-523e26dcc1fc",
    "perlinInOut",
    "perlinInOut")

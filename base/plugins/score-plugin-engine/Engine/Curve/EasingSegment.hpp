#pragma once
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <score_plugin_engine_export.h>

namespace Engine
{
namespace EasingCurve
{
struct EasingData
{
};

template <typename Easing_T>
class EasingSegment final : public ::Curve::SegmentModel
{
public:
  key_type concreteKey() const final override
  {
    return Metadata<ConcreteKey_k, EasingSegment>::get();
  }

  void serialize_impl(const VisitorVariant& vis) const override
  {
  }

  using data_type = EasingData;
  EasingSegment(const Id<SegmentModel>& id, QObject* parent):
    Curve::SegmentModel{id, parent}
  {
  }
  EasingSegment(const Curve::SegmentData& dat, QObject* parent):
    Curve::SegmentModel{dat, parent}
  {
  }

  EasingSegment(
      const EasingSegment& other,
      const Curve::SegmentModel::id_type& id,
      QObject* parent)
      : Curve::SegmentModel{other.start(), other.end(), id, parent}
  {
  }

  EasingSegment(DataStream::Deserializer& vis, QObject* parent)
    : Curve::SegmentModel{vis, parent}
  {
  }
  EasingSegment(JSONObject::Deserializer& vis, QObject* parent)
    : Curve::SegmentModel{vis, parent}
  {
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
    if (std::size_t(numInterp + 1) != m_data.size())
      m_valid = false;
    if (!m_valid)
    {
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
  ossia::curve_segment<Y> makeFunction() const
  {
    return ossia::curve_segment_ease<Y, Easing_T>{};
  }
  ossia::curve_segment<float> makeFloatFunction() const override
  {
    return makeFunction<float>();
  }
  ossia::curve_segment<int> makeIntFunction() const override
  {
    return makeFunction<int>();
  }
  ossia::curve_segment<bool> makeBoolFunction() const override
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
DataStreamWriter::write(Engine::EasingCurve::EasingData& segmt)
{
}

template <>
inline void JSONObjectReader::read(
    const Engine::EasingCurve::EasingData& segmt)
{
}

template <>
inline void
JSONObjectWriter::write(Engine::EasingCurve::EasingData& segmt)
{
}

Q_DECLARE_METATYPE(Engine::EasingCurve::EasingData)

// cat easings | xargs -L1 bash -c 'echo $(uuidgen)' | paste - easings | sed
// 's/\t/ /' > easings2
// cat easings2 | awk '{ print
// "CURVE_SEGMENT_METADATA(SCORE_PLUGIN_ENGINE_EXPORT,
// Engine::EasingCurve::Segment_" $2 ">, \"" $1 "\", \"" $2 "\", \"" $2 "\")";}
// '

CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backIn,
    "fb5cb6c1-47fd-497c-9d69-7a87adbaf3b3",
    "backIn",
    "backIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backOut,
    "0edbd8f5-67c2-41f2-ae80-f014e5c24aa6",
    "backOut",
    "backOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_backInOut,
    "3825c351-698d-4930-9862-28c5f7f51c61",
    "backInOut",
    "backInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceIn,
    "51fafa98-aa8e-48f0-adae-c21c3eeb63ca",
    "bounceIn",
    "bounceIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceOut,
    "75ce6961-22b3-4a9e-b989-3131098bd092",
    "bounceOut",
    "bounceOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_bounceInOut,
    "30d5c3dc-5a8c-44a8-95b4-67ca3ff5088b",
    "bounceInOut",
    "bounceInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticIn,
    "a2f38b24-f2c9-42d7-bb5e-a51a821d2ffd",
    "quadraticIn",
    "quadraticIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticOut,
    "9717560b-fae1-4035-afbf-031f3581d132",
    "quadraticOut",
    "quadraticOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quadraticInOut,
    "fc333d55-064a-4af8-b4a8-23fe16e80ecc",
    "quadraticInOut",
    "quadraticInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicIn,
    "b6bddf8b-b2cb-46d6-8d90-31b3029317e8",
    "cubicIn",
    "cubicIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicOut,
    "ff9a7726-2d8f-43f2-a95c-e395fdbd5aa9",
    "cubicOut",
    "cubicOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_cubicInOut,
    "21556d2a-ac8a-4acf-bf42-5eca9795047c",
    "cubicInOut",
    "cubicInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticIn,
    "72678775-535f-438e-b4bb-1077af3fab99",
    "quarticIn",
    "quarticIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticOut,
    "bd6f4867-2c30-4267-aa97-0ed767883d22",
    "quarticOut",
    "quarticOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quarticInOut,
    "93cac01a-dedc-4cb4-ad2d-d81b31e6187c",
    "quarticInOut",
    "quarticInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticIn,
    "cd08eabe-51a4-429e-8740-c097c1b34b83",
    "quinticIn",
    "quinticIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticOut,
    "cb6e340b-faee-440c-8ef5-05ec6f7f4791",
    "quinticOut",
    "quinticOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_quinticInOut,
    "b2cce6a7-c651-429f-ae46-d322991e92d4",
    "quinticInOut",
    "quinticInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineIn,
    "e8a44b49-4d91-4066-8e25-7669d8927792",
    "sineIn",
    "sineIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineOut,
    "e5d2dea4-061e-4139-8da2-1d21b0414273",
    "sineOut",
    "sineOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_sineInOut,
    "61bd1bb4-353b-435f-8b4c-ed61ed43bcf9",
    "sineInOut",
    "sineInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularIn,
    "45b06858-2e16-4cdc-82cc-70f8545bab03",
    "circularIn",
    "circularIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularOut,
    "32a67b2a-61f5-49cd-b1c4-37419350fca8",
    "circularOut",
    "circularOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_circularInOut,
    "b70a58f2-2fff-4fda-b1fa-cad6c7c11b88",
    "circularInOut",
    "circularInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialIn,
    "8db88491-bed5-4a0d-bcb6-45811c5a7722",
    "exponentialIn",
    "exponentialIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialOut,
    "40f17c1a-9611-4468-b114-c6338fb0fbb7",
    "exponentialOut",
    "exponentialOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_exponentialInOut,
    "46224c2f-f60d-4f60-93b8-a0ebe6931d00",
    "exponentialInOut",
    "exponentialInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticIn,
    "6e301164-e079-466b-a518-12fe89048283",
    "elasticIn",
    "elasticIn", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticOut,
    "1f1fddd4-7a23-4c15-a3ec-6e88e9787a97",
    "elasticOut",
    "elasticOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_elasticInOut,
    "8bad1486-b616-4ebe-aa5e-844162545f8b",
    "elasticInOut",
    "elasticInOut", "Easing")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_ENGINE_EXPORT,
    Engine::EasingCurve::Segment_perlinInOut,
    "66cf32a5-86c5-4747-89d4-523e26dcc1fc",
    "perlinInOut",
    "perlinInOut", "Easing")

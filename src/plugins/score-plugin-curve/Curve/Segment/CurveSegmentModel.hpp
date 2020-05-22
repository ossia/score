#pragma once
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/editor/curve/curve_segment.hpp>

#include <boost/align/aligned_allocator_adaptor.hpp>

#include <QPoint>
#include <QVariant>

#include <score_plugin_curve_export.h>

#include <functional>
#include <vector>
#include <verdigris>

class QObject;
namespace Curve
{
// Gives the data.
class SCORE_PLUGIN_CURVE_EXPORT SegmentModel : public IdentifiedObject<SegmentModel>,
                                               public score::SerializableInterface<SegmentFactory>
{
  W_OBJECT(SegmentModel)

  SCORE_SERIALIZE_FRIENDS
public:
  using data_vector = std::
      vector<QPointF, boost::alignment::aligned_allocator_adaptor<std::allocator<QPointF>, 32>>;
  Selectable selection;
  SegmentModel(const Id<SegmentModel>& id, QObject* parent);
  SegmentModel(const SegmentData& id, QObject* parent);

  // Used for cloning :
  // Previous and following shall be set afterwards by the cloner.
  SegmentModel(Curve::Point s, Curve::Point e, const Id<SegmentModel>& id, QObject* parent);

  SegmentModel(DataStream::Deserializer& vis, QObject* parent);
  SegmentModel(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~SegmentModel();

  virtual void updateData(int numInterp) const = 0; // Will interpolate.
  virtual double valueAt(double x) const = 0;

  const data_vector& data() const { return m_data; }

  void setStart(const Curve::Point& pt);
  Curve::Point start() const { return m_start; }

  void setEnd(const Curve::Point& pt);
  Curve::Point end() const { return m_end; }

  void setPrevious(const OptionalId<SegmentModel>& previous);
  const OptionalId<SegmentModel>& previous() const { return m_previous; }

  void setFollowing(const OptionalId<SegmentModel>& following);
  const OptionalId<SegmentModel>& following() const { return m_following; }

  // Between -1 and 1, to map to the real parameter.
  virtual void setVerticalParameter(double p);
  virtual void setHorizontalParameter(double p);
  virtual std::optional<double> verticalParameter() const;
  virtual std::optional<double> horizontalParameter() const;

  virtual ossia::curve_segment<double> makeDoubleFunction() const = 0;
  virtual ossia::curve_segment<float> makeFloatFunction() const = 0;
  virtual ossia::curve_segment<int> makeIntFunction() const = 0;

  SegmentData toSegmentData() const
  {
    return {id(), start(), end(), previous(), following(), concreteKey(), toSegmentSpecificData()};
  }

public:
  void dataChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, dataChanged)
  void previousChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, previousChanged)
  void followingChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, followingChanged)
  void startChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, startChanged)
  void endChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, endChanged)

protected:
  virtual void on_startChanged() = 0;
  virtual void on_endChanged() = 0;

  virtual QVariant toSegmentSpecificData() const = 0;

  mutable data_vector m_data; // A data cache.
  mutable bool m_valid{};     // Used to perform caching.
  // TODO it seems that m_valid is never true.

  Curve::Point m_start, m_end;

private:
  OptionalId<SegmentModel> m_previous, m_following;
};

class PowerSegment;
struct PowerSegmentData;

using DefaultCurveSegmentModel = PowerSegment;
using DefaultCurveSegmentData = PowerSegmentData;
}

OBJECTKEY_METADATA(SCORE_PLUGIN_CURVE_EXPORT, Curve::SegmentModel, "CurveSegmentModel")

// extern template class SCORE_PLUGIN_CURVE_EXPORT
// IdContainer<Curve::SegmentModel>;

W_REGISTER_ARGTYPE(const Curve::SegmentModel&)
W_REGISTER_ARGTYPE(Curve::SegmentModel)
W_REGISTER_ARGTYPE(Id<Curve::SegmentModel>)

#pragma once
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <QPoint>
#include <QVariant>
#include <boost/align/aligned_allocator_adaptor.hpp>
#include <functional>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/selection/Selectable.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/std/Optional.hpp>
#include <vector>

#include <score/serialization/VisitorInterface.hpp>
#include <score/model/Identifier.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/editor/curve/curve_segment.hpp>
#include <score_plugin_curve_export.h>

class QObject;
namespace Curve
{
// Gives the data.
class SCORE_PLUGIN_CURVE_EXPORT SegmentModel
    : public IdentifiedObject<SegmentModel>,
      public score::SerializableInterface<SegmentFactory>
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS
public:
  using data_vector = std::
      vector<QPointF, boost::alignment::aligned_allocator_adaptor<std::allocator<QPointF>, 32>>;
  Selectable selection;
  SegmentModel(const Id<SegmentModel>& id, QObject* parent);
  SegmentModel(const SegmentData& id, QObject* parent);

  // Used for cloning :
  // Previous and following shall be set afterwards by the cloner.
  SegmentModel(
      Curve::Point s,
      Curve::Point e,
      const Id<SegmentModel>& id,
      QObject* parent);

  SegmentModel(DataStream::Deserializer& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  SegmentModel(JSONObject::Deserializer& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  virtual ~SegmentModel();

  virtual void updateData(int numInterp) const = 0; // Will interpolate.
  virtual double valueAt(double x) const = 0;

  const data_vector& data() const
  {
    return m_data;
  }

  void setStart(const Curve::Point& pt);
  Curve::Point start() const
  {
    return m_start;
  }

  void setEnd(const Curve::Point& pt);
  Curve::Point end() const
  {
    return m_end;
  }

  void setPrevious(const OptionalId<SegmentModel>& previous);
  const OptionalId<SegmentModel>& previous() const
  {
    return m_previous;
  }

  void setFollowing(const OptionalId<SegmentModel>& following);
  const OptionalId<SegmentModel>& following() const
  {
    return m_following;
  }

  // Between -1 and 1, to map to the real parameter.
  virtual void setVerticalParameter(double p);
  virtual void setHorizontalParameter(double p);
  virtual optional<double> verticalParameter() const;
  virtual optional<double> horizontalParameter() const;

  virtual ossia::curve_segment<float>
  makeFloatFunction() const = 0;
  virtual ossia::curve_segment<int> makeIntFunction() const = 0;
  virtual ossia::curve_segment<bool> makeBoolFunction() const = 0;

  SegmentData toSegmentData() const
  {
    return {id(),
            start(),
            end(),
            previous(),
            following(),
            concreteKey(),
            toSegmentSpecificData()};
  }

Q_SIGNALS:
  void dataChanged();
  void previousChanged();
  void followingChanged();
  void startChanged();
  void endChanged();

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

OBJECTKEY_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT, Curve::SegmentModel, "CurveSegmentModel")

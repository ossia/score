#pragma once
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <QPoint>
#include <QVariant>
#include <boost/align/aligned_allocator_adaptor.hpp>
#include <functional>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <vector>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/Identifier.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore_plugin_curve_export.h>

class QObject;
namespace Curve
{
// Gives the data.
class ISCORE_PLUGIN_CURVE_EXPORT SegmentModel
    : public IdentifiedObject<SegmentModel>,
      public iscore::SerializableInterface<SegmentFactory>
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS(Curve::SegmentModel, DataStream)
  ISCORE_SERIALIZE_FRIENDS(Curve::SegmentModel, JSONObject)
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

  template <typename Impl>
  SegmentModel(Deserializer<Impl>& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  virtual SegmentModel*
  clone(const Id<SegmentModel>& id, QObject* parent) const = 0;

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

  virtual std::function<float(double, float, float)>
  makeFloatFunction() const = 0;
  virtual std::function<int(double, int, int)> makeIntFunction() const = 0;
  virtual std::function<bool(double, bool, bool)> makeBoolFunction() const = 0;

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

signals:
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
    ISCORE_PLUGIN_CURVE_EXPORT, Curve::SegmentModel, "CurveSegmentModel")

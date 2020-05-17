#pragma once

#include <score/model/Identifier.hpp>
#include <score/plugins/Interface.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>
#include <QVariant>

#include <score_plugin_curve_export.h>

class QObject;
struct VisitorVariant;

namespace Curve
{
class Category_k;
class SegmentModel;
struct SegmentData;
class SCORE_PLUGIN_CURVE_EXPORT SegmentFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(SegmentFactory, "608ecec9-d8bc-4b6b-8e9e-31867a310f1e")
public:
  virtual ~SegmentFactory();

  virtual QString prettyName() const = 0;
  virtual QString category() const = 0;

  virtual SegmentModel* make(const Id<SegmentModel>&, QObject* parent) = 0;

  virtual SegmentModel* load(const VisitorVariant& data, QObject* parent) = 0;

  virtual SegmentModel* load(const SegmentData& data, QObject* parent) = 0;

  virtual QVariant makeCurveSegmentData() const = 0;

  virtual void
  serializeCurveSegmentData(const QVariant& data, const VisitorVariant& visitor) const = 0;
  virtual QVariant makeCurveSegmentData(const VisitorVariant& visitor) const = 0;
};

template <typename T>
class SegmentFactory_T : public SegmentFactory
{
public:
  virtual ~SegmentFactory_T() = default;

  SegmentModel* make(const Id<SegmentModel>& id, QObject* parent) override
  {
    return new T{id, parent};
  }

  SegmentModel* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new T{deserializer, parent};
    });
  }

  SegmentModel* load(const SegmentData& dat, QObject* parent) override
  {
    return new T{dat, parent};
  }

  QVariant makeCurveSegmentData() const override
  {
    return QVariant::fromValue(typename T::data_type{});
  }

  void
  serializeCurveSegmentData(const QVariant& data, const VisitorVariant& visitor) const override
  {
    score::serialize_dyn(visitor, data.value<typename T::data_type>());
  }

  QVariant makeCurveSegmentData(const VisitorVariant& vis) const override
  {
    return QVariant::fromValue(score::deserialize_dyn<typename T::data_type>(vis));
  }

  UuidKey<Curve::SegmentFactory> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, T>::get();
  }

  QString prettyName() const override { return Metadata<PrettyName_k, T>::get(); }
  QString category() const override { return Metadata<Category_k, T>::get(); }
};
}

#include <JS/Qml/ValueTypes.Qt6.hpp>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <ossia-qt/token_request.hpp>

#include <private/qqmlglobal_p.h>
#include <private/qqmlvaluetype_p.h>

namespace JS
{

qreal Vec2fValueType::x() const
{
  return v.x();
}

qreal Vec2fValueType::y() const
{
  return v.y();
}

void Vec2fValueType::setX(qreal x)
{
  v.setX(x);
}

void Vec2fValueType::setY(qreal y)
{
  v.setY(y);
}

qreal Vec2fValueType::dotProduct(const QVector2D& vec) const
{
  return QVector2D::dotProduct(v, vec);
}

QVector2D Vec2fValueType::times(const QVector2D& vec) const
{
  return v * vec;
}

QVector2D Vec2fValueType::times(qreal scalar) const
{
  return v * scalar;
}

QVector2D Vec2fValueType::plus(const QVector2D& vec) const
{
  return v + vec;
}

QVector2D Vec2fValueType::minus(const QVector2D& vec) const
{
  return v - vec;
}

QVector2D Vec2fValueType::normalized() const
{
  return v.normalized();
}

qreal Vec2fValueType::length() const
{
  return v.length();
}

QVector3D Vec2fValueType::toVector3d() const
{
  return v.toVector3D();
}

QVector4D Vec2fValueType::toVector4d() const
{
  return v.toVector4D();
}

bool Vec2fValueType::fuzzyEquals(const QVector2D& vec, qreal epsilon) const
{
  qreal absEps = qAbs(epsilon);
  if(qAbs(v.x() - vec.x()) > absEps)
    return false;
  if(qAbs(v.y() - vec.y()) > absEps)
    return false;
  return true;
}

bool Vec2fValueType::fuzzyEquals(const QVector2D& vec) const
{
  return qFuzzyCompare(v, vec);
}

QString Vec2fValueType::toString() const
{
  return QString(QLatin1String("QVector2D(%1, %2)")).arg(v.x()).arg(v.y());
}

QString Vec3fValueType::toString() const
{
  return QString(QLatin1String("QVector3D(%1, %2, %3)"))
      .arg(v.x())
      .arg(v.y())
      .arg(v.z());
}

qreal Vec3fValueType::x() const
{
  return v.x();
}

qreal Vec3fValueType::y() const
{
  return v.y();
}

qreal Vec3fValueType::z() const
{
  return v.z();
}

void Vec3fValueType::setX(qreal x)
{
  v.setX(x);
}

void Vec3fValueType::setY(qreal y)
{
  v.setY(y);
}

void Vec3fValueType::setZ(qreal z)
{
  v.setZ(z);
}

QVector3D Vec3fValueType::crossProduct(const QVector3D& vec) const
{
  return QVector3D::crossProduct(v, vec);
}

qreal Vec3fValueType::dotProduct(const QVector3D& vec) const
{
  return QVector3D::dotProduct(v, vec);
}

QVector3D Vec3fValueType::times(const QMatrix4x4& m) const
{
  return v * m;
}

QVector3D Vec3fValueType::times(const QVector3D& vec) const
{
  return v * vec;
}

QVector3D Vec3fValueType::times(qreal scalar) const
{
  return v * scalar;
}

QVector3D Vec3fValueType::plus(const QVector3D& vec) const
{
  return v + vec;
}

QVector3D Vec3fValueType::minus(const QVector3D& vec) const
{
  return v - vec;
}

QVector3D Vec3fValueType::normalized() const
{
  return v.normalized();
}

qreal Vec3fValueType::length() const
{
  return v.length();
}

QVector2D Vec3fValueType::toVector2d() const
{
  return v.toVector2D();
}

QVector4D Vec3fValueType::toVector4d() const
{
  return v.toVector4D();
}

bool Vec3fValueType::fuzzyEquals(const QVector3D& vec, qreal epsilon) const
{
  qreal absEps = qAbs(epsilon);
  if(qAbs(v.x() - vec.x()) > absEps)
    return false;
  if(qAbs(v.y() - vec.y()) > absEps)
    return false;
  if(qAbs(v.z() - vec.z()) > absEps)
    return false;
  return true;
}

bool Vec3fValueType::fuzzyEquals(const QVector3D& vec) const
{
  return qFuzzyCompare(v, vec);
}

QString Vec4fValueType::toString() const
{
  return QString(QLatin1String("QVector4D(%1, %2, %3, %4)"))
      .arg(v.x())
      .arg(v.y())
      .arg(v.z())
      .arg(v.w());
}

qreal Vec4fValueType::x() const
{
  return v.x();
}

qreal Vec4fValueType::y() const
{
  return v.y();
}

qreal Vec4fValueType::z() const
{
  return v.z();
}

qreal Vec4fValueType::w() const
{
  return v.w();
}

void Vec4fValueType::setX(qreal x)
{
  v.setX(x);
}

void Vec4fValueType::setY(qreal y)
{
  v.setY(y);
}

void Vec4fValueType::setZ(qreal z)
{
  v.setZ(z);
}

void Vec4fValueType::setW(qreal w)
{
  v.setW(w);
}

qreal Vec4fValueType::dotProduct(const QVector4D& vec) const
{
  return QVector4D::dotProduct(v, vec);
}

QVector4D Vec4fValueType::times(const QVector4D& vec) const
{
  return v * vec;
}

QVector4D Vec4fValueType::times(const QMatrix4x4& m) const
{
  return v * m;
}

QVector4D Vec4fValueType::times(qreal scalar) const
{
  return v * scalar;
}

QVector4D Vec4fValueType::plus(const QVector4D& vec) const
{
  return v + vec;
}

QVector4D Vec4fValueType::minus(const QVector4D& vec) const
{
  return v - vec;
}

QVector4D Vec4fValueType::normalized() const
{
  return v.normalized();
}

qreal Vec4fValueType::length() const
{
  return v.length();
}

QVector2D Vec4fValueType::toVector2d() const
{
  return v.toVector2D();
}

QVector3D Vec4fValueType::toVector3d() const
{
  return v.toVector3D();
}

bool Vec4fValueType::fuzzyEquals(const QVector4D& vec, qreal epsilon) const
{
  qreal absEps = qAbs(epsilon);
  if(qAbs(v.x() - vec.x()) > absEps)
    return false;
  if(qAbs(v.y() - vec.y()) > absEps)
    return false;
  if(qAbs(v.z() - vec.z()) > absEps)
    return false;
  if(qAbs(v.w() - vec.w()) > absEps)
    return false;
  return true;
}

bool Vec4fValueType::fuzzyEquals(const QVector4D& vec) const
{
  return qFuzzyCompare(v, vec);
}

double TokenRequestValueType::previous_date() const noexcept
{
  return req.prev_date.impl;
}

double TokenRequestValueType::date() const noexcept
{
  return req.date.impl;
}

double TokenRequestValueType::parent_duration() const noexcept
{
  return req.parent_duration.impl;
}

double TokenRequestValueType::offset() const noexcept
{
  return req.offset.impl;
}

double TokenRequestValueType::speed() const noexcept
{
  return req.speed;
}

double TokenRequestValueType::tempo() const noexcept
{
  return req.tempo;
}

double TokenRequestValueType::musical_start_last_signature() const noexcept
{
  return req.musical_start_last_signature;
}

double TokenRequestValueType::musical_start_last_bar() const noexcept
{
  return req.musical_start_last_bar;
}

double TokenRequestValueType::musical_start_position() const noexcept
{
  return req.musical_start_position;
}

double TokenRequestValueType::musical_end_last_bar() const noexcept
{
  return req.musical_end_last_bar;
}

double TokenRequestValueType::musical_end_position() const noexcept
{
  return req.musical_end_position;
}

double TokenRequestValueType::signature_upper() const noexcept
{
  return req.signature.upper;
}

double TokenRequestValueType::signature_lower() const noexcept
{
  return req.signature.lower;
}

double TokenRequestValueType::model_read_duration() const noexcept
{
  return req.model_read_duration().impl;
}

double TokenRequestValueType::physical_start(double ratio) const noexcept
{
  return req.physical_start(ratio);
}

double TokenRequestValueType::physical_read_duration(double ratio) const noexcept
{
  return req.physical_read_duration(ratio);
}

double TokenRequestValueType::physical_write_duration(double ratio) const noexcept
{
  return req.physical_write_duration(ratio);
}

bool TokenRequestValueType::in_range(double time) const noexcept
{
  return req.in_range({int64_t(time)});
}

double
TokenRequestValueType::to_physical_time_in_tick(double time, double ratio) const noexcept
{
  return req.to_physical_time_in_tick(time, ratio);
}

double TokenRequestValueType::from_physical_time_in_tick(
    double time, double ratio) const noexcept
{
  return req.from_physical_time_in_tick(time, ratio).impl;
}

double TokenRequestValueType::position() const noexcept
{
  return req.position();
}

bool TokenRequestValueType::forward() const noexcept
{
  return req.forward();
}

bool TokenRequestValueType::backward() const noexcept
{
  return req.backward();
}

bool TokenRequestValueType::paused() const noexcept
{
  return req.paused();
}

double TokenRequestValueType::get_quantification_date(double ratio) const noexcept
{
  if(auto res = req.get_quantification_date(ratio))
    return res->impl;
  return -1;
}

double TokenRequestValueType::get_physical_quantification_date(
    double rate, double ratio) const noexcept
{
  if(auto res = req.get_physical_quantification_date(rate, ratio))
    return *res;
  return -1;
}

void TokenRequestValueType::set_end_time(double time) noexcept
{
  return req.set_end_time({int64_t(time)});
}

void TokenRequestValueType::set_start_time(double time) noexcept
{
  return req.set_start_time({int64_t(time)});
}

bool TokenRequestValueType::unexpected_bar_change() const noexcept
{
  return req.unexpected_bar_change();
}

int SampleTimings::start_sample() const noexcept
{
  return tm.start_sample;
}

int SampleTimings::length() const noexcept
{
  return tm.length;
}

int ExecutionStateValueType::sample_rate() const noexcept
{
  return req.sampleRate();
}

int ExecutionStateValueType::buffer_size() const noexcept
{
  return req.bufferSize();
}

double ExecutionStateValueType::model_to_physical() const noexcept
{
  return req.modelToSamples();
}

double ExecutionStateValueType::physical_to_model() const noexcept
{
  return req.samplesToModel();
}

double ExecutionStateValueType::physical_date() const noexcept
{
  return req.samplesSinceStart();
}

double ExecutionStateValueType::start_date_ns() const noexcept
{
  return req.startDate();
}

double ExecutionStateValueType::current_date_ns() const noexcept
{
  return req.currentDate();
}

SampleTimings ExecutionStateValueType::timings(TokenRequestValueType tk) const noexcept
{
  return SampleTimings{req.timings(tk.req)};
}
}

#endif

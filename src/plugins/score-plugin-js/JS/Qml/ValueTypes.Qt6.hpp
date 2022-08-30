#pragma once
#include <QObject>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <JS/Qml/Metatypes.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <ossia-qt/time.hpp>

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtQml/qqmlregistration.h>

#include <verdigris>

namespace JS
{

struct Vec2fValueType
{
  QVector2D v;
  Q_GADGET
public:
  Q_INVOKABLE
  QString toString() const;

  qreal x() const;
  qreal y() const;
  void setX(qreal);
  void setY(qreal);

  Q_INVOKABLE
  qreal dotProduct(const QVector2D& vec) const;

  Q_INVOKABLE
  QVector2D times(const QVector2D& vec) const;
  Q_INVOKABLE
  QVector2D times(qreal scalar) const;

  Q_INVOKABLE
  QVector2D plus(const QVector2D& vec) const;
  Q_INVOKABLE
  QVector2D minus(const QVector2D& vec) const;
  Q_INVOKABLE
  QVector2D normalized() const;
  Q_INVOKABLE
  qreal length() const;

  Q_INVOKABLE
  QVector3D toVector3d() const;
  Q_INVOKABLE
  QVector4D toVector4d() const;

  Q_INVOKABLE
  bool fuzzyEquals(const QVector2D& vec, qreal epsilon) const;
  Q_INVOKABLE
  bool fuzzyEquals(const QVector2D& vec) const;

  Q_PROPERTY(qreal x READ x WRITE setX FINAL)
  Q_PROPERTY(qreal y READ y WRITE setY FINAL)
};

struct Vec3fValueType
{
  QVector3D v;
  Q_GADGET
public:
  Q_INVOKABLE
  QString toString() const;

  qreal x() const;
  qreal y() const;
  qreal z() const;
  void setX(qreal);
  void setY(qreal);
  void setZ(qreal);

  Q_INVOKABLE
  QVector3D crossProduct(const QVector3D& vec) const;
  Q_INVOKABLE
  qreal dotProduct(const QVector3D& vec) const;

  Q_INVOKABLE
  QVector3D times(const QVector3D& vec) const;
  Q_INVOKABLE
  QVector3D times(const QMatrix4x4& m) const;
  Q_INVOKABLE
  QVector3D times(qreal scalar) const;

  Q_INVOKABLE
  QVector3D plus(const QVector3D& vec) const;
  Q_INVOKABLE
  QVector3D minus(const QVector3D& vec) const;
  Q_INVOKABLE
  QVector3D normalized() const;
  Q_INVOKABLE
  qreal length() const;

  Q_INVOKABLE
  QVector2D toVector2d() const;
  Q_INVOKABLE
  QVector4D toVector4d() const;

  Q_INVOKABLE
  bool fuzzyEquals(const QVector3D& vec, qreal epsilon) const;
  Q_INVOKABLE
  bool fuzzyEquals(const QVector3D& vec) const;

  Q_PROPERTY(qreal x READ x WRITE setX FINAL)
  Q_PROPERTY(qreal y READ y WRITE setY FINAL)
  Q_PROPERTY(qreal z READ z WRITE setZ FINAL)
};

struct Vec4fValueType
{
  QVector4D v;
  Q_GADGET
public:
  Q_INVOKABLE
  QString toString() const;

  qreal x() const;
  qreal y() const;
  qreal z() const;
  qreal w() const;
  void setX(qreal);
  void setY(qreal);
  void setZ(qreal);
  void setW(qreal);

  Q_INVOKABLE
  qreal dotProduct(const QVector4D& vec) const;

  Q_INVOKABLE
  QVector4D times(const QVector4D& vec) const;
  Q_INVOKABLE
  QVector4D times(const QMatrix4x4& m) const;
  Q_INVOKABLE
  QVector4D times(qreal scalar) const;

  Q_INVOKABLE
  QVector4D plus(const QVector4D& vec) const;
  Q_INVOKABLE
  QVector4D minus(const QVector4D& vec) const;
  Q_INVOKABLE
  QVector4D normalized() const;
  Q_INVOKABLE
  qreal length() const;

  Q_INVOKABLE
  QVector2D toVector2d() const;
  Q_INVOKABLE
  QVector3D toVector3d() const;

  Q_INVOKABLE
  bool fuzzyEquals(const QVector4D& vec, qreal epsilon) const;
  Q_INVOKABLE
  bool fuzzyEquals(const QVector4D& vec) const;

  Q_PROPERTY(qreal x READ x WRITE setX FINAL)
  Q_PROPERTY(qreal y READ y WRITE setY FINAL)
  Q_PROPERTY(qreal z READ z WRITE setZ FINAL)
  Q_PROPERTY(qreal w READ w WRITE setW FINAL)
};

struct TokenRequestValueType
{
  Q_GADGET
public:
  ossia::token_request req;

  double previous_date() const noexcept;
  double date() const noexcept;
  double parent_duration() const noexcept;
  double offset() const noexcept;
  double speed() const noexcept;
  double tempo() const noexcept;

  double musical_start_last_signature() const noexcept;
  double musical_start_last_bar() const noexcept;
  double musical_start_position() const noexcept;
  double musical_end_last_bar() const noexcept;
  double musical_end_position() const noexcept;

  double signature_upper() const noexcept;
  double signature_lower() const noexcept;

  Q_INVOKABLE
  double model_read_duration() const noexcept;
  Q_INVOKABLE
  double physical_start(double ratio) const noexcept;
  Q_INVOKABLE
  double physical_read_duration(double ratio) const noexcept;
  Q_INVOKABLE
  double physical_write_duration(double ratio) const noexcept;
  Q_INVOKABLE
  bool in_range(double time) const noexcept;
  Q_INVOKABLE
  double to_physical_time_in_tick(double time, double ratio) const noexcept;
  Q_INVOKABLE
  double from_physical_time_in_tick(double time, double ratio) const noexcept;

  Q_INVOKABLE
  double position() const noexcept;
  Q_INVOKABLE
  bool forward() const noexcept;
  Q_INVOKABLE
  bool backward() const noexcept;
  Q_INVOKABLE
  bool paused() const noexcept;

  Q_INVOKABLE
  double get_quantification_date(double ratio) const noexcept;

  Q_INVOKABLE
  double get_physical_quantification_date(double rate, double ratio) const noexcept;

  Q_INVOKABLE
  void set_end_time(double time) noexcept;
  Q_INVOKABLE
  void set_start_time(double time) noexcept;

  Q_INVOKABLE
  bool unexpected_bar_change() const noexcept;

  Q_PROPERTY(double previous_date READ previous_date FINAL)
  Q_PROPERTY(double date READ date FINAL)
  Q_PROPERTY(double parent_duration READ parent_duration FINAL)
  Q_PROPERTY(double offset READ offset FINAL)
  Q_PROPERTY(double speed READ speed FINAL)
  Q_PROPERTY(double tempo READ tempo FINAL)

  Q_PROPERTY(double musical_start_last_signature READ musical_start_last_signature FINAL)
  Q_PROPERTY(double musical_start_last_bar READ musical_start_last_bar FINAL)
  Q_PROPERTY(double musical_start_position READ musical_start_position FINAL)
  Q_PROPERTY(double musical_end_last_bar READ musical_end_last_bar FINAL)
  Q_PROPERTY(double musical_end_position READ musical_end_position FINAL)

  Q_PROPERTY(double signature_upper READ signature_upper FINAL)
  Q_PROPERTY(double signature_lower READ signature_lower FINAL)
};

struct SampleTimings
{
  Q_GADGET
  QML_VALUE_TYPE(SampleTimings)
public:
  ossia::exec_state_facade::sample_timings tm{};

  int start_sample() const noexcept;
  int length() const noexcept;

  Q_PROPERTY(int start_sample READ start_sample FINAL)
  Q_PROPERTY(int length READ length FINAL)
};

struct ExecutionStateValueType
{
  Q_GADGET
  QML_VALUE_TYPE(ExecutionStateValueType)

public:
  ossia::exec_state_facade req;
  int sample_rate() const noexcept;
  int buffer_size() const noexcept;
  double model_to_physical() const noexcept;
  double physical_to_model() const noexcept;
  double physical_date() const noexcept;
  double start_date_ns() const noexcept;
  double current_date_ns() const noexcept;

  Q_INVOKABLE
  SampleTimings timings(TokenRequestValueType tk) const noexcept;

  Q_PROPERTY(int sample_rate READ sample_rate FINAL)
  Q_PROPERTY(int buffer_size READ buffer_size FINAL)
  Q_PROPERTY(double model_to_physical READ model_to_physical FINAL)
  Q_PROPERTY(double physical_to_model READ physical_to_model FINAL)
  Q_PROPERTY(double physical_date READ physical_date FINAL)
  Q_PROPERTY(double start_date_ns READ start_date_ns FINAL)
  Q_PROPERTY(double current_date_ns READ current_date_ns FINAL)
};
}

Q_DECLARE_METATYPE(JS::TokenRequestValueType)
Q_DECLARE_METATYPE(JS::SampleTimings)
#endif

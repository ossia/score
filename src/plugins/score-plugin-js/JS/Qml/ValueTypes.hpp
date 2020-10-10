#pragma once
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <verdigris>

#include <JS/Qml/Metatypes.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia-qt/time.hpp>

namespace JS
{
void registerQmlValueTypeProvider();

class Vec2fValueType
{
    QVector2D v;
    W_GADGET(Vec2fValueType)
public:

    QString toString() const;
    W_INVOKABLE(toString)

    qreal x() const;
    qreal y() const;
    void setX(qreal);
    void setY(qreal);

    qreal dotProduct(const QVector2D &vec) const;
    W_INVOKABLE(dotProduct)

    QVector2D times(const QVector2D &vec) const;
    W_INVOKABLE(times, (const QVector2D&))
    QVector2D times(qreal scalar) const;
    W_INVOKABLE(times, (qreal))

    QVector2D plus(const QVector2D &vec) const;
    W_INVOKABLE(plus)
    QVector2D minus(const QVector2D &vec) const;
    W_INVOKABLE(minus)
    QVector2D normalized() const;
    W_INVOKABLE(normalized)
    qreal length() const;
    W_INVOKABLE(length)

    QVector3D toVector3d() const;
    W_INVOKABLE(toVector3d)
    QVector4D toVector4d() const;
    W_INVOKABLE(toVector4d)

    bool fuzzyEquals(const QVector2D &vec, qreal epsilon) const;
    W_INVOKABLE(fuzzyEquals, (const QVector2D &, qreal))
    bool fuzzyEquals(const QVector2D &vec) const;
    W_INVOKABLE(fuzzyEquals, (const QVector2D &))

    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
};

class Vec3fValueType
{
    QVector3D v;
    W_GADGET(Vec3fValueType)
public:
    QString toString() const;
    W_INVOKABLE(toString)

    qreal x() const;
    qreal y() const;
    qreal z() const;
    void setX(qreal);
    void setY(qreal);
    void setZ(qreal);

    QVector3D crossProduct(const QVector3D &vec) const;
    W_INVOKABLE(crossProduct)
    qreal dotProduct(const QVector3D &vec) const;
    W_INVOKABLE(dotProduct)

    QVector3D times(const QVector3D &vec) const;
    W_INVOKABLE(times, (const QVector3D&))
    QVector3D times(const QMatrix4x4 &m) const;
    W_INVOKABLE(times, (const QMatrix4x4&))
    QVector3D times(qreal scalar) const;
    W_INVOKABLE(times, (qreal))

    QVector3D plus(const QVector3D &vec) const;
    W_INVOKABLE(plus)
    QVector3D minus(const QVector3D &vec) const;
    W_INVOKABLE(minus)
    QVector3D normalized() const;
    W_INVOKABLE(normalized)
    qreal length() const;
    W_INVOKABLE(length)

    QVector2D toVector2d() const;
    W_INVOKABLE(toVector2d)
    QVector4D toVector4d() const;
    W_INVOKABLE(toVector4d)

    bool fuzzyEquals(const QVector3D &vec, qreal epsilon) const;
    W_INVOKABLE(fuzzyEquals, (const QVector3D &, qreal))
    bool fuzzyEquals(const QVector3D &vec) const;
    W_INVOKABLE(fuzzyEquals, (const QVector3D &))

    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
    W_PROPERTY(qreal, z READ z WRITE setZ FINAL)
};

class Vec4fValueType
{
    QVector4D v;
    W_GADGET(Vec4fValueType)
public:

    QString toString() const;
    W_INVOKABLE(toString)

    qreal x() const;
    qreal y() const;
    qreal z() const;
    qreal w() const;
    void setX(qreal);
    void setY(qreal);
    void setZ(qreal);
    void setW(qreal);


    qreal dotProduct(const QVector4D &vec) const;
    W_INVOKABLE(dotProduct)

    QVector4D times(const QVector4D &vec) const;
    W_INVOKABLE(times, (const QVector4D&))
    QVector4D times(const QMatrix4x4 &m) const;
    W_INVOKABLE(times, (const QMatrix4x4&))
    QVector4D times(qreal scalar) const;
    W_INVOKABLE(times, (qreal))

    QVector4D plus(const QVector4D &vec) const;
    W_INVOKABLE(plus)
    QVector4D minus(const QVector4D &vec) const;
    W_INVOKABLE(minus)
    QVector4D normalized() const;
    W_INVOKABLE(normalized)
    qreal length() const;
    W_INVOKABLE(length)

    QVector2D toVector2d() const;
    W_INVOKABLE(toVector2d)
    QVector3D toVector3d() const;
    W_INVOKABLE(toVector3d)

    bool fuzzyEquals(const QVector4D &vec, qreal epsilon) const;
    W_INVOKABLE(fuzzyEquals, (const QVector4D &, qreal))
    bool fuzzyEquals(const QVector4D &vec) const;
    W_INVOKABLE(fuzzyEquals, (const QVector4D &))

    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
    W_PROPERTY(qreal, z READ z WRITE setZ FINAL)
    W_PROPERTY(qreal, w READ w WRITE setW FINAL)
};

class TokenRequestValueType
{
  ossia::token_request req;
  W_GADGET(TokenRequestValueType)
public:

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

  double model_read_duration() const noexcept;
  W_INVOKABLE(model_read_duration)
  double physical_start(double ratio) const noexcept;
  W_INVOKABLE(physical_start)
  double physical_read_duration(double ratio) const noexcept;
  W_INVOKABLE(physical_read_duration)
  double physical_write_duration(double ratio) const noexcept;
  W_INVOKABLE(physical_write_duration)
  bool in_range(double time) const noexcept;
  W_INVOKABLE(in_range)
  double to_physical_time_in_tick(double time, double ratio) const noexcept;
  W_INVOKABLE(to_physical_time_in_tick)
  double from_physical_time_in_tick(double time, double ratio) const noexcept;
  W_INVOKABLE(from_physical_time_in_tick)

  double position() const noexcept;
  W_INVOKABLE(position)
  bool forward() const noexcept;
  W_INVOKABLE(forward)
  bool backward() const noexcept;
  W_INVOKABLE(backward)
  bool paused() const noexcept;
  W_INVOKABLE(paused)

  double get_quantification_date(double ratio) const noexcept;
  W_INVOKABLE(get_quantification_date)

  double get_physical_quantification_date(double rate, double ratio) const noexcept;
  W_INVOKABLE(get_physical_quantification_date)

  void reduce_end_time(double time) noexcept;
  W_INVOKABLE(reduce_end_time)
  void increase_start_time(double time) noexcept;
  W_INVOKABLE(increase_start_time)

  bool unexpected_bar_change() const noexcept;
  W_INVOKABLE(unexpected_bar_change)

  W_PROPERTY(double, previous_date READ previous_date FINAL)
  W_PROPERTY(double, date READ date FINAL)
  W_PROPERTY(double, parent_duration READ parent_duration FINAL)
  W_PROPERTY(double, offset READ offset FINAL)
  W_PROPERTY(double, speed READ speed FINAL)
  W_PROPERTY(double, tempo READ tempo FINAL)

  W_PROPERTY(double, musical_start_last_signature READ musical_start_last_signature FINAL)
  W_PROPERTY(double, musical_start_last_bar READ musical_start_last_bar FINAL)
  W_PROPERTY(double, musical_start_position READ musical_start_position FINAL)
  W_PROPERTY(double, musical_end_last_bar READ musical_end_last_bar FINAL)
  W_PROPERTY(double, musical_end_position READ musical_end_position FINAL)

  W_PROPERTY(double, signature_upper READ signature_upper FINAL)
  W_PROPERTY(double, signature_lower READ signature_lower FINAL)
};

class ExecutionStateValueType
{
  ossia::exec_state_facade req;
  W_GADGET(ExecutionStateValueType)
public:

  int sample_rate() const noexcept;
  int buffer_size() const noexcept;
  double model_to_physical() const noexcept;
  double physical_to_model() const noexcept;
  double physical_date() const noexcept;
  double start_date_ns() const noexcept;
  double current_date_ns() const noexcept;

  W_PROPERTY(int, sample_rate READ sample_rate FINAL)
  W_PROPERTY(int, buffer_size READ buffer_size FINAL)
  W_PROPERTY(double, model_to_physical READ model_to_physical FINAL)
  W_PROPERTY(double, physical_to_model READ physical_to_model FINAL)
  W_PROPERTY(double, physical_date READ physical_date FINAL)
  W_PROPERTY(double, start_date_ns READ start_date_ns FINAL)
  W_PROPERTY(double, current_date_ns READ current_date_ns FINAL)
};
}

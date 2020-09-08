#pragma once
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <verdigris>

namespace JS
{
void registerQmlValueTypeProvider();

class Vec2fValueType
{
    QVector2D v;
public:
    Q_INVOKABLE QString toString() const;

    qreal x() const;
    qreal y() const;
    void setX(qreal);
    void setY(qreal);
    Q_INVOKABLE qreal dotProduct(const QVector2D &vec) const;
    Q_INVOKABLE QVector2D times(const QVector2D &vec) const;
    Q_INVOKABLE QVector2D times(qreal scalar) const;
    Q_INVOKABLE QVector2D plus(const QVector2D &vec) const;
    Q_INVOKABLE QVector2D minus(const QVector2D &vec) const;
    Q_INVOKABLE QVector2D normalized() const;
    Q_INVOKABLE qreal length() const;
    Q_INVOKABLE QVector3D toVector3d() const;
    Q_INVOKABLE QVector4D toVector4d() const;
    Q_INVOKABLE bool fuzzyEquals(const QVector2D &vec, qreal epsilon) const;
    Q_INVOKABLE bool fuzzyEquals(const QVector2D &vec) const;

    W_GADGET(Vec2fValueType)
    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
};

class Vec3fValueType
{
    QVector3D v;
public:
    Q_INVOKABLE QString toString() const;

    qreal x() const;
    qreal y() const;
    qreal z() const;
    void setX(qreal);
    void setY(qreal);
    void setZ(qreal);

    Q_INVOKABLE QVector3D crossProduct(const QVector3D &vec) const;
    Q_INVOKABLE qreal dotProduct(const QVector3D &vec) const;
    Q_INVOKABLE QVector3D times(const QMatrix4x4 &m) const;
    Q_INVOKABLE QVector3D times(const QVector3D &vec) const;
    Q_INVOKABLE QVector3D times(qreal scalar) const;
    Q_INVOKABLE QVector3D plus(const QVector3D &vec) const;
    Q_INVOKABLE QVector3D minus(const QVector3D &vec) const;
    Q_INVOKABLE QVector3D normalized() const;
    Q_INVOKABLE qreal length() const;
    Q_INVOKABLE QVector2D toVector2d() const;
    Q_INVOKABLE QVector4D toVector4d() const;
    Q_INVOKABLE bool fuzzyEquals(const QVector3D &vec, qreal epsilon) const;
    Q_INVOKABLE bool fuzzyEquals(const QVector3D &vec) const;

    W_GADGET(Vec3fValueType)
    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
    W_PROPERTY(qreal, z READ z WRITE setZ FINAL)
};

class Vec4fValueType
{
    QVector4D v;
public:
    Q_INVOKABLE QString toString() const;

    qreal x() const;
    qreal y() const;
    qreal z() const;
    qreal w() const;
    void setX(qreal);
    void setY(qreal);
    void setZ(qreal);
    void setW(qreal);

    Q_INVOKABLE qreal dotProduct(const QVector4D &vec) const;
    Q_INVOKABLE QVector4D times(const QVector4D &vec) const;
    Q_INVOKABLE QVector4D times(const QMatrix4x4 &m) const;
    Q_INVOKABLE QVector4D times(qreal scalar) const;
    Q_INVOKABLE QVector4D plus(const QVector4D &vec) const;
    Q_INVOKABLE QVector4D minus(const QVector4D &vec) const;
    Q_INVOKABLE QVector4D normalized() const;
    Q_INVOKABLE qreal length() const;
    Q_INVOKABLE QVector2D toVector2d() const;
    Q_INVOKABLE QVector3D toVector3d() const;
    Q_INVOKABLE bool fuzzyEquals(const QVector4D &vec, qreal epsilon) const;
    Q_INVOKABLE bool fuzzyEquals(const QVector4D &vec) const;

    W_GADGET(Vec4fValueType)
    W_PROPERTY(qreal, x READ x WRITE setX FINAL)
    W_PROPERTY(qreal, y READ y WRITE setY FINAL)
    W_PROPERTY(qreal, z READ z WRITE setZ FINAL)
    W_PROPERTY(qreal, w READ w WRITE setW FINAL)
};

}

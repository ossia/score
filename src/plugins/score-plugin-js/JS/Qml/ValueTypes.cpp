#include <JS/Qml/ValueTypes.hpp>
#include <private/qqmlglobal_p.h>
#include <private/qqmlvaluetype_p.h>
#include <wobjectimpl.h>
W_GADGET_IMPL(JS::Vec2fValueType)
W_GADGET_IMPL(JS::Vec3fValueType)
W_GADGET_IMPL(JS::Vec4fValueType)
W_GADGET_IMPL(JS::TokenRequestValueType)
W_GADGET_IMPL(JS::ExecutionStateValueType)

// Most of this code is refactored out of the qtdeclarative source.
/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

namespace JS
{

class OssiaTypeProvider : public QQmlValueTypeProvider
{
public:

#if defined(QT_NO_DEBUG) && !defined(QT_FORCE_ASSERTS)
    #define ASSERT_VALID_SIZE(size, min) Q_UNUSED(size)
#else
    #define ASSERT_VALID_SIZE(size, min) Q_ASSERT(size >= min)
#endif

    static QVector2D vector2DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 1) {
            int index = s.indexOf(QLatin1Char(','));

            bool xGood, yGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index + 1).toFloat(&yGood);

            if (xGood && yGood) {
                if (ok) *ok = true;
                return QVector2D(xCoord, yCoord);
            }
        }

        if (ok) *ok = false;
        return QVector2D();
    }

    static QVector3D vector3DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 2) {
            int index = s.indexOf(QLatin1Char(','));
            int index2 = s.indexOf(QLatin1Char(','), index+1);

            bool xGood, yGood, zGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index + 1, index2 - index - 1).toFloat(&yGood);
            float zCoord = s.midRef(index2 + 1).toFloat(&zGood);

            if (xGood && yGood && zGood) {
                if (ok) *ok = true;
                return QVector3D(xCoord, yCoord, zCoord);
            }
        }

        if (ok) *ok = false;
        return QVector3D();
    }

    static QVector4D vector4DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 3) {
            int index = s.indexOf(QLatin1Char(','));
            int index2 = s.indexOf(QLatin1Char(','), index+1);
            int index3 = s.indexOf(QLatin1Char(','), index2+1);

            bool xGood, yGood, zGood, wGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index + 1, index2 - index - 1).toFloat(&yGood);
            float zCoord = s.midRef(index2 + 1, index3 - index2 - 1).toFloat(&zGood);
            float wCoord = s.midRef(index3 + 1).toFloat(&wGood);

            if (xGood && yGood && zGood && wGood) {
                if (ok) *ok = true;
                return QVector4D(xCoord, yCoord, zCoord, wCoord);
            }
        }

        if (ok) *ok = false;
        return QVector4D();
    }

    const QMetaObject *getMetaObjectForMetaType(int type) override
    {
        switch (type) {
        case QMetaType::QVector2D:
            return &Vec2fValueType::staticMetaObject;
        case QMetaType::QVector3D:
            return &Vec3fValueType::staticMetaObject;
        case QMetaType::QVector4D:
            return &Vec4fValueType::staticMetaObject;
        default:
          {
            static const int tokenId = QMetaTypeId<ossia::token_request>::qt_metatype_id();
            if(type == tokenId)
              return &TokenRequestValueType::staticMetaObject;
            static const int execStateId = QMetaTypeId<ossia::exec_state_facade>::qt_metatype_id();
            if(type == execStateId)
              return &ExecutionStateValueType::staticMetaObject;
            break;
          }
        }

        return nullptr;
    }

    bool init(int type, QVariant& dst) override
    {
        switch (type) {
        case QMetaType::QVector2D:
            dst.setValue<QVector2D>(QVector2D());
            return true;
        case QMetaType::QVector3D:
            dst.setValue<QVector3D>(QVector3D());
            return true;
        case QMetaType::QVector4D:
            dst.setValue<QVector4D>(QVector4D());
            return true;
        default:
          {
            static const int tokenId = QMetaTypeId<JS::TokenRequestValueType>::qt_metatype_id();
            if(type == tokenId)
              dst.setValue<ossia::token_request>({});
            break;
          }
        }

        return false;
    }

    bool create(int type, int argc, const void *argv[], QVariant *v) override
    {
        switch (type) {
        case QMetaType::QVector2D:
            if (argc == 1) {
                const float *xy = reinterpret_cast<const float*>(argv[0]);
                QVector2D v2(xy[0], xy[1]);
                *v = QVariant(v2);
                return true;
            }
            break;
        case QMetaType::QVector3D:
            if (argc == 1) {
                const float *xyz = reinterpret_cast<const float*>(argv[0]);
                QVector3D v3(xyz[0], xyz[1], xyz[2]);
                *v = QVariant(v3);
                return true;
            }
            break;
        case QMetaType::QVector4D:
            if (argc == 1) {
                const float *xyzw = reinterpret_cast<const float*>(argv[0]);
                QVector4D v4(xyzw[0], xyzw[1], xyzw[2], xyzw[3]);
                *v = QVariant(v4);
                return true;
            }
            break;
        default: break;
        }

        return false;
    }

    template<typename T>
    bool createFromStringTyped(void *data, size_t dataSize, T initValue)
    {
        ASSERT_VALID_SIZE(dataSize, sizeof(T));
        T *t = reinterpret_cast<T *>(data);
        new (t) T(initValue);
        return true;
    }

    bool createFromString(int type, const QString &s, void *data, size_t dataSize) override
    {
        bool ok = false;

        switch (type) {
        case QMetaType::QVector2D:
            return createFromStringTyped<QVector2D>(data, dataSize, vector2DFromString(s, &ok));
        case QMetaType::QVector3D:
            return createFromStringTyped<QVector3D>(data, dataSize, vector3DFromString(s, &ok));
        case QMetaType::QVector4D:
            return createFromStringTyped<QVector4D>(data, dataSize, vector4DFromString(s, &ok));
        default: break;
        }

        return false;
    }

    bool createStringFrom(int type, const void *data, QString *s) override
    {
        return false;
    }

    bool variantFromString(const QString &s, QVariant *v) override
    {
        bool ok = false;

        QVector2D v2 = vector2DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v2);
            return true;
        }

        QVector3D v3 = vector3DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v3);
            return true;
        }

        QVector4D v4 = vector4DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v4);
            return true;
        }
        return false;
    }

    bool variantFromString(int type, const QString &s, QVariant *v) override
    {
        bool ok = false;

        switch (type) {
        case QMetaType::QVector2D:
            {
            *v = QVariant::fromValue(vector2DFromString(s, &ok));
            return true;
            }
        case QMetaType::QVector3D:
            {
            *v = QVariant::fromValue(vector3DFromString(s, &ok));
            return true;
            }
        case QMetaType::QVector4D:
            {
            *v = QVariant::fromValue(vector4DFromString(s, &ok));
            return true;
            }
        default:
            break;
        }

        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    bool variantFromJsObject(int type, const QV4::Value &object, QV4::ExecutionEngine *v4, QVariant *v) override
    {
      return false;
      /*
        QV4::Scope scope(v4);
#ifndef QT_NO_DEBUG
        QV4::ScopedObject obj(scope, object);
        Q_ASSERT(obj);
#endif
        bool ok = false;
        switch (type) {
        case QMetaType::QColorSpace:
            *v = QVariant::fromValue(colorSpaceFromObject(object, v4, &ok));
            break;
        case QMetaType::QFont:
            *v = QVariant::fromValue(fontFromObject(object, v4, &ok));
            break;
        case QMetaType::QMatrix4x4:
            *v = QVariant::fromValue(matrix4x4FromObject(object, v4, &ok));
        default: break;
        }

        return ok;
        */
    }
#endif

    template<typename T>
    bool typedEqual(const void *lhs, const QVariant& rhs)
    {
        return (*(reinterpret_cast<const T *>(lhs)) == rhs.value<T>());
    }

    bool equal(int type, const void *lhs, const QVariant &rhs) override
    {
        switch (type) {
        case QMetaType::QVector2D:
            return typedEqual<QVector2D>(lhs, rhs);
        case QMetaType::QVector3D:
            return typedEqual<QVector3D>(lhs, rhs);
        case QMetaType::QVector4D:
            return typedEqual<QVector4D>(lhs, rhs);
        default: break;
        }

        return false;
    }

    template<typename T>
    bool typedStore(const void *src, void *dst, size_t dstSize)
    {
        ASSERT_VALID_SIZE(dstSize, sizeof(T));
        const T *srcT = reinterpret_cast<const T *>(src);
        T *dstT = reinterpret_cast<T *>(dst);
        new (dstT) T(*srcT);
        return true;
    }

    bool store(int type, const void *src, void *dst, size_t dstSize) override
    {
        return false;
    }

    template<typename T>
    bool typedRead(const QVariant& src, int dstType, void *dst)
    {
        T *dstT = reinterpret_cast<T *>(dst);
        if (src.userType() == dstType) {
            *dstT = src.value<T>();
        } else {
            *dstT = T();
        }
        return true;
    }

    bool read(const QVariant &src, void *dst, int dstType) override
    {
        switch (dstType) {
        case QMetaType::QVector2D:
            return typedRead<QVector2D>(src, dstType, dst);
        case QMetaType::QVector3D:
            return typedRead<QVector3D>(src, dstType, dst);
        case QMetaType::QVector4D:
            return typedRead<QVector4D>(src, dstType, dst);
        default: break;
        }

        return false;
    }

    template<typename T>
    bool typedWrite(const void *src, QVariant& dst)
    {
        const T *srcT = reinterpret_cast<const T *>(src);
        if (dst.value<T>() != *srcT) {
            dst = *srcT;
            return true;
        }
        return false;
    }

    bool write(int type, const void *src, QVariant& dst) override
    {
        switch (type) {
        case QMetaType::QVector2D:
            return typedWrite<QVector2D>(src, dst);
        case QMetaType::QVector3D:
            return typedWrite<QVector3D>(src, dst);
        case QMetaType::QVector4D:
            return typedWrite<QVector4D>(src, dst);
        default: break;
        }

        return false;
    }
#undef ASSERT_VALID_SIZE
};


void registerQmlValueTypeProvider()
{
    static OssiaTypeProvider valueTypeProvider;

    QQml_addValueTypeProvider(&valueTypeProvider);
}

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

qreal Vec2fValueType::dotProduct(const QVector2D &vec) const
{
    return QVector2D::dotProduct(v, vec);
}

QVector2D Vec2fValueType::times(const QVector2D &vec) const
{
    return v * vec;
}

QVector2D Vec2fValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector2D Vec2fValueType::plus(const QVector2D &vec) const
{
    return v + vec;
}

QVector2D Vec2fValueType::minus(const QVector2D &vec) const
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

bool Vec2fValueType::fuzzyEquals(const QVector2D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    return true;
}

bool Vec2fValueType::fuzzyEquals(const QVector2D &vec) const
{
    return qFuzzyCompare(v, vec);
}

QString Vec2fValueType::toString() const
{
    return QString(QLatin1String("QVector2D(%1, %2)")).arg(v.x()).arg(v.y());
}

QString Vec3fValueType::toString() const
{
    return QString(QLatin1String("QVector3D(%1, %2, %3)")).arg(v.x()).arg(v.y()).arg(v.z());
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

QVector3D Vec3fValueType::crossProduct(const QVector3D &vec) const
{
    return QVector3D::crossProduct(v, vec);
}

qreal Vec3fValueType::dotProduct(const QVector3D &vec) const
{
  qDebug("in here");
    return QVector3D::dotProduct(v, vec);
}

QVector3D Vec3fValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector3D Vec3fValueType::times(const QVector3D &vec) const
{
    return v * vec;
}

QVector3D Vec3fValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector3D Vec3fValueType::plus(const QVector3D &vec) const
{
    return v + vec;
}

QVector3D Vec3fValueType::minus(const QVector3D &vec) const
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

bool Vec3fValueType::fuzzyEquals(const QVector3D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    if (qAbs(v.z() - vec.z()) > absEps)
        return false;
    return true;
}

bool Vec3fValueType::fuzzyEquals(const QVector3D &vec) const
{
    return qFuzzyCompare(v, vec);
}

QString Vec4fValueType::toString() const
{
    return QString(QLatin1String("QVector4D(%1, %2, %3, %4)")).arg(v.x()).arg(v.y()).arg(v.z()).arg(v.w());
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

qreal Vec4fValueType::dotProduct(const QVector4D &vec) const
{
    return QVector4D::dotProduct(v, vec);
}

QVector4D Vec4fValueType::times(const QVector4D &vec) const
{
    return v * vec;
}

QVector4D Vec4fValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector4D Vec4fValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector4D Vec4fValueType::plus(const QVector4D &vec) const
{
    return v + vec;
}

QVector4D Vec4fValueType::minus(const QVector4D &vec) const
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

bool Vec4fValueType::fuzzyEquals(const QVector4D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    if (qAbs(v.z() - vec.z()) > absEps)
        return false;
    if (qAbs(v.w() - vec.w()) > absEps)
        return false;
    return true;
}

bool Vec4fValueType::fuzzyEquals(const QVector4D &vec) const
{
    return qFuzzyCompare(v, vec);
}

double TokenRequestValueType::previous_date() const noexcept { return req.prev_date.impl; }


double TokenRequestValueType::date() const noexcept { return req.date.impl; }


double TokenRequestValueType::parent_duration() const noexcept { return req.parent_duration.impl; }


double TokenRequestValueType::offset() const noexcept { return req.offset.impl; }


double TokenRequestValueType::speed() const noexcept { return req.speed; }


double TokenRequestValueType::tempo() const noexcept { return req.tempo; }


double TokenRequestValueType::musical_start_last_signature() const noexcept { return req.musical_start_last_signature; }


double TokenRequestValueType::musical_start_last_bar() const noexcept { return req.musical_start_last_bar; }


double TokenRequestValueType::musical_start_position() const noexcept { return req.musical_start_position; }


double TokenRequestValueType::musical_end_last_bar() const noexcept { return req.musical_end_last_bar; }


double TokenRequestValueType::musical_end_position() const noexcept { return req.musical_end_position; }


double TokenRequestValueType::signature_upper() const noexcept { return req.signature.upper; }


double TokenRequestValueType::signature_lower() const noexcept { return req.signature.lower; }


double TokenRequestValueType::model_read_duration() const noexcept { return req.model_read_duration().impl; }


double TokenRequestValueType::physical_start(double ratio) const noexcept { return req.physical_start(ratio); }


double TokenRequestValueType::physical_read_duration(double ratio) const noexcept { return req.physical_read_duration(ratio); }


double TokenRequestValueType::physical_write_duration(double ratio) const noexcept { return req.physical_write_duration(ratio); }


bool TokenRequestValueType::in_range(double time) const noexcept { return req.in_range({int64_t(time)}); }


double TokenRequestValueType::to_physical_time_in_tick(double time, double ratio) const noexcept { return req.to_physical_time_in_tick(time, ratio); }


double TokenRequestValueType::from_physical_time_in_tick(double time, double ratio) const noexcept { return req.from_physical_time_in_tick(time, ratio).impl; }


double TokenRequestValueType::position() const noexcept { return req.position(); }


bool TokenRequestValueType::forward() const noexcept { return req.forward(); }


bool TokenRequestValueType::backward() const noexcept { return req.backward(); }


bool TokenRequestValueType::paused() const noexcept { return req.paused(); }


double TokenRequestValueType::get_quantification_date(double ratio) const noexcept
{
  if(auto res = req.get_quantification_date(ratio))
    return res->impl;
  return -1;
}


double TokenRequestValueType::get_physical_quantification_date(double rate, double ratio) const noexcept
{
  if(auto res = req.get_physical_quantification_date(rate, ratio))
    return *res;
  return -1;
}


void TokenRequestValueType::reduce_end_time(double time) noexcept { return req.reduce_end_time({int64_t(time)}); }


void TokenRequestValueType::increase_start_time(double time) noexcept { return req.increase_start_time({int64_t(time)}); }


bool TokenRequestValueType::unexpected_bar_change() const noexcept { return req.unexpected_bar_change(); }

int ExecutionStateValueType::sample_rate() const noexcept { return req.sampleRate(); }

int ExecutionStateValueType::buffer_size() const noexcept { return req.bufferSize(); }

double ExecutionStateValueType::model_to_physical() const noexcept { return req.modelToSamples(); }

double ExecutionStateValueType::physical_to_model() const noexcept { return req.samplesToModel(); }

double ExecutionStateValueType::physical_date() const noexcept { return req.samplesSinceStart(); }

double ExecutionStateValueType::start_date_ns() const noexcept { return req.startDate(); }

double ExecutionStateValueType::current_date_ns() const noexcept { return req.currentDate(); }

}

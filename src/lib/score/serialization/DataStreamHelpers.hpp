#pragma once
/*


#include <QQmlListProperty>

#include <sys/types.h>

#include <string>
#include <vector>
#include <verdigris>


*/
#include <score/tools/Debug.hpp>
#include <score_lib_base_export.h>

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDataStream>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QRect>
#include <QRectF>
#include <QPointer>
#include <QVector>
#include <QList>
#include <QMap>
#include <QColor>
#include <QVariant>

#include <string>
#include <memory>
#include <type_traits>

template<typename T> struct is_shared_ptr : std::false_type {};
template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};
template<typename T>
static constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;
template<typename T> struct is_qpointer : std::false_type {};
template<typename T> struct is_qpointer<QPointer<T>> : std::true_type {};
template<typename T>
static constexpr bool is_qpointer_v = is_qpointer<T>::value;

/*
template<typename T> struct is_qqmllistproperty: std::false_type {};
template<typename T> struct is_qqmllistproperty<QQmlListProperty<T>> : std::true_type {};
template<typename T>
static constexpr bool is_qqmllistproperty_v = is_qqmllistproperty<T>::value;

template<typename T> struct is_qmap: std::false_type {};
template<typename K, typename V> struct is_qmap<QMap<K, V>> : std::true_type {};
template<typename T> static constexpr bool is_qmap_v = is_qmap<T>::value;

class QSslError;
class QModelIndex;
class QMimeData;
class QItemSelection;
class QItemSelectionRange;
class QLinearGradient;

template<typename T>
static constexpr bool is_datastream_serializable =
       !std::is_arithmetic<T>::value
    && !std::is_enum<T>::value
    && !std::is_same<T, QStringList>::value
    && !std::is_pointer<T>::value
    && !std::is_same<T, std::string>::value
    && !std::is_same<T, QPointF>::value
    && !std::is_same<T, QSizeF>::value
    && !is_qmap_v<T>
  #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    && !std::is_same<T, QIterable<QMetaSequence>>::value
    && !std::is_same<T, QIterable<QMetaAssociation>>::value
    && !std::is_same<T, QModelIndex>::value
    && !std::is_same<T, QMimeData>::value
    && !std::is_same<T, QSslError>::value
    && !std::is_same<T, QItemSelection>::value
    && !std::is_same<T, QItemSelectionRange>::value
    && !std::is_same<T, QLinearGradient>::value
    && !std::is_same<T, QtMetaTypePrivate::QPairVariantInterfaceImpl>::value
    && !is_shared_ptr_v<T>
    && !is_qpointer_v<T>
    && !is_qqmllistproperty_v<T>
  #endif
;
*/
#if defined(SCORE_DEBUG_DELIMITERS)
#define SCORE_DEBUG_INSERT_DELIMITER \
  do                                 \
  {                                  \
    insertDelimiter();               \
  } while (0)
#define SCORE_DEBUG_INSERT_DELIMITER2(Vis) \
  do                                       \
  {                                        \
    Vis.insertDelimiter();                 \
  } while (0)
#define SCORE_DEBUG_CHECK_DELIMITER \
  do                                \
  {                                 \
    checkDelimiter();               \
  } while (0)
#define SCORE_DEBUG_CHECK_DELIMITER2(Vis) \
  do                                      \
  {                                       \
    Vis.checkDelimiter();                 \
  } while (0)
#else
#define SCORE_DEBUG_INSERT_DELIMITER
#define SCORE_DEBUG_INSERT_DELIMITER2(Vis)
#define SCORE_DEBUG_CHECK_DELIMITER
#define SCORE_DEBUG_CHECK_DELIMITER2(Vis)
#endif

/**
 * \file DataStreamVisitor.hpp
 *
 * This file contains facilities
 * to serialize an object using QDataStream.
 *
 * Generally, it is used with QByteArrays, but it works with any QIODevice.
 */
template <typename T>
using enable_if_QDataStreamSerializable = typename std::enable_if_t<
    std::is_arithmetic<T>::value || std::is_same<T, QStringList>::value
    || std::is_same<T, QVector2D>::value || std::is_same<T, QVector3D>::value
    || std::is_same<T, QVector4D>::value || std::is_same<T, QPointF>::value
    || std::is_same<T, QPoint>::value || std::is_same<T, std::string>::value>;

template <class, class Enable = void>
struct is_QDataStreamSerializable : std::false_type
{
};

template <class T>
struct is_QDataStreamSerializable<
    T,
    enable_if_QDataStreamSerializable<typename std::decay<T>::type>> : std::true_type
{
};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
SCORE_LIB_BASE_EXPORT QDataStream& operator<<(QDataStream& s, char c);
SCORE_LIB_BASE_EXPORT QDataStream& operator>>(QDataStream& s, char& c);
#endif

SCORE_LIB_BASE_EXPORT QDataStream& operator<<(QDataStream& stream, const std::string& obj);
SCORE_LIB_BASE_EXPORT QDataStream& operator>>(QDataStream& stream, std::string& obj);

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
template <typename T>
typename std::enable_if<std::is_enum<T>::value, QDataStream&>::type&
operator<<(QDataStream& s, const T& t)
{
  return s << static_cast<typename std::underlying_type<T>::type>(t);
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, QDataStream&>::type&
operator>>(QDataStream& s, T& t)
{
  return s >> reinterpret_cast<typename std::underlying_type<T>::type&>(t);
}
#endif

class QIODevice;

struct DataStreamInput
{
  QDataStream& stream;
};
struct DataStreamOutput
{
  QDataStream& stream;
};

#if (INTPTR_MAX == INT64_MAX) && !defined(__APPLE__) && !defined(_WIN32)
inline QDataStream& operator<<(QDataStream& s, uint64_t val)
{
  s << (quint64)val;
  return s;
}
inline QDataStream& operator>>(QDataStream& s, uint64_t& val)
{
  s >> (quint64&)val;
  return s;
}
inline QDataStream& operator<<(QDataStream& s, int64_t val)
{
  s << (qint64)val;
  return s;
}
inline QDataStream& operator>>(QDataStream& s, int64_t& val)
{
  s >> (qint64&)val;
  return s;
}
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
inline QDataStream& operator<<(QDataStream& s, std::size_t val)
{
  s << (quint64)val;
  return s;
}
inline QDataStream& operator>>(QDataStream& s, std::size_t& val)
{
  s >> (quint64&)val;
  return s;
}
#endif
#endif




template <typename T, std::enable_if_t<!std::is_enum_v<T>>* = nullptr>
DataStreamInput& operator<<(DataStreamInput& s, const T& obj);

template <typename T, std::enable_if_t<!std::is_enum_v<T>>* = nullptr>
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj);

/*
template <
    typename T,
    std::enable_if_t<!is_datastream_serializable<T>>*
    = nullptr>
DataStreamInput& operator<<(DataStreamInput& s, const T& obj);
template <
    typename T,
    std::enable_if_t<!is_datastream_serializable<T>>*
    = nullptr>
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj);
*/

#define DATASTREAM_QT_BUILTIN(T)                              \
OSSIA_INLINE                                                  \
DataStreamInput& operator<<(DataStreamInput& s, const T& obj) \
{ s.stream << obj; return s; }                                \
OSSIA_INLINE                                                  \
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj)     \
{ s.stream >> obj; return s;}
#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define TO_TEMPLATE_ARGS8(M1, T1, M2, T2, M3, T3, M4, T4) M1 T1, M2 T2, M3 T3, M4 T4
#define TO_TEMPLATE_ARGS6(M1, T1, M2, T2, M3, T3) M1 T1, M2 T2, M3 T3
#define TO_TEMPLATE_ARGS4(M1, T1, M2, T2) M1 T1, M2 T2
#define TO_TEMPLATE_ARGS2(M1, T1) M1 T1

#define TO_TEMPLATE_ARGS(...) \
  GET_MACRO(__VA_ARGS__,  TO_TEMPLATE_ARGS8,  TO_TEMPLATE_ARGS7,  TO_TEMPLATE_ARGS6,  TO_TEMPLATE_ARGS5, TO_TEMPLATE_ARGS4, TO_TEMPLATE_ARGS3, TO_TEMPLATE_ARGS2)(__VA_ARGS__)

#define TO_TEMPLATE_USES8(M1, T1, M2, T2, M3, T3, M4, T4) T1, T2, T3, T4
#define TO_TEMPLATE_USES6(M1, T1, M2, T2, M3, T3) T1, T2, T3
#define TO_TEMPLATE_USES4(M1, T1, M2, T2) T1, T2
#define TO_TEMPLATE_USES2(M1, T1) T1

#define TO_TEMPLATE_USES(...) \
  GET_MACRO(__VA_ARGS__,  TO_TEMPLATE_USES8,  TO_TEMPLATE_USES7,  TO_TEMPLATE_USES6,  TO_TEMPLATE_USES5, TO_TEMPLATE_USES4, TO_TEMPLATE_USES3, TO_TEMPLATE_USES2)(__VA_ARGS__)

#define DATASTREAM_QT_BUILTIN_T(T, ...) \
  template<TO_TEMPLATE_ARGS(__VA_ARGS__)> OSSIA_INLINE                               \
  DataStreamInput& operator<<(DataStreamInput& s, const T<TO_TEMPLATE_USES(__VA_ARGS__)>& obj) \
{return s; }                                  \
  template<TO_TEMPLATE_ARGS(__VA_ARGS__)> OSSIA_INLINE                               \
  DataStreamOutput& operator>>(DataStreamOutput& s, T<TO_TEMPLATE_USES(__VA_ARGS__)>& obj)     \
{ return s;}


DATASTREAM_QT_BUILTIN(bool)
DATASTREAM_QT_BUILTIN(char)
DATASTREAM_QT_BUILTIN(qint8)
DATASTREAM_QT_BUILTIN(qint16)
DATASTREAM_QT_BUILTIN(qint32)
DATASTREAM_QT_BUILTIN(qint64)
DATASTREAM_QT_BUILTIN(int64_t)
DATASTREAM_QT_BUILTIN(quint8)
DATASTREAM_QT_BUILTIN(quint16)
DATASTREAM_QT_BUILTIN(quint32)
DATASTREAM_QT_BUILTIN(quint64)
DATASTREAM_QT_BUILTIN(float)
DATASTREAM_QT_BUILTIN(double)
DATASTREAM_QT_BUILTIN(QString)
DATASTREAM_QT_BUILTIN(QStringList)
DATASTREAM_QT_BUILTIN(QPoint)
DATASTREAM_QT_BUILTIN(QPointF)
DATASTREAM_QT_BUILTIN(QRect)
DATASTREAM_QT_BUILTIN(QRectF)
DATASTREAM_QT_BUILTIN(QVector2D)
DATASTREAM_QT_BUILTIN(QVector3D)
DATASTREAM_QT_BUILTIN(QVector4D)
DATASTREAM_QT_BUILTIN(QByteArray)
DATASTREAM_QT_BUILTIN(QSizeF)
DATASTREAM_QT_BUILTIN(QColor)
DATASTREAM_QT_BUILTIN(QVariant)
DATASTREAM_QT_BUILTIN(std::string)

DATASTREAM_QT_BUILTIN_T(QList, typename, T)

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
DATASTREAM_QT_BUILTIN_T(QVector, typename, T)
#endif
DATASTREAM_QT_BUILTIN_T(QMap, typename, K, typename, V)



template<typename T, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
OSSIA_INLINE
DataStreamInput& operator<<(DataStreamInput& s, const T& obj)
{ s.stream << obj; return s; }

template<typename T, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
OSSIA_INLINE
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj)
{ s.stream >> obj; return s;}

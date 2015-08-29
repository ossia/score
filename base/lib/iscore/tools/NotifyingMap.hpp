#pragma once
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <utility>
#include <array>
#include <iostream>


#define QT_MOC_LITERAL_CUSTOM(idx, ofs, len) Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, qptrdiff(offsetof(qt_meta_stringdata_NotifyingMap_t, stringdata0) + ofs - idx * sizeof(QByteArrayData)))

template <typename T, size_t n>
constexpr size_t array_length(const T (&)[n])
{ return n; }

template <typename T>
constexpr size_t my_string_length(T&& t)
{ return array_length(std::forward<T&&>(t)) - 1; }

template<typename T>
constexpr bool string_empty(T&& t)
{ return string_length(std::forward<T&&>(t)) == 0; }

// Found on stackoverflow: http://stackoverflow.com/a/28708885/1495627
template<unsigned...>struct seq{using type=seq;};
template<unsigned N, unsigned... Is>
struct gen_seq_x : gen_seq_x<N-1, N-1, Is...>{};
template<unsigned... Is>
struct gen_seq_x<0, Is...> : seq<Is...>{};
template<unsigned N>
using gen_seq=typename gen_seq_x<N>::type;

template<size_t S>
using size=std::integral_constant<size_t, S>;

template<class T, size_t N>
constexpr size<N> length( T const(&)[N] ) { return {}; }
template<class T, size_t N>
constexpr size<N> length( std::array<T, N> const& ) { return {}; }

template<class T>
using length_t = decltype(length(std::declval<T>()));

constexpr size_t string_size() { return 0; }
template<class...Ts>
constexpr size_t string_size( size_t i, Ts... ts ) {
  return (i?i-1:0) + string_size(ts...);
}
template<class...Ts>
using string_length=size< string_size( length_t<Ts>{}... )>;

template<class...Ts>
using combined_string = std::array<char, string_length<Ts...>{}+1>;

template<class Lhs, class Rhs, unsigned...I1, unsigned...I2>
constexpr const combined_string<Lhs,Rhs>
concat_impl( Lhs const& lhs, Rhs const& rhs, seq<I1...>, seq<I2...>)
{
    return {{ lhs[I1]..., rhs[I2]..., '\0' }};
}

template<class Lhs, class Rhs>
constexpr const combined_string<Lhs,Rhs>
concat(Lhs const& lhs, Rhs const& rhs)
{
    return concat_impl(lhs, rhs, gen_seq<string_length<Lhs>{}>{}, gen_seq<string_length<Rhs>{}>{});
}

template<class T0, class T1, class... Ts>
constexpr const combined_string<T0, T1, Ts...>
concat(T0 const&t0, T1 const&t1, Ts const&...ts)
{
    return concat(t0, concat(t1, ts...));
}

template<class T>
constexpr const combined_string<T>
concat(T const&t) {
    return concat(t, "");
}
constexpr const combined_string<>
concat() {
    return concat("");
}



// The parent of the childs are the parents of the map.
// Hence the objects shall not be deleted upon deletion of the map
// to prevent a double-free.
template<typename T>
class NotifyingMap : public QObject
{
        // Note : T requires a constexpr const char[] className member .
        static constexpr auto typeLength()
        { return my_string_length(T::className); }
    public:
        QT_WARNING_PUSH
        Q_OBJECT_NO_OVERRIDE_WARNING

        static constexpr const struct qt_meta_stringdata_NotifyingMap_t
        {
            QByteArrayData data[5]{
                QT_MOC_LITERAL_CUSTOM(0, 0, 12), // "NotifyingMap"
                QT_MOC_LITERAL_CUSTOM(1, 13, 5), // "added"
                QT_MOC_LITERAL_CUSTOM(2, 19, 0), // ""
                QT_MOC_LITERAL_CUSTOM(3, 20, 1 + typeLength()), // "T"
                QT_MOC_LITERAL_CUSTOM(4, 22 + typeLength(), 7) // "removed"
            };
            std::array<char, 29 + typeLength()> stringdata0 =
                    concat("NotifyingMap\0added\0\0", T::className, "\0removed");
        } staticStringData{};

        static const constexpr uint staticMetaData[] = {
         // content:
               7,       // revision
               0,       // classname
               0,    0, // classinfo
               2,   14, // methods
               0,    0, // properties
               0,    0, // enums/sets
               0,    0, // constructors
               0,       // flags
               2,       // signalCount

         // signals: name, argc, parameters, tag, flags
               1,    1,   24,    2, 0x06 /* Public */,
               4,    1,   27,    2, 0x06 /* Public */,

         // signals: parameters
            QMetaType::Void, 0x80000000 | 3,    2,
            QMetaType::Void, 0x80000000 | 3,    2,

               0        // eod
        };

        virtual const QMetaObject *metaObject() const
        {
            return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
        }

        virtual void *qt_metacast(const char * _clname)
        {
            if (!_clname) return Q_NULLPTR;
            if (!strcmp(_clname, staticStringData.stringdata0.data()))
                return static_cast<void*>(const_cast< NotifyingMap*>(this));
            return QObject::qt_metacast(_clname);
        }

        virtual int qt_metacall(QMetaObject::Call _c, int _id, void ** _a)
        {
            _id = QObject::qt_metacall(_c, _id, _a);
            if (_id < 0)
                return _id;
            if (_c == QMetaObject::InvokeMetaMethod) {
                if (_id < 2)
                    qt_static_metacall(this, _c, _id, _a);
                _id -= 2;
            } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
                if (_id < 2)
                    *reinterpret_cast<int*>(_a[0]) = -1;
                _id -= 2;
            }
            return _id;
        }

        QT_WARNING_POP

    private:
        Q_DECL_HIDDEN_STATIC_METACALL static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void ** _a)
        {
            if (_c == QMetaObject::InvokeMetaMethod) {
                NotifyingMap *_t = static_cast<NotifyingMap *>(_o);
                Q_UNUSED(_t)
                switch (_id) {
                case 0: _t->added((*reinterpret_cast< const T(*)>(_a[1]))); break;
                case 1: _t->removed((*reinterpret_cast< const T(*)>(_a[1]))); break;
                default: ;
                }
            } else if (_c == QMetaObject::IndexOfMethod) {
                int *result = reinterpret_cast<int *>(_a[0]);
                void **func = reinterpret_cast<void **>(_a[1]);
                {
                    typedef void (NotifyingMap::*_t)(const T & );
                    if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&NotifyingMap::added)) {
                        *result = 0;
                    }
                }
                {
                    typedef void (NotifyingMap::*_t)(const T & );
                    if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&NotifyingMap::removed)) {
                        *result = 1;
                    }
                }
            }
        }


        struct QPrivateSignal {};

    public:
        static const constexpr QMetaObject staticMetaObject{
            { &QObject::staticMetaObject,
              staticStringData.data,
              staticMetaData,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
        };

    public:
        // The real interface starts here
        using value_type = T;
        auto begin() const { return m_map.begin(); }
        auto cbegin() const { return m_map.cbegin(); }
        auto end() const { return m_map.end(); }
        auto cend() const { return m_map.cend(); }

        void add(T* t) {
            m_map.insert(t);
            emit added(*t);
        }

        void remove(T* elt) {
            m_map.remove(elt->id());
            emit removed(*elt);
            delete elt;
        }
        void remove(const Id<T>& id) {
            auto elt = *m_map.get().find(id);

            m_map.remove(id);
            emit removed(*elt);
            delete elt;
        }

        auto size() const { return m_map.size(); }
        bool empty() const { return m_map.empty(); }
        const auto& map() const { return m_map; }
        const auto& get() const { return m_map.get(); }
        auto& at(const Id<T>& id) { return m_map.at(id); }
        auto& at(const Id<T>& id) const { return m_map.at(id); }
        auto find(const Id<T>& id) const { return m_map.find(id); }

        // signals:
        void added(const T& _t1)
        {
            void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
            QMetaObject::activate(this, &staticMetaObject, 0, _a);
        }

        void removed(const T& _t1)
        {
            void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
            QMetaObject::activate(this, &staticMetaObject, 1, _a);
        }

    private:
        IdContainer<T> m_map;
};


template<typename T>
constexpr const QMetaObject NotifyingMap<T>::staticMetaObject;
template<typename T>
constexpr const uint NotifyingMap<T>::staticMetaData[];
template<typename T>
constexpr const typename NotifyingMap<T>::qt_meta_stringdata_NotifyingMap_t NotifyingMap<T>::staticStringData;

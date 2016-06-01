#pragma once
#include <QtGlobal>

#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
template<typename T>
struct QMapKeyAdaptor
{
        QList<typename T::key_type> keys;
        QMapKeyAdaptor(const T& map):
            keys{map.keys()}
        {

        }

        auto begin() const
        {
            return keys.begin();
        }

        auto end() const
        {
            return keys.end();
        }
};
#else
template<typename T>
struct QMapKeyAdaptor
{
        const T& map;

        auto begin() const
        {
            return map.keyBegin();
        }

        auto end() const
        {
            return map.keyEnd();
        }
};
#endif

template<typename T>
auto QMap_keys(const T& t)
{
    return QMapKeyAdaptor<T>{t};
}

#include "Skin.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <boost/assign/list_of.hpp>

// Taken from http://stackoverflow.com/a/31841462
template <typename L, typename R>
boost::bimap<L, R>
make_bimap(std::initializer_list<typename boost::bimap<L, R>::value_type> list)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

#define ISCORE_INSERT_COLOR(Col) {#Col, &Col}
Skin::Skin():
    SansFont{"Ubuntu"},
    MonoFont{"APCCourier-Bold", 8},
    m_colorMap(make_bimap<QString, const QColor*>({
        ISCORE_INSERT_COLOR(Dark),
        ISCORE_INSERT_COLOR(HalfDark),
        ISCORE_INSERT_COLOR(Gray),
        ISCORE_INSERT_COLOR(HalfLight),
        ISCORE_INSERT_COLOR(Light),
        ISCORE_INSERT_COLOR(Emphasis1),
        ISCORE_INSERT_COLOR(Emphasis2),
        ISCORE_INSERT_COLOR(Emphasis3),
        ISCORE_INSERT_COLOR(Emphasis4),
        ISCORE_INSERT_COLOR(Base1),
        ISCORE_INSERT_COLOR(Base2),
        ISCORE_INSERT_COLOR(Base3),
        ISCORE_INSERT_COLOR(Base4),
        ISCORE_INSERT_COLOR(Warn1),
        ISCORE_INSERT_COLOR(Warn2),
        ISCORE_INSERT_COLOR(Warn3),
        ISCORE_INSERT_COLOR(Background1),
        ISCORE_INSERT_COLOR(Transparent1),
        ISCORE_INSERT_COLOR(Transparent2),
        ISCORE_INSERT_COLOR(Transparent3),
        ISCORE_INSERT_COLOR(Smooth1),
        ISCORE_INSERT_COLOR(Smooth2),
        ISCORE_INSERT_COLOR(Smooth3),
        ISCORE_INSERT_COLOR(Tender1),
        ISCORE_INSERT_COLOR(Tender2),
        ISCORE_INSERT_COLOR(Tender3)
        }))
{
}

Skin& Skin::instance()
{
    static Skin s;
    return s;
}

#define ISCORE_CONVERT_COLOR(Col) do { fromColor(#Col, Col); } while(0)
void Skin::load(const QJsonObject& obj)
{
    auto fromColor = [&] (const QString& key, QColor& col) {
        auto arr = obj[key].toArray();
        if(arr.size() == 3)
            col = QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
        else if(arr.size() == 4)
            col = QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt());
    };

    ISCORE_CONVERT_COLOR(Dark);
    ISCORE_CONVERT_COLOR(HalfDark);
    ISCORE_CONVERT_COLOR(Gray);
    ISCORE_CONVERT_COLOR(HalfLight);
    ISCORE_CONVERT_COLOR(Light);

    ISCORE_CONVERT_COLOR(Emphasis1);
    ISCORE_CONVERT_COLOR(Emphasis2);
    ISCORE_CONVERT_COLOR(Emphasis3);
    ISCORE_CONVERT_COLOR(Emphasis4);

    ISCORE_CONVERT_COLOR(Base1);
    ISCORE_CONVERT_COLOR(Base2);
    ISCORE_CONVERT_COLOR(Base3);
    ISCORE_CONVERT_COLOR(Base4);

    ISCORE_CONVERT_COLOR(Warn1);
    ISCORE_CONVERT_COLOR(Warn2);
    ISCORE_CONVERT_COLOR(Warn3);

    ISCORE_CONVERT_COLOR(Background1);

    ISCORE_CONVERT_COLOR(Transparent1);
    ISCORE_CONVERT_COLOR(Transparent2);
    ISCORE_CONVERT_COLOR(Transparent3);

    ISCORE_CONVERT_COLOR(Smooth1);
    ISCORE_CONVERT_COLOR(Smooth2);
    ISCORE_CONVERT_COLOR(Smooth3);

    ISCORE_CONVERT_COLOR(Tender1);
    ISCORE_CONVERT_COLOR(Tender2);
    ISCORE_CONVERT_COLOR(Tender3);

    emit changed();
}

#define ISCORE_MAKE_PAIR_COLOR(Col) vec.push_back(qMakePair(Col, QStringLiteral(#Col)));
QVector<QPair<QColor,QString>> Skin::getColors() const
{
    QVector<QPair<QColor,QString>> vec;
    vec.reserve(26);

    ISCORE_MAKE_PAIR_COLOR(Dark);
    ISCORE_MAKE_PAIR_COLOR(HalfDark);
    ISCORE_MAKE_PAIR_COLOR(Gray);
    ISCORE_MAKE_PAIR_COLOR(HalfLight);
    ISCORE_MAKE_PAIR_COLOR(Light);
    ISCORE_MAKE_PAIR_COLOR(Emphasis1);
    ISCORE_MAKE_PAIR_COLOR(Emphasis2);
    ISCORE_MAKE_PAIR_COLOR(Emphasis3);
    ISCORE_MAKE_PAIR_COLOR(Emphasis4);
    ISCORE_MAKE_PAIR_COLOR(Base1);
    ISCORE_MAKE_PAIR_COLOR(Base2);
    ISCORE_MAKE_PAIR_COLOR(Base3);
    ISCORE_MAKE_PAIR_COLOR(Base4);
    ISCORE_MAKE_PAIR_COLOR(Warn1);
    ISCORE_MAKE_PAIR_COLOR(Warn2);
    ISCORE_MAKE_PAIR_COLOR(Warn3);
    ISCORE_MAKE_PAIR_COLOR(Background1);
    ISCORE_MAKE_PAIR_COLOR(Transparent1);
    ISCORE_MAKE_PAIR_COLOR(Transparent2);
    ISCORE_MAKE_PAIR_COLOR(Transparent3);
    ISCORE_MAKE_PAIR_COLOR(Smooth1);
    ISCORE_MAKE_PAIR_COLOR(Smooth2);
    ISCORE_MAKE_PAIR_COLOR(Smooth3);
    ISCORE_MAKE_PAIR_COLOR(Tender1);
    ISCORE_MAKE_PAIR_COLOR(Tender2);
    ISCORE_MAKE_PAIR_COLOR(Tender3);

    return vec;
}


const QColor* Skin::fromString(const QString &s) const
{
    auto it = m_colorMap.left.find(s);
    return it != m_colorMap.left.end() ? it->second : nullptr;
}

QString Skin::toString(const QColor* c) const
{
    auto it = m_colorMap.right.find(c);
    return it != m_colorMap.right.end() ? it->second : nullptr;
}

#undef ISCORE_INSERT_COLOR
#undef ISCORE_CONVERT_COLOR
#undef ISCORE_MAKE_PAIR_COLOR

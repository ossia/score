#include "Skin.hpp"
#include <QJsonObject>

Skin::Skin():
    SansFont{"Ubuntu"},
    MonoFont{"APCCourier-Bold", 8}
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

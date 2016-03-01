#pragma once
#include <iscore_lib_process_export.h>
#include <QFont>
#include <QColor>

struct ISCORE_LIB_PROCESS_EXPORT Skin
{
        static Skin& instance();

        void load(const QJsonObject& style);

        QFont SansFont;
        QFont MonoFont;

        QColor Dark{Qt::black};
        QColor HalfDark{Qt::darkGray};
        QColor Gray{Qt::gray};
        QColor HalfLight{Qt::lightGray};
        QColor Light{Qt::white};

        QColor Emphasis1{Qt::cyan};
        QColor Emphasis2{233, 208, 89};
        QColor Emphasis3{179, 90, 209};

        QColor Base1{3, 195, 221};
        QColor Base2{3, 150, 250};
        QColor Base3{34, 224, 0};
        QColor Base4{179, 179, 179};

        QColor Warn1{Qt::yellow};
        QColor Warn2{200,150,0};
        QColor Warn3{Qt::red};

        QColor Background1{37, 41, 48};

        QColor Transparent1{0, 127, 229, 76};
        QColor Transparent2{170, 170, 170, 70};
        QColor Transparent3{37, 41, 48, 40};

        QColor Smooth1{222, 0, 0};
        QColor Smooth2{109,222,0};
        QColor Smooth3{240, 220, 0};

        QColor Tender1{199, 31, 44};
        QColor Tender2{216, 178, 24};
        QColor Tender3{128, 215, 62};

    private:
        Skin();

};

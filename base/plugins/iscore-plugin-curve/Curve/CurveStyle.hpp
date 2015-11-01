#pragma once
#include <QColor>

struct CurveStyle
{
        QColor Point{128, 215, 62};
        QColor PointSelected{233, 208, 89};

        QColor Segment{199, 31, 44};
        QColor SegmentSelected{216, 178, 24};
        QColor SegmentDisabled{127, 127, 127};
};

#include "CurveStyle.hpp"

CurveStyle& CurveStyle::instance()
{
    static CurveStyle s;
    return s;
}

#include "ProcessFonts.hpp"

QFont ProcessFonts::Sans()
{
    static const QFont f("Ubuntu");
    return f;
}

QFont ProcessFonts::Mono()
{
    static const QFont f("APCCourier-Bold", 8);
    return f;
}

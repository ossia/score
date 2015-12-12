#include "ProcessFonts.hpp"

#if defined(ISCORE_IEEE_SKIN)
QFont ProcessFonts::Sans()
{
    static const QFont f("Arial", 10);
    return f;
}

QFont ProcessFonts::Mono()
{
    static const QFont f("Courier New", 8);
    return f;
}
#else
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
#endif

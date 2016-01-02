#include "ProcessFonts.hpp"

namespace Process
{
namespace Fonts
{

#if defined(ISCORE_IEEE_SKIN)
QFont Sans()
{
    static const QFont f("Arial", 10);
    return f;
}

QFont Mono()
{
    static const QFont f("Courier New", 8);
    return f;
}
#else
QFont Sans()
{
    static const QFont f("Ubuntu");
    return f;
}

QFont Mono()
{
    static const QFont f("APCCourier-Bold", 8);
    return f;
}
#endif

}
}

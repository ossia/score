#include "Skin.hpp"

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

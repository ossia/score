#pragma once
#include <QString>
#include <map>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <iscore_lib_base_export.h>

namespace iscore
{
class Menu;
struct Menus
{
        static StringKey<Menu> File() { return StringKey<Menu>{"File"}; }
        static StringKey<Menu> Export() { return StringKey<Menu>{"Export"}; }
        static StringKey<Menu> Edit() { return StringKey<Menu>{"Edit"}; }
        static StringKey<Menu> Object() { return StringKey<Menu>{"Object"}; }
        static StringKey<Menu> Play() { return StringKey<Menu>{"Play"}; }
        static StringKey<Menu> Tool() { return StringKey<Menu>{"Tool"}; }
        static StringKey<Menu> View() { return StringKey<Menu>{"View"}; }
        static StringKey<Menu> Windows() { return StringKey<Menu>{"Windows"}; }
        static StringKey<Menu> Settings() { return StringKey<Menu>{"Settings"}; }
        static StringKey<Menu> About() { return StringKey<Menu>{"About"}; }
};
}

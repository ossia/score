#include "ColorReference.hpp"
#include <iscore/tools/Todo.hpp>

namespace iscore
{
optional<ColorRef> ColorRef::ColorFromString(const QString & txt)
{
    auto res = iscore::Skin::instance().fromString(txt);;
    if(res)
        return ColorRef(res);
    else
        return {};
}

optional<ColorRef> ColorRef::SimilarColor(QColor other)
{
    ISCORE_TODO_("Load similar colors");
    return {};
}
}

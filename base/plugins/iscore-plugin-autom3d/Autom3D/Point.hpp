#pragma once
#include <QVector3D>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
namespace Autom3D
{
using Point = QVector3D;
inline State::tuple_t toTuple(Point p)
{
    return {
        State::ValueImpl{p.x()},
        State::ValueImpl{p.y()},
        State::ValueImpl{p.z()}
    };
}

inline Point fromTuple(State::tuple_t t)
{
    if(t.size() >= 3)
    {
    return {
        State::convert::value<float>(t[0]),
        State::convert::value<float>(t[1]),
        State::convert::value<float>(t[2])
    };
    }
    else
    {
        return {0., 0., 0.};
    }
}
}

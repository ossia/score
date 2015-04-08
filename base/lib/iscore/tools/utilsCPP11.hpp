#pragma once
#include <algorithm>
template <typename Vector, typename Functor>
void vec_erase_remove_if(Vector& v, Functor&& f)
{
    v.erase(std::remove_if(std::begin(v), std::end(v), f), std::end(v));
}

#include <QByteArray>
#include <QDataStream>
template<typename InputVector, typename OutputVector>
void serializeVectorOfPointers(const InputVector& in, OutputVector& out)
{
    for(const auto& elt : in)
    {
        QByteArray arr;

        QDataStream s(&arr, QIODevice::WriteOnly);
        s.setVersion(QDataStream::Qt_5_3);
        s << *elt;

        out.push_back(arr);
    }
}

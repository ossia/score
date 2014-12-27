#pragma once
#include "interface/serialization/DataStreamVisitor.hpp"

template<typename T>
class NamedType;

using NamedObject = NamedType<QObject>;

template<>
void Visitor<Reader<DataStream>>::visit<NamedObject>(NamedObject&);
template<>
void Visitor<Writer<DataStream>>::visit<NamedObject>(NamedObject&);

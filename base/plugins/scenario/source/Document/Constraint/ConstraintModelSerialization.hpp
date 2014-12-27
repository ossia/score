#pragma once
class ConstraintModel;
#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit<ConstraintModel>(ConstraintModel&);
template<>
void Visitor<Writer<DataStream>>::visit<ConstraintModel>(ConstraintModel&);

QDataStream& operator <<(QDataStream& s, const ConstraintModel& constraint);
QDataStream& operator >>(QDataStream& s, ConstraintModel& constraint);
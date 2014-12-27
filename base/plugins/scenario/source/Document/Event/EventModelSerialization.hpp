#pragma once
class EventModel;

#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit<EventModel>(EventModel&);
template<>
void Visitor<Writer<DataStream>>::visit<EventModel>(EventModel&);

QDataStream& operator <<(QDataStream& s, const EventModel& event);
QDataStream& operator >>(QDataStream& s, EventModel& event);
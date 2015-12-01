#include <boost/optional/optional.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include "CommentBlockModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<>
void Visitor<Reader<DataStream>>::readFrom(const CommentBlockModel& comment)
{

}

template<>
void Visitor<Writer<DataStream>>::writeTo(CommentBlockModel& comment)
{

}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const CommentBlockModel& comment)
{

}

template<>
void Visitor<Writer<JSONObject>>::writeTo(CommentBlockModel& comment)
{

}

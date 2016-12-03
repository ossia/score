#include <QJsonObject>
#include <algorithm>

#include "JSONVisitor.hpp"

Visitor<Reader<JSONObject>>::Visitor()
    : components{iscore::AppComponents()}, strings{iscore::StringConstant()}
{
}

Visitor<Writer<JSONObject>>::Visitor()
    : components{iscore::AppComponents()}, strings{iscore::StringConstant()}
{
}

Visitor<Writer<JSONObject>>::Visitor(const QJsonObject& obj)
    : m_obj{obj}
    , components{iscore::AppComponents()}
    , strings{iscore::StringConstant()}
{
}

Visitor<Writer<JSONObject>>::Visitor(QJsonObject&& obj)
    : m_obj{std::move(obj)}
    , components{iscore::AppComponents()}
    , strings{iscore::StringConstant()}
{
}

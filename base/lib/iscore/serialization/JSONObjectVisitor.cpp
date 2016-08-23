#include <QJsonObject>
#include <algorithm>

#include "JSONVisitor.hpp"

Visitor<Reader<JSONObject>>::Visitor():
    context{iscore::AppContext()},
    strings{iscore::StringConstant()}
{

}

Visitor<Writer<JSONObject>>::Visitor():
    context{iscore::AppContext()},
    strings{iscore::StringConstant()}
{

}

Visitor<Writer<JSONObject>>::Visitor(const QJsonObject& obj):
    m_obj{obj},
    context{iscore::AppContext()},
    strings{iscore::StringConstant()}
{

}

Visitor<Writer<JSONObject>>::Visitor (QJsonObject&& obj):
    m_obj{std::move(obj)},
    context{iscore::AppContext()},
    strings{iscore::StringConstant()}
{

}

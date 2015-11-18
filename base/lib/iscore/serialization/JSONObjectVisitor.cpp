#include "JSONVisitor.hpp"
#include <core/application/Application.hpp>
Visitor<Reader<JSONObject>>::Visitor():
    context{iscore::Application::instance()}
{

}

Visitor<Writer<JSONObject>>::Visitor():
    context{iscore::Application::instance()}
{

}

Visitor<Writer<JSONObject>>::Visitor(const QJsonObject& obj):
    m_obj{obj},
    context{iscore::Application::instance()}
{

}

Visitor<Writer<JSONObject>>::Visitor (QJsonObject&& obj):
    m_obj{std::move(obj)},
    context{iscore::Application::instance()}
{

}

#include <core/application/Application.hpp>
#include <qjsonobject.h>
#include <algorithm>

#include "JSONVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

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

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <algorithm>

#include "JSONVisitor.hpp"

JSONObjectReader::JSONObjectReader()
    : components{iscore::AppComponents()}, strings{iscore::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter()
    : components{iscore::AppComponents()}, strings{iscore::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter(const QJsonObject& o)
    : obj{o}
    , components{iscore::AppComponents()}
    , strings{iscore::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter(QJsonObject&& o)
    : obj{std::move(o)}
    , components{iscore::AppComponents()}
    , strings{iscore::StringConstant()}
{
}

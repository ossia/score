// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSONVisitor.hpp"

#include <QJsonObject>

JSONObjectReader::JSONObjectReader()
    : components{score::AppComponents()}, strings{score::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter()
    : components{score::AppComponents()}, strings{score::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter(const QJsonObject& o)
    : obj{o}
    , components{score::AppComponents()}
    , strings{score::StringConstant()}
{
}

JSONObjectWriter::JSONObjectWriter(QJsonObject&& o)
    : obj{std::move(o)}
    , components{score::AppComponents()}
    , strings{score::StringConstant()}
{
}

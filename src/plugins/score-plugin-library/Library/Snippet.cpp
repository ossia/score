#include "Snippet.hpp"

#include <score/serialization/JSONVisitor.hpp>

#include <QBuffer>
#include <QFile>

namespace Library
{
SnippetInfo readSnippetInfo(const QByteArray& json)
{
  SnippetInfo info;
  const auto doc = readJson(json);
  if(!doc.IsObject())
    return info;
  const auto it = doc.FindMember("Snippet");
  if(it == doc.MemberEnd() || !it->value.IsObject())
    return info;
  const auto& obj = it->value;

  if(auto d = obj.FindMember("Description"); d != obj.MemberEnd() && d->value.IsString())
    info.description = QString::fromUtf8(d->value.GetString(), d->value.GetStringLength());
  if(auto t = obj.FindMember("Tags"); t != obj.MemberEnd() && t->value.IsArray())
    for(const auto& tag : t->value.GetArray())
      if(tag.IsString())
        info.tags.push_back(QString::fromUtf8(tag.GetString(), tag.GetStringLength()));
  if(auto th = obj.FindMember("Thumbnail"); th != obj.MemberEnd() && th->value.IsString())
    info.thumbnail.loadFromData(
        QByteArray::fromBase64(
            QByteArray{th->value.GetString(), int(th->value.GetStringLength())}),
        "PNG");
  return info;
}

SnippetInfo readSnippetInfoFile(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return readSnippetInfo(f.readAll());
}

QByteArray withSnippetInfo(const QByteArray& json, const SnippetInfo& info)
{
  auto doc = readJson(json);
  if(!doc.IsObject())
    return json;
  addSnippetInfo(doc, info);
  return jsonToByteArray(doc);
}

void addSnippetInfo(rapidjson::Document& doc, const SnippetInfo& info)
{
  if(!doc.IsObject())
    return;
  auto& alloc = doc.GetAllocator();
  doc.RemoveMember("Snippet");

  rapidjson::Value obj(rapidjson::kObjectType);
  {
    const QByteArray d = info.description.toUtf8();
    obj.AddMember(
        "Description", rapidjson::Value(d.constData(), rapidjson::SizeType(d.size()), alloc),
        alloc);
  }
  {
    rapidjson::Value tags(rapidjson::kArrayType);
    for(const auto& tag : info.tags)
    {
      const QByteArray t = tag.toUtf8();
      tags.PushBack(
          rapidjson::Value(t.constData(), rapidjson::SizeType(t.size()), alloc), alloc);
    }
    obj.AddMember("Tags", tags, alloc);
  }
  if(!info.thumbnail.isNull())
  {
    QByteArray png;
    QBuffer buf{&png};
    buf.open(QIODevice::WriteOnly);
    info.thumbnail.save(&buf, "PNG");
    const QByteArray b64 = png.toBase64();
    obj.AddMember(
        "Thumbnail",
        rapidjson::Value(b64.constData(), rapidjson::SizeType(b64.size()), alloc), alloc);
  }
  doc.AddMember("Snippet", obj, alloc);
}

QString snippetSearchText(const SnippetInfo& info)
{
  return (info.tags + QStringList{info.description}).join(' ');
}
}

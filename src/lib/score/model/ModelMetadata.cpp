// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ModelMetadata.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/network/base/name_validation.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::ModelMetadata)
namespace score
{
ModelMetadata::ModelMetadata()
{
  m_color = &score::Skin::Base1;
}

const QString& ModelMetadata::getName() const noexcept
{
  return m_scriptingName;
}

const QString& ModelMetadata::getComment() const noexcept
{
  return m_comment;
}

ColorRef ModelMetadata::getColor() const noexcept
{
  return m_color;
}

const QString& ModelMetadata::getLabel() const noexcept
{
  return m_label;
}

const QVariantMap& ModelMetadata::getExtendedMetadata() const noexcept
{
  return m_extendedMetadata;
}

bool ModelMetadata::touchedName() const noexcept
{
  return m_touchedName;
}

void ModelMetadata::setName(const QString& arg) noexcept
{
  if (m_scriptingName == arg)
  {
    return;
  }

  if (parent() && parent()->parent())
  {
    // TODO use an object pool of some sorts instead
    static std::vector<QString> bros;
    const auto& cld = parent()->parent()->children();
    bros.reserve(cld.size());
    const QObjectList* cld2{};
    int cld2_N = 0;

    std::size_t cur_bros_idx = 0;
    std::size_t cur_bros_size = bros.size();

    for (auto c : cld)
    {
      if (auto bro = qobject_cast<IdentifiedObjectAbstract*>(c))
      {
        cld2 = &bro->children();
        cld2_N = cld2->size();
        for (int j = 0; j < cld2_N; j++)
        {
          if (auto m = qobject_cast<ModelMetadata*>((*cld2)[j]))
          {
            if (const auto& n = m->getName(); !n.isEmpty())
            {
              if (cur_bros_idx < cur_bros_size)
              {
                bros[cur_bros_idx] = n;
              }
              else
              {
                bros.push_back(std::move(n));
                cur_bros_size++;
              }
              cur_bros_idx++;
            }
            break;
          }
        }
      }
    }

    m_scriptingName = ossia::net::sanitize_name(arg, bros);

    for (std::size_t i = 0; i < cur_bros_idx; i++)
      bros[i].clear();
  }
  else
  {
    m_scriptingName = arg;
    ossia::net::sanitize_name(m_scriptingName);
  }

  m_touchedName = true;
  NameChanged(m_scriptingName);
  metadataChanged();
}

void ModelMetadata::setComment(const QString& arg) noexcept
{
  if (m_comment == arg)
  {
    return;
  }

  m_comment = arg;
  CommentChanged(arg);
  metadataChanged();
}

void ModelMetadata::setColor(ColorRef arg) noexcept
{
  if (m_color == arg)
  {
    return;
  }

  m_color = arg;
  ColorChanged(arg);
  metadataChanged();
}

void ModelMetadata::setLabel(const QString& arg) noexcept
{
  if (m_label == arg)
  {
    return;
  }

  m_label = arg;
  LabelChanged(arg);
  metadataChanged();
}

void ModelMetadata::setExtendedMetadata(const QVariantMap& arg) noexcept
{
  if (m_extendedMetadata == arg)
  {
    return;
  }

  m_extendedMetadata = arg;
  ExtendedMetadataChanged(arg);
  metadataChanged();
}
}

// MOVEME
template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const score::ColorRef& md)
{
  m_stream << md.name();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(score::ColorRef& md)
{
  QString col_name;
  m_stream >> col_name;

  auto col = score::ColorRef::ColorFromString(col_name);
  if (col)
    md = *col;
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const score::ModelMetadata& md)
{
  m_stream << md.m_scriptingName << md.m_comment << md.m_color << md.m_label
           << md.m_extendedMetadata << md.m_touchedName;

  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(score::ModelMetadata& md)
{
  m_stream >> md.m_scriptingName >> md.m_comment >> md.m_color >> md.m_label
      >> md.m_extendedMetadata >> md.m_touchedName;

  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void JSONReader::read(const score::ModelMetadata& md)
{
  stream.StartObject();
  obj[strings.ScriptingName] = md.m_scriptingName;
  obj[strings.Comment] = md.m_comment;
  obj[strings.Color] = md.m_color.name();
  obj[strings.Label] = md.m_label;
  obj[strings.Touched] = md.m_touchedName;
  if (!md.m_extendedMetadata.empty())
  {
    obj[strings.Extended] = md.m_extendedMetadata;
  }
  stream.EndObject();
}

template <>
SCORE_LIB_BASE_EXPORT void JSONWriter::write(score::ModelMetadata& md)
{
  md.m_scriptingName = obj[strings.ScriptingName].toString();
  md.m_comment = obj[strings.Comment].toString();

  const auto& color_val = obj[strings.Color];
  if (color_val.isString())
  {
    auto col_name = color_val.toString();
    auto col = score::ColorRef::ColorFromString(col_name);
    if (col)
      md.m_color = *col;
    else
      md.m_color = score::Skin::instance().fromString("Base1");
  }
  else
  {
    md.m_color = score::Skin::instance().fromString("Base1");
  }

  md.m_color.getBrush();

  md.m_label = obj[strings.Label].toString();

  {
    if (auto map = obj.tryGet(strings.Extended))
    {
      md.m_extendedMetadata <<= *map;
    }
  }

  md.m_touchedName = obj[strings.Touched].toBool();
}

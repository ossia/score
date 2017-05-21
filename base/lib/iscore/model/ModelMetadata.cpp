#include "ModelMetadata.hpp"
#include <ossia/network/base/name_validation.hpp>
#include <QJsonObject>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <ossia-qt/js_utilities.hpp>
namespace iscore
{
ModelMetadata::ModelMetadata(const ModelMetadata& other) : QObject{}
{
  setName(other.getName());

  qDebug() << other.getName() << getName();
  setComment(other.getComment());
  setColor(other.getColor());
  setLabel(other.getLabel());
  setExtendedMetadata(other.getExtendedMetadata());
}

ModelMetadata& iscore::ModelMetadata::operator=(const ModelMetadata& other)
{
  setName(other.getName());
  setComment(other.getComment());
  setColor(other.getColor());
  setLabel(other.getLabel());
  setExtendedMetadata(other.getExtendedMetadata());

  return *this;
}

const QString& ModelMetadata::getName() const
{
  return m_scriptingName;
}

const QString& ModelMetadata::getComment() const
{
  return m_comment;
}

ColorRef ModelMetadata::getColor() const
{
  return m_color;
}

const QString& ModelMetadata::getLabel() const
{
  return m_label;
}

const QVariantMap& ModelMetadata::getExtendedMetadata() const
{
  return m_extendedMetadata;
}

void ModelMetadata::setName(const QString& arg)
{
  if (m_scriptingName == arg)
  {
    return;
  }

  if (parent() && parent()->parent())
  {
    // TODO use an object pool of some sorts instead
    static std::vector<QString> bros;

    auto parent_bros
        = parent()->parent()->findChildren<IdentifiedObjectAbstract*>(
          QString{}, Qt::FindDirectChildrenOnly);

    bros.clear();
    bros.reserve(parent_bros.size());
    for (auto o : parent_bros)
    {
      auto objs = o->findChildren<ModelMetadata*>(
            QString{}, Qt::FindDirectChildrenOnly);
      if (!objs.empty())
      {
         auto n = objs[0]->getName();
         if(!n.isEmpty())
             bros.push_back(std::move(n));
      }
    }

    m_scriptingName = ossia::net::sanitize_name(arg, bros);
  }
  else
  {
    m_scriptingName = arg;
    ossia::net::sanitize_name(m_scriptingName);
  }

  emit NameChanged(arg);
  emit metadataChanged();
}

void ModelMetadata::setComment(const QString& arg)
{
  if (m_comment == arg)
  {
    return;
  }

  m_comment = arg;
  emit CommentChanged(arg);
  emit metadataChanged();
}

void ModelMetadata::setColor(ColorRef arg)
{
  if (m_color == arg)
  {
    return;
  }

  m_color = arg;
  emit ColorChanged(arg);
  emit metadataChanged();
}

void ModelMetadata::setLabel(const QString& arg)
{
  if (m_label == arg)
  {
    return;
  }

  m_label = arg;
  emit LabelChanged(arg);
  emit metadataChanged();
}

void ModelMetadata::setExtendedMetadata(const QVariantMap& arg)
{
  if (m_extendedMetadata == arg)
  {
    return;
  }

  m_extendedMetadata = arg;
  emit ExtendedMetadataChanged(arg);
  emit metadataChanged();
}
}

// MOVEME
template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const iscore::ColorRef& md)
{
  m_stream << md.name();
}

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamWriter::write(iscore::ColorRef& md)
{
  QString col_name;
  m_stream >> col_name;

  auto col = iscore::ColorRef::ColorFromString(col_name);
  if (col)
    md = *col;
}

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const iscore::ModelMetadata& md)
{
  m_stream << md.m_scriptingName << md.m_comment << md.m_color << md.m_label
           << md.m_extendedMetadata;

  insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamWriter::write(iscore::ModelMetadata& md)
{
  m_stream >> md.m_scriptingName >> md.m_comment >> md.m_color >> md.m_label
      >> md.m_extendedMetadata;

  checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectReader::read(const iscore::ModelMetadata& md)
{
  obj[strings.ScriptingName] = md.m_scriptingName;
  obj[strings.Comment] = md.m_comment;
  obj[strings.Color] = md.m_color.name();
  obj[strings.Label] = md.m_label;
  if (!md.m_extendedMetadata.empty())
  {
    obj.insert(
          strings.Extended, QJsonObject::fromVariantMap(md.m_extendedMetadata));
  }
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectWriter::write(iscore::ModelMetadata& md)
{
  md.m_scriptingName = obj[strings.ScriptingName].toString();
  md.m_comment = obj[strings.Comment].toString();

  QJsonValue color_val = obj[strings.Color];
  if (color_val.isArray())
  {
    // Old save format
    const auto& color = color_val.toArray();
    QColor col(color[0].toInt(), color[1].toInt(), color[2].toInt());
    auto sim_col = iscore::ColorRef::SimilarColor(col);
    if (sim_col)
      md.m_color = *sim_col;
  }
  else if (color_val.isString())
  {
    auto col_name = color_val.toString();
    auto col = iscore::ColorRef::ColorFromString(col_name);
    if (col)
      md.m_color = *col;
  }

  md.m_label = obj[strings.Label].toString();

  {
    auto it = obj.find(strings.Extended);
    if (it != obj.end())
    {
      md.m_extendedMetadata = it->toObject().toVariantMap();
    }
  }
}

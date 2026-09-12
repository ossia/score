#include <Gris/Algo/SpeakerSetupIO.hpp>

#include <QByteArray>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Gris
{
namespace
{
// Legacy (v0) tags, from StructGRIS sg_LogicStrucs.cpp
constexpr auto LEGACY_ROOT = "SPEAKER_SETUP";
constexpr auto LEGACY_SPEAKER_PREFIX = "SPEAKER_";
constexpr auto LEGACY_POSITION = "POSITION";
constexpr auto LEGACY_HIGHPASS = "HIGHPASS";

// v4 tags, from StructGRIS Utilities/ValueTreeUtilities.hpp
constexpr auto VT_SETUP_VERSION = "SPEAKER_SETUP_VERSION";
constexpr auto VT_GROUP = "SPEAKER_GROUP";
constexpr auto VT_GROUP_NAME = "SPEAKER_GROUP_NAME";
constexpr auto VT_SPEAKER = "SPEAKER";
constexpr auto VT_PATCH_ID = "SPEAKER_PATCH_ID";
constexpr auto VT_NEXT_PATCH_ID = "NEXT_SPEAKER_PATCH_ID";
constexpr auto VT_IO_STATE = "IO_STATE";
constexpr auto VT_CARTESIAN = "CARTESIAN_POSITION";
constexpr auto VT_GAIN = "GAIN";
constexpr auto VT_HIGHPASS_FREQ = "HIGHPASS_FREQ";
constexpr auto VT_DIRECT_OUT_ONLY = "DIRECT_OUT_ONLY";
constexpr auto VT_YAW = "YAW";
constexpr auto VT_PITCH = "PITCH";
constexpr auto VT_ROLL = "ROLL";
constexpr auto VT_UUID = "UUID";

constexpr auto CURRENT_SETUP_VERSION = 1;
constexpr auto MAIN_GROUP_NAME = "Main Speaker Group";

[[nodiscard]] float attrFloat(
    QXmlStreamAttributes const& attrs, QString const& name, float fallback = 0.f) noexcept
{
  if(!attrs.hasAttribute(name))
    return fallback;
  bool ok{};
  auto const value = attrs.value(name).toFloat(&ok);
  return ok ? value : fallback;
}

[[nodiscard]] int attrInt(
    QXmlStreamAttributes const& attrs, QString const& name, int fallback = 0) noexcept
{
  if(!attrs.hasAttribute(name))
    return fallback;
  bool ok{};
  auto const value = attrs.value(name).toInt(&ok);
  return ok ? value : fallback;
}

[[nodiscard]] bool attrBool(
    QXmlStreamAttributes const& attrs, QString const& name, bool fallback = false) noexcept
{
  if(!attrs.hasAttribute(name))
    return fallback;
  auto const value = attrs.value(name);
  return value == QStringLiteral("1") || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

/** The v4 format stores a position as the "x, y, z" string that
 *  juce::VariantConverter<Position> produces. */
[[nodiscard]] std::optional<CartesianVector> parseCartesianString(QStringView str) noexcept
{
  auto const parts = str.toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
  if(parts.size() != 3)
    return std::nullopt;

  bool okX{}, okY{}, okZ{};
  auto const x = parts[0].trimmed().toFloat(&okX);
  auto const y = parts[1].trimmed().toFloat(&okY);
  auto const z = parts[2].trimmed().toFloat(&okZ);
  if(!okX || !okY || !okZ)
    return std::nullopt;
  return CartesianVector{x, y, z};
}

[[nodiscard]] QString cartesianToString(CartesianVector const& v)
{
  return QStringLiteral("%1, %2, %3")
      .arg(v.x, 0, 'g', 9)
      .arg(v.y, 0, 'g', 9)
      .arg(v.z, 0, 'g', 9);
}

//==============================================================================
// 1. legacy: <SPEAKER_SETUP VERSION="3.x"> with flat SPEAKER_n children
//==============================================================================
[[nodiscard]] SpeakerSetupReadResult readLegacy(QXmlStreamReader& xml)
{
  SpeakerSetupReadResult result;
  result.format = SpeakerSetupFormat::legacy;

  SpeakerSetup setup;
  auto const rootAttrs = xml.attributes();
  setup.spatMode
      = spatModeFromString(rootAttrs.value(QStringLiteral("SPAT_MODE")).toString().toStdString());
  if(setup.spatMode == SpatMode::invalid)
    setup.spatMode = SpatMode::vbap;
  setup.diffusion = attrFloat(rootAttrs, QStringLiteral("DIFFUSION"));
  setup.generalMute = attrBool(rootAttrs, QStringLiteral("GENERAL_MUTE"));

  SpeakerGroup main;
  main.name = MAIN_GROUP_NAME;

  while(!xml.atEnd())
  {
    auto const token = xml.readNext();
    if(token == QXmlStreamReader::EndElement && xml.name() == QLatin1String(LEGACY_ROOT))
      break;
    if(token != QXmlStreamReader::StartElement)
      continue;

    auto const name = xml.name().toString();
    if(!name.startsWith(QLatin1String(LEGACY_SPEAKER_PREFIX)))
      continue;

    bool ok{};
    auto const patch = name.mid(int(std::char_traits<char>::length(LEGACY_SPEAKER_PREFIX))).toInt(&ok);
    if(!ok)
      continue;

    SpeakerEntry entry;
    entry.patch = output_patch_t{patch};

    auto const attrs = xml.attributes();
    entry.data.state
        = sliceStateFromString(attrs.value(QStringLiteral("STATE")).toString().toStdString());
    entry.data.gain = attrFloat(attrs, QStringLiteral("GAIN"));
    entry.data.isDirectOutOnly = attrBool(attrs, QStringLiteral("DIRECT_OUT_ONLY"));

    // Children: POSITION (required) and HIGHPASS (optional)
    while(!xml.atEnd())
    {
      auto const childToken = xml.readNext();
      if(childToken == QXmlStreamReader::EndElement && xml.name() == name)
        break;
      if(childToken != QXmlStreamReader::StartElement)
        continue;

      auto const childAttrs = xml.attributes();
      if(xml.name() == QLatin1String(LEGACY_POSITION))
      {
        entry.data.position = Position{CartesianVector{
            attrFloat(childAttrs, QStringLiteral("X")),
            attrFloat(childAttrs, QStringLiteral("Y")),
            attrFloat(childAttrs, QStringLiteral("Z"))}};
      }
      else if(xml.name() == QLatin1String(LEGACY_HIGHPASS))
      {
        entry.data.highpassFreq = attrFloat(childAttrs, QStringLiteral("FREQ"));
      }
    }

    main.speakers.push_back(entry);
  }

  setup.groups.push_back(std::move(main));
  result.setup = std::move(setup);
  return result;
}

//==============================================================================
// 2. intermediate: <SpeakerSetup Name Dimension> with <Ring>/<Speaker .../>
//==============================================================================
[[nodiscard]] SpeakerSetupReadResult readIntermediate(QXmlStreamReader& xml)
{
  SpeakerSetupReadResult result;
  result.format = SpeakerSetupFormat::intermediate;

  SpeakerSetup setup;
  // This variant carries no spat mode; the dimension is the only hint, and a
  // 2D layout is always dome-like.
  setup.spatMode = SpatMode::vbap;

  int ringIndex{};
  while(!xml.atEnd())
  {
    auto const token = xml.readNext();
    if(token == QXmlStreamReader::EndElement && xml.name() == QLatin1String("SpeakerSetup"))
      break;
    if(token != QXmlStreamReader::StartElement)
      continue;

    if(xml.name() == QLatin1String("Ring"))
    {
      SpeakerGroup group;
      group.name = "Ring " + std::to_string(++ringIndex);

      while(!xml.atEnd())
      {
        auto const ringToken = xml.readNext();
        if(ringToken == QXmlStreamReader::EndElement && xml.name() == QLatin1String("Ring"))
          break;
        if(ringToken != QXmlStreamReader::StartElement || xml.name() != QLatin1String("Speaker"))
          continue;

        auto const attrs = xml.attributes();
        SpeakerEntry entry;
        entry.patch = output_patch_t{attrInt(attrs, QStringLiteral("OutputPatch"), 1)};
        entry.data.position = Position{CartesianVector{
            attrFloat(attrs, QStringLiteral("PositionX")),
            attrFloat(attrs, QStringLiteral("PositionY")),
            attrFloat(attrs, QStringLiteral("PositionZ"))}};
        entry.data.gain = attrFloat(attrs, QStringLiteral("Gain"));
        entry.data.highpassFreq = attrFloat(attrs, QStringLiteral("HighPassCutoff"));
        entry.data.isDirectOutOnly = attrBool(attrs, QStringLiteral("DirectOut"));
        group.speakers.push_back(entry);
      }

      setup.groups.push_back(std::move(group));
    }
  }

  result.setup = std::move(setup);
  return result;
}

//==============================================================================
// 3. current v4: <SPEAKER_SETUP> with nested <SPEAKER_GROUP>/<SPEAKER>
//==============================================================================
void readValueTreeGroup(QXmlStreamReader& xml, SpeakerSetup& setup)
{
  SpeakerGroup group;
  auto const attrs = xml.attributes();
  group.name = attrs.value(QLatin1String(VT_GROUP_NAME)).toString().toStdString();
  group.yaw = degrees_t{attrFloat(attrs, QLatin1String(VT_YAW))};
  group.pitch = degrees_t{attrFloat(attrs, QLatin1String(VT_PITCH))};
  group.roll = degrees_t{attrFloat(attrs, QLatin1String(VT_ROLL))};
  if(auto const pos = parseCartesianString(attrs.value(QLatin1String(VT_CARTESIAN))))
    group.position = *pos;

  while(!xml.atEnd())
  {
    auto const token = xml.readNext();
    if(token == QXmlStreamReader::EndElement && xml.name() == QLatin1String(VT_GROUP))
      break;
    if(token != QXmlStreamReader::StartElement)
      continue;

    if(xml.name() == QLatin1String(VT_SPEAKER))
    {
      auto const spkAttrs = xml.attributes();
      SpeakerEntry entry;
      entry.patch = output_patch_t{attrInt(spkAttrs, QLatin1String(VT_PATCH_ID), 1)};
      if(auto const pos = parseCartesianString(spkAttrs.value(QLatin1String(VT_CARTESIAN))))
        entry.data.position = Position{*pos};
      entry.data.state = sliceStateFromString(
          spkAttrs.value(QLatin1String(VT_IO_STATE)).toString().toStdString());
      entry.data.gain = attrFloat(spkAttrs, QLatin1String(VT_GAIN));
      entry.data.highpassFreq = attrFloat(spkAttrs, QLatin1String(VT_HIGHPASS_FREQ));
      entry.data.isDirectOutOnly = attrBool(spkAttrs, QLatin1String(VT_DIRECT_OUT_ONLY));
      group.speakers.push_back(entry);
    }
    else if(xml.name() == QLatin1String(VT_GROUP))
    {
      // Nested groups are flattened one level at a time: the child group keeps
      // its own transform, which is what StructGRIS does when it walks the tree.
      readValueTreeGroup(xml, setup);
    }
  }

  setup.groups.push_back(std::move(group));
}

[[nodiscard]] SpeakerSetupReadResult readValueTree(QXmlStreamReader& xml)
{
  SpeakerSetupReadResult result;
  result.format = SpeakerSetupFormat::valueTree;

  SpeakerSetup setup;
  auto const rootAttrs = xml.attributes();
  setup.spatMode
      = spatModeFromString(rootAttrs.value(QStringLiteral("SPAT_MODE")).toString().toStdString());
  if(setup.spatMode == SpatMode::invalid)
    setup.spatMode = SpatMode::vbap;
  setup.diffusion = attrFloat(rootAttrs, QStringLiteral("DIFFUSION"));
  setup.generalMute = attrBool(rootAttrs, QStringLiteral("GENERAL_MUTE"));

  while(!xml.atEnd())
  {
    auto const token = xml.readNext();
    if(token == QXmlStreamReader::EndElement && xml.name() == QLatin1String(LEGACY_ROOT))
      break;
    if(token == QXmlStreamReader::StartElement && xml.name() == QLatin1String(VT_GROUP))
      readValueTreeGroup(xml, setup);
  }

  result.setup = std::move(setup);
  return result;
}
} // namespace

//==============================================================================
SpeakerSetupReadResult readSpeakerSetup(QByteArray const& data)
{
  QXmlStreamReader xml{data};

  while(!xml.atEnd())
  {
    if(xml.readNext() != QXmlStreamReader::StartElement)
      continue;

    if(xml.name() == QLatin1String(LEGACY_ROOT))
    {
      // The v4 file keeps the same root element as the legacy one; what tells
      // them apart is the version attribute and the SPEAKER_GROUP children.
      auto const attrs = xml.attributes();
      bool const isValueTree
          = attrs.hasAttribute(QLatin1String(VT_SETUP_VERSION))
            || attrs.hasAttribute(QLatin1String(VT_UUID))
            || attrs.hasAttribute(QLatin1String(VT_NEXT_PATCH_ID));
      return isValueTree ? readValueTree(xml) : readLegacy(xml);
    }

    if(xml.name() == QLatin1String("SpeakerSetup"))
      return readIntermediate(xml);

    break;
  }

  SpeakerSetupReadResult result;
  result.error = xml.hasError() ? xml.errorString()
                                : QStringLiteral("Not a SpatGRIS speaker setup file");
  return result;
}

SpeakerSetupReadResult readSpeakerSetupFile(QString const& path)
{
  QFile file{path};
  if(!file.open(QIODevice::ReadOnly))
  {
    SpeakerSetupReadResult result;
    result.error = QStringLiteral("Could not open %1: %2").arg(path, file.errorString());
    return result;
  }
  return readSpeakerSetup(file.readAll());
}

//==============================================================================
QByteArray writeSpeakerSetup(SpeakerSetup const& setup)
{
  QByteArray out;
  QXmlStreamWriter xml{&out};
  xml.setAutoFormatting(true);
  xml.writeStartDocument();

  xml.writeStartElement(QLatin1String(LEGACY_ROOT));
  xml.writeAttribute(QLatin1String(VT_SETUP_VERSION), QString::number(CURRENT_SETUP_VERSION));
  xml.writeAttribute(
      QStringLiteral("SPAT_MODE"), QString::fromUtf8(toString(setup.spatMode).data()));
  xml.writeAttribute(QStringLiteral("DIFFUSION"), QString::number(setup.diffusion));
  xml.writeAttribute(QStringLiteral("GENERAL_MUTE"), setup.generalMute ? "1" : "0");

  int nextPatch{};
  for(auto const& group : setup.groups)
    for(auto const& speaker : group.speakers)
      nextPatch = std::max(nextPatch, speaker.patch.get() + 1);
  xml.writeAttribute(QLatin1String(VT_NEXT_PATCH_ID), QString::number(std::max(nextPatch, 1)));

  for(auto const& group : setup.groups)
  {
    xml.writeStartElement(QLatin1String(VT_GROUP));
    xml.writeAttribute(
        QLatin1String(VT_GROUP_NAME), QString::fromStdString(group.name));
    xml.writeAttribute(QLatin1String(VT_CARTESIAN), cartesianToString(group.position));
    xml.writeAttribute(QLatin1String(VT_YAW), QString::number(group.yaw.get()));
    xml.writeAttribute(QLatin1String(VT_PITCH), QString::number(group.pitch.get()));
    xml.writeAttribute(QLatin1String(VT_ROLL), QString::number(group.roll.get()));

    for(auto const& speaker : group.speakers)
    {
      xml.writeStartElement(QLatin1String(VT_SPEAKER));
      xml.writeAttribute(
          QLatin1String(VT_PATCH_ID), QString::number(speaker.patch.get()));
      xml.writeAttribute(
          QLatin1String(VT_CARTESIAN), cartesianToString(speaker.data.position.getCartesian()));
      xml.writeAttribute(
          QLatin1String(VT_IO_STATE), QString::fromUtf8(toString(speaker.data.state).data()));
      xml.writeAttribute(QLatin1String(VT_GAIN), QString::number(speaker.data.gain));
      xml.writeAttribute(
          QLatin1String(VT_HIGHPASS_FREQ), QString::number(speaker.data.highpassFreq));
      xml.writeAttribute(
          QLatin1String(VT_DIRECT_OUT_ONLY), speaker.data.isDirectOutOnly ? "1" : "0");
      xml.writeEndElement();
    }

    xml.writeEndElement();
  }

  xml.writeEndElement();
  xml.writeEndDocument();
  return out;
}

QString writeSpeakerSetupFile(SpeakerSetup const& setup, QString const& path)
{
  QFile file{path};
  if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return QStringLiteral("Could not write %1: %2").arg(path, file.errorString());

  auto const data = writeSpeakerSetup(setup);
  if(file.write(data) != data.size())
    return QStringLiteral("Short write to %1").arg(path);
  return {};
}

} // namespace Gris

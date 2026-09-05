// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProjectInfo.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>
#include <score/tools/Zip.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QBuffer>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::ProjectInfo::Model)
W_OBJECT_IMPL(score::ProjectInfo::View)

namespace score::ProjectInfo
{
Model::Model(const score::DocumentContext& ctx, QObject* parent)
    : ProjectSettingsModel{ctx, "ProjectInfo", parent}
    , m_Created{QDateTime::currentDateTime()}
{
}

Model::~Model() = default;

QByteArray Model::encodeThumbnail(const QImage& img)
{
  if(img.isNull())
    return {};

  // Fill the whole thumbnail: scale up to cover, then crop the center.
  QImage scaled = img.scaled(
      thumbnailSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  const QRect crop{
      (scaled.width() - thumbnailSize.width()) / 2,
      (scaled.height() - thumbnailSize.height()) / 2, thumbnailSize.width(),
      thumbnailSize.height()};
  scaled = scaled.copy(crop).convertToFormat(QImage::Format_RGB32);

  QByteArray data;
  QBuffer buf{&data};
  buf.open(QIODevice::WriteOnly);
  scaled.save(&buf, "PNG");
  return data;
}

QImage Model::thumbnailImage() const
{
  return QImage::fromData(m_Thumbnail, "PNG");
}

SCORE_PROJECTSETTINGS_PARAMETER_CPP(QString, Model, Name)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QString, Model, Author)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QString, Model, Description)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QString, Model, Url)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QDateTime, Model, Created)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QDateTime, Model, LastSaved)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(QByteArray, Model, Thumbnail)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(bool, Model, AutomaticThumbnail)

static QString dateText(const QDateTime& dt)
{
  return dt.isValid() ? QLocale{}.toString(dt.toLocalTime(), QLocale::ShortFormat)
                      : QObject::tr("Unknown");
}

View::View()
{
  m_widg = new score::FormWidget{tr("Project")};
  auto lay = m_widg->layout();
  lay->setVerticalSpacing(6);

  m_name = new QLineEdit{m_widg};
  m_name->setPlaceholderText(tr("Untitled project"));
  lay->addRow(tr("Name"), m_name);
  connect(m_name, &QLineEdit::editingFinished, this, [this] {
    NameChanged(m_name->text());
  });

  m_author = new QLineEdit{m_widg};
  lay->addRow(tr("Author"), m_author);
  connect(m_author, &QLineEdit::editingFinished, this, [this] {
    AuthorChanged(m_author->text());
  });

  m_description = new QPlainTextEdit{m_widg};
  m_description->setPlaceholderText(
      tr("What this score does, how to run it, what it needs..."));
  m_description->setFixedHeight(64);
  lay->addRow(tr("Description"), m_description);
  connect(m_description, &QPlainTextEdit::textChanged, this, [this] {
    DescriptionChanged(m_description->toPlainText());
  });

  m_url = new QLineEdit{m_widg};
  m_url->setPlaceholderText(tr("https://... (documentation, tutorial, project page)"));
  m_url->setToolTip(
      tr("Opened in the browser when this score is used as an example from the start "
         "screen"));
  lay->addRow(tr("Web page"), m_url);
  connect(
      m_url, &QLineEdit::editingFinished, this, [this] { UrlChanged(m_url->text()); });

  // Both dates on one row
  auto dates = new QWidget{m_widg};
  auto datesLay = new QHBoxLayout{dates};
  datesLay->setContentsMargins(0, 0, 0, 0);
  datesLay->setSpacing(18);
  m_created = new QLabel{dates};
  m_lastSaved = new QLabel{dates};
  datesLay->addWidget(m_created);
  auto savedTitle = new QLabel{tr("Last saved"), dates};
  savedTitle->setEnabled(false);
  datesLay->addWidget(savedTitle);
  datesLay->addWidget(m_lastSaved);
  datesLay->addStretch();
  lay->addRow(tr("Created"), dates);

  // Thumbnail
  m_thumbnailPreview = new QLabel{m_widg};
  m_thumbnailPreview->setFixedSize(Model::thumbnailSize / 3);
  m_thumbnailPreview->setAlignment(Qt::AlignCenter);
  m_thumbnailPreview->setFrameShape(QFrame::StyledPanel);

  auto thumbWidget = new QWidget{m_widg};
  auto thumbLay = new QHBoxLayout{thumbWidget};
  thumbLay->setContentsMargins(0, 0, 0, 0);
  thumbLay->addWidget(m_thumbnailPreview);

  auto btns = new QVBoxLayout;
  auto choose = new QPushButton{tr("Choose image..."), thumbWidget};
  choose->setToolTip(tr("Use a fixed image instead of the automatic capture"));
  connect(choose, &QPushButton::clicked, this, &View::chooseThumbnail);
  auto clear = new QPushButton{tr("Clear"), thumbWidget};
  connect(clear, &QPushButton::clicked, this, [this] { ThumbnailChanged({}); });
  m_AutomaticThumbnail = new QCheckBox{tr("Auto-thumbnail"), thumbWidget};
  m_AutomaticThumbnail->setToolTip(
      tr("Refresh the thumbnail from the document view every time the score is saved"));
  connect(
      m_AutomaticThumbnail, &QCheckBox::toggled, this, &View::AutomaticThumbnailChanged);
  btns->addWidget(choose);
  btns->addWidget(clear);
  btns->addWidget(m_AutomaticThumbnail);
  btns->addStretch();
  thumbLay->addLayout(btns);
  thumbLay->addStretch();
  lay->addRow(tr("Thumbnail"), thumbWidget);
}

void View::setName(QString v)
{
  if(m_name->text() != v)
    m_name->setText(v);
}

void View::setAuthor(QString v)
{
  if(m_author->text() != v)
    m_author->setText(v);
}

void View::setDescription(QString v)
{
  if(m_description->toPlainText() != v)
    m_description->setPlainText(v);
}

void View::setUrl(QString v)
{
  if(m_url->text() != v)
    m_url->setText(v);
}

void View::setCreated(QDateTime dt)
{
  m_created->setText(dateText(dt));
}

void View::setLastSaved(QDateTime dt)
{
  m_lastSaved->setText(dateText(dt));
}

void View::setThumbnail(QByteArray data)
{
  m_thumbnail = data;
  const auto img = QImage::fromData(data, "PNG");
  if(img.isNull())
  {
    m_thumbnailPreview->setPixmap({});
    m_thumbnailPreview->setText(tr("No thumbnail yet"));
  }
  else
  {
    m_thumbnailPreview->setPixmap(
        QPixmap::fromImage(img).scaled(
            m_thumbnailPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
}

void View::chooseThumbnail()
{
  QStringList formats;
  for(const auto& f : QImageReader::supportedImageFormats())
    formats.push_back("*." + QString::fromLatin1(f));

  const auto file = QFileDialog::getOpenFileName(
      m_widg, tr("Choose a thumbnail"), {}, tr("Images (%1)").arg(formats.join(' ')));
  if(file.isEmpty())
    return;

  QImage img{file};
  if(img.isNull())
    return;

  ThumbnailChanged(Model::encodeThumbnail(img));
  // A hand-picked image should not be overwritten on the next save.
  AutomaticThumbnailChanged(false);
}

SETTINGS_UI_TOGGLE_IMPL(AutomaticThumbnail)

QWidget* View::getWidget()
{
  return m_widg;
}

Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::ProjectSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(Name);
  SETTINGS_PRESENTER(Author);
  SETTINGS_PRESENTER(Description);
  SETTINGS_PRESENTER(Url);
  SETTINGS_PRESENTER(Thumbnail);
  SETTINGS_PRESENTER(AutomaticThumbnail);

  con(m, &Model::CreatedChanged, &v, &View::setCreated);
  v.setCreated(m.getCreated());
  con(m, &Model::LastSavedChanged, &v, &View::setLastSaved);
  v.setLastSaved(m.getLastSaved());
}

QString Presenter::settingsName()
{
  return tr("Project");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_project_on.png"),
      QStringLiteral(":/icons/settings_project_off.png"),
      QStringLiteral(":/icons/settings_project_off.png"));
}

static std::optional<Info> parseInfo(const QByteArray& data);

std::optional<Info> peek(const QString& path)
{
  if(path.endsWith(".zip", Qt::CaseInsensitive))
  {
    // A project archive: the score is one of its members
    const auto summary = summarizeZipArchive(path);
    if(!summary)
      return std::nullopt;
    QString error;
    return parseInfo(readZipMember(path, summary->scoreFile, error));
  }

  if(!path.endsWith(".score", Qt::CaseInsensitive)
     && !path.endsWith(".scorejson", Qt::CaseInsensitive))
    return std::nullopt;

  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return std::nullopt;

  return parseInfo(score::mapAsByteArray(f));
}

static std::optional<Info> parseInfo(const QByteArray& data)
{
  if(data.isEmpty())
    return std::nullopt;

  try
  {
    const rapidjson::Document doc = readJson(data);
    if(!doc.IsObject())
      return std::nullopt;

    const auto plugins = doc.FindMember("Plugins");
    if(plugins == doc.MemberEnd() || !plugins->value.IsArray())
      return std::nullopt;

    for(const auto& plug : plugins->value.GetArray())
    {
      if(!plug.IsObject())
        continue;
      const auto uuid = plug.FindMember("uuid");
      if(uuid == plug.MemberEnd() || !uuid->value.IsString())
        continue;
      if(std::string_view{uuid->value.GetString(), uuid->value.GetStringLength()}
         != uuid_string)
        continue;

      Info info;
      auto str = [&](const char* key) -> QString {
        auto it = plug.FindMember(key);
        if(it != plug.MemberEnd() && it->value.IsString())
          return QString::fromUtf8(it->value.GetString(), it->value.GetStringLength());
        return {};
      };
      info.name = str("Name");
      info.author = str("Author");
      info.description = str("Description");
      info.url = str("Url");
      info.created = QDateTime::fromString(str("Created"), Qt::ISODateWithMs);
      info.lastSaved = QDateTime::fromString(str("LastSaved"), Qt::ISODateWithMs);
      if(const auto thumb = str("Thumbnail"); !thumb.isEmpty())
        info.thumbnail
            = QImage::fromData(QByteArray::fromBase64(thumb.toLatin1()), "PNG");
      return info;
    }
  }
  catch(...)
  {
  }
  return std::nullopt;
}
}

template <>
void DataStreamReader::read(const score::ProjectInfo::Model& m)
{
  // Dates as ISO strings: the DataStream visitor has no QDateTime overload
  m_stream << m.m_Name << m.m_Author << m.m_Description << m.m_Url
           << m.m_Created.toUTC().toString(Qt::ISODateWithMs)
           << m.m_LastSaved.toUTC().toString(Qt::ISODateWithMs) << m.m_Thumbnail
           << m.m_AutomaticThumbnail;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(score::ProjectInfo::Model& m)
{
  QString created, saved;
  m_stream >> m.m_Name >> m.m_Author >> m.m_Description >> m.m_Url >> created >> saved
      >> m.m_Thumbnail >> m.m_AutomaticThumbnail;
  m.m_Created = QDateTime::fromString(created, Qt::ISODateWithMs);
  m.m_LastSaved = QDateTime::fromString(saved, Qt::ISODateWithMs);
  checkDelimiter();
}

template <>
void JSONReader::read(const score::ProjectInfo::Model& m)
{
  obj["Name"] = m.m_Name;
  obj["Author"] = m.m_Author;
  obj["Description"] = m.m_Description;
  obj["Url"] = m.m_Url;
  // UTC with an explicit offset: unambiguous wherever the file is opened
  obj["Created"] = m.m_Created.toUTC().toString(Qt::ISODateWithMs);
  obj["LastSaved"] = m.m_LastSaved.toUTC().toString(Qt::ISODateWithMs);
  obj["Thumbnail"] = QString::fromLatin1(m.m_Thumbnail.toBase64());
  obj["AutomaticThumbnail"] = m.m_AutomaticThumbnail;
}

template <>
void JSONWriter::write(score::ProjectInfo::Model& m)
{
  // A hand-edited or damaged file may hold the wrong type: ignore such fields
  auto str = [&](const std::string& key, QString& out) {
    if(auto v = obj.tryGet(key); v && v->isString())
      out = v->toString();
  };
  str("Name", m.m_Name);
  str("Author", m.m_Author);
  str("Description", m.m_Description);
  str("Url", m.m_Url);
  if(auto v = obj.tryGet("Created"); v && v->isString())
    m.m_Created = QDateTime::fromString(v->toString(), Qt::ISODateWithMs);
  if(auto v = obj.tryGet("LastSaved"); v && v->isString())
    m.m_LastSaved = QDateTime::fromString(v->toString(), Qt::ISODateWithMs);
  if(auto v = obj.tryGet("Thumbnail"); v && v->isString())
    m.m_Thumbnail = QByteArray::fromBase64(v->toString().toLatin1());
  if(auto v = obj.tryGet("AutomaticThumbnail"); v && v->obj.IsBool())
    m.m_AutomaticThumbnail = v->toBool();
}

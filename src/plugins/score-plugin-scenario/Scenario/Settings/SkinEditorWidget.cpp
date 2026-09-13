#include "SkinEditorWidget.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/FilePath.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtColorWidgets/ColorWheel>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::Settings::SkinEditorWidget)

namespace Scenario
{
namespace Settings
{

SkinEditorWidget::SkinEditorWidget(QWidget* parent)
    : QWidget{parent}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(makeSkinRow());

  auto editors = new QHBoxLayout;
  editors->addWidget(makeColorEditor(), 1);
  editors->addWidget(makeFontEditor(), 1);
  lay->addLayout(editors, 1);

  // m_applying: this widget's own edits must not bounce back as a reload and
  // fight the control being dragged.
  connect(&score::Skin::instance(), &score::Skin::changed, this, [this] {
    if(!m_applying)
      reloadFromSkin();
  });
}

void SkinEditorWidget::reloadFromSkin()
{
  score::Skin& s = score::Skin::instance();

  for(int i = 0; i < m_colorList->count(); i++)
  {
    auto* item = m_colorList->item(i);
    if(auto* b = s.fromString(item->text()))
    {
      QPixmap p{16, 16};
      p.fill(b->color());
      item->setIcon(p);
    }
  }
  if(auto* cur = m_colorList->currentItem())
  {
    if(auto* b = s.fromString(cur->text()))
    {
      m_loading = true;
      m_wheel->setColor(b->color());
      m_loading = false;
    }
  }

  loadFontRole();
}

QWidget* SkinEditorWidget::makeSkinRow()
{
  auto w = new QWidget;
  auto lay = new QHBoxLayout{w};
  lay->setContentsMargins(0, 0, 0, 0);

  m_skin = new QComboBox;
  // From the resource, so adding a file to score.qrc is enough.
  m_skin->addItem(tr("Default"), QStringLiteral(":/skin/DefaultSkin.json"));
  {
    QDirIterator builtin{
        QStringLiteral(":/skin"), {QStringLiteral("*.json")}, QDir::Files};
    QStringList names;
    while(builtin.hasNext())
    {
      builtin.next();
      const QString base = builtin.fileInfo().completeBaseName();
      if(base != QLatin1String("DefaultSkin"))
        names.push_back(base);
    }
    names.sort();
    for(const QString& base : names)
    {
      // "GalmuriMicroSkin" -> "Galmuri Micro", "Default12pxSkin" -> "Default
      // 12px".
      QString label = base;
      label.remove(QStringLiteral("Skin"));
      label.replace(QRegularExpression{QStringLiteral("([a-z0-9])([A-Z])")},
                    QStringLiteral("\\1 \\2"));
      label.replace(QRegularExpression{QStringLiteral("([a-zA-Z])([0-9])")},
                    QStringLiteral("\\1 \\2"));
      m_skin->addItem(label.trimmed(), QVariant{":/skin/" + base + ".json"});
    }
  }

  const QString skinPath = score::AppContext()
                               .settings<Library::Settings::Model>()
                               .getDefaultLibraryPath()
                           + "/Skins/";
  QDir skinDir(skinPath, QStringLiteral("*.json"));
  for(const auto& skin : skinDir.entryList())
  {
    auto name = skin;
    name.remove(QStringLiteral(".json"));
    m_skin->addItem(name, QVariant{skinPath + "/" + skin});
  }

  auto browse = new QPushButton{tr("Browse...")};
  auto save = new QPushButton{tr("Save as...")};

  QSettings set;
  m_applyColours = new QCheckBox{tr("Apply colours")};
  m_applyFonts = new QCheckBox{tr("Apply fonts")};
  m_applyColours->setChecked(set.value(QStringLiteral("Skin/ApplyColours"), true).toBool());
  m_applyFonts->setChecked(set.value(QStringLiteral("Skin/ApplyFonts"), true).toBool());
  m_applyColours->setToolTip(
      tr("Take the colours from the skin you pick. Turn off to keep the "
         "current ones."));
  m_applyFonts->setToolTip(
      tr("Take the fonts from the skin you pick. Turn off to keep the "
         "current ones."));

  connect(m_applyColours, &QCheckBox::toggled, this, [](bool v) {
    QSettings{}.setValue(QStringLiteral("Skin/ApplyColours"), v);
  });
  connect(m_applyFonts, &QCheckBox::toggled, this, [](bool v) {
    QSettings{}.setValue(QStringLiteral("Skin/ApplyFonts"), v);
  });

  lay->addWidget(new QLabel{tr("Skin")});
  lay->addWidget(m_skin, 1);
  lay->addWidget(m_applyColours);
  lay->addWidget(m_applyFonts);
  lay->addWidget(browse);
  lay->addWidget(save);

  connect(
      m_skin, SignalUtils::QComboBox_currentIndexChanged_int(), this, [this](int i) {
    if(!m_loading)
      skinChanged(m_skin->itemData(i).toString());
      });

  connect(browse, &QPushButton::clicked, this, [this, skinPath] {
    auto f = QFileDialog::getOpenFileName(
        this, tr("Load skin"), score::pickerStartFolder(skinPath),
        QStringLiteral("*.json"));
    if(!f.isEmpty())
      skinChanged(f);
  });

  connect(save, &QPushButton::clicked, this, [this, skinPath] {
    auto f = QFileDialog::getSaveFileName(
        this, tr("Save edited skin file"), score::pickerStartFolder(skinPath),
        QStringLiteral("*.json"));
    if(f.isEmpty())
      return;

    // The list only iterates *.json.
    if(QFileInfo{f}.suffix().isEmpty())
      f += QStringLiteral(".json");

    QFile fl{f};
    if(!fl.open(QIODevice::WriteOnly))
    {
      score::warning(this, tr("Skin"), tr("Could not write %1").arg(f));
      return;
    }

    QJsonDocument doc;
    doc.setObject(score::Skin::instance().toJson());
    fl.write(doc.toJson());

    // skinChanged() reads this path back synchronously and QFile buffers.
    fl.close();
    skinChanged(f);
  });

  return w;
}

int SkinEditorWidget::selectedParts() noexcept
{
  QSettings s;
  int parts = 0;
  if(s.value(QStringLiteral("Skin/ApplyColours"), true).toBool())
    parts |= score::Skin::Colours;
  if(s.value(QStringLiteral("Skin/ApplyFonts"), true).toBool())
    parts |= score::Skin::Fonts;
  return parts;
}

void SkinEditorWidget::setSkin(const QString& skin)
{
  int idx = m_skin->findData(QVariant::fromValue(skin));

  // The setting is either the bare word "Default" or a path, and a path from
  // Browse is in no list.
  if(idx == -1 && (skin.isEmpty() || skin == QLatin1String("Default")))
    idx = m_skin->findData(QStringLiteral(":/skin/DefaultSkin.json"));
  if(idx == -1)
  {
    m_loading = true;
    m_skin->addItem(QFileInfo{skin}.completeBaseName(), QVariant{skin});
    m_loading = false;
    idx = m_skin->count() - 1;
  }

  if(idx == m_skin->currentIndex())
    return;
  m_loading = true;
  m_skin->setCurrentIndex(idx);
  m_loading = false;
}

QWidget* SkinEditorWidget::makeColorEditor()
{
  auto box = new QGroupBox{tr("Colours")};
  auto lay = new QHBoxLayout{box};

  m_colorList = new QListWidget;
  auto form = new QFormLayout;
  lay->addWidget(m_colorList);
  lay->addLayout(form);

  m_wheel = new color_widgets::ColorWheel;
  m_wheel->setMinimumSize(QSize{100, 100});
  m_wheel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  form->setWidget(0, QFormLayout::SpanningRole, m_wheel);

  m_hex = new QLineEdit;
  form->addRow(tr("Hex"), m_hex);

  score::Skin& s = score::Skin::instance();
  for(auto& col : s.getColors())
  {
    QPixmap p{16, 16};
    p.fill(col.first);
    m_colorList->addItem(new QListWidgetItem(p, col.second));
  }

  connect(
      m_colorList, &QListWidget::currentItemChanged, this,
      [this, &s](QListWidgetItem* cur, auto) {
    if(!cur)
      return;
    if(auto b = s.fromString(cur->text()))
    {
      m_loading = true;
      m_wheel->setColor(b->color());
      m_hex->setText(b->color().name(QColor::HexRgb));
      m_loading = false;
    }
      });

  connect(
      m_wheel, &color_widgets::ColorWheel::colorChanged, this,
      [this, &s](QColor c) {
    auto item = m_colorList->currentItem();
    if(!item || m_loading)
      return;
    if(auto brush = s.fromString(item->text()))
      brush->reload(c);
    QPixmap p{16, 16};
    p.fill(c);
    item->setIcon(p);
    m_hex->setText(c.name(QColor::HexRgb));
    m_applying = true;
    s.changed();
    m_applying = false;
      });

  connect(m_hex, &QLineEdit::editingFinished, this, [this, &s] {
    auto item = m_colorList->currentItem();
    if(!item)
      return;
    const QColor c{m_hex->text()};
    if(!c.isValid())
      return;
    if(auto brush = s.fromString(item->text()); brush && brush->color() != c)
    {
      QPixmap p{16, 16};
      p.fill(c);
      item->setIcon(p);
      brush->reload(c);
      m_loading = true;
      m_wheel->setColor(c);
      m_loading = false;
      m_applying = true;
      s.changed();
      m_applying = false;
    }
  });

  m_colorList->setCurrentRow(0);
  return box;
}

QWidget* SkinEditorWidget::makeFontEditor()
{
  auto box = new QGroupBox{tr("Fonts")};
  auto lay = new QHBoxLayout{box};

  m_fontList = new QListWidget;
  auto form = new QFormLayout;
  lay->addWidget(m_fontList);
  lay->addLayout(form);

  for(auto& [key, font] : score::Skin::instance().fonts())
    m_fontList->addItem(QString::fromUtf8(key));

  m_fontFamily = new QComboBox;
  m_fontFamily->addItems(QFontDatabase::families());
  m_fontStyle = new QComboBox;
  m_fontSize = new QSpinBox;
  m_fontSize->setRange(4, 96);
  m_fontSize->setSuffix(tr(" px"));
  m_fontAntialias = new QCheckBox{tr("Antialias")};
  m_fontHinting = new QComboBox;
  // QFont::HintingPreference; the skin stores the name.
  m_fontHinting->addItems({tr("Default"), tr("None"), tr("Vertical"), tr("Full")});
  m_fontHinting->setToolTip(
      tr("How glyphs are snapped to the pixel grid. Full gives the crispest "
         "stems at small sizes; Vertical keeps the designed letter spacing; "
         "None leaves the outline unhinted."));
  // Editable, so you can type your own sample.
  m_fontPreview = new QPlainTextEdit;
  m_fontPreview->setPlainText(
      QStringLiteral("Interval 3 - gain -6.0 dB\nAaBbCc 0123456789"));
  m_fontPreview->setMinimumHeight(64);
  m_fontPreview->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_fontPreview->setStyleSheet(
      QStringLiteral("QPlainTextEdit { background: #131313; color: #d0d0d0;"
                     " border: 1px solid #3d3d3d; }"));

  m_fontHint = new QLabel;
  m_fontHint->setWordWrap(true);

  form->addRow(tr("Family"), m_fontFamily);
  form->addRow(tr("Style"), m_fontStyle);
  form->addRow(tr("Size"), m_fontSize);
  form->addRow(QString{}, m_fontHint);
  form->addRow(tr("Hinting"), m_fontHinting);
  form->addRow(QString{}, m_fontAntialias);
  form->addRow(tr("Preview"), m_fontPreview);

  connect(m_fontList, &QListWidget::currentRowChanged, this, [this](int) {
    loadFontRole();
  });
  connect(m_fontFamily, &QComboBox::currentTextChanged, this, [this](const QString& fam) {
    if(m_loading)
      return;
    // A new family has its own styles; keep the closest rather than whichever
    // sorts first.
    const QString wanted = m_fontStyle->currentText();
    {
      QSignalBlocker b{m_fontStyle};
      m_fontStyle->clear();
      m_fontStyle->addItems(QFontDatabase::styles(fam));
      const int idx = m_fontStyle->findText(wanted);
      m_fontStyle->setCurrentIndex(
          idx != -1 ? idx
                    : std::max(0, m_fontStyle->findText(QStringLiteral("Regular"))));
    }
    applyFontRole();
  });
  connect(m_fontStyle, &QComboBox::currentTextChanged, this, [this](const QString&) {
    applyFontRole();
  });
  connect(m_fontSize, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int) {
    applyFontRole();
  });
  connect(m_fontAntialias, &QCheckBox::toggled, this, [this](bool) { applyFontRole(); });
  connect(m_fontHinting, &QComboBox::currentTextChanged, this, [this](const QString&) {
    applyFontRole();
  });

  m_fontList->setCurrentRow(0);
  loadFontRole();
  return box;
}

//! The QFont* for the selected role, or nullptr.
static QFont* roleFont(QListWidget* list)
{
  const int row = list->currentRow();
  auto roles = score::Skin::instance().fonts();
  if(row < 0 || row >= (int)roles.size())
    return nullptr;
  return roles[row].second;
}

void SkinEditorWidget::loadFontRole()
{
  QFont* f = roleFont(m_fontList);
  if(!f)
    return;

  m_loading = true;
  const QString fam = f->families().value(0, f->family());
  // setCurrentText is a no-op on a non-editable combo when the text is absent,
  // which would leave the previous role's family showing and then write it
  // back. A skin may name a font this machine does not have.
  if(m_fontFamily->findText(fam) == -1)
    m_fontFamily->insertItem(0, fam);
  m_fontFamily->setCurrentIndex(m_fontFamily->findText(fam));

  m_fontStyle->clear();
  const QStringList styles = QFontDatabase::styles(fam);
  m_fontStyle->addItems(styles);

  // styleName() is only set when the skin asked for one by name; otherwise
  // ask Qt what it resolved to, or the combo misreports whichever sorts first.
  QString style = f->styleName();
  if(style.isEmpty() || !styles.contains(style))
    style = QFontDatabase::styleString(*f);
  int idx = m_fontStyle->findText(style);
  if(idx == -1)
    idx = m_fontStyle->findText(QStringLiteral("Regular"));
  m_fontStyle->setCurrentIndex(std::max(0, idx));

  // QFontInfo, not QFontMetrics::height(), which is ascent+descent+leading
  // and would pin an unsized role larger than it drew.
  m_fontSize->setValue(f->pixelSize() > 0 ? f->pixelSize() : QFontInfo{*f}.pixelSize());
  m_fontAntialias->setChecked(!(int(f->styleStrategy()) & int(QFont::NoAntialias)));

  switch(f->hintingPreference())
  {
    case QFont::PreferNoHinting:
      m_fontHinting->setCurrentIndex(1);
      break;
    case QFont::PreferVerticalHinting:
      m_fontHinting->setCurrentIndex(2);
      break;
    case QFont::PreferFullHinting:
      m_fontHinting->setCurrentIndex(3);
      break;
    default:
      m_fontHinting->setCurrentIndex(0);
      break;
  }
  m_loading = false;

  refreshFontPreview();
}

void SkinEditorWidget::applyFontRole()
{
  if(m_loading)
    return;
  QFont* f = roleFont(m_fontList);
  if(!f)
    return;

  f->setFamilies({m_fontFamily->currentText()});
  f->setPixelSize(m_fontSize->value());

  // Only pin a style the family really has, or Qt fakes it.
  const QString style = m_fontStyle->currentText();
  if(QFontDatabase::styles(m_fontFamily->currentText()).contains(style))
    f->setStyleName(style);
  else
    f->setStyleName(QString{});

  switch(m_fontHinting->currentIndex())
  {
    case 1:
      f->setHintingPreference(QFont::PreferNoHinting);
      break;
    case 2:
      f->setHintingPreference(QFont::PreferVerticalHinting);
      break;
    case 3:
      f->setHintingPreference(QFont::PreferFullHinting);
      break;
    default:
      f->setHintingPreference(QFont::PreferDefaultHinting);
      break;
  }

  auto strat = int(f->styleStrategy());
  if(m_fontAntialias->isChecked())
    strat &= ~int(QFont::NoAntialias);
  else
    strat |= int(QFont::NoAntialias);
  f->setStyleStrategy(QFont::StyleStrategy(strat));

  refreshFontPreview();

  m_applying = true;
  score::Skin::instance().changed();
  m_applying = false;
}

void SkinEditorWidget::refreshFontPreview()
{
  QFont* f = roleFont(m_fontList);
  if(!f)
    return;

  m_fontPreview->setFont(*f);

  // Name the grid multiples rather than leaving them to be found by squinting.
  const int grid = score::pixelFontGrid(f->families().value(0, f->family()));
  if(grid > 0)
  {
    QStringList ok;
    for(int k = 1; k <= 4; ++k)
      ok << QString::number(grid * k);
    m_fontHint->setText(tr("Pixel font: sharp at %1 px").arg(ok.join(", ")));
  }
  else
  {
    m_fontHint->clear();
  }
}
}
}

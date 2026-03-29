#include "LibavSettingsWidget.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/Libav/LibavDevice.hpp>
#include <Media/LibavIntrospection.hpp>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cfloat>
#include <climits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

namespace Gfx
{
static QString optionsToText(const ossia::hash_map<QString, QString>& options)
{
  QString text;
  for(auto& [k, v] : options)
  {
    if(!text.isEmpty())
      text += '\n';
    text += k + "=" + v;
  }
  return text;
}

static ossia::hash_map<QString, QString> textToOptions(const QString& text)
{
  ossia::hash_map<QString, QString> opts;
  for(auto& line : text.split('\n', Qt::SkipEmptyParts))
  {
    auto parts = line.split('=', Qt::SkipEmptyParts);
    if(parts.size() == 2)
      opts[parts[0].trimmed()] = parts[1].trimmed();
  }
  return opts;
}

// Option dialog

// ============================================================================
// FFMPEG OPTIONS DIALOG
// ============================================================================

static void populateOptionsTable(QTableWidget* table, const AVClass* klass)
{
  if(!klass)
    return;

  // First pass: collect all const entries grouped by unit
  std::map<std::string, std::vector<const AVOption*>> unit_consts;
  {
    const AVOption* opt = nullptr;
    while((opt = av_opt_next(&klass, opt)))
    {
      if(opt->type == AV_OPT_TYPE_CONST && opt->unit)
        unit_consts[opt->unit].push_back(opt);
    }
  }

  // Second pass: collect actual options (non-CONST)
  struct OptionInfo
  {
    QString name, type, defVal, range, description;
  };
  std::vector<OptionInfo> options;

  const AVOption* opt = nullptr;
  while((opt = av_opt_next(&klass, opt)))
  {
    if(opt->type == AV_OPT_TYPE_CONST)
      continue;

    OptionInfo info;
    info.name = QString::fromUtf8(opt->name);
    info.description = opt->help ? QString::fromUtf8(opt->help) : QString{};

    // Type string
    switch(opt->type)
    {
      case AV_OPT_TYPE_BOOL:
        info.type = "bool";
        info.defVal = opt->default_val.i64 ? "true" : "false";
        break;
      case AV_OPT_TYPE_INT:
        info.type = "int";
        info.defVal = QString::number(opt->default_val.i64);
        if(opt->min != INT_MIN || opt->max != INT_MAX)
          info.range = QString("[%1, %2]").arg((int64_t)opt->min).arg((int64_t)opt->max);
        break;
      case AV_OPT_TYPE_INT64:
        info.type = "int64";
        info.defVal = QString::number(opt->default_val.i64);
        break;
      case AV_OPT_TYPE_UINT64:
        info.type = "uint64";
        info.defVal = QString::number((quint64)opt->default_val.i64);
        break;
      case AV_OPT_TYPE_DOUBLE:
      case AV_OPT_TYPE_FLOAT:
        info.type = opt->type == AV_OPT_TYPE_FLOAT ? "float" : "double";
        info.defVal = QString::number(opt->default_val.dbl);
        if(opt->min != -DBL_MAX || opt->max != DBL_MAX)
          info.range = QString("[%1, %2]").arg(opt->min).arg(opt->max);
        break;
      case AV_OPT_TYPE_STRING:
        info.type = "string";
        info.defVal
            = opt->default_val.str ? QString::fromUtf8(opt->default_val.str) : QString{};
        break;
      case AV_OPT_TYPE_FLAGS:
        info.type = "flags";
        info.defVal = QString("0x%1").arg(opt->default_val.i64, 0, 16);
        break;
      case AV_OPT_TYPE_DURATION:
        info.type = "duration";
        break;
      case AV_OPT_TYPE_RATIONAL:
        info.type = "rational";
        break;
      default:
        info.type = "other";
        break;
    }

    // For enum-like int options, list allowed values from unit consts
    if(opt->unit)
    {
      auto it = unit_consts.find(opt->unit);
      if(it != unit_consts.end())
      {
        QStringList vals;
        for(auto* c : it->second)
        {
          QString entry = QString::fromUtf8(c->name);
          if(c->help && c->help[0])
            entry += " (" + QString::fromUtf8(c->help) + ")";
          vals << entry;
        }
        if(!vals.empty())
          info.range = vals.join(", ");
      }
    }

    options.push_back(std::move(info));
  }

  table->setRowCount(options.size());
  table->setColumnCount(5);
  table->setHorizontalHeaderLabels(
      {"Name", "Type", "Default", "Range / Values", "Description"});

  for(int row = 0; row < (int)options.size(); row++)
  {
    auto& o = options[row];
    table->setItem(row, 0, new QTableWidgetItem(o.name));
    table->setItem(row, 1, new QTableWidgetItem(o.type));
    table->setItem(row, 2, new QTableWidgetItem(o.defVal));
    table->setItem(row, 3, new QTableWidgetItem(o.range));
    table->setItem(row, 4, new QTableWidgetItem(o.description));
  }

  table->setWordWrap(true);
  table->resizeColumnsToContents();
  // Cap the Range/Values column so it wraps instead of stretching endlessly
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  table->horizontalHeader()->setStretchLastSection(true);
  table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

static void showOptionsDialog(
    QWidget* parent, const QString& vencoder, const QString& aencoder,
    const QString& muxer)
{
  auto dlg = new QDialog{parent};
  dlg->setWindowTitle("FFmpeg Options Reference");
  dlg->resize(800, 500);

  auto tabs = new QTabWidget{dlg};

  // Video codec tab
  if(!vencoder.isEmpty())
  {
    auto codec = avcodec_find_encoder_by_name(vencoder.toStdString().c_str());
    if(codec && codec->priv_class)
    {
      auto table = new QTableWidget{dlg};
      table->setEditTriggers(QAbstractItemView::NoEditTriggers);
      table->setSelectionBehavior(QAbstractItemView::SelectRows);
      populateOptionsTable(table, codec->priv_class);
      tabs->addTab(table, QString("Video: %1").arg(vencoder));
    }
  }

  // Audio codec tab
  if(!aencoder.isEmpty())
  {
    auto codec = avcodec_find_encoder_by_name(aencoder.toStdString().c_str());
    if(codec && codec->priv_class)
    {
      auto table = new QTableWidget{dlg};
      table->setEditTriggers(QAbstractItemView::NoEditTriggers);
      table->setSelectionBehavior(QAbstractItemView::SelectRows);
      populateOptionsTable(table, codec->priv_class);
      tabs->addTab(table, QString("Audio: %1").arg(aencoder));
    }
  }

  // Muxer tab
  if(!muxer.isEmpty())
  {
    auto fmt = av_guess_format(muxer.toStdString().c_str(), nullptr, nullptr);
    if(fmt && fmt->priv_class)
    {
      auto table = new QTableWidget{dlg};
      table->setEditTriggers(QAbstractItemView::NoEditTriggers);
      table->setSelectionBehavior(QAbstractItemView::SelectRows);
      populateOptionsTable(table, fmt->priv_class);
      tabs->addTab(table, QString("Muxer: %1").arg(muxer));
    }
  }

  // General format options tab
  {
    auto table = new QTableWidget{dlg};
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    populateOptionsTable(table, avformat_get_class());
    tabs->addTab(table, "General");
  }

  auto layout = new QVBoxLayout{dlg};
  layout->addWidget(tabs);
  auto buttons = new QDialogButtonBox{QDialogButtonBox::Close, dlg};
  QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
  layout->addWidget(buttons);

  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
}

// Main widget
LibavSettingsWidget::LibavSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  m_direction = new QComboBox{this};
  m_direction->addItem(tr("Input"), (int)LibavSettings::Input);
  m_direction->addItem(tr("Output"), (int)LibavSettings::Output);

  m_stack = new QStackedWidget{this};

  // ── Input page ──
  {
    auto* page = new QWidget;
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);

    m_inPath = new QLineEdit{page};
    m_inPath->setPlaceholderText("rtsp://... or /path/to/frames_%05d.png");

    m_inOptions = new QPlainTextEdit{page};
    m_inOptions->setMaximumHeight(80);
    m_inOptions->setPlaceholderText(
        "key=value (one per line)\ne.g. framerate=30, start_number=0");

    form->addRow(tr("Path / URL"), m_inPath);
    form->addRow(tr("Options"), m_inOptions);

    auto* pageLayout = new QVBoxLayout{page};
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addLayout(form);
    pageLayout->addStretch(1);

    m_stack->addWidget(page);
  }

  // ── Output page ──
  {
    auto* page = new QWidget;
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);

    m_outPath = new QLineEdit{page};
    m_outPath->setPlaceholderText("/path/to/output.mp4 or udp://...");

    m_width = new QSpinBox{page};
    m_width->setRange(1, 7680);
    m_width->setValue(1280);
    m_height = new QSpinBox{page};
    m_height->setRange(1, 4320);
    m_height->setValue(720);
    m_rate = new QSpinBox{page};
    m_rate->setRange(1, 240);
    m_rate->setValue(30);
    m_audioChannels = new QSpinBox{page};
    m_audioChannels->setRange(0, 8);
    m_audioChannels->setValue(2);

    m_muxer = new QComboBox{page};
    m_muxer->addItem("");
    for(auto& m : LibavIntrospection::instance().muxers)
      if(m.format && m.format->name)
        m_muxer->addItem(QString::fromUtf8(m.format->name));

    m_vencoder = new QComboBox{page};
    m_vencoder->addItem("");
    for(auto& c : LibavIntrospection::instance().vcodecs)
      if(c.codec && c.codec->name)
        m_vencoder->addItem(QString::fromUtf8(c.codec->name));

    m_aencoder = new QComboBox{page};
    m_aencoder->addItem("");
    for(auto& c : LibavIntrospection::instance().acodecs)
      if(c.codec && c.codec->name)
        m_aencoder->addItem(QString::fromUtf8(c.codec->name));

    m_pixfmt = new QComboBox{page};
    m_pixfmt->addItem("");
    m_pixfmt->addItem("rgba");
    m_pixfmt->addItem("yuv420p");
    m_pixfmt->addItem("yuv422p");
    m_pixfmt->addItem("yuv444p");
    m_pixfmt->addItem("nv12");

    m_smpfmt = new QComboBox{page};
    m_smpfmt->addItem("");
    m_smpfmt->addItem("s16");
    m_smpfmt->addItem("s32");
    m_smpfmt->addItem("flt");
    m_smpfmt->addItem("fltp");

    m_outOptions = new QPlainTextEdit{page};
    m_outOptions->setMaximumHeight(80);
    m_outOptions->setPlaceholderText(
        "key=value (one per line)\ne.g. crf=23, preset=fast");

    m_showOptions = new QPushButton{tr("Show options..."), page};

    form->addRow(tr("Path / URL"), m_outPath);
    form->addRow(tr("Width"), m_width);
    form->addRow(tr("Height"), m_height);
    form->addRow(tr("Rate"), m_rate);
    form->addRow(tr("Audio Channels"), m_audioChannels);
    form->addRow(tr("Muxer"), m_muxer);
    form->addRow(tr("Video Encoder"), m_vencoder);
    form->addRow(tr("Audio Encoder"), m_aencoder);
    form->addRow(tr("Pixel Format"), m_pixfmt);
    form->addRow(tr("Sample Format"), m_smpfmt);
    m_inputTransfer = new QComboBox{page};
    m_inputTransfer->addItem(tr("sRGB (default)"), 13);
    m_inputTransfer->addItem(tr("Linear"), 8);
    m_inputTransfer->addItem(tr("HDR10 (PQ)"), 16);
    m_inputTransfer->addItem(tr("HLG"), 18);
    m_inputTransfer->addItem(tr("Passthrough"), 2);

    m_validationLabel = new QLabel{page};
    m_validationLabel->setWordWrap(true);

    form->addRow(tr("Options"), m_outOptions);
    form->addRow("", m_showOptions);
    form->addRow(tr("Input Transfer"), m_inputTransfer);
    form->addRow(m_validationLabel);

    auto* pageLayout = new QVBoxLayout{page};
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addLayout(form);
    pageLayout->addStretch(1);

    m_stack->addWidget(page);
  }

  auto* topLayout = new QFormLayout;
  topLayout->addRow(tr("Device Name"), m_deviceNameEdit);
  topLayout->addRow(tr("Direction"), m_direction);

  auto* outerLayout = new QVBoxLayout;
  outerLayout->addLayout(topLayout);
  outerLayout->addWidget(m_stack);
  outerLayout->addStretch(1);
  setLayout(outerLayout);

  // Align label columns across all three form layouts:
  // find the widest label, then set that as minimum width for all labels.
  {
    QList<QFormLayout*> forms;
    forms << topLayout;
    // Retrieve the form layouts from the stacked pages (inside their QVBoxLayouts)
    for(int p = 0; p < m_stack->count(); p++)
    {
      auto* pageLayout = m_stack->widget(p)->layout();
      if(auto* vbox = qobject_cast<QVBoxLayout*>(pageLayout))
      {
        if(auto* form = qobject_cast<QFormLayout*>(vbox->itemAt(0)->layout()))
          forms << form;
      }
    }

    int maxLabelWidth = 0;
    for(auto* form : forms)
    {
      for(int r = 0; r < form->rowCount(); r++)
      {
        if(auto* label = form->itemAt(r, QFormLayout::LabelRole))
        {
          if(auto* w = label->widget())
            maxLabelWidth = std::max(maxLabelWidth, w->sizeHint().width());
        }
      }
    }

    for(auto* form : forms)
    {
      for(int r = 0; r < form->rowCount(); r++)
      {
        if(auto* label = form->itemAt(r, QFormLayout::LabelRole))
        {
          if(auto* w = label->widget())
            w->setMinimumWidth(maxLabelWidth);
        }
      }
    }
  }

  m_deviceNameEdit->setText("FFmpeg");
  m_direction->setCurrentIndex(0);

  connect(
      m_direction, qOverload<int>(&QComboBox::currentIndexChanged), this,
      [this](int) { onDirectionChanged(); });
  connect(m_showOptions, &QPushButton::clicked, this, [this] {
    showOptionsDialog(
        this, m_vencoder->currentText(), m_aencoder->currentText(),
        m_muxer->currentText());
  });
  connect(m_muxer, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    onMuxerChanged();
    validateOutput();
  });
  connect(
      m_vencoder, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    onVEncoderChanged();
    validateOutput();
  });
  connect(
      m_aencoder, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    onAEncoderChanged();
    validateOutput();
  });

  onDirectionChanged();
}

void LibavSettingsWidget::validateOutput()
{
  if(!m_validationLabel)
    return;

  QStringList errors;

  auto muxerName = m_muxer->currentText();
  auto vencoderName = m_vencoder->currentText();
  auto aencoderName = m_aencoder->currentText();

  // Check muxer
  if(!muxerName.isEmpty())
  {
    auto* fmt = av_guess_format(muxerName.toStdString().c_str(), nullptr, nullptr);
    if(!fmt)
      errors << tr("Muxer '%1' not found").arg(muxerName);
  }

  // Check video encoder
  if(!vencoderName.isEmpty())
  {
    auto* codec = avcodec_find_encoder_by_name(vencoderName.toStdString().c_str());
    if(!codec)
      errors << tr("Video encoder '%1' not found").arg(vencoderName);
  }

  // Check audio encoder
  if(!aencoderName.isEmpty())
  {
    auto* codec = avcodec_find_encoder_by_name(aencoderName.toStdString().c_str());
    if(!codec)
      errors << tr("Audio encoder '%1' not found").arg(aencoderName);
  }

  if(errors.isEmpty())
  {
    if(!muxerName.isEmpty() || !vencoderName.isEmpty() || !aencoderName.isEmpty())
      m_validationLabel->setText(
          tr("<span style='color: green;'>Configuration valid</span>"));
    else
      m_validationLabel->clear();
  }
  else
  {
    m_validationLabel->setText(
        tr("<span style='color: red;'>%1</span>").arg(errors.join("<br>")));
  }
}

void LibavSettingsWidget::onDirectionChanged()
{
  bool isOutput = m_direction->currentData().toInt() == (int)LibavSettings::Output;
  m_stack->setCurrentIndex(isOutput ? 1 : 0);
}

void LibavSettingsWidget::onMuxerChanged()
{
  auto muxerName = m_muxer->currentText();
  if(muxerName.isEmpty())
    return;

  // Find the MuxerInfo for the selected muxer
  const MuxerInfo* minfo = nullptr;
  for(auto& m : LibavIntrospection::instance().muxers)
  {
    if(m.format && m.format->name && muxerName == m.format->name)
    {
      minfo = &m;
      break;
    }
  }
  if(!minfo)
    return;

  // Filter video encoders
  auto prevV = m_vencoder->currentText();
  m_vencoder->blockSignals(true);
  m_vencoder->clear();
  m_vencoder->addItem("");
  if(!minfo->vcodecs.empty())
  {
    for(auto* vc : minfo->vcodecs)
      if(vc->codec && vc->codec->name)
        m_vencoder->addItem(QString::fromUtf8(vc->codec->name));
  }
  else
  {
    // No restriction — show all
    for(auto& c : LibavIntrospection::instance().vcodecs)
      if(c.codec && c.codec->name)
        m_vencoder->addItem(QString::fromUtf8(c.codec->name));
  }
  // Restore previous selection if still available
  int idx = m_vencoder->findText(prevV);
  m_vencoder->setCurrentIndex(idx >= 0 ? idx : 0);
  m_vencoder->blockSignals(false);

  // Filter audio encoders
  auto prevA = m_aencoder->currentText();
  m_aencoder->blockSignals(true);
  m_aencoder->clear();
  m_aencoder->addItem("");
  if(!minfo->acodecs.empty())
  {
    for(auto* ac : minfo->acodecs)
      if(ac->codec && ac->codec->name)
        m_aencoder->addItem(QString::fromUtf8(ac->codec->name));
  }
  else
  {
    for(auto& c : LibavIntrospection::instance().acodecs)
      if(c.codec && c.codec->name)
        m_aencoder->addItem(QString::fromUtf8(c.codec->name));
  }
  idx = m_aencoder->findText(prevA);
  m_aencoder->setCurrentIndex(idx >= 0 ? idx : 0);
  m_aencoder->blockSignals(false);

  onVEncoderChanged();
  onAEncoderChanged();
}

void LibavSettingsWidget::onVEncoderChanged()
{
  auto codecName = m_vencoder->currentText();
  auto prev = m_pixfmt->currentText();
  m_pixfmt->clear();
  m_pixfmt->addItem("");

  if(codecName.isEmpty())
    return;

  auto codec = avcodec_find_encoder_by_name(codecName.toStdString().c_str());
  if(!codec)
    return;

  // Query supported pixel formats
  const AVPixelFormat* fmts = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
  avcodec_get_supported_config(
      nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void**)&fmts, nullptr);
#else
  fmts = codec->pix_fmts;
#endif

  if(fmts)
  {
    for(int i = 0; fmts[i] != AV_PIX_FMT_NONE; i++)
    {
      auto name = av_get_pix_fmt_name(fmts[i]);
      if(name)
        m_pixfmt->addItem(QString::fromUtf8(name));
    }
  }
  else
  {
    // No restriction — show common formats
    m_pixfmt->addItem("yuv420p");
    m_pixfmt->addItem("yuv422p");
    m_pixfmt->addItem("yuv444p");
    m_pixfmt->addItem("rgba");
    m_pixfmt->addItem("nv12");
  }

  int idx = m_pixfmt->findText(prev);
  if(idx >= 0)
    m_pixfmt->setCurrentIndex(idx);
  else if(m_pixfmt->count() > 1)
    m_pixfmt->setCurrentIndex(1); // First actual format (skip empty)
  else
    m_pixfmt->setCurrentIndex(0);
}

void LibavSettingsWidget::onAEncoderChanged()
{
  auto codecName = m_aencoder->currentText();
  auto prev = m_smpfmt->currentText();
  m_smpfmt->clear();
  m_smpfmt->addItem("");

  if(codecName.isEmpty())
    return;

  auto codec = avcodec_find_encoder_by_name(codecName.toStdString().c_str());
  if(!codec)
    return;

  // Query supported sample formats
  const AVSampleFormat* fmts = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
  avcodec_get_supported_config(
      nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&fmts, nullptr);
#else
  fmts = codec->sample_fmts;
#endif

  if(fmts)
  {
    for(int i = 0; fmts[i] != AV_SAMPLE_FMT_NONE; i++)
    {
      auto name = av_get_sample_fmt_name(fmts[i]);
      if(name)
        m_smpfmt->addItem(QString::fromUtf8(name));
    }
  }
  else
  {
    m_smpfmt->addItem("s16");
    m_smpfmt->addItem("s32");
    m_smpfmt->addItem("flt");
    m_smpfmt->addItem("fltp");
  }

  int idx = m_smpfmt->findText(prev);
  if(idx >= 0)
    m_smpfmt->setCurrentIndex(idx);
  else if(m_smpfmt->count() > 1)
    m_smpfmt->setCurrentIndex(1);
  else
    m_smpfmt->setCurrentIndex(0);
}

Device::DeviceSettings LibavSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = LibavProtocolFactory::static_concreteKey();

  LibavSettings set;
  set.direction = (LibavSettings::Direction)m_direction->currentData().toInt();

  if(set.direction == LibavSettings::Input)
  {
    set.path = m_inPath->text();
    set.options = textToOptions(m_inOptions->toPlainText());
  }
  else
  {
    set.path = m_outPath->text();
    set.width = m_width->value();
    set.height = m_height->value();
    set.rate = m_rate->value();
    set.audio_channels = m_audioChannels->value();
    set.muxer = m_muxer->currentText();
    set.video_encoder_short = m_vencoder->currentText();
    set.audio_encoder_short = m_aencoder->currentText();
    set.video_render_pixfmt = "rgba";
    set.video_converted_pixfmt = m_pixfmt->currentText();
    set.audio_converted_smpfmt = m_smpfmt->currentText();
    set.options = textToOptions(m_outOptions->toPlainText());
    set.input_transfer = m_inputTransfer->currentData().toInt();
  }

  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void LibavSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.trimmed();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_deviceNameEdit->setText(prettyName);

  if(settings.deviceSpecificSettings.canConvert<LibavSettings>())
  {
    const auto& set = settings.deviceSpecificSettings.value<LibavSettings>();
    m_direction->setCurrentIndex(m_direction->findData((int)set.direction));

    if(set.direction == LibavSettings::Input)
    {
      m_inPath->setText(set.path);
      m_inOptions->setPlainText(optionsToText(set.options));
    }
    else
    {
      m_outPath->setText(set.path);
      m_width->setValue(set.width);
      m_height->setValue(set.height);
      m_rate->setValue(set.rate);
      m_audioChannels->setValue(set.audio_channels);
      m_muxer->setCurrentText(set.muxer);
      m_vencoder->setCurrentText(set.video_encoder_short);
      m_aencoder->setCurrentText(set.audio_encoder_short);
      m_pixfmt->setCurrentText(set.video_converted_pixfmt);
      m_smpfmt->setCurrentText(set.audio_converted_smpfmt);
      m_outOptions->setPlainText(optionsToText(set.options));
      int idx = m_inputTransfer->findData(set.input_transfer);
      if(idx >= 0)
        m_inputTransfer->setCurrentIndex(idx);
    }

    onDirectionChanged();
  }
}

}
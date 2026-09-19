#include <Gris/Algo/SpeakerSetupIO.hpp>
#include <Gris/SpeakerSetupInlet.hpp>

#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/tools/FilePath.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <verdigris>

namespace Gris
{
SpeakerSetupInlet::SpeakerSetupInlet(
    const QString& name, Id<Process::Port> id, QObject* parent)
    : ControlInlet{name, id, parent}
{
  displayHandledExplicitly = true;
  setValue(std::string{});
  setInit(value());
}

SpeakerSetupInlet::SpeakerSetupInlet(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
SpeakerSetupInlet::SpeakerSetupInlet(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
SpeakerSetupInlet::SpeakerSetupInlet(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
SpeakerSetupInlet::SpeakerSetupInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}

SpeakerSetupInlet::~SpeakerSetupInlet() = default;

SpeakerSetup SpeakerSetupInlet::setup() const noexcept
{
  auto const str = ossia::convert<std::string>(value());
  if(str.empty())
    return {};

  auto res = readSpeakerSetup(QByteArray::fromStdString(str));
  if(!res)
    return {};
  return *res.setup;
}

void SpeakerSetupInlet::setSetup(SpeakerSetup const& s)
{
  setValue(writeSpeakerSetup(s).toStdString());
}
} // namespace Gris

//==============================================================================
namespace
{
/** SpatGRIS's speaker table, rebuilt with Qt.
 *
 * One row per speaker; positions are shown in both coordinate systems, as in
 * SpatGRIS, and editing either updates the other. Everything score's own mixer
 * covers -- mute/solo, master gain -- is deliberately absent.
 */
class SpeakerTable : public QWidget
{
  W_OBJECT(SpeakerTable)
public:
  enum Column
  {
    Patch = 0,
    X,
    Y,
    Z,
    Azimuth,
    Elevation,
    Distance,
    Gain,
    Highpass,
    DirectOut,
    ColumnCount
  };

  const score::DocumentContext& ctx;
  QTableWidget* table{};
  QComboBox* spatMode{};
  QLabel* summary{};

  explicit SpeakerTable(const score::DocumentContext& c, QWidget* parent = nullptr)
      : QWidget{parent}
      , ctx{c}
  {
    setContentsMargins(0, 0, 0, 0);
    auto lay = new QVBoxLayout{this};

    auto topLay = new QHBoxLayout;
    topLay->addWidget(new QLabel{tr("Mode")});
    spatMode = new QComboBox;
    spatMode->addItems({tr("Dome (VBAP)"), tr("Cube (MBAP)"), tr("Hybrid")});
    topLay->addWidget(spatMode);
    topLay->addStretch(1);

    auto load = new QPushButton{tr("Load…")};
    auto save = new QPushButton{tr("Save…")};
    topLay->addWidget(load);
    topLay->addWidget(save);
    lay->addLayout(topLay);

    table = new QTableWidget;
    table->setColumnCount(ColumnCount);
    table->setHorizontalHeaderLabels(
        {tr("Out"), tr("X"), tr("Y"), tr("Z"), tr("Azimuth"), tr("Elevation"),
         tr("Distance"), tr("Gain (dB)"), tr("Highpass"), tr("Direct out")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    lay->addWidget(table, 1);

    auto btnLay = new QHBoxLayout;
    auto add = new QPushButton{tr("+")};
    auto rm = new QPushButton{tr("-")};
    btnLay->addWidget(add);
    btnLay->addWidget(rm);
    summary = new QLabel;
    btnLay->addWidget(summary, 1);
    lay->addLayout(btnLay);

    connect(add, &QPushButton::clicked, this, &SpeakerTable::onAddSpeaker);
    connect(rm, &QPushButton::clicked, this, &SpeakerTable::onRemoveSpeaker);
    connect(load, &QPushButton::clicked, this, &SpeakerTable::onLoad);
    connect(save, &QPushButton::clicked, this, &SpeakerTable::onSave);
    connect(
        spatMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int idx) {
      m_setup.spatMode = static_cast<Gris::SpatMode>(idx);
      setupChanged();
        });
    connect(table, &QTableWidget::itemChanged, this, &SpeakerTable::onCellEdited);
  }

  [[nodiscard]] Gris::SpeakerSetup const& setup() const noexcept { return m_setup; }

  void setSetup(Gris::SpeakerSetup s)
  {
    m_setup = std::move(s);
    rebuild();
  }

  void setupChanged() W_SIGNAL(setupChanged);

private:
  Gris::SpeakerSetup m_setup{};
  bool m_updating{};

  [[nodiscard]] Gris::SpeakerEntry* entryAt(int row) noexcept
  {
    int i = 0;
    for(auto& group : m_setup.groups)
      for(auto& speaker : group.speakers)
        if(i++ == row)
          return &speaker;
    return nullptr;
  }

  void setCell(int row, int col, double value)
  {
    auto* item = new QTableWidgetItem{QString::number(value, 'f', 4)};
    table->setItem(row, col, item);
  }

  void rebuild()
  {
    QSignalBlocker blocker{table};
    m_updating = true;

    {
      QSignalBlocker modeBlocker{spatMode};
      auto idx = static_cast<int>(m_setup.spatMode);
      spatMode->setCurrentIndex(idx < 0 ? 0 : idx);
    }

    auto const flat = m_setup.flattened();
    table->setRowCount(int(flat.size()));

    int row = 0;
    for(auto const& group : m_setup.groups)
    {
      for(auto const& speaker : group.speakers)
      {
        auto const cart = speaker.data.position.getCartesian();
        auto const polar = speaker.data.position.getPolar();

        table->setItem(
            row, Patch, new QTableWidgetItem{QString::number(speaker.patch.get())});
        setCell(row, X, cart.x);
        setCell(row, Y, cart.y);
        setCell(row, Z, cart.z);
        setCell(row, Azimuth, polar.azimuth.getAsDegrees());
        setCell(row, Elevation, polar.elevation.getAsDegrees());
        setCell(row, Distance, polar.length);
        setCell(row, Gain, speaker.data.gain);
        setCell(row, Highpass, speaker.data.highpassFreq);

        auto* direct = new QTableWidgetItem{};
        direct->setFlags(
            Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        direct->setCheckState(
            speaker.data.isDirectOutOnly ? Qt::Checked : Qt::Unchecked);
        table->setItem(row, DirectOut, direct);

        ++row;
      }
    }

    summary->setText(tr("%1 speakers, %2 spatialized, %3 output channels")
                         .arg(flat.size())
                         .arg(m_setup.numSpatializedSpeakers())
                         .arg(m_setup.maxOutputPatch()));
    m_updating = false;
  }

  void onCellEdited(QTableWidgetItem* item)
  {
    if(m_updating || !item)
      return;

    auto* entry = entryAt(item->row());
    if(!entry)
      return;

    bool ok{};
    auto const number = item->text().toDouble(&ok);

    switch(item->column())
    {
      case Patch: {
        auto const patch = item->text().toInt(&ok);
        if(ok && patch >= 1 && patch <= Gris::MAX_NUM_SPEAKERS)
          entry->patch = Gris::output_patch_t{patch};
        break;
      }
      case X:
      case Y:
      case Z: {
        if(!ok)
          break;
        auto c = entry->data.position.getCartesian();
        if(item->column() == X)
          c.x = float(number);
        else if(item->column() == Y)
          c.y = float(number);
        else
          c.z = float(number);
        entry->data.position = Gris::Position{c};
        break;
      }
      case Azimuth:
      case Elevation:
      case Distance: {
        if(!ok)
          break;
        auto p = entry->data.position.getPolar();
        if(item->column() == Azimuth)
          p.azimuth = Gris::degrees_t{float(number)}.toRadians();
        else if(item->column() == Elevation)
          p.elevation = Gris::degrees_t{float(number)}.toRadians();
        else
          p.length = float(number);
        entry->data.position = Gris::Position{p};
        break;
      }
      case Gain:
        if(ok)
          entry->data.gain = float(number);
        break;
      case Highpass:
        if(ok)
          entry->data.highpassFreq = float(number);
        break;
      case DirectOut:
        entry->data.isDirectOutOnly = item->checkState() == Qt::Checked;
        break;
      default:
        break;
    }

    // Editing one coordinate system has to refresh the other.
    rebuild();
    setupChanged();
  }

  void onAddSpeaker()
  {
    if(m_setup.groups.empty())
    {
      Gris::SpeakerGroup group;
      group.name = "Main Speaker Group";
      m_setup.groups.push_back(std::move(group));
    }

    Gris::SpeakerEntry entry;
    entry.patch = Gris::output_patch_t{m_setup.maxOutputPatch() + 1};
    entry.data.position
        = Gris::Position{Gris::PolarVector{Gris::radians_t{}, Gris::radians_t{}, 1.f}};
    m_setup.groups.back().speakers.push_back(entry);

    rebuild();
    setupChanged();
  }

  void onRemoveSpeaker()
  {
    auto const selected = table->selectionModel()->selectedRows();
    if(selected.isEmpty())
      return;

    std::vector<int> rows;
    for(auto const& index : selected)
      rows.push_back(index.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>{});

    for(int row : rows)
    {
      int i = 0;
      bool done = false;
      for(auto& group : m_setup.groups)
      {
        for(auto it = group.speakers.begin(); it != group.speakers.end(); ++it)
        {
          if(i++ == row)
          {
            group.speakers.erase(it);
            done = true;
            break;
          }
        }
        if(done)
          break;
      }
    }

    rebuild();
    setupChanged();
  }

  void onLoad()
  {
    auto const path = QFileDialog::getOpenFileName(
        this, tr("Load a SpatGRIS speaker setup"), score::pickerStartFolder({}, ctx),
        tr("Speaker setups (*.xml)"));
    if(path.isEmpty())
      return;

    auto res = Gris::readSpeakerSetupFile(path);
    if(!res)
    {
      summary->setText(tr("Could not read %1: %2").arg(path, res.error));
      return;
    }

    m_setup = std::move(*res.setup);
    rebuild();
    setupChanged();
  }

  void onSave()
  {
    auto const path = QFileDialog::getSaveFileName(
        this, tr("Save the speaker setup"), score::pickerStartFolder({}, ctx),
        tr("Speaker setups (*.xml)"));
    if(path.isEmpty())
      return;

    if(auto const err = Gris::writeSpeakerSetupFile(m_setup, path); !err.isEmpty())
      summary->setText(err);
  }
};
} // namespace

//==============================================================================
QWidget* WidgetFactory::SpeakerSetupWidget::make_widget(
    const Gris::SpeakerSetupInlet& inlet, const score::DocumentContext& ctx,
    QWidget* parent, QObject* context)
{
  auto widget = new SpeakerTable{ctx, parent};
  widget->setSetup(inlet.setup());

  QObject::connect(widget, &SpeakerTable::setupChanged, context, [widget, &inlet, &ctx] {
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::SetControlValue>(
        inlet, ossia::value{Gris::writeSpeakerSetup(widget->setup()).toStdString()});
  });

  QObject::connect(
      &inlet, &Gris::SpeakerSetupInlet::valueChanged, widget,
      [widget](const ossia::value& v) {
    auto const str = ossia::convert<std::string>(v);
    if(str.empty())
      return;
    if(auto res = Gris::readSpeakerSetup(QByteArray::fromStdString(str)))
      widget->setSetup(std::move(*res.setup));
      });

  return widget;
}

QGraphicsItem* WidgetFactory::SpeakerSetupWidget::make_item(
    const Gris::SpeakerSetupInlet& slider, const Gris::SpeakerSetupInlet& inlet,
    const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  // The table lives in the inspector; the timeline slot stays empty.
  return new score::EmptyItem{parent};
}

W_OBJECT_IMPL(SpeakerTable)

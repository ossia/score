#include "ProfilerPanel.hpp"

#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/Profiler/Profiler.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>


namespace Execution
{
namespace
{
enum Column
{
  NameCol,
  AudioPercentCol,
  AudioUsCol,
  RenderCpuCol,
  GpuCol,
  GpuMaxCol,
  TargetsCol,
  ColumnCount
};

constexpr int ValueRole = Qt::UserRole + 1;
constexpr int FractionRole = Qt::UserRole + 2;
constexpr int IsGroupRole = Qt::UserRole + 3;
constexpr int KeyRole = Qt::UserRole + 3;

class Item final : public QTreeWidgetItem
{
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem& other) const override
  {
    const int c = treeWidget() ? treeWidget()->sortColumn() : 0;
    if(c == NameCol)
      return text(c).localeAwareCompare(other.text(c)) < 0;
    return data(c, ValueRole).toDouble() < other.data(c, ValueRole).toDouble();
  }
};

//! Draws each cost as a bar proportional to the largest one of its column.
class BarDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx)
      const override
  {
    const double f = idx.data(FractionRole).toDouble();
    if(f > 0.)
    {
      QColor c = opt.palette.color(QPalette::Highlight);
      c.setAlphaF(0.35);
      QRectF r = opt.rect.adjusted(1, 2, -1, -2);
      r.setWidth(r.width() * std::min(1., f));
      p->fillRect(r, c);
    }
    QStyledItemDelegate::paint(p, opt, idx);
  }
};

//! Audio load and frame times over the last reports and frames.
class Timeline final : public QWidget
{
public:
  explicit Timeline(const Profiler& p, QWidget* parent)
      : QWidget{parent}
      , m_profiler{p}
  {
    setMinimumHeight(70);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    const auto& pal = palette();
    p.fillRect(rect(), pal.color(QPalette::Base));

    const int half = width() / 2;
    drawPlot(
        p, QRectF(0, 0, half - 4, height()), tr("Audio load (%)"),
        {{&m_profiler.audioHistory(), QColor(80, 170, 255)}}, 10.);
    drawPlot(
        p, QRectF(half + 4, 0, width() - half - 4, height()), tr("Frame (ms): GPU, render CPU"),
        {{&m_profiler.gpuHistory(), QColor(255, 140, 60)},
         {&m_profiler.renderCpuHistory(), QColor(120, 200, 120)}},
        0.);
  }

private:
  struct Line
  {
    const std::deque<float>* values;
    QColor color;
  };

  void drawPlot(QPainter& p, QRectF r, const QString& title, std::vector<Line> lines, double minMax)
  {
    const auto& pal = palette();
    p.setPen(pal.color(QPalette::Mid));
    p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));

    double top = 0.;
    for(auto& l : lines)
      for(float v : *l.values)
        top = std::max(top, double(v));
    top = std::max(top * 1.15, minMax > 0. ? minMax : 1.);

    const QRectF plot = r.adjusted(2, 14, -2, -2);
    for(auto& l : lines)
    {
      const auto& v = *l.values;
      if(v.size() < 2)
        continue;
      QPainterPath path;
      const double dx = plot.width() / double(v.size() - 1);
      for(std::size_t i = 0; i < v.size(); i++)
      {
        const QPointF pt{
            plot.left() + i * dx,
            plot.bottom() - plot.height() * std::clamp(double(v[i]) / top, 0., 1.)};
        if(i == 0)
          path.moveTo(pt);
        else
          path.lineTo(pt);
      }
      p.setPen(QPen(l.color, 1.2));
      p.drawPath(path);
    }

    p.setPen(pal.color(QPalette::Text));
    p.drawText(
        r.adjusted(4, 1, -4, 0), Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("%1 — max %2").arg(title).arg(top, 0, 'g', 3));
  }

  const Profiler& m_profiler;
};

QString ms(double ns)
{
  return ns < 0. ? QString{} : QString::number(ns / 1e6, 'f', 3);
}
}

class ProfilerWidget final : public QWidget
{
public:
  ProfilerWidget(const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
      , m_ctx{ctx}
      , m_profiler{ctx.plugin<Execution::DocumentPlugin>().profiler()}
  {
    auto& settings = ctx.app.settings<Execution::Settings::Model>();
    auto lay = new QVBoxLayout{this};
    lay->setContentsMargins(2, 2, 2, 2);

    auto bar = new QHBoxLayout;
    auto enable = new QCheckBox{tr("Benchmark"), this};
    enable->setChecked(settings.getBench());
    score::setHelp(
        enable, tr("Measure every process while the score plays: audio timings start "
                   "with the next play, render timings at once"));
    connect(enable, &QCheckBox::toggled, this, [&settings](bool b) {
      settings.setBench(b);
    });
    connect(&settings, &Execution::Settings::Model::BenchChanged, enable, [enable](bool b) {
      QSignalBlocker block{enable};
      enable->setChecked(b);
    });
    bar->addWidget(enable);

    bar->addWidget(new QLabel{tr("Audio spike above"), this});
    auto audioSpike = m_audioSpike = new QDoubleSpinBox{this};
    audioSpike->setRange(0., 1000.);
    audioSpike->setSuffix(QStringLiteral(" %"));
    audioSpike->setValue(m_profiler.audioSpikePercent());
    score::setHelp(
        audioSpike, tr("Keep the audio ticks that took longer than this part of their "
                       "duration (0: never)"));
    connect(audioSpike, &QDoubleSpinBox::valueChanged, this, [this](double v) {
      m_profiler.setAudioSpikePercent(v);
    });
    bar->addWidget(audioSpike);

    bar->addWidget(new QLabel{tr("Frame spike above"), this});
    auto renderSpike = m_renderSpike = new QDoubleSpinBox{this};
    renderSpike->setRange(0., 1000.);
    renderSpike->setDecimals(2);
    renderSpike->setSuffix(QStringLiteral(" ms"));
    renderSpike->setValue(m_profiler.renderSpikeMs());
    score::setHelp(
        renderSpike, tr("Keep the frames whose GPU or render-thread time exceeds this "
                        "(0: never)"));
    connect(renderSpike, &QDoubleSpinBox::valueChanged, this, [this](double v) {
      m_profiler.setRenderSpikeMs(v);
    });
    bar->addWidget(renderSpike);

    m_selectedOnly = new QCheckBox{tr("Selected only"), this};
    connect(m_selectedOnly, &QCheckBox::toggled, this, [this] { refresh(); });
    bar->addWidget(m_selectedOnly);
    bar->addStretch(1);

    auto clear = new QPushButton{tr("Clear"), this};
    connect(clear, &QPushButton::clicked, this, [this] { m_profiler.clear(); });
    bar->addWidget(clear);
    auto save = new QPushButton{tr("Export…"), this};
    connect(save, &QPushButton::clicked, this, [this] { exportJson(); });
    bar->addWidget(save);
    lay->addLayout(bar);

    m_timeline = new Timeline{m_profiler, this};
    lay->addWidget(m_timeline);

    m_summary = new QLabel{this};
    lay->addWidget(m_summary);

    auto split = new QSplitter{Qt::Vertical, this};
    m_tree = new QTreeWidget{split};
    m_tree->setColumnCount(ColumnCount);
    m_tree->setHeaderLabels(
        {tr("Process"), tr("Audio %"), tr("Audio µs"), tr("Render CPU ms"), tr("GPU ms"),
         tr("GPU max ms"), tr("Targets MB")});
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(GpuCol, Qt::DescendingOrder);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setSectionResizeMode(NameCol, QHeaderView::Stretch);
    m_tree->header()->setStretchLastSection(false);
    auto bars = new BarDelegate{m_tree};
    for(int c = AudioPercentCol; c < ColumnCount; c++)
      m_tree->setItemDelegateForColumn(c, bars);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* it, int) {
      selectItem(it);
    });

    m_spikes = new QTreeWidget{split};
    m_spikes->setColumnCount(4);
    m_spikes->setHeaderLabels(
        {tr("Spike"), tr("When (s)"), tr("Took ms"), tr("Budget / GPU ms")});
    m_spikes->setUniformRowHeights(true);
    m_spikes->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    split->addWidget(m_tree);
    split->addWidget(m_spikes);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);

    connect(&m_profiler, &Profiler::updated, this, [this] { m_dirty = true; });
    startTimer(250);
    refresh();
  }

private:
  void showEvent(QShowEvent* e) override
  {
    QWidget::showEvent(e);
    refresh();
  }

  void timerEvent(QTimerEvent*) override
  {
    if(m_dirty && isVisible())
      refresh();
  }

  void selectItem(QTreeWidgetItem* it)
  {
    const auto key = it->data(NameCol, KeyRole).value<quintptr>();
    auto e = m_profiler.entries().find(key);
    if(e == m_profiler.entries().end() || !e->second.process)
      return;
    m_ctx.selectionStack.pushNewSelection(
        {const_cast<Process::ProcessModel*>(e->second.process.data())});
  }

  QTreeWidgetItem* groupFor(const Process::ProcessModel* proc)
  {
    QStringList path;
    const QObject* leaf{};
    if(proc)
    {
      for(auto* o = proc->parent(); o; o = o->parent())
      {
        if(auto itv = qobject_cast<const Scenario::IntervalModel*>(o))
        {
          path.prepend(itv->metadata().getName());
          if(!leaf)
            leaf = itv;
        }
      }
    }
    else
    {
      path << tr("Render outputs");
    }

    QTreeWidgetItem* parent = nullptr;
    QString key;
    for(const auto& name : path)
    {
      key += QLatin1Char('/') + name;
      auto it = m_groups.find(key);
      if(it == m_groups.end())
      {
        auto item = parent ? new Item{parent} : new Item{m_tree};
        item->setText(NameCol, name);
        item->setData(NameCol, IsGroupRole, true);
        item->setExpanded(true);
        it = m_groups.insert(key, item);
      }
      parent = *it;
      m_usedGroups.insert(key);
    }
    return parent;
  }

  static void setValue(QTreeWidgetItem* it, int col, double v, const QString& text)
  {
    it->setData(col, ValueRole, v);
    it->setText(col, text);
    it->setTextAlignment(col, Qt::AlignRight | Qt::AlignVCenter);
  }

  void refresh()
  {
    m_dirty = false;
    m_timeline->update();
    for(auto [box, v] : {std::pair{m_audioSpike, m_profiler.audioSpikePercent()},
                         std::pair{m_renderSpike, m_profiler.renderSpikeMs()}})
    {
      if(!box->hasFocus() && box->value() != v)
      {
        QSignalBlocker block{box};
        box->setValue(v);
      }
    }

    const auto& load = m_profiler.audioLoad();
    const auto& gpu = m_profiler.frameGpuNs();
    const auto& cpu = m_profiler.frameCpuNs();
    QStringList summary;
    if(!load.empty())
      summary << tr("Audio: %1% avg, %2% max of the tick")
                     .arg(load.avg(), 0, 'f', 1)
                     .arg(load.max(), 0, 'f', 1);
    if(!cpu.empty())
      summary << tr("Frame: %1 fps, render CPU %2 ms")
                     .arg(m_profiler.framesPerSecond(), 0, 'f', 1)
                     .arg(cpu.avg() / 1e6, 0, 'f', 3);
    if(!gpu.empty())
      summary << tr("GPU %1 ms avg, %2 ms max")
                     .arg(gpu.avg() / 1e6, 0, 'f', 3)
                     .arg(gpu.max() / 1e6, 0, 'f', 3);
    if(summary.isEmpty())
      summary << tr("No measure yet: enable Benchmark and play");
    m_summary->setText(summary.join(QStringLiteral("  ·  ")));

    m_tree->setSortingEnabled(false);
    m_usedGroups.clear();
    ossia::hash_set<quintptr> seen;
    std::array<double, ColumnCount> colMax{};

    struct Row
    {
      QTreeWidgetItem* item;
      std::array<double, ColumnCount> v;
    };
    std::vector<Row> rows;

    const bool selectedOnly = m_selectedOnly->isChecked();
    for(const auto& [key, e] : m_profiler.entries())
    {
      if(e.isProcess && !e.process)
        continue;
      if(selectedOnly && (!e.process || !e.process->selection.get()))
        continue;
      seen.insert(key);

      auto it = m_items.find(key);
      auto group = groupFor(e.process.data());
      if(it == m_items.end())
      {
        auto item = group ? new Item{group} : new Item{m_tree};
        item->setData(NameCol, KeyRole, QVariant::fromValue(key));
        it = m_items.emplace(key, item).first;
      }
      auto item = it->second;
      if(item->parent() != group)
      {
        if(auto p = item->parent())
          p->removeChild(item);
        else
          m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(item));
        if(group)
          group->addChild(item);
        else
          m_tree->addTopLevelItem(item);
      }
      item->setText(NameCol, e.name);

      std::array<double, ColumnCount> v{};
      v[AudioPercentCol] = e.audioPercent.empty() ? 0. : e.audioPercent.avg();
      v[AudioUsCol] = e.audioNs.empty() ? 0. : e.audioNs.avg() / 1e3;
      v[RenderCpuCol] = e.renderCpuNs.empty() ? 0. : e.renderCpuNs.avg() / 1e6;
      v[GpuCol] = e.gpuNs.empty() ? 0. : e.gpuNs.avg() / 1e6;
      v[GpuMaxCol] = e.gpuNs.empty() ? 0. : e.gpuNs.max() / 1e6;
      v[TargetsCol] = e.targetBytes / (1024. * 1024.);

      setValue(item, AudioPercentCol, v[AudioPercentCol],
               e.audioPercent.empty() ? QString{} : QString::number(v[AudioPercentCol], 'f', 2));
      setValue(item, AudioUsCol, v[AudioUsCol],
               e.audioNs.empty() ? QString{} : QString::number(v[AudioUsCol], 'f', 1));
      setValue(item, RenderCpuCol, v[RenderCpuCol],
               e.renderCpuNs.empty() ? QString{} : ms(e.renderCpuNs.avg()));
      setValue(item, GpuCol, v[GpuCol], e.gpuNs.empty() ? QString{} : ms(e.gpuNs.avg()));
      setValue(item, GpuMaxCol, v[GpuMaxCol], e.gpuNs.empty() ? QString{} : ms(e.gpuNs.max()));
      setValue(item, TargetsCol, v[TargetsCol],
               e.targetBytes > 0 ? QString::number(v[TargetsCol], 'f', 1) : QString{});
      for(int c = AudioPercentCol; c < ColumnCount; c++)
        colMax[c] = std::max(colMax[c], v[c]);
      rows.push_back({item, v});
    }

    for(auto it = m_items.begin(); it != m_items.end();)
    {
      if(!seen.contains(it->first))
      {
        delete it->second;
        it = m_items.erase(it);
      }
      else
        ++it;
    }
    for(auto it = m_groups.begin(); it != m_groups.end();)
    {
      if(!m_usedGroups.contains(it.key()))
      {
        auto item = it.value();
        while(item->childCount() > 0)
        {
          auto c = item->takeChild(0);
          if(auto p = item->parent())
            p->addChild(c);
          else
            m_tree->addTopLevelItem(c);
        }
        delete item;
        it = m_groups.erase(it);
      }
      else
        ++it;
    }

    for(auto& r : rows)
      for(int c = AudioPercentCol; c < ColumnCount; c++)
        r.item->setData(c, FractionRole, colMax[c] > 0. ? r.v[c] / colMax[c] : 0.);

    for(auto g : m_groups)
    {
      std::array<double, ColumnCount> sum{};
      std::function<void(QTreeWidgetItem*)> acc = [&](QTreeWidgetItem* it) {
        for(int i = 0; i < it->childCount(); i++)
        {
          auto c = it->child(i);
          if(!c->data(NameCol, IsGroupRole).toBool())
            for(int col = AudioPercentCol; col < ColumnCount; col++)
              if(col != GpuMaxCol)
                sum[col] += c->data(col, ValueRole).toDouble();
          acc(c);
        }
      };
      acc(g);
      setValue(g, AudioPercentCol, sum[AudioPercentCol],
               sum[AudioPercentCol] > 0. ? QString::number(sum[AudioPercentCol], 'f', 2) : QString{});
      setValue(g, AudioUsCol, sum[AudioUsCol],
               sum[AudioUsCol] > 0. ? QString::number(sum[AudioUsCol], 'f', 1) : QString{});
      setValue(g, RenderCpuCol, sum[RenderCpuCol],
               sum[RenderCpuCol] > 0. ? QString::number(sum[RenderCpuCol], 'f', 3) : QString{});
      setValue(g, GpuCol, sum[GpuCol],
               sum[GpuCol] > 0. ? QString::number(sum[GpuCol], 'f', 3) : QString{});
      setValue(g, TargetsCol, sum[TargetsCol],
               sum[TargetsCol] > 0. ? QString::number(sum[TargetsCol], 'f', 1) : QString{});
      auto f = g->font(NameCol);
      f.setBold(true);
      g->setFont(NameCol, f);
    }
    m_tree->setSortingEnabled(true);

    const auto& spikes = m_profiler.spikes();
    if(m_profiler.spikeGeneration() != m_shownSpikes)
    {
      m_shownSpikes = m_profiler.spikeGeneration();
      m_spikes->clear();
      for(auto it = spikes.rbegin(); it != spikes.rend(); ++it)
      {
        const auto& s = *it;
        auto item = new QTreeWidgetItem{m_spikes};
        item->setText(0, s.kind == ProfilerSpike::Audio ? tr("Audio tick") : tr("Frame"));
        item->setText(
            1, s.frame >= 0 ? tr("frame %1").arg(s.frame)
                            : QString::number(s.time / 1000., 'f', 2));
        item->setText(2, QString::number(s.totalMs, 'f', 3));
        item->setText(3, s.otherMs >= 0. ? QString::number(s.otherMs, 'f', 3) : QString{});
        for(const auto& [name, t] : s.top)
        {
          auto c = new QTreeWidgetItem{item};
          c->setText(0, name);
          c->setText(2, QString::number(t, 'f', 3));
        }
      }
    }
  }

  void exportJson()
  {
    const auto file = QFileDialog::getSaveFileName(
        this, tr("Export the profile"), QStringLiteral("profile.json"),
        tr("JSON (*.json)"));
    if(file.isEmpty())
      return;
    auto obj = m_profiler.toJson();
    auto history = [](const std::deque<float>& d) {
      QJsonArray a;
      for(float v : d)
        a.append(v);
      return a;
    };
    obj["audioLoadHistory"] = history(m_profiler.audioHistory());
    obj["gpuMsHistory"] = history(m_profiler.gpuHistory());
    obj["renderCpuMsHistory"] = history(m_profiler.renderCpuHistory());
    QFile f{file};
    if(f.open(QIODevice::WriteOnly))
      f.write(QJsonDocument{obj}.toJson());
  }

  const score::DocumentContext& m_ctx;
  Profiler& m_profiler;
  Timeline* m_timeline{};
  QLabel* m_summary{};
  QCheckBox* m_selectedOnly{};
  QDoubleSpinBox* m_audioSpike{};
  QDoubleSpinBox* m_renderSpike{};
  QTreeWidget* m_tree{};
  QTreeWidget* m_spikes{};
  ossia::hash_map<quintptr, QTreeWidgetItem*> m_items;
  QHash<QString, QTreeWidgetItem*> m_groups;
  QSet<QString> m_usedGroups;
  bool m_dirty{true};
  uint64_t m_shownSpikes{~uint64_t{0}};
};

ProfilerPanel::ProfilerPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QVBoxLayout>);
  score::setHelp(
      m_widget,
      QObject::tr("Execution cost of each process: audio time per tick, render-thread "
                  "and GPU time per frame, and the ticks and frames that took too long"));
}

QWidget* ProfilerPanel::widget()
{
  return m_widget;
}

const score::PanelStatus& ProfilerPanel::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false, false, Qt::BottomDockWidgetArea, 20, QObject::tr("Profiler"), "profiler",
      QObject::tr("Ctrl+Alt+P")};
  return status;
}

void ProfilerPanel::on_modelChanged(score::MaybeDocument, score::MaybeDocument newm)
{
  delete m_cur;
  if(newm)
  {
    m_cur = new ProfilerWidget{*newm, m_widget};
    m_widget->layout()->addWidget(m_cur);
  }
}

std::unique_ptr<score::PanelDelegate>
ProfilerPanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<ProfilerPanel>(ctx);
}
}

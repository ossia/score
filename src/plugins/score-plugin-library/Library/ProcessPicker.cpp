#include "ProcessPicker.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Dataflow/PortType.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QAbstractTableModel>
#include <QApplication>
#include <QBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QSizeGrip>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/Snippet.hpp>

#include <algorithm>
#include <memory>

namespace Library
{
namespace
{
using Key = UuidKey<Process::ProcessModel>;
using Ident = Process::ProcessIdentifier;

struct IdentHash
{
  std::size_t operator()(const Ident& i) const noexcept
  {
    return std::hash<Key>{}(i.key) ^ (std::size_t(qHash(i.effect)) * 0x9e3779b97f4a7c15ull);
  }
};
template <typename T>
using IdentMap = ossia::hash_map<Ident, T, IdentHash>;

constexpr auto recentSetting = "Library/RecentProcesses";
constexpr int maxRecent = 12;
constexpr int maxResults = 300;
//! Ranks of rankOf, then for the presets found by their name
constexpr int presetRank = 5;
constexpr QSize pickerSize{1180, 540};
//! Around the text of a row, above and below
constexpr int rowPadding = 10;
constexpr int iconSize = 16;

//! The last processes picked, newest first: their key, a newline, their data
std::vector<Ident> readRecent()
{
  std::vector<Ident> recent;
  for(const QString& s : QSettings{}.value(recentSetting).toStringList())
  {
    const auto sep = s.indexOf(QLatin1Char('\n'));
    if(sep > 0)
      recent.push_back(Ident{Key::fromString(s.left(sep)), s.mid(sep + 1)});
  }
  return recent;
}

void remember(const Ident& id)
{
  auto recent = readRecent();
  ossia::remove_erase(recent, id);
  recent.insert(recent.begin(), id);
  if(recent.size() > maxRecent)
    recent.resize(maxRecent);
  QStringList stored;
  for(const auto& r : recent)
    stored.push_back(
        QString::fromLatin1(score::uuids::toByteArray(r.key.impl())) + QLatin1Char('\n')
        + r.effect);
  QSettings{}.setValue(recentSetting, stored);
}

//! A process of the library, with where it is in it
struct Entry
{
  Ident id;
  QString name;
  QIcon icon;
  QString searchText;
  QStringList categories;
  QString where;
  const Process::ProcessModelFactory* factory{};
  //! Asked when first shown: building it can be costly (plug-ins)
  mutable std::optional<Process::Descriptor> descriptor;

  const Process::Descriptor& describe() const
  {
    if(!descriptor)
      descriptor = factory->descriptor(id.effect);
    return *descriptor;
  }
};

//! A folder of the library and the processes below it, which are contiguous
struct LibraryFolder
{
  QString name;
  QIcon icon;
  int depth{};
  //! The processes below it, which are contiguous
  int begin{}, end{};
  //! The processes right in it: for a process with entries of its own, the
  //! process then its entries
  std::vector<int> own{};
  std::vector<int> folders{};
};

//! What the picker lists of the library at one time
struct Snapshot
{
  std::vector<Entry> entries;
  std::vector<LibraryFolder> folders;
  std::vector<int> roots;
  IdentMap<int> byId;

  /**
   * The folders and processes below `node`, as the library panel has them.
   * A process with entries of its own, such as a script language's files, is
   * a folder too.
   *
   * `icon` is the one of the closest folder that has one, for the processes
   * with none of their own.
   */
  int collect(
      const ProcessNode& node, const QStringList& path, const QIcon& icon,
      const Process::ProcessFactoryList& factories, std::vector<int>& subfolders,
      std::vector<int>& own)
  {
    int count = 0;
    for(const ProcessNode& child : node)
    {
      const QIcon childIcon = child.icon.isNull() ? icon : child.icon;
      const int begin = int(entries.size());

      // Snippets are no process: they are pasted
      if(child.key != Key{} && child.key != snippetKey())
        if(auto fact = factories.get(child.key))
        {
          byId[Ident{child.key, child.customData}] = begin;
          entries.push_back(Entry{
              Ident{child.key, child.customData}, child.prettyName, childIcon,
              child.searchText, path, path.join(QStringLiteral(" / ")), fact, {}});
          count++;
        }

      if(!child.hasChildren())
      {
        if(int(entries.size()) > begin)
          own.push_back(begin);
        continue;
      }

      QStringList childPath = path;
      childPath.push_back(child.prettyName);
      LibraryFolder folder{child.prettyName, childIcon, int(path.size()), begin};
      if(int(entries.size()) > begin)
        folder.own.push_back(begin);
      const int n = collect(
          child, childPath, childIcon, factories, folder.folders, folder.own);
      folder.end = int(entries.size());
      count += n;
      if(folder.end > folder.begin)
      {
        subfolders.push_back(int(folders.size()));
        folders.push_back(std::move(folder));
      }
    }
    return count;
  }
};

//! The snapshot of a library, built again when it changed since the last one
class LibraryIndex final : public QObject
{
public:
  static std::shared_ptr<const Snapshot>
  of(const ProcessesItemModel& model, const Process::ProcessFactoryList& factories)
  {
    auto& m = const_cast<ProcessesItemModel&>(model);
    auto index = static_cast<LibraryIndex*>(
        m.findChild<QObject*>(objectNameKey(), Qt::FindDirectChildrenOnly));
    if(!index)
      index = new LibraryIndex{m};
    if(!index->m_snapshot)
    {
      auto s = std::make_shared<Snapshot>();
      std::vector<int> rootProcesses;
      s->collect(model.rootNode(), {}, QIcon{}, factories, s->roots, rootProcesses);
      index->m_snapshot = std::move(s);
    }
    return index->m_snapshot;
  }

private:
  static QString objectNameKey() { return QStringLiteral("ProcessPickerIndex"); }

  explicit LibraryIndex(ProcessesItemModel& model)
      : QObject{&model}
  {
    setObjectName(objectNameKey());
    const auto outdated = [this] { m_snapshot.reset(); };
    connect(&model, &QAbstractItemModel::rowsInserted, this, outdated);
    connect(&model, &QAbstractItemModel::rowsRemoved, this, outdated);
    connect(&model, &QAbstractItemModel::rowsMoved, this, outdated);
    connect(&model, &QAbstractItemModel::modelReset, this, outdated);
    connect(&model, &QAbstractItemModel::layoutChanged, this, outdated);
    connect(&model, &QAbstractItemModel::dataChanged, this, outdated);
  }

  std::shared_ptr<const Snapshot> m_snapshot;
};

//! A line of the processes column: a process, or one of its presets found by
//! a search
struct Row
{
  //! -1 for a heading
  int entry{};
  int preset{-1};
  //! Which of the headings it is, the name of a folder
  int heading{-1};
};

enum CategoryKind
{
  Folder,
  Recent,
  All
};
constexpr int FolderRole = Qt::UserRole;
constexpr int KindRole = Qt::UserRole + 1;
constexpr int PresetRole = Qt::UserRole;
//! Shown dim on the right of a row: where a process is, how many a category has
constexpr int DetailRole = Qt::UserRole + 10;
//! The row ends a group: a line under it
constexpr int GroupEndRole = Qt::UserRole + 11;
//! The row names the folder of the rows below it
constexpr int HeadingRole = Qt::UserRole + 12;

class Results final : public QAbstractTableModel
{
public:
  Results(
      const Snapshot& s, const std::vector<Process::Preset>& p, QObject* parent)
      : QAbstractTableModel{parent}
      , snapshot{s}
      , presets{p}
  {
  }

  void setRows(std::vector<Row> r, std::vector<QString> h, int depth)
  {
    beginResetModel();
    rows = std::move(r);
    headings = std::move(h);
    shownDepth = depth;
    endResetModel();
  }

  Qt::ItemFlags flags(const QModelIndex& idx) const override
  {
    if(idx.isValid() && ossia::valid_index(idx.row(), rows) && rows[idx.row()].entry < 0)
      return Qt::NoItemFlags;
    return QAbstractTableModel::flags(idx);
  }

  //! The preset of the row, if it is still there
  const Process::Preset* preset(const Row& r) const noexcept
  {
    return ossia::valid_index(r.preset, presets) ? &presets[r.preset] : nullptr;
  }

  int rowCount(const QModelIndex& parent) const override
  {
    return parent.isValid() ? 0 : int(rows.size());
  }
  int columnCount(const QModelIndex& parent) const override
  {
    return parent.isValid() ? 0 : 1;
  }

  QVariant data(const QModelIndex& idx, int role) const override
  {
    if(!idx.isValid() || !ossia::valid_index(idx.row(), rows))
      return {};
    const auto& r = rows[idx.row()];
    if(r.entry < 0)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return headings[r.heading];
        case HeadingRole:
          return true;
      }
      return {};
    }
    const auto& e = snapshot.entries[r.entry];
    switch(role)
    {
      case Qt::DisplayRole:
        if(auto p = preset(r))
          return QString{e.name + QStringLiteral(" / ") + p->name};
        return e.name;
      case Qt::DecorationRole:
        return e.icon;
      case Qt::ToolTipRole:
        return e.where.isEmpty() ? e.name : e.where + QStringLiteral(" / ") + e.name;
      case DetailRole:
        // Under the headings of a folder, where it is needs no saying. Else all
        // of it: entries of the same name differ only by where they are.
        if(!headings.empty() || shownDepth > 0)
          return {};
        return e.where;
    }
    return {};
  }

  const Snapshot& snapshot;
  const std::vector<Process::Preset>& presets;
  std::vector<Row> rows;
  std::vector<QString> headings;
  //! How many of the categories of its processes the category shown is
  int shownDepth{};
};

//! How well `name` matches `q`: lower is better
int rankOf(const QString& name, const QString& q)
{
  if(name.compare(q, Qt::CaseInsensitive) == 0)
    return 0;
  if(name.startsWith(q, Qt::CaseInsensitive))
    return 1;
  const int i = name.indexOf(q, 0, Qt::CaseInsensitive);
  if(i > 0)
  {
    // At the start of a word: "lowpass" in "Ladder lowpass", "Pass" in "LowPass"
    const QChar before = name[i - 1];
    if(!before.isLetterOrNumber() || (before.isLower() && name[i].isUpper()))
      return 2;
    return 3;
  }
  return 4;
}

bool hasCommandModifier(const QKeyEvent& ev)
{
  return ev.modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

bool isPrintable(const QKeyEvent& ev)
{
  if(hasCommandModifier(ev))
    return false;
  const auto t = ev.text();
  return !t.isEmpty() && t[0].isPrint();
}

QString portTypeName(Process::PortType t)
{
  switch(t)
  {
    case Process::PortType::Message:
      return QObject::tr("Message");
    case Process::PortType::Audio:
      return QObject::tr("Audio");
    case Process::PortType::Midi:
      return QObject::tr("MIDI");
    case Process::PortType::Texture:
      return QObject::tr("Texture");
    case Process::PortType::Geometry:
      return QObject::tr("Geometry");
  }
  return {};
}

//! The port types of a list, each in the colour of its ports
QString portList(const std::vector<Process::PortType>& ports)
{
  if(ports.empty())
    return QObject::tr("none");
  // Each type once, in the order of their first port
  std::vector<Process::PortType> types;
  for(auto t : ports)
    if(!ossia::contains(types, t))
      types.push_back(t);
  QStringList names;
  for(auto t : types)
    names.push_back(
        QStringLiteral("<font color=\"%1\">%2</font>")
            .arg(Process::portBrush(t).color().lighter(130).name(), portTypeName(t)));
  return names.join(QStringLiteral(", "));
}

/**
 * Paints the rows of the picker's columns: the icon, the name with what was
 * searched in bold, and on the right, dim, what DetailRole says.
 *
 * The current row is filled in the accent colour in the column the keys go
 * to, and only outlined in the others. The views' own palette has no
 * highlight, for the branches of the categories not to be filled.
 */
class PickerDelegate final : public QStyledItemDelegate
{
public:
  using IsActive = std::function<bool(const QWidget*)>;
  PickerDelegate(IsActive active, const QStringList* words, QObject* parent)
      : QStyledItemDelegate{parent}
      , m_active{std::move(active)}
      , m_words{words}
  {
  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& idx) const override
  {
    auto s = QStyledItemDelegate::sizeHint(option, idx);
    s.setHeight(std::max(s.height(), option.fontMetrics.height() + rowPadding));
    return s;
  }

  void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx)
      const override
  {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, idx);
    const QPalette& pal = qApp->palette();
    if(idx.data(HeadingRole).toBool())
    {
      paintHeading(p, opt, pal);
      return;
    }
    const QColor accent = pal.color(QPalette::Highlight);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool active = selected && m_active(opt.widget);

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5);
    if(active)
    {
      p->setPen(Qt::NoPen);
      p->setBrush(accent);
      p->drawRoundedRect(box, 3, 3);
    }
    else if(selected)
    {
      p->setPen(QPen{accent, 1.});
      p->setBrush(Qt::NoBrush);
      p->drawRoundedRect(box, 3, 3);
    }
    else if(opt.state & QStyle::State_MouseOver)
    {
      QColor c = pal.color(QPalette::Text);
      c.setAlpha(20);
      p->setPen(Qt::NoPen);
      p->setBrush(c);
      p->drawRoundedRect(box, 3, 3);
    }
    p->setRenderHint(QPainter::Antialiasing, false);

    if(idx.data(GroupEndRole).toBool())
    {
      QColor c = pal.color(QPalette::Text);
      c.setAlpha(80);
      p->setPen(c);
      p->drawLine(opt.rect.left() + 4, opt.rect.bottom(), opt.rect.right() - 4, opt.rect.bottom());
    }

    QRect r = opt.rect.adjusted(6, 0, -6, 0);
    if(!opt.icon.isNull())
      opt.icon.paint(
          p, QRect{r.left(), r.center().y() - iconSize / 2 + 1, iconSize, iconSize});
    r.setLeft(r.left() + iconSize + 6);

    const QColor text = active ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Text);
    QFont small = opt.font;
    if(small.pointSizeF() > 0)
      small.setPointSizeF(small.pointSizeF() * 0.9);
    const QFontMetrics fm{opt.font}, sfm{small};

    // The name first, what is on its right with the room left
    const QString detail = idx.data(DetailRole).toString();
    constexpr int gap = 12;
    const int avail = r.width();
    const int nameW = fm.horizontalAdvance(opt.text);
    int detailW = detail.isEmpty() ? 0 : sfm.horizontalAdvance(detail);
    if(detailW > 0 && nameW + gap + detailW > avail)
      detailW = std::clamp(avail - nameW - gap, std::min(detailW, avail / 2), detailW);
    const int nameAvail = std::max(0, avail - (detailW > 0 ? detailW + gap : 0));
    const QString shown = fm.elidedText(opt.text, Qt::ElideRight, nameAvail);

    QList<QTextLayout::FormatRange> found;
    if(m_words)
    {
      QTextCharFormat f;
      f.setFontWeight(QFont::Bold);
      f.setForeground(active ? text : score::Skin::instance().Base4.color());
      for(const QString& w : *m_words)
        for(int i = shown.indexOf(w, 0, Qt::CaseInsensitive); i >= 0;
            i = shown.indexOf(w, i + w.size(), Qt::CaseInsensitive))
          found.push_back({i, int(w.size()), f});
    }
    QTextLayout layout{shown, opt.font};
    layout.setFormats(found);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(nameAvail + fm.averageCharWidth() * 4);
    layout.endLayout();
    p->setPen(text);
    layout.draw(p, QPointF(r.left(), r.top() + (r.height() - line.height()) / 2.));

    if(detailW > 0)
    {
      p->setFont(small);
      p->setPen(active ? text : pal.color(QPalette::PlaceholderText));
      p->drawText(
          QRect{r.right() - detailW, r.top(), detailW + 1, r.height()},
          Qt::AlignRight | Qt::AlignVCenter, sfm.elidedText(detail, Qt::ElideLeft, detailW));
    }
    p->restore();
  }

private:
  //! The name of a folder, dim, with a line after it
  static void
  paintHeading(QPainter* p, const QStyleOptionViewItem& opt, const QPalette& pal)
  {
    p->save();
    QFont f = opt.font;
    f.setBold(true);
    if(f.pointSizeF() > 0)
      f.setPointSizeF(f.pointSizeF() * 0.9);
    const QFontMetrics fm{f};
    const QRect r = opt.rect.adjusted(6, 0, -6, 0);
    const QString text = fm.elidedText(opt.text, Qt::ElideMiddle, r.width());
    p->setFont(f);
    p->setPen(pal.color(QPalette::PlaceholderText));
    p->drawText(r, Qt::AlignLeft | Qt::AlignBottom, text);
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(40);
    p->setPen(c);
    const int x = r.left() + fm.horizontalAdvance(text) + 8;
    const int y = r.bottom() - fm.height() / 2 + 1;
    if(x < r.right())
      p->drawLine(x, y, r.right(), y);
    p->restore();
  }

  IsActive m_active;
  const QStringList* m_words{};
};

//! The views draw no highlight of their own: the delegate does
void setupView(QAbstractItemView& v, QAbstractItemDelegate& delegate)
{
  v.setItemDelegate(&delegate);
  v.setFrameShape(QFrame::NoFrame);
  v.setMouseTracking(true);
  v.viewport()->setAttribute(Qt::WA_Hover);
  v.setIconSize({iconSize, iconSize});
  v.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  QPalette pal = v.palette();
  for(auto group : {QPalette::Active, QPalette::Inactive})
  {
    pal.setColor(group, QPalette::Highlight, Qt::transparent);
    pal.setColor(group, QPalette::HighlightedText, pal.color(QPalette::Text));
  }
  v.setPalette(pal);
}

//! The keys and what they do, after small key caps
class KeyHints final : public QWidget
{
public:
  KeyHints(std::vector<std::pair<QString, QString>> keys, QWidget* parent)
      : QWidget{parent}
      , m_keys{std::move(keys)}
  {
    setFixedHeight(fontMetrics().height() + 6);
  }

  QSize sizeHint() const override
  {
    const auto fm = fontMetrics();
    int w = 0;
    for(const auto& [key, what] : m_keys)
      w += fm.horizontalAdvance(key) + 10 + 5 + fm.horizontalAdvance(what) + 18;
    return {w, height()};
  }
  QSize minimumSizeHint() const override { return sizeHint(); }

  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = palette();
    QColor cap = pal.color(QPalette::Text);
    cap.setAlpha(40);
    const auto fm = fontMetrics();
    qreal x = 0.5;
    const qreal h = height() - 1;
    for(const auto& [key, what] : m_keys)
    {
      const qreal w = fm.horizontalAdvance(key) + 10;
      p.setPen(Qt::NoPen);
      p.setBrush(cap);
      p.drawRoundedRect(QRectF{x, 0.5, w, h}, 3, 3);
      p.setPen(pal.color(QPalette::Text));
      p.drawText(QRectF{x, 0, w, qreal(height())}, Qt::AlignCenter, key);
      x += w + 5;
      p.setPen(pal.color(QPalette::PlaceholderText));
      const qreal tw = fm.horizontalAdvance(what);
      p.drawText(QRectF{x, 0, tw + 1, qreal(height())}, Qt::AlignLeft | Qt::AlignVCenter, what);
      x += tw + 18;
    }
  }

private:
  std::vector<std::pair<QString, QString>> m_keys;
};

//! What the picker is narrowed to: a rounded tag in the colour of the port
class PortChip final : public QWidget
{
public:
  PortChip(QColor color, QWidget* parent)
      : QWidget{parent}
      , m_color{color}
  {
  }

  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = m_color;
    fill.setAlpha(50);
    p.setPen(QPen{m_color, 1.});
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
  }

private:
  QColor m_color;
};
}

struct ProcessPicker::Impl
{
  Impl(
      ProcessPicker& self, const score::GUIApplicationContext& ctx,
      const ProcessesItemModel& library, Process::ProcessFlags acceptable,
      Callback picked, PickerOptions options)
      : self{self}
      , factories{ctx.interfaces<Process::ProcessFactoryList>()}
      , plugin{ctx.applicationPlugin<Process::ApplicationPlugin>()}
      , snapshot{LibraryIndex::of(library, factories)}
      , acceptable{acceptable}
      , options{options}
      , picked{std::move(picked)}
  {
    computeAccepted();
  }

  //! What can go where it was opened, and the last ones picked among them
  void computeAccepted()
  {
    const auto& entries = snapshot->entries;
    accepted.assign(entries.size() + 1, 0);
    ossia::hash_map<const Process::ProcessModelFactory*, bool> kinds;
    for(std::size_t i = 0; i < entries.size(); i++)
    {
      auto [it, inserted] = kinds.try_emplace(entries[i].factory, false);
      if(inserted)
        it->second = acceptsKind(*entries[i].factory);
      accepted[i + 1] = accepted[i] + (it->second ? 1 : 0);
    }

    // The ranks of the last ones picked, read once
    recentRank.assign(entries.size(), maxRecent);
    recentRows.clear();
    const auto recent = readRecent();
    for(std::size_t r = 0; r < recent.size(); r++)
      if(auto it = snapshot->byId.find(recent[r]); it != snapshot->byId.end())
        if(isAccepted(it->second))
        {
          recentRank[it->second] = int(r);
          recentRows.push_back(Row{it->second});
        }
  }

  ProcessPicker& self;
  const Process::ProcessFactoryList& factories;
  Process::ApplicationPlugin& plugin;
  //! Kept as it was at the opening, whatever the library does meanwhile
  std::shared_ptr<const Snapshot> snapshot;
  Process::ProcessFlags acceptable{};
  PickerOptions options;
  Callback picked;

  //! Prefix sums of the entries accepted here, for counts by folder
  std::vector<int> accepted;
  std::vector<int> recentRank;
  std::vector<Row> recentRows;
  bool refreshQueued{};

  QLineEdit* search{};
  QTreeWidget* categories{};
  QTreeView* processes{};
  Results* results{};
  QListWidget* presets{};
  QTreeWidgetItem* all{};
  //! What is searched, for the rows to show it
  QStringList words;

  //! What the picker is narrowed to when opened from a port
  QWidget* portChip{};
  QMetaObject::Connection focusChanged;

  //! What the current process is
  struct
  {
    QLabel* icon{};
    QLabel* name{};
    QLabel* where{};
    QLabel* text{};
    QLabel* ports{};
    QLabel* presetsTitle{};
  } details;

  bool isAccepted(int entry) const noexcept
  {
    return accepted[entry + 1] != accepted[entry];
  }
  int acceptedIn(const LibraryFolder& f) const noexcept
  {
    return accepted[f.end] - accepted[f.begin];
  }

  //! Whether processes of this kind can go where the picker was opened. Their
  //! ports are those the kind says with no data: per file they are many.
  bool acceptsKind(const Process::ProcessModelFactory& f) const
  {
    const auto flags = f.flags();
    if(flags & Process::ProcessFlags::Deprecated)
      return false;
    if((int64_t(flags) & int64_t(acceptable)) == 0)
      return false;
    if(!options.ports)
      return true;
    const auto d = f.descriptor(QString{});
    const auto& list = options.ports->inlet ? d.inlets : d.outlets;
    return !list || ossia::contains(*list, options.ports->type);
  }

  void addFolders(const std::vector<int>& folders, QTreeWidgetItem* parent)
  {
    for(int f : folders)
    {
      const auto& folder = snapshot->folders[f];
      if(acceptedIn(folder) == 0)
        continue;
      auto item = parent ? new QTreeWidgetItem{parent} : new QTreeWidgetItem{categories};
      item->setText(0, folder.name);
      item->setIcon(0, folder.icon);
      item->setData(0, FolderRole, f);
      item->setData(0, KindRole, Folder);
      item->setData(0, DetailRole, QString::number(acceptedIn(folder)));
      addFolders(folder.folders, item);
    }
  }

  QTreeWidgetItem*
  addSpecial(int index, const QString& name, CategoryKind kind, std::size_t count)
  {
    auto item = new QTreeWidgetItem;
    item->setText(0, name);
    item->setData(0, KindRole, kind);
    item->setData(0, DetailRole, QString::number(count));
    categories->insertTopLevelItem(index, item);
    return item;
  }

  //! The categories with something that can go where it was opened. Returns
  //! the one to show first.
  QTreeWidgetItem* fillCategories()
  {
    QSignalBlocker b{categories};
    categories->clear();
    addFolders(snapshot->roots, nullptr);
    all = addSpecial(0, QObject::tr("All"), All, std::size_t(accepted.back()));
    all->setData(0, GroupEndRole, true);
    QTreeWidgetItem* first = categories->topLevelItem(1);
    if(!recentRows.empty())
      first = addSpecial(0, QObject::tr("Recent"), Recent, recentRows.size());
    return first ? first : all;
  }

  //! Offers everything, not only what can be cabled to the port
  void clearPortFilter()
  {
    if(!options.ports)
      return;
    options.ports.reset();
    portChip->hide();
    computeAccepted();
    auto first = fillCategories();
    if(search->text().isEmpty())
    {
      categories->setCurrentItem(first);
    }
    else
    {
      QSignalBlocker b{categories};
      categories->setCurrentItem(all);
      runSearch(search->text());
    }
    search->setFocus();
  }

  std::vector<Row> rowsIn(int begin, int end) const
  {
    std::vector<Row> rows;
    rows.reserve(accepted[end] - accepted[begin]);
    for(int i = begin; i < end; i++)
      if(isAccepted(i))
        rows.push_back(Row{i});
    return rows;
  }

  /**
   * The processes of a folder as the library panel has them: those right in
   * it, then those of each of its folders under a heading with its name.
   * `prefix` is the names of the folders above it below the one shown, none
   * for the one shown, which needs no heading.
   */
  void folderRows(
      int f, const std::optional<QString>& prefix, std::vector<Row>& rows,
      std::vector<QString>& headings) const
  {
    const auto& folder = snapshot->folders[f];
    std::optional<QString> name;
    if(prefix)
      name = prefix->isEmpty() ? folder.name : *prefix + QStringLiteral(" / ") + folder.name;
    bool headed = !name;
    for(int e : folder.own)
    {
      if(!isAccepted(e))
        continue;
      if(!headed)
      {
        rows.push_back(Row{-1, -1, int(headings.size())});
        headings.push_back(*name);
        headed = true;
      }
      rows.push_back(Row{e});
    }
    for(int sub : folder.folders)
      folderRows(sub, name ? name : QString{}, rows, headings);
  }

  void showCategory(QTreeWidgetItem* item)
  {
    if(!item)
    {
      setRows({}, 0);
      return;
    }
    switch(item->data(0, KindRole).toInt())
    {
      case Recent:
        setRows(recentRows, 0);
        break;
      case All:
        setRows(rowsIn(0, int(snapshot->entries.size())), 0);
        break;
      default: {
        const int f = item->data(0, FolderRole).toInt();
        std::vector<Row> rows;
        std::vector<QString> headings;
        folderRows(f, std::nullopt, rows, headings);
        setRows(std::move(rows), snapshot->folders[f].depth + 1, std::move(headings));
        break;
      }
    }
  }

  void runSearch(const QString& text)
  {
    words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if(words.isEmpty())
    {
      showCategory(categories->currentItem());
      return;
    }

    struct Scored
    {
      Row row;
      int rank{};
    };
    std::vector<Scored> found;
    const auto& entries = snapshot->entries;
    for(int i = 0; i < int(entries.size()); i++)
    {
      if(!isAccepted(i))
        continue;
      const auto& e = entries[i];
      const bool all = std::all_of(words.begin(), words.end(), [&](const QString& w) {
        return e.name.contains(w, Qt::CaseInsensitive)
               || e.where.contains(w, Qt::CaseInsensitive)
               || e.searchText.contains(w, Qt::CaseInsensitive);
      });
      if(all)
        found.push_back({Row{i}, rankOf(e.name, words.front())});
    }

    // The presets named like it, under their process
    if(options.presets)
    {
      const QString whole = words.join(QLatin1Char(' '));
      const auto& list = plugin.presets;
      for(int p = 0; p < int(list.size()); p++)
      {
        if(!list[p].name.contains(whole, Qt::CaseInsensitive))
          continue;
        if(auto it = snapshot->byId.find(list[p].key); it != snapshot->byId.end())
          if(isAccepted(it->second))
            found.push_back({Row{it->second, p}, presetRank});
      }
    }

    const auto better = [&](const Scored& a, const Scored& b) {
      if(a.rank != b.rank)
        return a.rank < b.rank;
      const int ra = recentRank[a.row.entry], rb = recentRank[b.row.entry];
      if(ra != rb)
        return ra < rb;
      const auto& na = entries[a.row.entry].name;
      const auto& nb = entries[b.row.entry].name;
      if(na.size() != nb.size())
        return na.size() < nb.size();
      const int c = QString::compare(na, nb, Qt::CaseInsensitive);
      if(c != 0)
        return c < 0;
      return a.row.entry < b.row.entry || (a.row.entry == b.row.entry && a.row.preset < b.row.preset);
    };
    // Only the best are shown
    const auto shown = std::min(found.size(), std::size_t(maxResults));
    std::partial_sort(found.begin(), found.begin() + shown, found.end(), better);
    found.resize(shown);

    std::vector<Row> rows;
    rows.reserve(found.size());
    for(auto& f : found)
      rows.push_back(f.row);
    setRows(std::move(rows), 0);
  }

  void setRows(std::vector<Row> rows, int depth, std::vector<QString> headings = {})
  {
    results->setRows(std::move(rows), std::move(headings), depth);
    // The first process: the headings cannot be chosen
    const auto& shown = results->rows;
    const auto first = std::find_if(
        shown.begin(), shown.end(), [](const Row& r) { return r.entry >= 0; });
    if(first != shown.end())
      processes->setCurrentIndex(results->index(int(first - shown.begin()), 0));
    else
      showPresets();
  }

  const Row* currentRow() const
  {
    const auto idx = processes->currentIndex();
    if(!idx.isValid() || !ossia::valid_index(idx.row(), results->rows))
      return nullptr;
    const Row& row = results->rows[idx.row()];
    return row.entry >= 0 ? &row : nullptr;
  }

  //! The presets of `id`: they are sorted by process
  std::pair<int, int> presetsOf(const Ident& id) const
  {
    const auto& list = plugin.presets;
    struct ByProcess
    {
      bool operator()(const Process::Preset& p, const Ident& i) const { return p.key < i; }
      bool operator()(const Ident& i, const Process::Preset& p) const { return i < p.key; }
    };
    const auto [b, e] = std::equal_range(list.begin(), list.end(), id, ByProcess{});
    return {int(b - list.begin()), int(e - list.begin())};
  }

  void showPresets()
  {
    presets->clear();
    const Row* row = currentRow();
    showDetails(row);
    if(!row)
      return;
    const auto& e = snapshot->entries[row->entry];

    if(options.presets)
    {
      auto def = new QListWidgetItem{QObject::tr("Default"), presets};
      def->setData(PresetRole, -1);
      presets->setCurrentItem(def);

      const auto [begin, end] = presetsOf(e.id);
      for(int p = begin; p < end; p++)
      {
        const auto& preset = plugin.presets[p];
        auto item = new QListWidgetItem{
            preset.category.isEmpty()
                ? preset.name
                : preset.category + QStringLiteral("/") + preset.name,
            presets};
        item->setData(PresetRole, p);
        if(!preset.description.isEmpty())
          item->setToolTip(preset.description);
        if(p == row->preset)
          presets->setCurrentItem(item);
      }
    }

  }

  void showDetails(const Row* row)
  {
    for(QLabel* l : {details.icon, details.name, details.where, details.text, details.ports})
      l->clear();
    if(!row)
      return;
    const auto& e = snapshot->entries[row->entry];
    const auto& d = e.describe();
    details.icon->setPixmap(e.icon.pixmap(QSize{20, 20}, details.icon->devicePixelRatioF()));
    details.name->setText(e.name);
    details.where->setText(e.where);

    QString text = d.description;
    if(const auto* p = results->preset(*row); p && !p->description.isEmpty())
      text = p->description;
    details.text->setText(text);
    details.text->setVisible(!text.isEmpty());

    QStringList ports;
    if(d.inlets)
      ports.push_back(QObject::tr("In: %1").arg(portList(*d.inlets)));
    if(d.outlets)
      ports.push_back(QObject::tr("Out: %1").arg(portList(*d.outlets)));
    if(!d.author.isEmpty())
      ports.push_back(QObject::tr("By %1").arg(d.author.toHtmlEscaped()));
    details.ports->setText(ports.join(QStringLiteral("<br>")));
    details.ports->setVisible(!ports.isEmpty());
  }

  //! The presets moved in the list: shown again, with the same choice
  void refresh()
  {
    if(!refreshQueued)
      return;
    refreshQueued = false;

    std::optional<Ident> process;
    if(const Row* row = currentRow())
      process = snapshot->entries[row->entry].id;
    const auto chosen = chosenPreset;

    runSearch(search->text());

    if(process)
      for(int r = 0; r < int(results->rows.size()); r++)
        if(results->rows[r].entry >= 0
           && snapshot->entries[results->rows[r].entry].id == *process)
        {
          processes->setCurrentIndex(results->index(r, 0));
          break;
        }
    if(chosen)
      for(int i = 0; i < presets->count(); i++)
      {
        const int p = presets->item(i)->data(PresetRole).toInt();
        if(p >= 0 && plugin.presets[p].key == chosen->key
           && plugin.presets[p].name == chosen->name
           && plugin.presets[p].path == chosen->path)
        {
          presets->setCurrentRow(i);
          break;
        }
      }
  }

  //! The preset chosen, as it was when chosen: indices move when presets come
  std::optional<Process::Preset> chosenPreset;

  void presetsChanged()
  {
    if(refreshQueued)
      return;
    refreshQueued = true;
    QTimer::singleShot(0, &self, [this] { refresh(); });
  }

  //! Closes the picker with the current choice
  bool accept()
  {
    refresh();
    const Row* row = currentRow();
    if(!row)
      return false;
    const auto& e = snapshot->entries[row->entry];
    PickedProcess p{e.id.key, e.id.effect, {}, e.name};
    if(options.presets)
      if(auto item = presets->currentItem())
        if(const int i = item->data(PresetRole).toInt();
           ossia::valid_index(i, plugin.presets))
          p.preset = plugin.presets[i];
    const auto* factory = e.factory;
    auto cb = std::move(picked);
    picked = {};
    self.close();
    if(!cb)
      return false;

    // Asked once the picker is gone: a process that needs more to be made,
    // such as a file.
    if(!p.preset && p.customData.isEmpty()
       && (factory->flags() & Process::ProcessFlags::RequiresCustomData))
    {
      p.customData = factory->customConstructionData();
      if(p.customData.isEmpty())
        return false;
    }
    remember(e.id);
    cb(p);
    return true;
  }

  void typeIntoSearch(QKeyEvent& ev)
  {
    search->setFocus();
    if(ev.key() == Qt::Key_Backspace)
      search->backspace();
    else
      search->insert(ev.text());
  }
};

ProcessPicker* ProcessPicker::open(
    const score::GUIApplicationContext& ctx, Process::ProcessFlags acceptable,
    QPoint globalPos, Callback picked, PickerOptions options)
{
  QWidget* window = QApplication::activeWindow();
  const ProcessesItemModel* library{};
  ProcessesItemModel* own{};
  if(auto panel = ctx.findPanel<ProcessPanel>())
    library = &panel->processWidget().processModel();
  else
    library = own = new ProcessesItemModel{ctx, nullptr};

  auto p = new ProcessPicker{
      ctx, *library, acceptable, std::move(picked), window, std::move(options)};
  if(own)
    own->setParent(p);

  // Where it was asked, inside the screen
  QRect r{globalPos, p->size()};
  if(auto screen = QGuiApplication::screenAt(globalPos))
  {
    const auto avail = screen->availableGeometry();
    r.moveRight(std::min(r.right(), avail.right()));
    r.moveBottom(std::min(r.bottom(), avail.bottom()));
    r.moveLeft(std::max(r.left(), avail.left()));
    r.moveTop(std::max(r.top(), avail.top()));
  }
  p->move(r.topLeft());
  p->show();
  p->m->search->setFocus();
  return p;
}

ProcessPicker::ProcessPicker(
    const score::GUIApplicationContext& ctx, const ProcessesItemModel& library,
    Process::ProcessFlags acceptable, Callback picked, QWidget* parent,
    PickerOptions options)
    : QFrame{parent, Qt::Popup}
    , m{std::make_unique<Impl>(
          *this, ctx, library, acceptable, std::move(picked), std::move(options))}
{
  setObjectName(QStringLiteral("ProcessPicker"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(pickerSize);

  // Lighter than what it is over, the lists darker in it: the edge is painted
  const QPalette& pal = qApp->palette();
  {
    QPalette own = pal;
    own.setColor(QPalette::Window, pal.color(QPalette::Window).lighter(140));
    setPalette(own);
    setAutoFillBackground(true);
  }

  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(8, 8, 8, 6);
  lay->setSpacing(6);

  // The search, and what it is narrowed to
  auto top = new QHBoxLayout;
  top->setSpacing(6);
  lay->addLayout(top);
  m->search = new QLineEdit{this};
  m->search->setPlaceholderText(
      m->options.presets ? tr("Search processes and presets") : tr("Search processes"));
  m->search->setClearButtonEnabled(true);
  m->search->addAction(QIcon{QStringLiteral(":/icons/search.png")}, QLineEdit::LeadingPosition);
  {
    QFont f = m->search->font();
    if(f.pointSizeF() > 0)
      f.setPointSizeF(f.pointSizeF() * 1.15);
    m->search->setFont(f);
  }
  top->addWidget(m->search, 1);

  if(!m->options.action.isEmpty())
  {
    auto chip = new PortChip{pal.color(QPalette::Highlight), this};
    auto chipLay = new QHBoxLayout{chip};
    chipLay->setContentsMargins(8, 2, 8, 2);
    chipLay->addWidget(new QLabel{m->options.action, chip});
    top->addWidget(chip);
  }

  if(const auto& ports = m->options.ports)
  {
    auto chip = new PortChip{Process::portBrush(ports->type).color(), this};
    auto chipLay = new QHBoxLayout{chip};
    chipLay->setContentsMargins(8, 0, 2, 0);
    chipLay->setSpacing(2);
    auto label = new QLabel{
        ports->inlet ? tr("With an input for %1").arg(portTypeName(ports->type).toLower())
                     : tr("With an output for %1").arg(portTypeName(ports->type).toLower()),
        chip};
    chipLay->addWidget(label);
    auto clear = new QToolButton{chip};
    clear->setObjectName(QStringLiteral("ProcessPickerChipClear"));
    clear->setAutoRaise(true);
    clear->setFocusPolicy(Qt::NoFocus);
    clear->setIcon(style()->standardIcon(QStyle::SP_LineEditClearButton));
    clear->setToolTip(tr("Show the processes that cannot be cabled to the port too"));
    connect(clear, &QToolButton::clicked, this, [this] { m->clearPortFilter(); });
    chipLay->addWidget(clear);
    chip->setToolTip(tr("Only the processes that can be cabled to the port"));
    m->portChip = chip;
    top->addWidget(chip);
  }

  auto columns = new QHBoxLayout;
  columns->setSpacing(6);
  lay->addLayout(columns, 1);

  // The rows are filled where the keys go: the processes take them from the
  // search too
  auto delegate = [this](const QStringList* words) {
    return new PickerDelegate{
        [this](const QWidget* view) {
      const QWidget* focus = QApplication::focusWidget();
      return focus == view
             || (view == m->processes && focus == m->search);
    }, words, this};
  };

  m->categories = new QTreeWidget{this};
  m->categories->setHeaderHidden(true);
  m->categories->setUniformRowHeights(true);
  m->categories->setIndentation(14);
  setupView(*m->categories, *delegate(nullptr));
  columns->addWidget(m->categories, 3);

  m->results = new Results{*m->snapshot, m->plugin.presets, this};
  m->processes = new QTreeView{this};
  m->processes->setModel(m->results);
  m->processes->setHeaderHidden(true);
  m->processes->setRootIsDecorated(false);
  m->processes->setUniformRowHeights(true);
  m->processes->setSelectionMode(QAbstractItemView::SingleSelection);
  m->processes->setEditTriggers(QAbstractItemView::NoEditTriggers);
  setupView(*m->processes, *delegate(&m->words));
  columns->addWidget(m->processes, 7);

  // What the current process is, and its presets where they can be loaded
  auto detailsPane = new QWidget{this};
  {
    QPalette dp = pal;
    dp.setColor(QPalette::Window, pal.color(QPalette::Base));
    detailsPane->setPalette(dp);
    detailsPane->setAutoFillBackground(true);
  }
  auto dl = new QVBoxLayout{detailsPane};
  dl->setContentsMargins(10, 10, 10, 4);
  dl->setSpacing(6);
  {
    auto head = new QHBoxLayout;
    head->setSpacing(8);
    m->details.icon = new QLabel{detailsPane};
    m->details.icon->setFixedSize(20, 20);
    head->addWidget(m->details.icon);
    m->details.name = new QLabel{detailsPane};
    QFont f = m->details.name->font();
    f.setBold(true);
    if(f.pointSizeF() > 0)
      f.setPointSizeF(f.pointSizeF() * 1.2);
    m->details.name->setFont(f);
    m->details.name->setTextFormat(Qt::PlainText);
    head->addWidget(m->details.name, 1);
    dl->addLayout(head);
  }
  QPalette dim = pal;
  dim.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
  m->details.where = new QLabel{detailsPane};
  m->details.where->setTextFormat(Qt::PlainText);
  m->details.where->setWordWrap(true);
  m->details.where->setPalette(dim);
  dl->addWidget(m->details.where);
  m->details.text = new QLabel{detailsPane};
  m->details.text->setTextFormat(Qt::PlainText);
  m->details.text->setWordWrap(true);
  m->details.text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  dl->addWidget(m->details.text);
  m->details.ports = new QLabel{detailsPane};
  m->details.ports->setTextFormat(Qt::RichText);
  m->details.ports->setWordWrap(true);
  dl->addWidget(m->details.ports);

  m->presets = new QListWidget{detailsPane};
  if(m->options.presets)
  {
    m->details.presetsTitle = new QLabel{tr("Presets"), detailsPane};
    QFont f = m->details.presetsTitle->font();
    f.setBold(true);
    m->details.presetsTitle->setFont(f);
    dl->addSpacing(4);
    dl->addWidget(m->details.presetsTitle);
    setupView(*m->presets, *delegate(nullptr));
    dl->addWidget(m->presets, 1);
  }
  else
  {
    // Where presets cannot be loaded, no list of them
    m->presets->hide();
    dl->addStretch(1);
  }
  columns->addWidget(detailsPane, 4);

  auto footer = new QHBoxLayout;
  lay->addLayout(footer);
  footer->addWidget(new KeyHints{
      {{tr("Return"), tr("add")},
       {tr("Up Down"), tr("choose")},
       {tr("Left Right"), m->options.presets ? tr("columns") : tr("categories")},
       {tr("Escape"), tr("close")}},
      this});
  footer->addStretch(1);
  // A pop-up is not one of the window manager's windows: it is moved by
  // dragging it where nothing else takes the mouse, and resized from here
  footer->addWidget(new QSizeGrip{this}, 0, Qt::AlignBottom | Qt::AlignRight);

  // The rows show where the keys go
  m->focusChanged = connect(qApp, &QApplication::focusChanged, this, [this] {
    for(QAbstractItemView* v :
        std::initializer_list<QAbstractItemView*>{m->categories, m->processes, m->presets})
      v->viewport()->update();
  });

  // The library as it was when opened
  QTreeWidgetItem* first = m->fillCategories();

  connect(
      m->categories, &QTreeWidget::currentItemChanged, this,
      [this](QTreeWidgetItem* item) {
    if(!m->search->text().isEmpty())
    {
      // Choosing a category ends a search
      QSignalBlocker b{m->search};
      m->search->clear();
    }
    m->showCategory(item);
  });
  connect(m->search, &QLineEdit::textChanged, this, [this](const QString& t) {
    // A search looks in every category
    if(!t.isEmpty() && m->categories->currentItem() != m->all)
    {
      QSignalBlocker b{m->categories};
      m->categories->setCurrentItem(m->all);
    }
    m->runSearch(t);
  });
  connect(
      m->processes->selectionModel(), &QItemSelectionModel::currentChanged, this,
      [this] { m->showPresets(); });
  connect(
      m->presets, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    m->chosenPreset.reset();
    if(item)
      if(const int p = item->data(PresetRole).toInt();
         ossia::valid_index(p, m->plugin.presets))
        m->chosenPreset = m->plugin.presets[p];
  });
  connect(m->processes, &QTreeView::doubleClicked, this, [this] { accept(); });
  connect(m->presets, &QListWidget::itemDoubleClicked, this, [this] { accept(); });

  // Presets come and go as the library is scanned: shown again once it settled
  const auto changed = [this] { m->presetsChanged(); };
  connect(&m->plugin, &Process::ApplicationPlugin::presetAdded, this, changed);
  connect(&m->plugin, &Process::ApplicationPlugin::presetRemoved, this, changed);
  connect(&m->plugin, &Process::ApplicationPlugin::presetChanged, this, changed);
  connect(&m->plugin, &Process::ApplicationPlugin::presetsCleared, this, changed);

  for(QWidget* w : std::initializer_list<QWidget*>{
          m->search, m->categories, m->processes, m->presets})
    w->installEventFilter(this);
  setTabOrder(m->search, m->categories);
  setTabOrder(m->categories, m->processes);
  setTabOrder(m->processes, m->presets);

  m->categories->setCurrentItem(first);
}

ProcessPicker::~ProcessPicker()
{
  // The focus leaves its widgets as they go, once m is gone
  disconnect(m->focusChanged);
}

void ProcessPicker::mousePressEvent(QMouseEvent* ev)
{
  if(ev->button() == Qt::LeftButton)
  {
    m_dragFrom = ev->globalPosition().toPoint() - pos();
    ev->accept();
    return;
  }
  QFrame::mousePressEvent(ev);
}

void ProcessPicker::mouseMoveEvent(QMouseEvent* ev)
{
  if(m_dragFrom && (ev->buttons() & Qt::LeftButton))
  {
    move(ev->globalPosition().toPoint() - *m_dragFrom);
    ev->accept();
    return;
  }
  QFrame::mouseMoveEvent(ev);
}

void ProcessPicker::mouseReleaseEvent(QMouseEvent* ev)
{
  m_dragFrom.reset();
  QFrame::mouseReleaseEvent(ev);
}

void ProcessPicker::paintEvent(QPaintEvent* ev)
{
  QFrame::paintEvent(ev);
  QPainter p{this};
  p.setPen(qApp->palette().color(QPalette::Highlight));
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ProcessPicker::setSearch(const QString& text)
{
  m->search->setText(text);
}

QStringList ProcessPicker::categories() const
{
  QStringList l;
  for(int i = 0; i < m->categories->topLevelItemCount(); i++)
    l.push_back(m->categories->topLevelItem(i)->text(0));
  return l;
}

QStringList ProcessPicker::processes() const
{
  QStringList l;
  for(int i = 0; i < m->results->rowCount({}); i++)
    if(m->results->rows[i].entry >= 0)
      l.push_back(m->results->index(i, 0).data().toString());
  return l;
}

QStringList ProcessPicker::presets() const
{
  QStringList l;
  for(int i = 0; i < m->presets->count(); i++)
    l.push_back(m->presets->item(i)->text());
  return l;
}

bool ProcessPicker::accept()
{
  return m->accept();
}

bool ProcessPicker::eventFilter(QObject* obj, QEvent* event)
{
  if(event->type() == QEvent::ShortcutOverride)
  {
    // Its own keys go to it, not to the application's shortcuts: Space,
    // Escape and the arrows do something else in the score. With Control,
    // Alt or Meta they are the field's own or the application's.
    if(!hasCommandModifier(*static_cast<QKeyEvent*>(event)))
    {
      event->accept();
      return true;
    }
    return false;
  }
  if(event->type() == QEvent::KeyRelease
     && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
  {
    // In the score the press of Escape does not reach a pop-up, its release
    // does
    close();
    return true;
  }
  if(event->type() != QEvent::KeyPress)
    return QFrame::eventFilter(obj, event);
  auto& ev = *static_cast<QKeyEvent*>(event);
  const int key = ev.key();

  if(key == Qt::Key_Escape)
  {
    close();
    return true;
  }

  if(obj == m->search)
  {
    switch(key)
    {
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        // The search keeps the focus: typing goes on
        QApplication::sendEvent(m->processes, &ev);
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        accept();
        return true;
      case Qt::Key_Right:
        if(m->options.presets && m->search->cursorPosition() == m->search->text().size())
        {
          m->presets->setFocus();
          return true;
        }
        break;
      case Qt::Key_Left:
        if(m->search->cursorPosition() == 0)
        {
          m->categories->setFocus();
          return true;
        }
        break;
    }
    return false;
  }

  if(obj == m->categories)
  {
    switch(key)
    {
      case Qt::Key_Right: {
        auto item = m->categories->currentItem();
        if(item && item->childCount() > 0 && !item->isExpanded())
          item->setExpanded(true);
        else
          m->processes->setFocus();
        return true;
      }
      case Qt::Key_Return:
      case Qt::Key_Enter:
        m->processes->setFocus();
        return true;
    }
  }
  else if(obj == m->processes)
  {
    switch(key)
    {
      case Qt::Key_Left:
        m->categories->setFocus();
        return true;
      case Qt::Key_Right:
        if(m->options.presets)
          m->presets->setFocus();
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        accept();
        return true;
    }
  }
  else if(obj == m->presets)
  {
    switch(key)
    {
      case Qt::Key_Left:
        m->processes->setFocus();
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        accept();
        return true;
    }
  }

  if(key == Qt::Key_Backspace || isPrintable(ev))
  {
    m->typeIntoSearch(ev);
    return true;
  }
  return QFrame::eventFilter(obj, event);
}
}

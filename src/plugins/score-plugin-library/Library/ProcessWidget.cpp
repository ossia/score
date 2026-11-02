#include "ProcessWidget.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/PresetTree.hpp>
#include <Process/Process.hpp>
#include <Process/PreviewSettings.hpp>
#include <Process/ProcessList.hpp>

#include <Library/PresetFile.hpp>
#include <Library/ItemModelFilterLineEdit.hpp>
#include <Library/LibraryWidget.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/RecursiveFilterProxy.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/widgets/Pixmap.hpp>
#include <score/tools/File.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/view/FixedTabWidget.hpp>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QScrollArea>
#include <QTimer>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QVBoxLayout>

namespace Library
{
class InfoWidget final : public QScrollArea
{
public:
  InfoWidget(QWidget* parent)
      : QScrollArea{parent}
  {
    // A long description scrolls rather than taking the presets' room
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMaximumHeight(160);
    auto content = new QWidget;
    auto lay = new score::MarginLess<QVBoxLayout>{content};
    lay->setAlignment(Qt::AlignTop);
    lay->setContentsMargins(6, 6, 6, 6);
    QFont f;
    f.setBold(true);
    m_name.setFont(f);
    m_name.setAlignment(Qt::AlignTop);
    lay->addWidget(&m_name);
    lay->addWidget(&m_author);
    m_author.setWordWrap(true);
    m_author.setAlignment(Qt::AlignTop);
    lay->addWidget(&m_io);
    lay->addWidget(&m_description);
    m_description.setWordWrap(true);
    m_description.setAlignment(Qt::AlignTop);
    lay->addWidget(&m_tags);
    m_tags.setWordWrap(true);
    m_tags.setAlignment(Qt::AlignTop);
    m_documentationLink.setAlignment(Qt::AlignBottom);
    lay->addStretch(2);
    lay->addWidget(&m_documentationLink);
    setWidget(content);
    setVisible(false);
  }

  //! As tall as its text, up to its maximum, past which it scrolls
  QSize sizeHint() const override
  {
    int h = 0;
    if(auto w = widget())
    {
      // Wrapped text is as tall as the width it wraps in
      const int width = viewport()->width();
      h = w->hasHeightForWidth() && width > 0 ? w->heightForWidth(width)
                                               : w->sizeHint().height();
      h += 2 * frameWidth();
    }
    return {QScrollArea::sizeHint().width(), std::min(h, maximumHeight())};
  }

  void setData(const std::optional<ProcessData>& d)
  {
    updateGeometry();
    m_author.setText(QString{});
    m_description.setText(QString{});
    m_io.setText(QString{});
    m_tags.setText(QString{});
    m_documentationLink.clear();

    if(d)
    {
      if(auto f
         = score::GUIAppContext().interfaces<Process::ProcessFactoryList>().get(d->key))
      {
        setVisible(true);
        auto desc = f->descriptor(d->customData);

        if(!d->prettyName.isEmpty())
          m_name.setText(d->prettyName);
        else
          m_name.setText(desc.prettyName);

        if(!desc.author.isEmpty())
          m_author.setText(tr("Provided by ") + desc.author);
        else
          m_author.setText(tr("Author unknown"));

        if(!desc.description.isEmpty())
          m_description.setText(desc.description);

        QString io;
        if(desc.inlets)
        {
          io += QString::number(desc.inlets->size()) + tr(" input");
          if(desc.inlets->size() > 1)
            io += "s";
        }
        if(desc.inlets && desc.outlets)
          io += ", ";
        if(desc.outlets)
        {
          io += QString::number(desc.outlets->size()) + tr(" output");
          if(desc.outlets->size() > 1)
            io += "s";
        }
        m_io.setText(io);
        if(!desc.tags.empty())
        {
          m_tags.setText(tr("Tags: ") + desc.tags.join(", "));
        }

        if(!desc.documentationLink.isEmpty())
        {
          QString linkText = "Explore the documentation";
          QString iconPath = ":/icons/undock_on.png";
          QString iconHtml = "<img src=\"" + iconPath + "\" width=\"16\" height=\"16\" style=\"vertical-align:middle;\" />";
          QString fullLink = "<a href=\"" + desc.documentationLink.toString() + "\">" + linkText + " " + iconHtml + "</a>";
          m_documentationLink.setTextFormat(Qt::RichText);
          m_documentationLink.setText(fullLink);
          m_documentationLink.setOpenExternalLinks(true);
        }

        return;
      }
    }

    setVisible(false);
  }

  QLabel m_name;
  QLabel m_io;
  QLabel m_description;
  QLabel m_author;
  QLabel m_tags;
  QLabel m_documentationLink;
};

ProcessWidget::ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_processModel{new ProcessesItemModel{ctx, this}}
    , m_presetModel{new PresetItemModel{ctx, this}}
    , m_preview{this}
{
  auto slay = new score::MarginLess<QVBoxLayout>{this};
  setLayout(slay);

  setStatusTip(
      QObject::tr("This panel shows the available processes.\n"
                  "They can be drag'n'dropped in the score, in intervals, "
                  "and sometimes in effect chains."));

  {
    m_notice.setWordWrap(true);
    m_notice.setTextFormat(Qt::RichText);
    m_notice.setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_notice.setOpenExternalLinks(false);
    m_notice.setContentsMargins(4, 4, 4, 4);
    m_notice.hide();
    slay->addWidget(&m_notice);
    connect(&m_notice, &QLabel::linkActivated, this, [this](const QString&) {
      if(m_noticeAction)
        m_noticeAction();
    });
  }

  QLineEdit* filter{};

  {
    auto processFilterProxy = new ProcessFilterProxy{this};
    processFilterProxy->setSourceModel(m_processModel);
    processFilterProxy->setFilterKeyColumn(0);
    filter = new ItemModelFilterLineEdit{*processFilterProxy, m_tv, this};
    m_processFilter = filter;
    slay->addWidget(filter);
    slay->addWidget(&m_tv, 3);
    m_tv.setModel(processFilterProxy);
    m_tv.setStatusTip(statusTip());
    m_tv.setUniformRowHeights(true);
    setup_treeview(m_tv);
  }

  {
    auto lay = new score::MarginLess<QHBoxLayout>{&m_targetBar};
    lay->setContentsMargins(4, 2, 2, 2);
    m_targetLabel.setTextFormat(Qt::PlainText);
    // A browser's preview toggle: lit while presets load as they are browsed
    auto audition = new QPushButton{&m_targetBar};
    audition->setCheckable(true);
    audition->setFlat(true);
    audition->setMinimumWidth(24);
    audition->setMaximumWidth(24);
    const auto onIcon = QIcon{score::get_pixmap(":/icons/headphones_on.png")};
    const auto offIcon = QIcon{score::get_pixmap(":/icons/headphones_off.png")};
    const auto showAudition = [=, this](bool on) {
      audition->setIcon(on ? onIcon : offIcon);
      audition->setToolTip(
          on ? tr("Presets load as you move through them. Click to only load "
                  "one on Enter or a double-click.")
             : tr("Presets load on Enter or a double-click. Click to load them "
                  "as you move through them."));
    };
    m_auditionEnabled = QSettings{}.value("Library/AuditionPresets", true).toBool();
    audition->setChecked(m_auditionEnabled);
    showAudition(m_auditionEnabled);
    connect(audition, &QPushButton::toggled, this, [this, showAudition](bool on) {
      m_auditionEnabled = on;
      QSettings{}.setValue("Library/AuditionPresets", on);
      showAudition(on);
      // What is heard is kept
      m_audition.commit();
      m_presetModel->refreshCurrent();
    });
    lay->addWidget(audition);
    lay->addWidget(&m_targetLabel, 1);
    auto save = new QToolButton{&m_targetBar};
    save->setText(tr("Save"));
    save->setToolTip(
        tr("Save the process's current state as a preset, in the selected folder"));
    connect(save, &QToolButton::clicked, this, [this] {
      if(auto proc = m_presetModel->target())
        m_presetModel->savePreset(
            *proc, m_presetModel->folderPath(m_presets.currentIndex()).join('/'));
    });
    lay->addWidget(save);
    auto close = new QToolButton{&m_targetBar};
    close->setText(QStringLiteral("x"));
    close->setToolTip(tr("Stop loading the presets into this process"));
    connect(close, &QToolButton::clicked, this, &ProcessWidget::clearTarget);
    lay->addWidget(close);
    m_targetBar.hide();
    slay->addWidget(&m_targetBar);
  }

  {
    auto search = new QLineEdit{this};
    search->setPlaceholderText(tr("Search presets"));
    search->setClearButtonEnabled(true);
    connect(search, &QLineEdit::textChanged, m_presetModel, &PresetItemModel::setFilterText);
    slay->addWidget(search);
    m_presets.setPresetModel(m_presetModel);
    slay->addWidget(&m_presets, 2);
  }

  slay->addWidget(&m_preview);
  {
    auto previewLay = new score::MarginLess<QHBoxLayout>{&m_preview};
    m_preview.setLayout(previewLay);
    m_preview.hide();
  }

  // Turning previews off has to reach the thumbnail already on screen, not wait
  // for the next selection. Both handlers share m_previewChild, so rebuild the
  // one that is actually showing: re-announcing the tree selection would
  // destroy a preset preview and clear state nothing asked to clear.
  con(Process::PreviewSettings::instance(), &Process::PreviewSettings::enabledChanged,
      this, [this](bool) {
    if(m_presetModel->preset(m_presets.currentIndex()) && m_refreshPresetPreview)
      m_refreshPresetPreview();
    else if(auto* sm = m_tv.selectionModel(); sm && !sm->selectedIndexes().isEmpty())
      m_tv.reselect();
  });

  auto infoWidg = new InfoWidget{this};
  score::setHelp(infoWidg, statusTip());
  slay->addWidget(infoWidg);

  connect(
      &m_tv, &ProcessTreeView::selected, this,
      [this, infoWidg, filter](const std::optional<Library::ProcessData>& pdata) {
#if defined(_WIN32)
    const bool filter_had_focus = filter->hasFocus();
    const bool tree_had_focus = m_tv.hasFocus();
#endif
    m_preview.hide();

    // Update info widget
    infoWidg->setData(pdata);

    // Another process chosen by the user: its presets no longer load into
    // the one whose button opened them
    if(!m_choosingTarget)
      if(auto t = m_presetModel->target(); t && (!pdata || pdata->key != t->concreteKey()))
        clearTarget();

    // Its presets
    if(pdata)
      m_presetModel->setProcess(Process::ProcessIdentifier{pdata->key, pdata->customData});
    else
      m_presetModel->setProcess(std::nullopt);

    // Update the preview
    delete m_previewChild;
    m_previewChild = nullptr;
    if(pdata)
    {
      if(QFile::exists(pdata->customData))
      {
        for(auto lib : libraryInterface(pdata->customData))
        {
          if((m_previewChild = lib->previewWidget(pdata->customData, &m_preview)))
          {
            m_preview.layout()->addWidget(m_previewChild);
            m_preview.show();
            break;
          }
        }
      }
    }

#if defined(_WIN32)
    for(int i = 0; i < 100; i++)
    {
      QTimer::singleShot(i, this, [=, this] {
        if(filter_had_focus)
          filter->setFocus();
        else if(tree_had_focus)
          m_tv.setFocus();
      });
    }
#endif
  });

  connect(
      &m_presets, &PresetTreeView::selected, this,
      [this, filter](const std::optional<Process::Preset>& selected) {
    if(!selected)
      return;

    // Heard at once in the process the presets load into
    if(auto proc = m_presetModel->target(); proc && m_auditionEnabled)
      if(m_audition.preview(*proc, *selected))
        m_presetModel->refreshCurrent();

#if defined(_WIN32)
    const bool filter_had_focus = filter->hasFocus();
    const bool tree_had_focus = m_tv.hasFocus();
#endif

    m_preview.hide();
    delete m_previewChild;
    m_previewChild = nullptr;

    const auto& preset = *selected;

    const auto buildPreview = [this](const Process::Preset& p) {
      delete m_previewChild;
      m_previewChild = nullptr;
      for(auto& lib : score::GUIAppContext().interfaces<LibraryInterfaceList>())
      {
        if((m_previewChild = lib.previewWidget(p, &m_preview)))
        {
          m_preview.layout()->addWidget(m_previewChild);
          m_preview.show();
          break;
        }
      }
    };
    buildPreview(preset);
    m_refreshPresetPreview = [this, buildPreview, p = preset] { buildPreview(p); };
#if defined(_WIN32)
    for(int i = 0; i < 100; i++)
    {
      QTimer::singleShot(i, this, [=, this] {
        if(filter_had_focus)
          filter->setFocus();
        else if(tree_had_focus)
          m_tv.setFocus();
      });
    }
#endif
  });
  m_presets.setMinimumHeight(100);

  auto& presetLib = ctx.applicationPlugin<Process::ApplicationPlugin>();
  con(presetLib, &Process::ApplicationPlugin::savePreset, this,
      [&](const Process::ProcessModel* proc) {
    SCORE_ASSERT(proc);
    this->m_presetModel->savePreset(*proc);
  });
  con(presetLib, &Process::ApplicationPlugin::showPresets, this,
      [this](const Process::ProcessModel* proc) {
    if(proc)
      showPresets(*proc);
  });
  con(presetLib, &Process::ApplicationPlugin::currentPresetChanged, this,
      [this](const Process::ProcessModel* proc) {
    if(proc && proc == m_presetModel->target())
    {
      m_presetModel->refreshCurrent();
      m_presets.showCurrentPreset();
    }
  });
  connect(
      &m_presets, &PresetTreeView::loadRequested, this,
      [this](const Process::Preset& preset) {
    auto proc = m_presetModel->target();
    if(!proc)
      return;
    // The one being heard is kept; another is loaded as it is
    if(m_audition.active() && Process::isCurrentPreset(*proc, preset))
      m_audition.commit();
    else
    {
      m_audition.commit();
      Process::loadPreset(*proc, preset, score::IDocument::documentContext(*proc));
    }
    m_presetModel->refreshCurrent();
  });
  connect(&m_presets, &PresetTreeView::cancelRequested, this, [this] {
    if(!m_audition.active())
      return;
    m_audition.rollback();
    m_presetModel->refreshCurrent();
    m_presets.showCurrentPreset();
  });
  // Doing anything else keeps what is heard: a click elsewhere takes the
  // focus before it acts
  con(*qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
    if(m_audition.active() && now != &m_presets && !m_presets.isAncestorOf(now))
      m_audition.commit();
  });
}

void ProcessWidget::raisePanel()
{
  // Wherever the panel is docked
  if(auto view = qobject_cast<score::View*>(score::GUIAppContext().mainWindow))
    for(auto tabs : {view->leftTabs, view->rightTabs, view->bottomTabs})
      if(tabs)
        tabs->showTab(this);
}

void ProcessWidget::showPresets(const Process::ProcessModel& proc)
{
  m_audition.commit();
  raisePanel();

  // Its entry in the process tree, which a search there could hide
  auto idx = m_processModel->find(proc.concreteKey(), proc.effect());
  if(auto proxy = qobject_cast<QSortFilterProxyModel*>(m_tv.model()))
  {
    if(!proxy->mapFromSource(idx).isValid() && m_processFilter)
      m_processFilter->clear();
    idx = proxy->mapFromSource(idx);
  }
  m_choosingTarget = true;
  if(idx.isValid())
  {
    for(auto p = idx.parent(); p.isValid(); p = p.parent())
      m_tv.expand(p);
    m_tv.setCurrentIndex(idx);
    // Once the rows just opened are laid out
    QTimer::singleShot(0, &m_tv, [this] {
      m_tv.scrollTo(m_tv.currentIndex(), QAbstractItemView::PositionAtCenter);
    });
  }
  m_choosingTarget = false;

  // The presets are those of this very process: its script, its plug-in
  m_presetModel->setTarget(&proc);
  score::GUIAppContext().applicationPlugin<Process::ApplicationPlugin>().setPresetTarget(
      &proc);
  disconnect(m_targetGone);
  m_targetGone = connect(&proc, &QObject::destroyed, this, [this] { clearTarget(); });

  // It is the one selected while its presets load into it; deselected, the
  // library is browsed for any process again
  auto& sel = score::IDocument::documentContext(proc).selectionStack;
  score::SelectionDispatcher{sel}.select(proc);
  disconnect(m_targetDeselected);
  m_targetDeselected = connect(
      &sel, &score::SelectionStack::currentSelectionChanged, this,
      [this](const Selection&, const Selection&) {
    // Once what deselected it is done: undoing its creation deselects it on
    // the way, which must not put anything in the history meanwhile
    QTimer::singleShot(0, this, [this] {
      auto t = m_presetModel->target();
      if(!t)
        return;
      const auto now
          = score::IDocument::documentContext(*t).selectionStack.currentSelection();
      if(!ossia::any_of(now, [t](const auto& obj) { return obj.data() == t; }))
        clearTarget();
    });
  });
  const auto name = proc.metadata().getLabel().isEmpty() ? proc.metadata().getName()
                                                         : proc.metadata().getLabel();
  m_targetLabel.setText(tr("Presets: %1").arg(name));
  m_targetBar.show();
  m_presets.showCurrentPreset();
  m_presets.setFocus();
}

void ProcessWidget::clearTarget()
{
  m_audition.commit();
  disconnect(m_targetGone);
  disconnect(m_targetDeselected);
  score::GUIAppContext().applicationPlugin<Process::ApplicationPlugin>().setPresetTarget(
      nullptr);
  m_presetModel->setTarget(nullptr);
  m_targetBar.hide();
}

void ProcessWidget::setNotice(const QString& html, std::function<void()> onActivated)
{
  m_noticeAction = std::move(onActivated);
  m_notice.setText(html);
  m_notice.setVisible(!html.isEmpty());
}

ProcessWidget::~ProcessWidget() { }

QSet<QString> PresetLibraryHandler::acceptedFiles() const noexcept
{
  return {"scp"};
}

QSet<QString> PresetLibraryHandler::acceptedMimeTypes() const noexcept
{
  return {score::mime::processpreset()};
}

void PresetLibraryHandler::setup(
    ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  presetLib = &ctx.applicationPlugin<Process::ApplicationPlugin>();
  processes = &ctx.interfaces<Process::ProcessFactoryList>();
  // Called at each scan of the library, which reads every preset again
  presetLib->clearPresets();
}

void PresetLibraryHandler::addPath(std::string_view path)
{
  QFile f{QString::fromUtf8(path.data(), path.length())};

  if(!f.open(QIODevice::ReadOnly))
    return;

  if(auto p = Process::Preset::fromJson(*processes, score::mapAsByteArray(f)))
  {
    p->path = f.fileName();
    presetLib->addPreset(std::move(*p));
  }
}

bool PresetLibraryHandler::onDrop(
    const QMimeData& mime, int row, int column, const QDir& parent)
{
  if(!mime.hasFormat(score::mime::processpreset()))
    return false;

  const auto data = mime.data(score::mime::processpreset());
  auto json = readJson(data);
  if(!json.IsObject() || !json.HasMember("Name") || !json["Name"].IsString())
    return true;

  // A name, not a path out of the folder it is dropped in
  const auto basename = QFileInfo{JsonValue{json["Name"]}.toString()}.fileName();
  if(basename.isEmpty())
    return false;
  const QString filename
      = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".scp");
  return writePresetData(filename, data);
}

}

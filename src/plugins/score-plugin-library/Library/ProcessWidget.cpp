#include "ProcessWidget.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/ProcessList.hpp>

#include <Library/ItemModelFilterLineEdit.hpp>
#include <Library/LibraryWidget.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/RecursiveFilterProxy.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/File.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

namespace Library
{
class InfoWidget final : public QScrollArea
{
public:
  InfoWidget(QWidget* parent)
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};
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
    setVisible(false);
  }

  void setData(const std::optional<ProcessData>& d)
  {
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

  QLineEdit* filter{};

  {
    auto processFilterProxy = new ProcessFilterProxy{this};
    processFilterProxy->setSourceModel(m_processModel);
    processFilterProxy->setFilterKeyColumn(0);
    filter = new ItemModelFilterLineEdit{*processFilterProxy, m_tv, this};
    slay->addWidget(filter);
    slay->addWidget(&m_tv, 3);
    m_tv.setModel(processFilterProxy);
    m_tv.setStatusTip(statusTip());
    m_tv.setUniformRowHeights(true);
    setup_treeview(m_tv);
  }

  auto presetFilterProxy = new PresetFilterProxy{this};
  {
    presetFilterProxy->setSourceModel(m_presetModel);
    m_lv.setModel(presetFilterProxy);
    m_lv.setAcceptDrops(true);
    m_lv.setDragEnabled(true);
    slay->addWidget(&m_lv, 1);
  }

  slay->addWidget(&m_preview);
  {
    auto previewLay = new score::MarginLess<QHBoxLayout>{&m_preview};
    m_preview.setLayout(previewLay);
    m_preview.hide();
  }

  auto infoWidg = new InfoWidget{this};
  score::setHelp(infoWidg, statusTip());
  slay->addWidget(infoWidg, 1);

  connect(
      &m_tv, &ProcessTreeView::selected, this,
      [this, infoWidg, filter,
       presetFilterProxy](const std::optional<Library::ProcessData>& pdata) {
#if defined(_WIN32)
    const bool filter_had_focus = filter->hasFocus();
    const bool tree_had_focus = m_tv.hasFocus();
#endif
    m_preview.hide();

    // Update info widget
    infoWidg->setData(pdata);

    // Update the filter
    if(pdata)
    {
      presetFilterProxy->currentFilter = {pdata->key, pdata->customData};
    }
    else
    {
      presetFilterProxy->currentFilter = {};
    }
    presetFilterProxy->invalidate();

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

  auto preset_sel = m_lv.selectionModel();
  connect(
      preset_sel, &QItemSelectionModel::currentRowChanged, this,
      [this, filter, presetFilterProxy](const QModelIndex& idx, const QModelIndex&) {
    if(!idx.isValid())
      return;
    auto midx = presetFilterProxy->mapToSource(idx);
    if(!midx.isValid())
      return;

    if(!ossia::valid_index(midx.row(), m_presetModel->presets))
      return;

#if defined(_WIN32)
    const bool filter_had_focus = filter->hasFocus();
    const bool tree_had_focus = m_tv.hasFocus();
#endif

    m_preview.hide();
    delete m_previewChild;
    m_previewChild = nullptr;

    const auto& preset = m_presetModel->presets[midx.row()];

    for(auto& lib : score::GUIAppContext().interfaces<LibraryInterfaceList>())
    {
      if((m_previewChild = lib.previewWidget(preset, &m_preview)))
      {
        m_preview.layout()->addWidget(m_previewChild);
        m_preview.show();
        break;
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
  m_lv.setMinimumHeight(100);

  auto& presetLib = ctx.applicationPlugin<Process::ApplicationPlugin>();
  con(presetLib, &Process::ApplicationPlugin::savePreset, this,
      [&](const Process::ProcessModel* proc) {
    SCORE_ASSERT(proc);
    this->m_presetModel->savePreset(*proc);
  });
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
}

void PresetLibraryHandler::addPath(std::string_view path)
{
  QFile f{QString::fromUtf8(path.data(), path.length())};

  if(!f.open(QIODevice::ReadOnly))
    return;

  if(auto p = Process::Preset::fromJson(*processes, score::mapAsByteArray(f)))
  {
    presetLib->addPreset(std::move(*p));
  }
}

bool PresetLibraryHandler::onDrop(
    const QMimeData& mime, int row, int column, const QDir& parent)
{
  if(!mime.hasFormat(score::mime::processpreset()))
    return false;

  auto json = readJson(mime.data(score::mime::processpreset()));
  if(!json.HasMember("Name"))
    return true;

  auto basename = JsonValue{json["Name"]}.toString();
  const QString filename
      = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".scp");

  QFile f{filename};
  if(!f.open(QIODevice::WriteOnly))
    return false;

  f.write(mime.data(score::mime::processpreset()));
  return true;
}

}

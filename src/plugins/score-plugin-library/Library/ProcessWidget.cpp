#include "ProcessWidget.hpp"

#include <Library/ItemModelFilterLineEdit.hpp>
#include <Library/LibraryWidget.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/RecursiveFilterProxy.hpp>
#include <Process/ApplicationPlugin.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/tools/File.hpp>

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Library
{
class InfoWidget final : public QScrollArea
{
public:
  InfoWidget(QWidget* parent)
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};
    lay->setContentsMargins(6, 6, 6, 6);
    QFont f;
    f.setBold(true);
    m_name.setFont(f);
    lay->addWidget(&m_name);
    lay->addWidget(&m_author);
    m_author.setWordWrap(true);
    lay->addWidget(&m_io);
    lay->addWidget(&m_description);
    m_description.setWordWrap(true);
    lay->addWidget(&m_tags);
    m_tags.setWordWrap(true);
    setVisible(false);
  }

  void setData(const std::optional<ProcessData>& d)
  {
    m_author.setText(QString{});
    m_description.setText(QString{});
    m_io.setText(QString{});
    m_tags.setText(QString{});

    if (d)
    {
      if (auto f = score::GUIAppContext()
                       .interfaces<Process::ProcessFactoryList>()
                       .get(d->key))
      {
        setVisible(true);
        auto desc = f->descriptor(QString{/*TODO pass customdata ?*/});

        if (!d->prettyName.isEmpty())
          m_name.setText(d->prettyName);
        else
          m_name.setText(desc.prettyName);

        if (!d->author.isEmpty())
          m_author.setText(tr("Provided by ") + d->author);
        else if (!desc.author.isEmpty())
          m_author.setText(tr("Provided by ") + desc.author);

        if (!d->description.isEmpty())
          m_description.setText(d->description);
        else if (!desc.description.isEmpty())
          m_description.setText(desc.description);

        QString io;
        if (desc.inlets)
        {
          io += QString::number(desc.inlets->size()) + tr(" input");
          if (desc.inlets->size() > 1)
            io += "s";
        }
        if (desc.inlets && desc.outlets)
          io += ", ";
        if (desc.outlets)
        {
          io += QString::number(desc.outlets->size()) + tr(" output");
          if (desc.outlets->size() > 1)
            io += "s";
        }
        m_io.setText(io);
        if (!desc.tags.empty())
        {
          m_tags.setText(tr("Tags: ") + desc.tags.join(", "));
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
};

ProcessWidget::ProcessWidget(
    const score::GUIApplicationContext& ctx,
    QWidget* parent)
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
    auto processFilterProxy = new RecursiveFilterProxy{this};
    processFilterProxy->setSourceModel(m_processModel);
    processFilterProxy->setFilterKeyColumn(0);
    slay->addWidget(
        new ItemModelFilterLineEdit{*processFilterProxy, m_tv, this});
    slay->addWidget(&m_tv, 3);
    m_tv.setModel(processFilterProxy);
    m_tv.setStatusTip(statusTip());
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
  infoWidg->setStatusTip(statusTip());
  slay->addWidget(infoWidg, 1);

  connect(&m_tv, &ProcessTreeView::selected, this, [=](const std::optional<Library::ProcessData>& pdata) {
    m_preview.hide();

    // Update info widget
    infoWidg->setData(pdata);

    // Update the filter
    if (pdata)
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
        for (auto lib : libraryInterface(pdata->customData))
        {
          if ((m_previewChild = lib->previewWidget(pdata->customData, &m_preview)))
          {
            m_preview.layout()->addWidget(m_previewChild);
            m_preview.show();
            break;
          }
        }
      }
    }
  });


  auto preset_sel = m_lv.selectionModel();
  connect(preset_sel, &QItemSelectionModel::currentRowChanged, this, [&](const QModelIndex& idx, const QModelIndex&) {
            if(!idx.isValid())
              return;
            if(idx.row() >= int32_t(m_presetModel->presets.size()) || idx.row() < 0)
              return;

            m_preview.hide();
            delete m_previewChild;
            m_previewChild = nullptr;


            const auto& preset = m_presetModel->presets[idx.row()];

            for (auto& lib : score::GUIAppContext().interfaces<LibraryInterfaceList>())
            {
              if ((m_previewChild = lib.previewWidget(preset, &m_preview)))
              {
                m_preview.layout()->addWidget(m_previewChild);
                m_preview.show();
                break;
              }
            }
          });
  m_lv.setMinimumHeight(100);
}

ProcessWidget::~ProcessWidget() { }

QSet<QString> PresetLibraryHandler::acceptedFiles() const noexcept { return {"scp"}; }

void PresetLibraryHandler::setup(ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  presetLib = &ctx.applicationPlugin<Process::ApplicationPlugin>();
  processes = &ctx.interfaces<Process::ProcessFactoryList>();
}

void PresetLibraryHandler::addPath(std::string_view path)
{
  QFile f{QString::fromUtf8(path.data(), path.length())};

  if (!f.open(QIODevice::ReadOnly))
    return;

  if (auto p = Process::Preset::fromJson(*processes, score::mapAsByteArray(f)))
  {
    presetLib->addPreset(std::move(*p));
  }
}

}

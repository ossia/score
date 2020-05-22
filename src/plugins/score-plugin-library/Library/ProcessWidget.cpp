#include "ProcessWidget.hpp"

#include <Library/ItemModelFilterLineEdit.hpp>
#include <Library/LibraryWidget.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/RecursiveFilterProxy.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MarginLess.hpp>

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
    lay->setMargin(6);
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
      if (auto f = score::GUIAppContext().interfaces<Process::ProcessFactoryList>().get(d->key))
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

ProcessWidget::ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_processModel{new ProcessesItemModel{ctx, this}}
    , m_presetModel{new PresetItemModel{ctx, this}}
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
    slay->addWidget(new ItemModelFilterLineEdit{*processFilterProxy, m_tv, this});
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

  auto infoWidg = new InfoWidget{this};
  infoWidg->setStatusTip(statusTip());
  slay->addWidget(infoWidg, 1);

  connect(&m_tv, &ProcessTreeView::selected, this, [=](const auto& pdata) {
    infoWidg->setData(pdata);
    if (pdata)
      presetFilterProxy->currentFilter = pdata->key;
    else
      presetFilterProxy->currentFilter = {};
    presetFilterProxy->invalidate();
  });

  m_lv.setMinimumHeight(100);
}

ProcessWidget::~ProcessWidget() { }

}

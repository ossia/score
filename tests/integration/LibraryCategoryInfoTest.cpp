// Choosing a folder in the process library describes no process, even when a
// process is registered under the nil uuid: the uuid an addon made from the
// avendish template keeps until its author replaces the placeholder.

#include <Process/ProcessList.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessTreeView.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QScrollArea>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
const QString placeholder_description
    = QStringLiteral("Apply an adjustable gain to multichannel audio.");

class PlaceholderUuidFactory final : public Process::ProcessModelFactory
{
public:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override { return {}; }
  QString prettyName() const noexcept override
  {
    return QStringLiteral("My Avendish Gain");
  }
  QString category() const noexcept override { return QStringLiteral("Audio/Utilities"); }
  Process::ProcessFlags flags() const noexcept override { return {}; }
  Process::Descriptor descriptor(QString) const noexcept override
  {
    Process::Descriptor d;
    d.prettyName = prettyName();
    d.description = placeholder_description;
    return d;
  }
  Process::ProcessModel* make(
      const TimeVal&, const QString&, const Id<Process::ProcessModel>&,
      const score::DocumentContext&, QObject*) override
  {
    return nullptr;
  }
  Process::ProcessModel*
  load(const VisitorVariant&, const score::DocumentContext&, QObject*) override
  {
    return nullptr;
  }
};

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
}

QStringList shownText(const QWidget& info)
{
  QStringList res;
  if(info.isHidden())
    return res;
  for(auto* l : info.findChildren<QLabel*>())
    if(!l->text().isEmpty())
      res.push_back(l->text());
  return res;
}

QModelIndex child(const QAbstractItemModel& m, const QModelIndex& parent, const QString& name)
{
  for(int i = 0; i < m.rowCount(parent); i++)
    if(auto idx = m.index(i, 0, parent); idx.data().toString() == name)
      return idx;
  return {};
}
}

TEST_CASE(
    "a folder of the process library shows no process's description",
    "[integration][library][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = const_cast<Process::ProcessFactoryList&>(
        ctx.interfaces<Process::ProcessFactoryList>());
    if(!procs.get({}))
      procs.insert(std::make_unique<PlaceholderUuidFactory>());
    REQUIRE(procs.get({}));

    auto panel = ctx.findPanel<Library::ProcessPanel>();
    REQUIRE(panel);
    auto& widget = panel->processWidget();
    auto* info = widget.findChild<QScrollArea*>(QStringLiteral("ProcessInfo"));
    REQUIRE(info);

    auto& tree = widget.processView();
    auto& m = *tree.model();
    const auto audio = child(m, {}, "Audio");
    REQUIRE(audio.isValid());
    const auto utilities = child(m, audio, "Utilities");
    REQUIRE(utilities.isValid());
    const auto gain = child(m, utilities, "Gain");
    REQUIRE(gain.isValid());

    // A process first, so that a stale description would remain to be seen
    tree.setCurrentIndex(gain);
    spin(20);
    const auto gainText = shownText(*info);
    CHECK(gainText.contains("Gain"));

    for(const auto& folder : {audio, utilities})
    {
      tree.setCurrentIndex(folder);
      spin(20);
      const auto text = shownText(*info);
      CHECK(text.isEmpty());
      CHECK(!text.contains(placeholder_description));
      CHECK(widget.presetModel().rowCount({}) == 0);
    }
  });
}

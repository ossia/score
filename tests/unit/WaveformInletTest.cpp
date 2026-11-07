#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Mouse.hpp>
#include <score_test/Events.hpp>
#include <score_test/Keyboard.hpp>
#include <score_test/Project.hpp>

#include <Dataflow/WaveformInlet.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/ProcessContext.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/document/DocumentInterface.hpp>
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QGraphicsView>
#include <QMenu>
#include <QImage>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>

TEST_CASE("Waveform ports reject invalid content without splitting their model and value", "[wavetable][port]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Dataflow::WaveformInlet inlet{Id<Process::Port>{91}, nullptr};
    inlet.setFrameSizeConstraint(32);
    inlet.setAmplitudeLevels(16);
    auto wave = Process::makeTriangleWavetable();
    inlet.setWaveform(wave);
    auto value = inlet.value();
    int changes{};
    QObject::connect(&inlet, &Process::ControlInlet::valueChanged, &inlet,
                     [&](const ossia::value&) { ++changes; });
    auto wrongSize = Process::makeTriangleWavetable(64);
    inlet.setValue(Process::wavetableToValue(wrongSize));
    auto bank = wave;
    bank.samples.insert(bank.samples.end(), wave.samples.begin(), wave.samples.end());
    inlet.setValue(Process::wavetableToValue(bank));
    inlet.setValue(std::vector<ossia::value>{});
    CHECK(inlet.wavetable() == wave);
    CHECK(inlet.value() == value);
    CHECK(changes == 0);
    wave.samples[3] = 1.f;
    inlet.setWaveform(wave);
    CHECK(inlet.wavetable() == wave);
    CHECK(changes == 1);
  });
}

TEST_CASE("Waveform and bank serialization retain data and editing constraints", "[wavetable][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto exercise = []<typename Port>() {
      Port inlet{Id<Process::Port>{91}, nullptr};
      inlet.setName(QStringLiteral("Authored wave"));
      inlet.setAmplitudeLevels(32);
      inlet.setFrameSizeConstraint(32);
      auto content = Process::makeTriangleWavetable();
      if constexpr(std::is_same_v<Port, Dataflow::WavetableInlet>)
      {
        const auto first = content.samples;
        for(float sample : first) content.samples.push_back(-sample);
      }
      inlet.setWaveform(content);
      inlet.setInit(inlet.value());
      State::AddressAccessor address;
      address.address.device = QStringLiteral("osc");
      address.address.path = QStringList{QStringLiteral("wave")};
      inlet.setAddress(address);
      REQUIRE(inlet.address() == address);
      const auto check = [&](const auto& copy) {
        CHECK(copy.wavetable() == content);
        CHECK(copy.value() == inlet.value());
        CHECK(copy.init() == inlet.init());
        CHECK(copy.name() == inlet.name());
        CHECK(copy.amplitudeLevels() == 32);
        CHECK(copy.frameSizeConstraint() == 32);
        CHECK(copy.address() == inlet.address());
      };
      const auto bytes = score::marshall<DataStream>(static_cast<Process::Inlet&>(inlet));
      DataStream::Deserializer binary{bytes};
      auto binaryCopy = Process::load_inlet(binary, nullptr);
      auto* binaryPort = dynamic_cast<Port*>(binaryCopy.get());
      REQUIRE(binaryPort);
      check(*binaryPort);
      const auto json = toValue(score::marshall<JSONObject>(static_cast<Process::Inlet&>(inlet)));
      JSONObject::Deserializer reader{json};
      auto jsonCopy = Process::load_inlet(reader, nullptr);
      auto* jsonPort = dynamic_cast<Port*>(jsonCopy.get());
      REQUIRE(jsonPort);
      check(*jsonPort);
    };
    exercise.template operator()<Dataflow::WaveformInlet>();
    exercise.template operator()<Dataflow::WavetableInlet>();
  });
}

TEST_CASE("Waveform ports take cables and addresses like other controls", "[wavetable][port]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto& interval = score::test::base_interval(*document);
    auto* waveform = new Dataflow::WaveformInlet{Id<Process::Port>{911}, &interval};
    auto* bank = new Dataflow::WavetableInlet{Id<Process::Port>{912}, &interval};
    auto* source = new Process::ControlOutlet{QStringLiteral("Value"), Id<Process::Port>{913}, &interval};
    CHECK(Dataflow::getPortsForConnection(*source, *waveform).second == waveform);
    CHECK(Dataflow::getPortsForConnection(*source, *bank).second == bank);

    auto& scenario = score::IDocument::get<Scenario::ScenarioDocumentModel>(*document);
    CommandDispatcher<> dispatcher{document->context().commandStack};
    dispatcher.submit<Dataflow::CreateCable>(
        scenario, Id<Process::Cable>{914}, Process::CableType::ImmediateGlutton, *source,
        *waveform);
    REQUIRE(scenario.cables.find(Id<Process::Cable>{914}) != scenario.cables.end());
    REQUIRE(waveform->cables().size() == 1);
    const auto cable = waveform->cables().front();

    State::AddressAccessor address;
    address.address.device = QStringLiteral("osc");
    address.address.path = QStringList{QStringLiteral("wave")};
    waveform->setAddress(address);
    CHECK(waveform->address() == address);

    // The address is saved with the port; its cables travel with the port's
    // data when a process is replaced or reloaded.
    const auto json = toValue(score::marshall<JSONObject>(static_cast<Process::Inlet&>(*waveform)));
    JSONObject::Deserializer reader{json};
    auto copy = Process::load_inlet(reader, nullptr);
    REQUIRE(dynamic_cast<Dataflow::WaveformInlet*>(copy.get()));
    CHECK(copy->address() == address);
    const auto bytes = score::marshall<DataStream>(static_cast<Process::Inlet&>(*waveform));
    DataStream::Deserializer binary{bytes};
    auto binaryCopy = Process::load_inlet(binary, nullptr);
    REQUIRE(binaryCopy);
    CHECK(binaryCopy->address() == address);
    Dataflow::WaveformInlet reloaded{Id<Process::Port>{915}, nullptr};
    reloaded.loadData(waveform->saveData());
    REQUIRE(reloaded.cables().size() == 1);
    CHECK(reloaded.cables().front() == cable);
    CHECK(reloaded.address() == address);
    CHECK(reloaded.wavetable() == waveform->wavetable());

    document->commandStack().undo();
    CHECK(waveform->cables().empty());
    CHECK(scenario.cables.find(Id<Process::Cable>{914}) == scenario.cables.end());
  });
}

TEST_CASE("Waveform ports show their port item when cables are shown", "[wavetable][port][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* waveform = new Dataflow::WaveformInlet{
        Id<Process::Port>{921}, &score::test::base_interval(*document)};
    auto* factory = ctx.interfaces<Process::PortFactoryList>().get(waveform->concreteKey());
    REQUIRE(factory);
    Process::DataflowManager dataflow;
    FocusDispatcher focus;
    Process::Context context{document->context(), dataflow, focus};
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 400., 200.}};
    scene.addItem(root);
    const bool cablesShown = Dataflow::CableItem::g_cables_enabled;
    Dataflow::CableItem::g_cables_enabled = true;
    auto layout = factory->makeFullItem(*waveform, context, root, waveform);
    REQUIRE(layout.port_item);
    REQUIRE(layout.control);
    CHECK(layout.port_item->isVisible());
    CHECK(layout.port_item->isEnabled());
    CHECK(layout.port_item->acceptDrops());
    layout.port_item->setPortVisible(false);
    CHECK_FALSE(layout.port_item->isVisible());
    layout.port_item->setPortVisible(true);
    CHECK(layout.port_item->isVisible());
    Dataflow::CableItem::g_cables_enabled = cablesShown;
  });
}

TEST_CASE("Execution values are read in place: a flat list is one frame, nested lists a bank", "[wavetable][execution]")
{
  // What a multislider sends: one frame of numbers in its own range.
  const std::vector<ossia::value> slider{0.f, 0.5f, 1.f, 0.25f};
  std::array<float, 8> prepared{};
  REQUIRE(Process::prepareWaveform(ossia::value{slider}, prepared, -1., 1.));
  const Process::WavetableData expected{4, {0.f, 0.5f, 1.f, 0.25f}};
  std::array<float, 8> reference{};
  REQUIRE(Process::prepareWaveform(expected, reference, -1., 1.));
  CHECK(prepared == reference);
  CHECK(prepared[1] == .25f);
  CHECK(prepared[2] == .5f);
  CHECK(prepared[4] == 1.f);

  // The canonical value, read in place, prepares exactly as its table does.
  auto bank = Process::makeTriangleWavetable(16);
  for(std::size_t i = 0; i < 16; ++i) bank.samples.push_back(i % 2 ? .5f : -.5f);
  const auto bankValue = Process::wavetableToValue(bank);
  for(std::size_t frame = 0; frame < 2; ++frame)
  {
    std::array<uint8_t, 32> fromValue{}, fromTable{};
    REQUIRE(Process::prepareWaveform(bankValue, fromValue, 0., 15., frame));
    REQUIRE(Process::prepareWaveform(bank, fromTable, 0., 15., frame));
    CHECK(fromValue == fromTable);
  }
  std::array<float, 8> untouched{};
  untouched.fill(7.f);
  CHECK_FALSE(Process::prepareWaveform(bankValue, untouched, -1., 1., 2));
  CHECK_FALSE(Process::prepareWaveform(ossia::value{slider}, untouched, -1., 1., 1));

  // Integers and out-of-range samples are clamped, not rejected.
  std::array<double, 4> clamped{};
  REQUIRE(Process::prepareWaveform(
      ossia::value{std::vector<ossia::value>{-3, 2.f, 0, 0.f}}, clamped, 0., 1.));
  CHECK(clamped == std::array<double, 4>{0., 1., .5, .5});

  // Shapes that are not frames leave the destination as it was.
  for(const ossia::value& invalid :
      {ossia::value{}, ossia::value{1.f}, ossia::value{std::vector<ossia::value>{}},
       ossia::value{std::vector<ossia::value>{0.5f}},
       ossia::value{std::vector<ossia::value>{0.f, std::string{"x"}}},
       ossia::value{std::vector<ossia::value>{0.f, std::numeric_limits<float>::quiet_NaN()}},
       ossia::value{std::vector<ossia::value>{std::vector<ossia::value>{0.f}}}})
    CHECK_FALSE(Process::prepareWaveform(invalid, untouched, -1., 1.));
  CHECK(std::all_of(untouched.begin(), untouched.end(), [](float v) { return v == 7.f; }));

  // The UI-side reader takes the same shapes.
  const auto display = Process::waveformFromValue(ossia::value{slider});
  REQUIRE(display);
  CHECK(display->frameCount() == 1);
  CHECK(display->frameSize == 4);
  const auto displayedBank = Process::waveformFromValue(bankValue);
  REQUIRE(displayedBank);
  CHECK(*displayedBank == bank);
}

TEST_CASE("The waveform editor draws what execution receives without editing the port", "[wavetable][gui][execution]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{931}, &score::test::base_interval(*document)};
    inlet->setFrameSizeConstraint(32);
    const auto value = inlet->value();
    std::unique_ptr<QGraphicsItem> editor{WidgetFactory::WaveformInletItems::make_item(
        *inlet, *inlet, document->context(), nullptr, inlet)};
    Dataflow::setWaveformItemRect(*editor, {0., 0., 240., 120.});
    const auto render = [&] {
      QImage image{240, 120, QImage::Format_ARGB32_Premultiplied};
      image.fill(Qt::black);
      QPainter painter{&image};
      editor->paint(&painter, nullptr, nullptr);
      return image;
    };
    const auto authored = render();
    const auto commands = document->commandStack().currentIndex();
    inlet->setExecutionValue(std::vector<ossia::value>{1.f, 1.f, -1.f, -1.f});
    CHECK(render() != authored);
    CHECK(inlet->value() == value);
    CHECK(document->commandStack().currentIndex() == commands);
    inlet->executionReset();
    CHECK(render() == authored);
  });
}

TEST_CASE("Sample edits undo against the document port after editor rebinding", "[wavetable][undo]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto& interval = score::test::base_interval(*document);
    auto* first = new Dataflow::WaveformInlet{Id<Process::Port>{901}, &interval};
    auto* second = new Dataflow::WaveformInlet{Id<Process::Port>{902}, &interval};
    auto original = Process::makeTriangleWavetable();
    first->setWaveform(original);
    second->setWaveform(original);
    auto changed = original;
    changed.samples[8] = -.75f;
    CommandDispatcher<> dispatcher{document->context().commandStack};
    dispatcher.submit<Process::SetControlValue>(*first, Process::wavetableToValue(changed));
    CHECK(first->wavetable() == changed);
    QGraphicsScene scene;
    auto* editor = WidgetFactory::WaveformInletItems::make_item(
        *first, *first, document->context(), nullptr, first);
    REQUIRE(editor);
    scene.addItem(editor);
    Dataflow::setWaveformItemModel(*editor, *second);
    document->commandStack().undo();
    CHECK(first->wavetable() == original);
    CHECK(second->wavetable() == original);
    document->commandStack().redo();
    CHECK(first->wavetable() == changed);
    CHECK(second->wavetable() == original);
  });
}

namespace
{
QAction* menuAction(QMenu& menu, const QString& path)
{
  const auto parts = path.split(QLatin1Char('/'));
  QMenu* current = &menu;
  for(int i = 0; i < parts.size(); ++i)
  {
    QAction* found{};
    for(auto* action : current->actions())
      if(action->text() == parts[i])
        found = action;
    if(!found || i + 1 == parts.size())
      return found;
    current = found->menu();
    if(!current)
      return nullptr;
  }
  return nullptr;
}

//! Triggers an enabled action of the editor's context menu.
bool triggerMenu(QGraphicsItem& editor, const QString& path)
{
  QMenu menu;
  Dataflow::fillWaveformItemMenu(editor, menu);
  auto* action = menuAction(menu, path);
  if(!action || !action->isEnabled())
    return false;
  action->trigger();
  return true;
}

QGraphicsItem* waveformEditor(QGraphicsScene& scene)
{
  for(auto* item : scene.items())
    if(item->data(0).toString() == QStringLiteral("WaveformEditor"))
      return item;
  return nullptr;
}

bool onLevels(std::span<const float> frame, int levels)
{
  return std::all_of(frame.begin(), frame.end(), [levels](float sample) {
    const double step = (sample + 1.) * .5 * (levels - 1);
    return std::abs(step - std::round(step)) < 1e-4;
  });
}

void click(QGraphicsView& view, QGraphicsItem& editor, QPointF point)
{
  const auto position = view.mapFromScene(editor.mapToScene(point));
  score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonPress, position,
                         Qt::LeftButton, Qt::LeftButton, {});
  score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonRelease, position,
                         Qt::LeftButton, Qt::NoButton, {});
}
}

TEST_CASE("Bank frames are chosen in the strip and edited from the context menu", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WavetableInlet{
        Id<Process::Port>{903}, &score::test::base_interval(*document)};
    auto first = Process::makeTriangleWavetable(8);
    auto original = first;
    for(float sample : first.samples) original.samples.push_back(-sample);
    inlet->setWaveform(original);
    std::unique_ptr<QWidget> widget{WidgetFactory::WaveformInletItems::make_widget(
        *inlet, document->context(), nullptr, inlet)};
    auto* view = qobject_cast<QGraphicsView*>(widget.get());
    REQUIRE(view);
    view->resize(560, 260);
    REQUIRE(score::test::showAndActivate(*view));
    auto* editor = waveformEditor(*view->scene());
    REQUIRE(editor);
    for(auto* item : view->scene()->items())
      CHECK_FALSE(dynamic_cast<QGraphicsProxyWidget*>(item));

    auto state = Dataflow::waveformItemState(*editor);
    REQUIRE_FALSE(state.frames.isNull());
    CHECK(state.frames.bottom() <= state.plot.top());
    CHECK(state.frame == 0);
    const auto commands = document->commandStack().currentIndex();
    // Two frames fit as thumbnails, side by side from the left.
    const auto thumbnail = [&](int frame) {
      return QPointF{state.frames.left() + 24. + 48. * frame, state.frames.center().y()};
    };
    click(*view, *editor, thumbnail(1));
    CHECK(Dataflow::waveformItemState(*editor).frame == 1);
    click(*view, *editor, thumbnail(0));
    CHECK(Dataflow::waveformItemState(*editor).frame == 0);
    CHECK(document->commandStack().currentIndex() == commands);

    REQUIRE(triggerMenu(*editor, QStringLiteral("Duplicate frame")));
    CHECK(document->commandStack().currentIndex() == commands + 1);
    CHECK(Dataflow::waveformItemState(*editor).frame == 1);
    auto expected = first;
    expected.samples.insert(expected.samples.end(), original.samples.begin(), original.samples.end());
    CHECK(inlet->wavetable() == expected);
    REQUIRE(triggerMenu(*editor, QStringLiteral("Move frame right")));
    expected = original;
    expected.samples.insert(expected.samples.end(), first.samples.begin(), first.samples.end());
    CHECK(inlet->wavetable() == expected);
    CHECK(Dataflow::waveformItemState(*editor).frame == 2);
    CHECK_FALSE(triggerMenu(*editor, QStringLiteral("Move frame right")));
    REQUIRE(triggerMenu(*editor, QStringLiteral("Remove frame")));
    CHECK(inlet->wavetable() == original);
    CHECK(document->commandStack().currentIndex() == commands + 3);
    document->commandStack().undo();
    CHECK(inlet->wavetable() == expected);
    document->commandStack().redo();
    CHECK(inlet->wavetable() == original);

    // Transforms apply to the chosen frame only.
    click(*view, *editor, thumbnail(1));
    REQUIRE(triggerMenu(*editor, QStringLiteral("Invert")));
    auto inverted = first;
    inverted.samples.insert(inverted.samples.end(), first.samples.begin(), first.samples.end());
    CHECK(inlet->wavetable() == inverted);
    document->commandStack().undo();
    CHECK(inlet->wavetable() == original);

    {
      QMenu menu;
      Dataflow::fillWaveformItemMenu(*editor, menu);
      auto* sixteen = menuAction(menu, QStringLiteral("Resample/16 samples"));
      REQUIRE(sixteen);
      CHECK_FALSE(sixteen->isChecked());
      sixteen->trigger();
    }
    const auto& resized = inlet->wavetable();
    REQUIRE(resized.frameSize == 16);
    REQUIRE(resized.frameCount() == 2);
    for(std::size_t frame = 0; frame < 2; ++frame)
      for(std::size_t sample = 0; sample < 16; ++sample)
        CHECK(resized.frame(frame)[sample]
              == Process::sampleWavetable(original, frame, double(sample) / 16));
    CHECK(Dataflow::waveformItemState(*editor).label == QStringLiteral("16 smp"));
    document->commandStack().undo();
    CHECK(inlet->wavetable() == original);
    REQUIRE(triggerMenu(*editor, QStringLiteral("Remove frame")));
    CHECK(inlet->wavetable() == first);
    CHECK(Dataflow::waveformItemState(*editor).frame == 0);
    CHECK_FALSE(triggerMenu(*editor, QStringLiteral("Remove frame")));
    CHECK(inlet->wavetable() == first);

    // Too many frames for thumbnails: arrows either side of the frame number.
    auto many = first;
    for(int i = 0; i < 11; ++i) many.samples.insert(many.samples.end(), first.samples.begin(), first.samples.end());
    inlet->setWaveform(many);
    view->resize(220, 140);
    QCoreApplication::processEvents();
    state = Dataflow::waveformItemState(*editor);
    REQUIRE(state.frames.width() < 12 * 24.);
    const QPointF next{state.frames.right() - 4., state.frames.center().y()};
    const QPointF previous{state.frames.left() + 4., state.frames.center().y()};
    click(*view, *editor, next);
    click(*view, *editor, next);
    CHECK(Dataflow::waveformItemState(*editor).frame == 2);
    click(*view, *editor, previous);
    CHECK(Dataflow::waveformItemState(*editor).frame == 1);
    click(*view, *editor, previous);
    click(*view, *editor, previous);
    CHECK(Dataflow::waveformItemState(*editor).frame == 0);
  });
}

TEST_CASE("Waveform columns are drawn where they are edited", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{907}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    Process::WavetableData flat{32, std::vector<float>(32, -1.f)};
    flat.samples[5] = 1.f;
    inlet->setWaveform(flat);
    QGraphicsScene scene;
    auto* editor = WidgetFactory::WaveformInletItems::make_item(
        *inlet, *inlet, document->context(), nullptr, inlet);
    scene.addItem(editor);
    Dataflow::setWaveformItemRect(*editor, {0., 0., 400., 120.});
    const auto plot = Dataflow::waveformItemState(*editor).plot;
    REQUIRE(plot.width() > 380.);
    REQUIRE(plot.height() > 110.);
    const double column = plot.width() / 32.;

    QImage image{400, 120, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::black);
    {
      QPainter painter{&image};
      editor->paint(&painter, nullptr, nullptr);
    }
    const QColor bar = score::Skin::instance().Base4.color();
    const auto capAt = [&](double x, double y) {
      for(int row = int(y) - 1; row <= int(y) + 1; ++row)
        if(row >= 0 && row < image.height() && image.pixelColor(int(x), row) == bar)
          return true;
      return false;
    };
    // Sample 5 is raised over its whole column, its neighbours are not.
    for(double offset : {.2, .5, .8})
    {
      CHECK(capAt(plot.left() + column * (5. + offset), plot.top()));
      CHECK_FALSE(capAt(plot.left() + column * (4. + offset), plot.top()));
      CHECK_FALSE(capAt(plot.left() + column * (6. + offset), plot.top()));
      CHECK(capAt(plot.left() + column * (4. + offset), plot.bottom() - 1.));
    }

    QGraphicsView view{&scene};
    view.setSceneRect(0., 0., 400., 120.);
    view.resize(440, 160);
    REQUIRE(score::test::showAndActivate(view));
    const auto commands = document->commandStack().currentIndex();
    // Either side of a column centre edits that column, as drawn.
    for(double offset : {.1, .9})
    {
      click(view, *editor, {plot.left() + column * (9. + offset), plot.top() + 1.});
      auto expected = flat;
      expected.samples[9] = 1.f;
      CHECK(inlet->wavetable() == expected);
      CHECK(document->commandStack().currentIndex() == commands + 1);
      document->commandStack().undo();
      CHECK(inlet->wavetable() == flat);
    }
  });
}

TEST_CASE("Hovering a sample reads out its index and level", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto& interval = score::test::base_interval(*document);
    auto* chip = new Dataflow::WaveformInlet{Id<Process::Port>{908}, &interval};
    chip->setAmplitudeLevels(32);
    chip->setFrameSizeConstraint(32);
    auto wave = chip->wavetable();
    wave.samples[12] = Process::quantizeSample(2. * 23 / 31 - 1., 32);
    chip->setWaveform(wave);
    auto* continuous = new Dataflow::WaveformInlet{Id<Process::Port>{909}, &interval};
    auto smooth = Process::makeTriangleWavetable(64);
    smooth.samples[3] = .5f;
    continuous->setWaveform(smooth);

    QGraphicsScene scene;
    auto* editor = WidgetFactory::WaveformInletItems::make_item(
        *chip, *chip, document->context(), nullptr, chip);
    scene.addItem(editor);
    Dataflow::setWaveformItemRect(*editor, {0., 0., 320., 120.});
    QGraphicsView view{&scene};
    view.setSceneRect(-40., -40., 400., 200.);
    view.resize(420, 220);
    REQUIRE(score::test::showAndActivate(view));
    const auto hover = [&](QPointF point) {
      score::test::mouseEvent(*view.viewport(), QEvent::MouseMove,
                             view.mapFromScene(editor->mapToScene(point)), Qt::NoButton,
                             Qt::NoButton, {});
      QCoreApplication::processEvents();
      return Dataflow::waveformItemState(*editor);
    };
    const auto commands = document->commandStack().currentIndex();
    auto state = Dataflow::waveformItemState(*editor);
    CHECK(state.label == QStringLiteral("32 smp, 5 bit"));
    CHECK(state.readout.isEmpty());
    const auto column = [&](std::size_t sample, std::size_t size) {
      return QPointF{state.plot.left() + state.plot.width() * (sample + .5) / size,
                     state.plot.center().y()};
    };
    CHECK(hover(column(12, 32)).readout == QStringLiteral("#12 23/31"));
    CHECK(hover(column(0, 32)).readout == QStringLiteral("#0 0/31"));
    CHECK(hover({-20., -20.}).readout.isEmpty());

    Dataflow::setWaveformItemModel(*editor, *continuous);
    state = Dataflow::waveformItemState(*editor);
    CHECK(state.label == QStringLiteral("64 smp"));
    CHECK(hover(column(3, 64)).readout == QStringLiteral("#3 0.50"));
    CHECK(document->commandStack().currentIndex() == commands);

    chip->setAmplitudeLevels(24);
    Dataflow::setWaveformItemModel(*editor, *chip);
    CHECK(Dataflow::waveformItemState(*editor).label == QStringLiteral("32 smp, 24 levels"));
  });
}

TEST_CASE("Each waveform menu action is one undoable command on the port's levels", "[wavetable][gui][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{910}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    QGraphicsScene scene;
    auto* editor = WidgetFactory::WaveformInletItems::make_item(
        *inlet, *inlet, document->context(), nullptr, inlet);
    scene.addItem(editor);
    auto& stack = document->commandStack();

    {
      QMenu menu;
      Dataflow::fillWaveformItemMenu(*editor, menu);
      CHECK_FALSE(menuAction(menu, QStringLiteral("Resample")));
      CHECK_FALSE(menuAction(menu, QStringLiteral("Add frame")));
    }

    using Shape = Process::WaveformShape;
    for(auto [name, shape] : std::initializer_list<std::pair<const char*, Shape>>{
            {"Sine", Shape::Sine}, {"Triangle", Shape::Triangle}, {"Saw up", Shape::SawUp},
            {"Saw down", Shape::SawDown}, {"Square", Shape::Square},
            {"Pulse 25%", Shape::Pulse25}, {"Pulse 12.5%", Shape::Pulse12},
            {"Noise", Shape::Noise}})
    {
      INFO(name);
      const auto before = inlet->wavetable();
      const auto index = stack.currentIndex();
      REQUIRE(triggerMenu(*editor, QStringLiteral("Shape/") + QString::fromLatin1(name)));
      CHECK(stack.currentIndex() == index + 1);
      const auto after = inlet->wavetable();
      CHECK(after.frameSize == 32);
      CHECK(onLevels(after.samples, 16));
      if(shape != Shape::Noise)
      {
        std::vector<float> expected(32);
        Process::generateWaveform(expected, shape, 16);
        CHECK(after.samples == expected);
      }
      stack.undo();
      CHECK(inlet->wavetable() == before);
      stack.redo();
      CHECK(inlet->wavetable() == after);
    }

    // Levels 3 to 10 of 16: neither centred nor at full scale.
    Process::WavetableData start{32, std::vector<float>(32)};
    for(std::size_t i = 0; i < 32; ++i)
      start.samples[i] = Process::quantizeSample(2. * (3 + (i * 5) % 8) / 15. - 1., 16);
    CommandDispatcher<> dispatcher{document->context().commandStack};
    using Transform = std::function<void(std::span<float>)>;
    for(auto [name, transform] : std::initializer_list<std::pair<const char*, Transform>>{
            {"Normalize", [](std::span<float> f) { Process::normalizeWaveform(f, 16); }},
            {"Invert", [](std::span<float> f) { Process::invertWaveform(f, 16); }},
            {"Reverse", [](std::span<float> f) { Process::reverseWaveform(f); }},
            {"Rotate left", [](std::span<float> f) { Process::rotateWaveform(f, true); }},
            {"Rotate right", [](std::span<float> f) { Process::rotateWaveform(f, false); }},
            {"Smooth", [](std::span<float> f) { Process::smoothWaveform(f, 16); }},
            {"Clear", [](std::span<float> f) { Process::clearWaveform(f, 16); }}})
    {
      INFO(name);
      dispatcher.submit<Process::SetControlValue>(*inlet, Process::wavetableToValue(start));
      REQUIRE(inlet->wavetable() == start);
      auto expected = start;
      transform(expected.samples);
      for(float& sample : expected.samples)
        sample = Process::quantizeSample(sample, 16);
      REQUIRE(expected != start);
      const auto index = stack.currentIndex();
      REQUIRE(triggerMenu(*editor, QString::fromLatin1(name)));
      CHECK(stack.currentIndex() == index + 1);
      CHECK(inlet->wavetable() == expected);
      CHECK(onLevels(inlet->wavetable().samples, 16));
      stack.undo();
      CHECK(inlet->wavetable() == start);
      stack.redo();
      CHECK(inlet->wavetable() == expected);
    }
    // An action that changes nothing adds no command.
    const auto index = stack.currentIndex();
    REQUIRE(triggerMenu(*editor, QStringLiteral("Clear")));
    CHECK(stack.currentIndex() == index);

    auto* unconstrained = new Dataflow::WaveformInlet{
        Id<Process::Port>{916}, &score::test::base_interval(*document)};
    Dataflow::setWaveformItemModel(*editor, *unconstrained);
    QMenu menu;
    Dataflow::fillWaveformItemMenu(*editor, menu);
    auto* current = menuAction(menu, QStringLiteral("Resample/32 samples"));
    REQUIRE(current);
    CHECK(current->isChecked());
    auto* larger = menuAction(menu, QStringLiteral("Resample/64 samples"));
    REQUIRE(larger);
    larger->trigger();
    CHECK(unconstrained->wavetable().frameSize == 64);
    CHECK(stack.currentIndex() == index + 1);
  });
}

TEST_CASE("A waveform pencil drag is one quantized undoable sample edit", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{904}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    const auto original = Process::makeTriangleWavetable();
    inlet->setWaveform(original);
    QGraphicsScene scene;
    auto* editor = WidgetFactory::WaveformInletItems::make_item(
        *inlet, *inlet, document->context(), nullptr, inlet);
    scene.addItem(editor);
    Dataflow::setWaveformItemRect(*editor, {0., 0., 400., 180.});
    QGraphicsView view{&scene};
    view.setSceneRect(0., 0., 400., 180.);
    view.resize(440, 220);
    REQUIRE(score::test::showAndActivate(view));
    const auto point = [&](double x, double y) {
      return view.mapFromScene(editor->mapToScene({x, y}));
    };
    const auto commandIndex = document->commandStack().currentIndex();
    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonPress,
                           point(22., 96.), Qt::LeftButton, Qt::LeftButton, {});
    score::test::mouseEvent(*view.viewport(), QEvent::MouseMove,
                           point(250., 42.), Qt::NoButton, Qt::LeftButton, {});
    CHECK(inlet->wavetable() == original);
    CHECK(document->commandStack().currentIndex() == commandIndex);
    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonRelease,
                           point(250., 42.), Qt::LeftButton, Qt::NoButton, {});
    const auto edited = inlet->wavetable();
    CHECK(document->commandStack().currentIndex() == commandIndex + 1);
    REQUIRE(edited != original);
    for(std::size_t sample = 2; sample < 20; ++sample)
    {
      const double native = (edited.samples[sample] + 1.) * 7.5;
      CHECK(std::abs(native - std::round(native)) < 1e-5);
    }
    CHECK(edited.samples[31] == original.samples[31]);
    document->commandStack().undo();
    CHECK(inlet->wavetable() == original);
    document->commandStack().redo();
    CHECK(inlet->wavetable() == edited);

    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonPress,
                           point(100., 145.), Qt::LeftButton, Qt::LeftButton, {});
    score::test::keyEvent(*view.viewport(), QEvent::KeyPress, Qt::Key_Escape, {}, {});
    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonRelease,
                           point(100., 145.), Qt::LeftButton, Qt::NoButton, {});
    CHECK(inlet->wavetable() == edited);
    CHECK(document->commandStack().currentIndex() == commandIndex + 1);

    auto* replacement = new Dataflow::WaveformInlet{
        Id<Process::Port>{905}, &score::test::base_interval(*document)};
    replacement->setWaveform(original);
    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonPress,
                           point(100., 145.), Qt::LeftButton, Qt::LeftButton, {});
    Dataflow::setWaveformItemModel(*editor, *replacement);
    score::test::mouseEvent(*view.viewport(), QEvent::MouseButtonRelease,
                           point(100., 145.), Qt::LeftButton, Qt::NoButton, {});
    CHECK(inlet->wavetable() == edited);
    CHECK(replacement->wavetable() == original);
    CHECK(document->commandStack().currentIndex() == commandIndex + 1);
  });
}

TEST_CASE("Waveform painting respects parent clipping and leaves later items visible", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    Dataflow::WaveformInlet inlet{Id<Process::Port>{906}, nullptr};
    std::unique_ptr<QGraphicsItem> editor{WidgetFactory::WaveformInletItems::make_item(
        inlet, inlet, document->context(), nullptr, &inlet)};
    Dataflow::setWaveformItemRect(*editor, {0., 0., 240., 120.});
    QImage image{300, 200, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::black);
    {
      QPainter painter{&image};
      painter.setClipRect(0, 0, 120, 180);
      // Share the painter without an intervening save/restore, as the optimized
      // main scene does when drawing the waveform followed by its I/O ports.
      editor->paint(&painter, nullptr, nullptr);
      painter.fillRect(QRect{10, 140, 20, 20}, Qt::red);
    }
    CHECK(image.pixelColor(15, 145) == QColor{Qt::red});
    bool parentClipRespected = true;
    for(int y = 0; y < image.height(); ++y)
      for(int x = 120; x < image.width(); ++x)
        parentClipRespected &= image.pixelColor(x, y) == QColor{Qt::black};
    CHECK(parentClipRespected);
  });
}

TEST_CASE("Waveform content a port cannot hold is replaced on load, not fatal", "[wavetable][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // A two-frame bank, saved, then read back as a single-frame port with
    // out-of-range editing constraints.
    Dataflow::WavetableInlet bank{Id<Process::Port>{91}, nullptr};
    auto content = Process::makeTriangleWavetable(16);
    const auto first = content.samples;
    for(float sample : first)
      content.samples.push_back(-sample);
    bank.setWaveform(content);
    bank.setInit(bank.value());
    bank.setAmplitudeLevels(8);

    QByteArray json
        = score::marshall<JSONObject>(static_cast<Process::Inlet&>(bank)).toByteArray();
    REQUIRE(json.contains("c2ee4fdc-9a49-4818-965c-cf5bf98cfa49"));
    json.replace(
        "c2ee4fdc-9a49-4818-965c-cf5bf98cfa49", "be15516a-a8be-490c-b3a8-82b25f985096");
    REQUIRE(json.contains("\"AmplitudeLevels\":8"));
    REQUIRE(json.contains("\"FrameSizeConstraint\":0"));
    json.replace("\"AmplitudeLevels\":8", "\"AmplitudeLevels\":1");
    json.replace("\"FrameSizeConstraint\":0", "\"FrameSizeConstraint\":-4");

    const auto doc = readJson(json);
    JSONObject::Deserializer reader{doc};
    std::unique_ptr<Process::Inlet> loaded;
    REQUIRE_NOTHROW(loaded = Process::load_inlet(reader, nullptr));
    auto* port = dynamic_cast<Dataflow::WaveformInlet*>(loaded.get());
    REQUIRE(port);
    REQUIRE_FALSE(dynamic_cast<Dataflow::WavetableInlet*>(loaded.get()));
    CHECK(port->wavetable().valid());
    CHECK(port->wavetable().frameCount() == 1);
    CHECK(Process::wavetableFromValue(port->value()) == port->wavetable());
    CHECK(Process::wavetableFromValue(port->init()) == port->wavetable());
    CHECK(port->amplitudeLevels() == 0);
    CHECK(port->frameSizeConstraint() == 0);
  });
}

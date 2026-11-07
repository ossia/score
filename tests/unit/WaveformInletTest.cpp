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
#include <Process/Dataflow/WidgetInlets.hpp>
#include <score/graphics/TextItem.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/Skin.hpp>

#include <QClipboard>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QLineEdit>
#include <QToolButton>
#include <QWheelEvent>
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
    using Values = std::vector<ossia::value>;
    for(const auto& invalid : std::vector<ossia::value>{
            Values{}, ossia::value{.5f}, ossia::value{std::string{"sine"}},
            Values{0.f}, Values{0.f, std::string{"a"}},
            Values{0.f, std::numeric_limits<float>::quiet_NaN()},
            Values{Values{0.f, 1.f}, Values{0.f, 1.f, 0.f}}})
      inlet.setValue(invalid);
    CHECK(inlet.wavetable() == wave);
    CHECK(inlet.value() == value);
    CHECK(changes == 0);
    wave.samples[3] = 1.f;
    inlet.setWaveform(wave);
    CHECK(inlet.wavetable() == wave);
    CHECK(changes == 1);
  });
}

TEST_CASE("Waveform ports take a value as execution reads it", "[wavetable][port]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    using Values = std::vector<ossia::value>;
    const auto onLevels = [](const Process::WavetableData& table, int levels) {
      return std::all_of(table.samples.begin(), table.samples.end(), [=](float s) {
        return Process::quantizeSample(s, levels) == s;
      });
    };
    Dataflow::WaveformInlet inlet{Id<Process::Port>{92}, nullptr};
    inlet.setFrameSizeConstraint(32);
    inlet.setAmplitudeLevels(16);

    // A flat list of the constraint's size, as setValue([...]) in a script.
    Values flat;
    for(int i = 0; i < 32; ++i)
      flat.push_back(float(i % 8) / 7.f);
    inlet.setValue(flat);
    CHECK(inlet.wavetable().frameSize == 32);
    CHECK(inlet.wavetable().frameCount() == 1);
    CHECK(inlet.wavetable().samples[7] == 1.f);
    CHECK(inlet.wavetable().samples[0] == Process::quantizeSample(0., 16));
    CHECK(onLevels(inlet.wavetable(), 16));
    CHECK(Process::wavetableFromValue(inlet.value()) == inlet.wavetable());

    // Another size is resampled to the constraint.
    inlet.setValue(Values{-1.f, 1.f, -1.f, 1.f});
    CHECK(inlet.wavetable().frameSize == 32);
    CHECK(inlet.wavetable().samples[0] == -1.f);
    CHECK(inlet.wavetable().samples[8] == 1.f);
    CHECK(onLevels(inlet.wavetable(), 16));

    // Bools, ints and samples a hair out of range are read and clamped.
    Values mixed(32, ossia::value{0.f});
    mixed[0] = true;
    mixed[1] = -1;
    mixed[2] = 1.0001f;
    mixed[3] = -1.5f;
    inlet.setValue(mixed);
    CHECK(inlet.wavetable().samples[0] == 1.f);
    CHECK(inlet.wavetable().samples[1] == -1.f);
    CHECK(inlet.wavetable().samples[2] == 1.f);
    CHECK(inlet.wavetable().samples[3] == -1.f);

    // A single frame port keeps the first frame of a bank.
    auto bank = Process::makeTriangleWavetable(32);
    for(int i = 0; i < 32; ++i)
      bank.samples.push_back(-1.f);
    inlet.setValue(Process::wavetableToValue(bank));
    CHECK(inlet.wavetable().frameCount() == 1);
    CHECK(inlet.wavetable().samples[16] == Process::quantizeSample(1., 16));

    // The canonical value of a port reads back unchanged.
    Dataflow::WaveformInlet free{Id<Process::Port>{93}, nullptr};
    auto canonical = Process::makeTriangleWavetable(64);
    canonical.samples[5] = .123f;
    free.setValue(Process::wavetableToValue(canonical));
    CHECK(free.wavetable() == canonical);
    CHECK(free.value() == Process::wavetableToValue(canonical));

    // A bank port takes every frame.
    Dataflow::WavetableInlet banks{Id<Process::Port>{94}, nullptr};
    banks.setValue(Process::wavetableToValue(bank));
    CHECK(banks.wavetable() == bank);
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

TEST_CASE("Laying a control item out again follows the presentation it is given", "[port][layout][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* slider = new Process::FloatSlider{
        0.f, 1.f, .5f, QStringLiteral("Gain"), Id<Process::Port>{922},
        &score::test::base_interval(*document)};
    auto* factory = ctx.interfaces<Process::PortFactoryList>().get(slider->concreteKey());
    REQUIRE(factory);
    Process::DataflowManager dataflow;
    FocusDispatcher focus;
    Process::Context context{document->context(), dataflow, focus};
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 400., 200.}};
    scene.addItem(root);

    // A custom label goes back to the port's name with a default presentation.
    Process::ControlPresentation custom;
    custom.label = QStringLiteral("Custom");
    auto item = factory->makeFullItem(*slider, context, root, slider, custom);
    REQUIRE(item.label);
    CHECK(item.label->text() == QStringLiteral("Custom"));
    factory->layoutFullItem(item, *slider, {});
    CHECK(item.label->text() == Process::displayName(*slider));
    factory->layoutFullItem(item, *slider, custom);
    CHECK(item.label->text() == QStringLiteral("Custom"));

    // A knob is laid out again where it was made.
    Process::ControlPresentation knob;
    knob.widget = Process::ControlPresentation::Widget::Knob;
    auto made = factory->makeFullItem(*slider, context, root, slider, knob);
    REQUIRE(made.label);
    REQUIRE(made.control);
    const auto label = made.label->pos();
    const auto control = made.control->pos();
    const auto port = made.port_item->pos();
    factory->layoutFullItem(made, *slider, knob);
    CHECK(made.label->pos() == label);
    CHECK(made.control->pos() == control);
    CHECK(made.port_item->pos() == port);
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
    auto* view = widget->findChild<QGraphicsView*>();
    REQUIRE(view);
    widget->resize(560, 320);
    REQUIRE(score::test::showAndActivate(*widget));
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
    widget->resize(220, 200);
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

namespace
{
//! An editor alone in a shown view, the item filling `rect`.
struct SceneEditor
{
  QGraphicsScene scene;
  QGraphicsView view{&scene};
  QGraphicsItem* editor{};

  SceneEditor(Dataflow::WaveformInlet& inlet, const score::DocumentContext& ctx, QRectF rect)
  {
    editor = WidgetFactory::WaveformInletItems::make_item(inlet, inlet, ctx, nullptr, &inlet);
    scene.addItem(editor);
    Dataflow::setWaveformItemRect(*editor, rect);
    view.setSceneRect(rect);
    view.resize(int(rect.width()) + 40, int(rect.height()) + 40);
  }

  QPoint at(QPointF point) const { return view.mapFromScene(editor->mapToScene(point)); }

  //! The centre of a sample's column at an amplitude.
  QPointF sample(std::size_t index, std::size_t size, double amplitude) const
  {
    const auto plot = Dataflow::waveformItemState(*editor).plot;
    return {plot.left() + plot.width() * (index + .5) / size,
            plot.top() + plot.height() * (1. - amplitude) * .5};
  }

  void mouse(QEvent::Type type, QPointF point, Qt::KeyboardModifiers modifiers = {})
  {
    const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    const Qt::MouseButtons buttons
        = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons{Qt::LeftButton};
    score::test::mouseEvent(*view.viewport(), type, at(point), button, buttons, modifiers);
  }

  void hover(QPointF point)
  {
    score::test::mouseEvent(*view.viewport(), QEvent::MouseMove, at(point), Qt::NoButton,
                           Qt::NoButton, {});
    QCoreApplication::processEvents();
  }

  void key(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = {}, bool repeat = false)
  {
    QKeyEvent event{type, key, modifiers, QString{}, repeat};
    QCoreApplication::sendEvent(&view, &event);
  }

  void press(int key, Qt::KeyboardModifiers modifiers = {})
  {
    this->key(QEvent::KeyPress, key, modifiers);
    this->key(QEvent::KeyRelease, key, modifiers);
  }

  //! Whether the editor takes `key` over from the application's shortcuts.
  bool overrides(int key, Qt::KeyboardModifiers modifiers = {})
  {
    QKeyEvent event{QEvent::ShortcutOverride, key, modifiers};
    event.ignore();
    QCoreApplication::sendEvent(&view, &event);
    return event.isAccepted();
  }
};

float level(int index, int levels)
{
  return Process::quantizeSample(2. * index / (levels - 1) - 1., levels);
}
}

TEST_CASE("Frames read and write as lists of numbers", "[wavetable]")
{
  const std::array<float, 4> frame{.5f, -.25f, 1.f, 0.f};
  CHECK(Dataflow::waveformFrameText(frame) == QStringLiteral("0.5 -0.25 1 0"));
  const auto plain = Dataflow::waveformFrameFromText(u"0.5 -0.25 1 0", 4, 0);
  REQUIRE(plain);
  CHECK(std::equal(plain->begin(), plain->end(), frame.begin(), frame.end()));
  // A score or JSON list, the first frame of a bank as a cable plays it,
  // comma or semicolon lists, bools as numbers.
  for(auto text : {u"[0.5, -0.25, 1, 0]", u"[[0.5, -0.25, 1, 0], [1, 1]]", u"0.5;-0.25;1;0",
                   u"(0.5,-0.25,1,0)\n", u"[0.5 -0.25 1 0]", u"0.5, -0.25, 1, 0;",
                   u"[0.5, -0.25, 1, false]"})
  {
    INFO(QString::fromUtf16(text).toStdString());
    const auto read = Dataflow::waveformFrameFromText(text, 4, 0);
    REQUIRE(read);
    CHECK(std::equal(read->begin(), read->end(), frame.begin(), frame.end()));
  }
  CHECK_FALSE(Dataflow::waveformFrameFromText(u"0.5 sine", 4, 0));
  CHECK_FALSE(Dataflow::waveformFrameFromText(u"", 4, 0));
  CHECK_FALSE(Dataflow::waveformFrameFromText(u"[ ]", 4, 0));
  CHECK_FALSE(Dataflow::waveformFrameFromText(u"0.5 nan", 4, 0));

  // Amplitudes are bipolar and clamped as a cable's are; a multislider's
  // 0 to 1 list keeps its values.
  const auto clamped = Dataflow::waveformFrameFromText(u"2 -3", 4, 0);
  REQUIRE(clamped);
  CHECK(*clamped == std::vector<float>{1.f, 0.f, -1.f, 0.f});
  const auto slider = Dataflow::waveformFrameFromText(u"[0, 0.5, 1, 0.5]", 8, 0);
  REQUIRE(slider);
  CHECK(*slider == std::vector<float>{0.f, .25f, .5f, .75f, 1.f, .75f, .5f, .25f});
  const auto constant = Dataflow::waveformFrameFromText(u"0.3", 3, 0);
  REQUIRE(constant);
  CHECK(*constant == std::vector<float>(3, .3f));
  const auto quantized = Dataflow::waveformFrameFromText(u"0.1 -0.9", 2, 4);
  REQUIRE(quantized);
  CHECK(*quantized == std::vector<float>{level(2, 4), level(0, 4)});

  // The text reads back to the same floats.
  std::vector<float> noise(64);
  Process::generateWaveform(noise, Process::WaveformShape::Noise, 0, 1234);
  const auto back = Dataflow::waveformFrameFromText(Dataflow::waveformFrameText(noise), 64, 0);
  REQUIRE(back);
  CHECK(*back == noise);
}

TEST_CASE("A Shift drag draws one straight quantized line", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{940}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    const Process::WavetableData flat{32, std::vector<float>(32, level(0, 16))};
    inlet->setWaveform(flat);
    SceneEditor editor{*inlet, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    auto& stack = document->commandStack();
    const auto commands = stack.currentIndex();

    editor.mouse(QEvent::MouseButtonPress, editor.sample(4, 32, -.8), Qt::ShiftModifier);
    editor.mouse(QEvent::MouseMove, editor.sample(16, 32, .95), Qt::ShiftModifier);
    editor.mouse(QEvent::MouseMove, editor.sample(28, 32, .8), Qt::ShiftModifier);
    CHECK(inlet->wavetable() == flat);
    CHECK(stack.currentIndex() == commands);
    CHECK(Dataflow::waveformItemState(*editor.editor).readout.startsWith(QStringLiteral("#28 ")));
    editor.mouse(QEvent::MouseButtonRelease, editor.sample(28, 32, .8), Qt::ShiftModifier);
    CHECK(stack.currentIndex() == commands + 1);

    // Only the line's own samples, the first drag point left behind.
    const auto& drawn = inlet->wavetable().samples;
    const float start = drawn[4];
    const float end = drawn[28];
    CHECK(std::abs(start + .8f) <= 1.f / 15.f);
    CHECK(std::abs(end - .8f) <= 1.f / 15.f);
    for(std::size_t i = 0; i < 32; ++i)
    {
      INFO(i);
      if(i < 4 || i > 28)
        CHECK(drawn[i] == flat.samples[i]);
      else
        CHECK(drawn[i] == Process::quantizeSample(start + (i - 4.) / 24. * (end - start), 16));
    }
    CHECK(onLevels(drawn, 16));
    stack.undo();
    CHECK(inlet->wavetable() == flat);
  });
}

TEST_CASE("A continuous pen catches the reference lines unless Alt is held", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* continuous = new Dataflow::WaveformInlet{
        Id<Process::Port>{941}, &score::test::base_interval(*document)};
    auto* chip = new Dataflow::WaveformInlet{
        Id<Process::Port>{942}, &score::test::base_interval(*document)};
    chip->setAmplitudeLevels(16);
    SceneEditor editor{*continuous, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    // Two pixels from a line: caught; Alt draws where the pen is.
    const double near = 2. * 2. / Dataflow::waveformItemState(*editor.editor).plot.height();
    const auto draw = [&](std::size_t sample, double amplitude, Qt::KeyboardModifiers modifiers) {
      editor.mouse(QEvent::MouseButtonPress, editor.sample(sample, 32, amplitude), modifiers);
      editor.mouse(QEvent::MouseButtonRelease, editor.sample(sample, 32, amplitude), modifiers);
    };
    draw(5, near, {});
    CHECK(continuous->wavetable().samples[5] == 0.f);
    draw(6, .5 - near, {});
    CHECK(continuous->wavetable().samples[6] == .5f);
    draw(7, near, Qt::AltModifier);
    CHECK(continuous->wavetable().samples[7] != 0.f);
    CHECK(std::abs(continuous->wavetable().samples[7] - near) < .015);
    draw(8, .3, {});
    CHECK(std::abs(continuous->wavetable().samples[8] - .3) < .015);

    // Levels are the port's own: Alt changes nothing.
    Dataflow::setWaveformItemModel(*editor.editor, *chip);
    draw(9, .3, Qt::AltModifier);
    CHECK(chip->wavetable().samples[9] == Process::quantizeSample(.3, 16));
  });
}

TEST_CASE("Focused waveform editors take keys, each edit one command", "[wavetable][gui][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{943}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    auto start = inlet->wavetable();
    start.samples[10] = level(5, 16);
    inlet->setWaveform(start);
    SceneEditor editor{*inlet, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    auto& stack = document->commandStack();
    const auto sample10 = [&] { return inlet->wavetable().samples[10]; };
    const auto state = [&] { return Dataflow::waveformItemState(*editor.editor); };

    // Without focus the application's shortcuts stay.
    editor.hover(editor.sample(10, 32, 0.));
    CHECK_FALSE(editor.overrides(Qt::Key_Delete));
    CHECK_FALSE(editor.overrides(Qt::Key_Up));
    editor.press(Qt::Key_Up);
    CHECK(sample10() == level(5, 16));

    editor.editor->setFocus();
    REQUIRE(editor.editor->hasFocus());
    for(int key : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_Delete,
                   Qt::Key_Backspace, Qt::Key_Escape})
      CHECK(editor.overrides(key));
    CHECK(editor.overrides(Qt::Key_C, Qt::ControlModifier));
    CHECK(editor.overrides(Qt::Key_V, Qt::ControlModifier));
    CHECK(editor.overrides(Qt::Key_Up, Qt::ShiftModifier));
    CHECK_FALSE(editor.overrides(Qt::Key_Z, Qt::ControlModifier));
    CHECK_FALSE(editor.overrides(Qt::Key_Left, Qt::ControlModifier));
    CHECK_FALSE(editor.overrides(Qt::Key_PageUp));
    CHECK_FALSE(editor.overrides(Qt::Key_Space));

    auto commands = stack.currentIndex();
    editor.press(Qt::Key_Up);
    CHECK(sample10() == level(6, 16));
    CHECK(stack.currentIndex() == ++commands);
    CHECK(state().readout == QStringLiteral("#10 6/15"));
    editor.press(Qt::Key_Up, Qt::ShiftModifier);
    CHECK(sample10() == level(8, 16));
    CHECK(stack.currentIndex() == ++commands);

    // A held key is one command; the edited sample stays the keyboard's
    // after the pointer leaves.
    editor.hover({-20., -20.});
    CHECK(state().readout == QStringLiteral("#10 8/15"));
    editor.key(QEvent::KeyPress, Qt::Key_Down);
    for(int i = 0; i < 3; ++i)
    {
      editor.key(QEvent::KeyRelease, Qt::Key_Down, {}, true);
      editor.key(QEvent::KeyPress, Qt::Key_Down, {}, true);
    }
    CHECK(sample10() == level(4, 16));
    editor.key(QEvent::KeyRelease, Qt::Key_Down);
    CHECK(stack.currentIndex() == ++commands);
    stack.undo();
    CHECK(sample10() == level(8, 16));
    stack.redo();
    CHECK(sample10() == level(4, 16));
    // The bottom level does not move further and adds nothing.
    for(int i = 0; i < 6; ++i)
      editor.press(Qt::Key_Down);
    CHECK(sample10() == level(0, 16));
    commands = stack.currentIndex();
    editor.press(Qt::Key_Down);
    CHECK(stack.currentIndex() == commands);

    // On the port's levels, so that rotations are exact.
    {
      auto onPort = inlet->wavetable();
      for(float& sample : onPort.samples)
        sample = Process::quantizeSample(sample, 16);
      CommandDispatcher<> dispatcher{document->context().commandStack};
      dispatcher.submit<Process::SetControlValue>(*inlet, Process::wavetableToValue(onPort));
      ++commands;
    }
    const auto before = inlet->wavetable();
    const auto expect = [&](auto function) {
      auto expected = before;
      function(std::span<float>{expected.samples});
      return expected;
    };
    editor.press(Qt::Key_Left);
    CHECK(inlet->wavetable() == expect([](std::span<float> f) { Process::rotateWaveform(f, true); }));
    CHECK(stack.currentIndex() == ++commands);
    editor.press(Qt::Key_Right);
    CHECK(inlet->wavetable() == before);
    CHECK(stack.currentIndex() == ++commands);

    // Copy, clear, paste: the same frame.
    editor.press(Qt::Key_C, Qt::ControlModifier);
    CHECK(QGuiApplication::clipboard()->text()
          == Dataflow::waveformFrameText(inlet->wavetable().frame(0)));
    CHECK(stack.currentIndex() == commands);
    editor.press(Qt::Key_Delete);
    CHECK(inlet->wavetable() == expect([](std::span<float> f) { Process::clearWaveform(f, 16); }));
    CHECK(stack.currentIndex() == ++commands);
    editor.press(Qt::Key_V, Qt::ControlModifier);
    CHECK(inlet->wavetable() == before);
    CHECK(stack.currentIndex() == ++commands);

    // A multislider's value pasted: resampled to the frame, on the levels.
    QGuiApplication::clipboard()->setText(QStringLiteral("[0.1, 0.4, 0.9, 0.4]"));
    editor.press(Qt::Key_V, Qt::ControlModifier);
    CHECK(stack.currentIndex() == ++commands);
    const auto pasted = Dataflow::waveformFrameFromText(u"0.1 0.4 0.9 0.4", 32, 16);
    REQUIRE(pasted);
    CHECK(inlet->wavetable().samples == *pasted);
    CHECK(onLevels(inlet->wavetable().samples, 16));
    // Text that is not a list changes nothing.
    QGuiApplication::clipboard()->setText(QStringLiteral("sine"));
    editor.press(Qt::Key_V, Qt::ControlModifier);
    CHECK(stack.currentIndex() == commands);
    CHECK(inlet->wavetable().samples == *pasted);

    // Continuous ports move by a 64th, or an eighth with Shift.
    auto* continuous = new Dataflow::WaveformInlet{
        Id<Process::Port>{944}, &score::test::base_interval(*document)};
    Process::WavetableData zero{32, std::vector<float>(32, 0.f)};
    continuous->setWaveform(zero);
    Dataflow::setWaveformItemModel(*editor.editor, *continuous);
    editor.hover(editor.sample(3, 32, 0.));
    editor.press(Qt::Key_Up);
    CHECK(continuous->wavetable().samples[3] == 1.f / 64.f);
    editor.press(Qt::Key_Down, Qt::ShiftModifier);
    CHECK(continuous->wavetable().samples[3] == 1.f / 64.f - 1.f / 8.f);
    CHECK(state().readout == QStringLiteral("#3 -0.11"));

    // Losing focus gives the keys back.
    editor.editor->clearFocus();
    CHECK_FALSE(editor.overrides(Qt::Key_Delete));
  });
}

TEST_CASE("Page keys choose the frame of a bank", "[wavetable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* bank = new Dataflow::WavetableInlet{
        Id<Process::Port>{945}, &score::test::base_interval(*document)};
    auto table = Process::makeTriangleWavetable(8);
    for(int i = 0; i < 2; ++i)
      table.samples.insert(table.samples.end(), table.samples.begin(), table.samples.begin() + 8);
    bank->setWaveform(table);
    SceneEditor editor{*bank, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    editor.editor->setFocus();
    const auto commands = document->commandStack().currentIndex();
    CHECK(editor.overrides(Qt::Key_PageDown));
    editor.press(Qt::Key_PageDown);
    editor.press(Qt::Key_PageDown);
    CHECK(Dataflow::waveformItemState(*editor.editor).frame == 2);
    editor.press(Qt::Key_PageDown);
    CHECK(Dataflow::waveformItemState(*editor.editor).frame == 2);
    editor.press(Qt::Key_PageUp);
    CHECK(Dataflow::waveformItemState(*editor.editor).frame == 1);
    CHECK(document->commandStack().currentIndex() == commands);
    // Edits go to the chosen frame.
    editor.press(Qt::Key_Delete);
    CHECK(bank->wavetable().frame(0)[2] == table.samples[2]);
    CHECK(bank->wavetable().frame(1)[2] == 0.f);
  });
}

TEST_CASE("What execution plays is drawn in front, the document behind", "[wavetable][gui][execution]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* inlet = new Dataflow::WaveformInlet{
        Id<Process::Port>{946}, &score::test::base_interval(*document)};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    SceneEditor editor{*inlet, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    const auto value = inlet->value();
    const auto commands = document->commandStack().currentIndex();
    const auto state = [&] { return Dataflow::waveformItemState(*editor.editor); };
    CHECK_FALSE(state().live);
    editor.hover(editor.sample(2, 32, 0.));
    const auto authored = state().readout;

    std::vector<ossia::value> square(32, -1.f);
    std::fill(square.begin(), square.begin() + 16, 1.f);
    inlet->setExecutionValue(square);
    CHECK(state().live);
    CHECK(state().readout == QStringLiteral("#2 15/15"));
    CHECK(state().label == QStringLiteral("32 smp, 4 bit"));
    CHECK(inlet->value() == value);

    // Drawing shows the edit, not what plays; once released the edit stays
    // in front until execution receives something again.
    editor.mouse(QEvent::MouseButtonPress, editor.sample(20, 32, .5));
    CHECK_FALSE(state().live);
    editor.mouse(QEvent::MouseButtonRelease, editor.sample(20, 32, .5));
    CHECK_FALSE(state().live);
    CHECK(document->commandStack().currentIndex() == commands + 1);
    inlet->setExecutionValue(square);
    CHECK(state().live);

    inlet->executionReset();
    CHECK_FALSE(state().live);
    editor.hover(editor.sample(2, 32, 0.));
    CHECK(state().readout == authored);
    CHECK(document->commandStack().currentIndex() == commands + 1);
  });
}

TEST_CASE("What execution received stops showing once nothing reaches the port", "[wavetable][gui][execution]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto& interval = score::test::base_interval(*document);
    auto* inlet = new Dataflow::WaveformInlet{Id<Process::Port>{948}, &interval};
    inlet->setAmplitudeLevels(16);
    inlet->setFrameSizeConstraint(32);
    auto* source = new Process::ControlOutlet{QStringLiteral("Value"), Id<Process::Port>{949}, &interval};
    SceneEditor editor{*inlet, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    const auto state = [&] { return Dataflow::waveformItemState(*editor.editor); };
    const auto render = [&] {
      QImage image{400, 180, QImage::Format_ARGB32_Premultiplied};
      image.fill(Qt::black);
      QPainter painter{&image};
      editor.editor->paint(&painter, nullptr, nullptr);
      return image;
    };
    const auto authored = render();
    std::vector<ossia::value> square(32, -1.f);
    std::fill(square.begin(), square.begin() + 16, 1.f);

    auto& scenario = score::IDocument::get<Scenario::ScenarioDocumentModel>(*document);
    CommandDispatcher<> dispatcher{document->context().commandStack};
    const auto connect = [&](int id) {
      dispatcher.submit<Dataflow::CreateCable>(
          scenario, Id<Process::Cable>{id}, Process::CableType::ImmediateGlutton, *source,
          *inlet);
    };

    // A multislider cabled in, then removed.
    connect(950);
    inlet->setExecutionValue(square);
    CHECK(state().live);
    CHECK(render() != authored);
    document->commandStack().undo();
    REQUIRE(inlet->cables().empty());
    CHECK_FALSE(state().live);
    CHECK(render() == authored);

    // Two cables: removing one keeps what the other sends.
    connect(951);
    connect(952);
    inlet->setExecutionValue(square);
    document->commandStack().undo();
    REQUIRE(inlet->cables().size() == 1);
    CHECK(state().live);
    document->commandStack().undo();
    CHECK_FALSE(state().live);

    // An address, then unset; a cable keeps it live.
    State::AddressAccessor address;
    address.address.device = QStringLiteral("osc");
    address.address.path = QStringList{QStringLiteral("wave")};
    inlet->setAddress(address);
    inlet->setExecutionValue(square);
    CHECK(state().live);
    connect(953);
    inlet->setAddress({});
    CHECK(state().live);
    document->commandStack().undo();
    CHECK_FALSE(state().live);
    CHECK(render() == authored);

    // With the stale wave still in front, an edit shows: a key, a stroke, a
    // menu entry and a paste each draw the document again.
    const auto commands = document->commandStack().currentIndex();
    editor.editor->setFocus();
    editor.hover(editor.sample(3, 32, 0.));
    const auto edits = std::vector<std::function<void()>>{
        [&] { editor.press(Qt::Key_Up); },
        [&] {
          editor.mouse(QEvent::MouseButtonPress, editor.sample(20, 32, .5));
          editor.mouse(QEvent::MouseButtonRelease, editor.sample(20, 32, .5));
        },
        [&] {
          QMenu menu;
          Dataflow::fillWaveformItemMenu(*editor.editor, menu);
          auto* invert = menuAction(menu, QStringLiteral("Invert"));
          REQUIRE(invert);
          invert->trigger();
        },
        [&] {
          QGuiApplication::clipboard()->setText(QStringLiteral("0.5 -0.5"));
          editor.press(Qt::Key_V, Qt::ControlModifier);
        }};
    std::size_t done{};
    for(const auto& edit : edits)
    {
      CAPTURE(done);
      inlet->setExecutionValue(square);
      REQUIRE(state().live);
      const auto before = inlet->wavetable();
      edit();
      QCoreApplication::processEvents();
      CHECK(inlet->wavetable() != before);
      CHECK_FALSE(state().live);
      // The front frame is the document's.
      editor.hover(editor.sample(20, 32, 0.));
      const float shown = inlet->wavetable().samples[20];
      CHECK(state().readout
            == QStringLiteral("#20 %1/15").arg(Process::sampleLevel(shown, 16)));
      ++done;
    }
    CHECK(done == edits.size());
    CHECK(document->commandStack().currentIndex() > commands);
  });
}

TEST_CASE("Bank thumbnails move by drag, duplicate by double-click, scroll by wheel", "[wavetable][gui][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* bank = new Dataflow::WavetableInlet{
        Id<Process::Port>{947}, &score::test::base_interval(*document)};
    Process::WavetableData table{8, {}};
    for(int frame = 0; frame < 3; ++frame)
      for(int i = 0; i < 8; ++i)
        table.samples.push_back(float(frame) / 4.f);
    bank->setWaveform(table);
    SceneEditor editor{*bank, document->context(), {0., 0., 400., 180.}};
    REQUIRE(score::test::showAndActivate(editor.view));
    auto& stack = document->commandStack();
    const auto strip = Dataflow::waveformItemState(*editor.editor).frames;
    const auto thumbnail = [&](int frame) {
      return QPointF{strip.left() + 24. + 48. * frame, strip.center().y()};
    };
    const auto frame = [&] { return Dataflow::waveformItemState(*editor.editor).frame; };
    const auto order = [&] {
      std::vector<float> firsts;
      for(std::size_t i = 0; i < bank->wavetable().frameCount(); ++i)
        firsts.push_back(bank->wavetable().frame(i)[0] * 4.f);
      return firsts;
    };
    const auto commands = stack.currentIndex();

    // A press that hardly moves only chooses.
    editor.mouse(QEvent::MouseButtonPress, thumbnail(0));
    editor.mouse(QEvent::MouseMove, thumbnail(0) + QPointF{2., 0.});
    editor.mouse(QEvent::MouseButtonRelease, thumbnail(0) + QPointF{2., 0.});
    CHECK(stack.currentIndex() == commands);

    editor.mouse(QEvent::MouseButtonPress, thumbnail(0));
    editor.mouse(QEvent::MouseMove, thumbnail(1));
    editor.mouse(QEvent::MouseMove, thumbnail(2));
    CHECK(order() == std::vector<float>{0.f, 1.f, 2.f});
    editor.mouse(QEvent::MouseButtonRelease, thumbnail(2));
    CHECK(order() == std::vector<float>{1.f, 2.f, 0.f});
    CHECK(frame() == 2);
    CHECK(stack.currentIndex() == commands + 1);
    editor.mouse(QEvent::MouseButtonPress, thumbnail(2));
    editor.mouse(QEvent::MouseMove, thumbnail(1));
    editor.mouse(QEvent::MouseButtonRelease, thumbnail(1));
    CHECK(order() == std::vector<float>{1.f, 0.f, 2.f});
    CHECK(frame() == 1);
    stack.undo();
    CHECK(order() == std::vector<float>{1.f, 2.f, 0.f});
    stack.undo();
    CHECK(bank->wavetable() == table);

    editor.mouse(QEvent::MouseButtonPress, thumbnail(1));
    editor.mouse(QEvent::MouseButtonRelease, thumbnail(1));
    editor.mouse(QEvent::MouseButtonDblClick, thumbnail(1));
    editor.mouse(QEvent::MouseButtonRelease, thumbnail(1));
    CHECK(order() == std::vector<float>{0.f, 1.f, 1.f, 2.f});
    CHECK(frame() == 2);
    CHECK(stack.currentIndex() == commands + 1);

    const auto wheel = [&](QPointF point, int delta) {
      const auto position = editor.at(point);
      QWheelEvent event{QPointF(position), QPointF(editor.view.viewport()->mapToGlobal(position)),
                        QPoint{}, QPoint{0, delta}, Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false};
      QCoreApplication::sendEvent(editor.view.viewport(), &event);
    };
    wheel(thumbnail(0), -120);
    CHECK(frame() == 3);
    wheel(thumbnail(0), -120);
    CHECK(frame() == 3);
    wheel(thumbnail(0), 120);
    wheel(thumbnail(0), 120);
    CHECK(frame() == 1);
    // The plot leaves the wheel to the view.
    wheel(editor.sample(3, 8, 0.), 120);
    CHECK(frame() == 1);
    CHECK(stack.currentIndex() == commands + 1);
  });
}

TEST_CASE("The inspector's shapes, menu and number list edit the chosen frame", "[wavetable][gui][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* document = score::test::new_document(ctx);
    REQUIRE(document);
    auto* bank = new Dataflow::WavetableInlet{
        Id<Process::Port>{948}, &score::test::base_interval(*document)};
    bank->setAmplitudeLevels(16);
    bank->setFrameSizeConstraint(8);
    Process::WavetableData table{8, std::vector<float>(16, level(4, 16))};
    table.samples[9] = level(12, 16);
    bank->setWaveform(table);
    std::unique_ptr<QWidget> panel{WidgetFactory::WaveformInletItems::make_widget(
        *bank, document->context(), nullptr, bank)};
    panel->resize(420, 260);
    REQUIRE(score::test::showAndActivate(*panel));
    auto* view = panel->findChild<QGraphicsView*>();
    REQUIRE(view);
    auto* editor = waveformEditor(*view->scene());
    REQUIRE(editor);
    auto* shapes = panel->findChild<QToolButton*>(QStringLiteral("WaveformShape"));
    auto* text = panel->findChild<QLineEdit*>(QStringLiteral("WaveformText"));
    REQUIRE(shapes);
    REQUIRE(shapes->menu());
    REQUIRE(text);
    // Narrow enough for the inspector's value column.
    CHECK(panel->minimumSizeHint().width() <= 110);
    auto* edit = panel->findChild<QToolButton*>(QStringLiteral("WaveformEdit"));
    REQUIRE(edit);
    REQUIRE(edit->menu());
    // The context menu without its shapes, which the row has.
    const auto action = [&](const QString& name) {
      edit->menu()->aboutToShow();
      CHECK_FALSE(menuAction(*edit->menu(), QStringLiteral("Shape")));
      return menuAction(*edit->menu(), name);
    };
    auto& stack = document->commandStack();
    CHECK(text->text() == Dataflow::waveformFrameText(bank->wavetable().frame(0)));

    // The second frame, chosen in the editor.
    const auto strip = Dataflow::waveformItemState(*editor).frames;
    click(*view, *editor, {strip.left() + 72., strip.center().y()});
    REQUIRE(Dataflow::waveformItemState(*editor).frame == 1);
    CHECK(text->text() == Dataflow::waveformFrameText(bank->wavetable().frame(1)));

    auto commands = stack.currentIndex();
    using Transform = std::function<void(std::span<float>)>;
    for(auto [name, transform] : std::initializer_list<std::pair<const char*, Transform>>{
            {"Normalize", [](std::span<float> f) { Process::normalizeWaveform(f, 16); }},
            {"Invert", [](std::span<float> f) { Process::invertWaveform(f, 16); }},
            {"Reverse", [](std::span<float> f) { Process::reverseWaveform(f); }},
            {"Smooth", [](std::span<float> f) { Process::smoothWaveform(f, 16); }}})
    {
      INFO(name);
      auto* tool = action(QString::fromLatin1(name));
      REQUIRE(tool);
      auto expected = bank->wavetable();
      std::span<float> second{expected.samples.data() + 8, 8};
      transform(second);
      for(float& sample : second)
        sample = Process::quantizeSample(sample, 16);
      tool->trigger();
      CHECK(bank->wavetable() == expected);
      CHECK(stack.currentIndex() == ++commands);
      CHECK(text->text() == Dataflow::waveformFrameText(bank->wavetable().frame(1)));
    }

    auto* square = menuAction(*shapes->menu(), QStringLiteral("Square"));
    REQUIRE(square);
    square->trigger();
    std::vector<float> expected(8);
    Process::generateWaveform(expected, Process::WaveformShape::Square, 16);
    CHECK(std::equal(expected.begin(), expected.end(), bank->wavetable().frame(1).begin()));
    CHECK(bank->wavetable().frame(0)[0] == level(4, 16));
    CHECK(stack.currentIndex() == ++commands);

    text->setText(QStringLiteral("-1 1"));
    text->returnPressed();
    const auto read = Dataflow::waveformFrameFromText(u"-1 1", 8, 16);
    REQUIRE(read);
    CHECK(std::equal(read->begin(), read->end(), bank->wavetable().frame(1).begin()));
    CHECK(stack.currentIndex() == ++commands);
    text->setText(QStringLiteral("not a list"));
    text->returnPressed();
    CHECK(stack.currentIndex() == commands);
    CHECK(text->text() == Dataflow::waveformFrameText(bank->wavetable().frame(1)));
    stack.undo();
    CHECK(text->text() == Dataflow::waveformFrameText(bank->wavetable().frame(1)));
  });
}

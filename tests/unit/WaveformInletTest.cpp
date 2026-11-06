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

#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QGraphicsView>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QImage>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
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
template <typename T>
T* editorControl(QGraphicsView& view, const char* name)
{
  for(auto* item : view.scene()->items())
    if(auto* proxy = dynamic_cast<QGraphicsProxyWidget*>(item); proxy && proxy->widget())
      if(auto* control = proxy->widget()->findChild<T*>(QString::fromLatin1(name)))
        return control;
  return nullptr;
}
}

TEST_CASE("Bank editor operations preserve frame order and share document undo", "[wavetable][gui]")
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
    auto* frames = editorControl<QComboBox>(*view, "WaveformFrameSelector");
    auto* duplicate = editorControl<QToolButton>(*view, "WaveformDuplicateFrame");
    auto* right = editorControl<QToolButton>(*view, "WaveformMoveFrameRight");
    auto* remove = editorControl<QToolButton>(*view, "WaveformRemoveFrame");
    auto* resolution = editorControl<QSpinBox>(*view, "WaveformResolution");
    REQUIRE(frames); REQUIRE(duplicate); REQUIRE(right); REQUIRE(remove); REQUIRE(resolution);
    frames->setCurrentIndex(0);
    duplicate->click();
    auto expected = first;
    expected.samples.insert(expected.samples.end(), original.samples.begin(), original.samples.end());
    CHECK(inlet->wavetable() == expected);
    right->click();
    expected = original;
    expected.samples.insert(expected.samples.end(), first.samples.begin(), first.samples.end());
    CHECK(inlet->wavetable() == expected);
    remove->click();
    CHECK(inlet->wavetable() == original);
    document->commandStack().undo();
    CHECK(inlet->wavetable() == expected);
    document->commandStack().redo();
    CHECK(inlet->wavetable() == original);

    resolution->setValue(16);
    REQUIRE(QMetaObject::invokeMethod(resolution, "editingFinished", Qt::DirectConnection));
    const auto& resized = inlet->wavetable();
    REQUIRE(resized.frameSize == 16);
    REQUIRE(resized.frameCount() == 2);
    for(std::size_t frame = 0; frame < 2; ++frame)
      for(std::size_t sample = 0; sample < 16; ++sample)
        CHECK(resized.frame(frame)[sample]
              == Process::sampleWavetable(original, frame, double(sample) / 16));
    document->commandStack().undo();
    CHECK(inlet->wavetable() == original);
    frames->setCurrentIndex(1);
    remove->click();
    CHECK(inlet->wavetable() == first);
    CHECK_FALSE(remove->isEnabled());
    remove->click();
    CHECK(inlet->wavetable() == first);
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

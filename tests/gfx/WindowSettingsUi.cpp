// Gfx/Window/WindowSettingsWidget.cpp (1161 lines, 0%) — the protocol settings
// widget of the window output device. It owns the two editing canvases, the
// per-output inspector, the output selector buttons and the preview windows,
// and it is the only place where a Gfx::WindowSettings is built from, and
// pushed back into, the UI.
//
// The widget exposes no accessor for any of its controls, so the test addresses
// them the way a user does: by the label of the QFormLayout row they sit in.
// That keeps the assertions readable and means a renamed row breaks the test
// loudly instead of silently drifting onto the wrong spin box.
//
// run_in_app: offscreen QPA, no GPU. The widget creates preview QWidgets, which
// the offscreen platform serves fine.

#include <Gfx/Window/OutputMapping.hpp>
#include <Gfx/Window/OutputPreview.hpp>
#include <Gfx/Window/WindowSettingsWidget.hpp>
#include <Gfx/WindowDevice.hpp>

#include <score_test/App.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace Gfx;

namespace
{
/// Every widget sitting in the field cell of the QFormLayout row labelled
/// `label`, anywhere under `root`. A row's field is either a widget or a
/// nested layout (the paired X/Y spin boxes), so both are flattened.
std::vector<QWidget*> fieldsForLabel(QWidget& root, const QString& label)
{
  std::vector<QWidget*> out;
  const auto collect = [&](QLayout* l, auto&& self) -> void {
    for(int i = 0; i < l->count(); ++i)
    {
      auto* it = l->itemAt(i);
      if(auto* w = it->widget())
        out.push_back(w);
      else if(auto* sub = it->layout())
        self(sub, self);
    }
  };

  for(auto* form : root.findChildren<QFormLayout*>())
  {
    for(int row = 0; row < form->rowCount(); ++row)
    {
      auto* labelItem = form->itemAt(row, QFormLayout::LabelRole);
      if(!labelItem)
        continue;
      auto* lw = qobject_cast<QLabel*>(labelItem->widget());
      if(!lw || lw->text() != label)
        continue;

      auto* fieldItem = form->itemAt(row, QFormLayout::FieldRole);
      if(!fieldItem)
        continue;
      if(auto* w = fieldItem->widget())
        out.push_back(w);
      else if(auto* sub = fieldItem->layout())
        collect(sub, collect);
    }
  }
  return out;
}

template <typename T>
T* fieldAs(QWidget& root, const QString& label, int nth = 0)
{
  int seen = 0;
  for(auto* w : fieldsForLabel(root, label))
    if(auto* t = qobject_cast<T*>(w))
      if(seen++ == nth)
        return t;
  return nullptr;
}

/// The QComboBox whose first entry is `first` — every combo in this widget has
/// a distinctive one.
QComboBox* comboStartingWith(QWidget& root, const QString& first)
{
  for(auto* c : root.findChildren<QComboBox*>())
    if(c->count() > 0 && c->itemText(0) == first)
      return c;
  return nullptr;
}

QPushButton* outputButton(QWidget& root, int idx)
{
  for(auto* b : root.findChildren<QPushButton*>())
    if(b->isCheckable() && b->text() == QString::number(idx))
      return b;
  return nullptr;
}

int outputButtonCount(QWidget& root)
{
  int n = 0;
  for(auto* b : root.findChildren<QPushButton*>())
    if(b->isCheckable())
      ++n;
  return n;
}

int visiblePreviewWindows()
{
  int n = 0;
  for(auto* w : QApplication::topLevelWidgets())
    if(auto* pw = dynamic_cast<PreviewWidget*>(w))
      if(pw->isVisible())
        ++n;
  return n;
}

OutputMapping mapping(QRectF src, QPoint pos, QSize size)
{
  OutputMapping m;
  m.sourceRect = src;
  m.windowPosition = pos;
  m.windowSize = size;
  return m;
}

Device::DeviceSettings makeSettings(WindowSettings ws, QString name = "win")
{
  Device::DeviceSettings s;
  s.name = std::move(name);
  s.deviceSpecificSettings = QVariant::fromValue(ws);
  return s;
}

WindowSettings settingsOf(const WindowSettingsWidget& w)
{
  return w.getSettings().deviceSpecificSettings.value<WindowSettings>();
}
}

TEST_CASE("WindowSettingsWidget defaults", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;

    const auto s = w.getSettings();
    const auto ws = s.deviceSpecificSettings.value<WindowSettings>();
    CHECK(ws.mode == WindowMode::Single);
    CHECK(ws.flag == SwapchainFlag::NoFlag);
    CHECK(ws.format == SwapchainFormat::SDR);
    CHECK(ws.inputWidth == 1920);
    CHECK(ws.inputHeight == 1080);
    // Single mode does not read the canvas at all.
    CHECK(ws.outputs.empty());

    // No output is selected, so the per-output inspector is hidden.
    CHECK(outputButtonCount(w) == 0);
  });
}

TEST_CASE("WindowSettingsWidget settings round-trip", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.flag = SwapchainFlag::sRGB;
    ws.format = SwapchainFormat::HDR10;
    ws.inputWidth = 1280;
    ws.inputHeight = 800;
    ws.outputs = {
        mapping({0.0, 0.0, 0.5, 1.0}, {10, 20}, {640, 480}),
        mapping({0.5, 0.0, 0.5, 1.0}, {700, 20}, {800, 600}),
        mapping({0.25, 0.25, 0.25, 0.25}, {10, 500}, {320, 240})};

    w.setSettings(makeSettings(ws, "myWindow"));

    const auto out = w.getSettings();
    CHECK(out.name == QStringLiteral("myWindow"));
    CHECK(out.protocol == WindowProtocolFactory::static_concreteKey());

    const auto got = settingsOf(w);
    CHECK(got.mode == WindowMode::MultiWindow);
    CHECK(got.flag == SwapchainFlag::sRGB);
    CHECK(got.format == SwapchainFormat::HDR10);
    CHECK(got.inputWidth == 1280);
    CHECK(got.inputHeight == 800);
    REQUIRE(got.outputs.size() == 3);
    for(std::size_t i = 0; i < 3; ++i)
    {
      INFO("output " << i);
      CHECK(got.outputs[i].sourceRect.x() == Approx(ws.outputs[i].sourceRect.x()).margin(0.01));
      CHECK(got.outputs[i].sourceRect.width()
            == Approx(ws.outputs[i].sourceRect.width()).margin(0.01));
      CHECK(got.outputs[i].windowPosition == ws.outputs[i].windowPosition);
      CHECK(got.outputs[i].windowSize == ws.outputs[i].windowSize);
    }

    // One selector button per mapping.
    CHECK(outputButtonCount(w) == 3);
    // ...and one preview window per mapping.
    CHECK(visiblePreviewWindows() == 3);
  });
}

TEST_CASE("WindowSettingsWidget mode switching", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;
    auto* mode = comboStartingWith(w, QStringLiteral("Single Window"));
    REQUIRE(mode != nullptr);

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {mapping({0, 0, 1, 1}, {0, 0}, {640, 480})};
    w.setSettings(makeSettings(ws));
    CHECK(visiblePreviewWindows() == 1);

    // Leaving multi-window mode destroys the preview windows...
    mode->setCurrentIndex(int(WindowMode::Background));
    CHECK(visiblePreviewWindows() == 0);
    CHECK(settingsOf(w).mode == WindowMode::Background);
    // ...and Background/Single do not report the canvas mappings.
    CHECK(settingsOf(w).outputs.empty());

    mode->setCurrentIndex(int(WindowMode::Single));
    CHECK(visiblePreviewWindows() == 0);
    CHECK(settingsOf(w).mode == WindowMode::Single);

    // ...and coming back re-creates them.
    mode->setCurrentIndex(int(WindowMode::MultiWindow));
    CHECK(visiblePreviewWindows() == 1);
    CHECK(settingsOf(w).outputs.size() == 1);
  });
}

TEST_CASE("WindowSettingsWidget flag and format combos", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;
    auto* flag = comboStartingWith(w, QStringLiteral("No Flag"));
    auto* format = comboStartingWith(w, QStringLiteral("SDR"));
    REQUIRE(flag != nullptr);
    REQUIRE(format != nullptr);

    flag->setCurrentIndex(1);
    CHECK(settingsOf(w).flag == SwapchainFlag::sRGB);
    flag->setCurrentIndex(0);
    CHECK(settingsOf(w).flag == SwapchainFlag::NoFlag);

    for(int i = 0; i < 4; ++i)
    {
      format->setCurrentIndex(i);
      INFO("format " << i);
      CHECK(settingsOf(w).format == SwapchainFormat(i));
    }
  });
}

TEST_CASE("WindowSettingsWidget per-output inspector", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.inputWidth = 1000;
    ws.inputHeight = 500;
    // Geometry small enough to survive the desktop canvas round trip: the
    // 800x600 offscreen virtual desktop clamps anything larger, and selecting
    // an output pushes the desktop item's geometry back onto the mapping.
    auto second = mapping({0.5, 0.0, 0.5, 1.0}, {320, 60}, {240, 180});
    second.fullscreen = true;
    ws.outputs = {mapping({0.0, 0.0, 0.5, 1.0}, {10, 20}, {200, 150}), second};
    w.setSettings(makeSettings(ws));

    auto* posX = fieldAs<QSpinBox>(w, QStringLiteral("Window Pos"), 0);
    auto* posY = fieldAs<QSpinBox>(w, QStringLiteral("Window Pos"), 1);
    auto* sizeW = fieldAs<QSpinBox>(w, QStringLiteral("Window Size"), 0);
    auto* sizeH = fieldAs<QSpinBox>(w, QStringLiteral("Window Size"), 1);
    auto* full = fieldAs<QCheckBox>(w, QStringLiteral("Fullscreen"));
    REQUIRE(posX);
    REQUIRE(posY);
    REQUIRE(sizeW);
    REQUIRE(sizeH);
    REQUIRE(full);

    SECTION("selecting an output loads that output's properties")
    {
      // Keyed on the fullscreen flag and the source rect rather than on the
      // window geometry: the widget is never shown here, so its
      // DesktopLayoutCanvas has no viewport and selecting an output pushes a
      // floor-clamped geometry back onto the mapping. That is an artefact of an
      // unshown widget (in the app the canvas is laid out inside the device
      // dialog), and pinning it would pin the artefact.
      auto* full = fieldAs<QCheckBox>(w, QStringLiteral("Fullscreen"));
      auto* srcX = fieldAs<QDoubleSpinBox>(w, QStringLiteral("Position"), 0);
      REQUIRE(full);
      REQUIRE(srcX);

      auto* b1 = outputButton(w, 1);
      REQUIRE(b1 != nullptr);
      b1->click();
      CHECK(full->isChecked());
      CHECK(srcX->value() == Approx(0.5).margin(0.01));

      auto* b0 = outputButton(w, 0);
      REQUIRE(b0 != nullptr);
      b0->click();
      CHECK_FALSE(full->isChecked());
      CHECK(srcX->value() == Approx(0.0).margin(0.01));

      // ...and back again, so neither value is simply stuck.
      outputButton(w, 1)->click();
      CHECK(full->isChecked());
      CHECK(srcX->value() == Approx(0.5).margin(0.01));
    }

    SECTION("editing a property writes back to that output only")
    {
      outputButton(w, 1)->click();
      posX->setValue(123);
      posY->setValue(456);
      sizeW->setValue(321);
      sizeH->setValue(654);
      full->setChecked(true);

      const auto got = settingsOf(w);
      REQUIRE(got.outputs.size() == 2);
      CHECK(got.outputs[1].windowPosition == QPoint{123, 456});
      CHECK(got.outputs[1].windowSize == QSize{321, 654});
      CHECK(got.outputs[1].fullscreen);
      CHECK(got.outputs[0].windowPosition == QPoint{10, 20});
      CHECK(got.outputs[0].windowSize == QSize{200, 150});
      CHECK_FALSE(got.outputs[0].fullscreen);
    }

    SECTION("the source rect spin boxes rewrite the mapping quad")
    {
      outputButton(w, 0)->click();
      auto* srcX = fieldAs<QDoubleSpinBox>(w, QStringLiteral("Position"), 0);
      auto* srcY = fieldAs<QDoubleSpinBox>(w, QStringLiteral("Position"), 1);
      auto* srcW = fieldAs<QDoubleSpinBox>(w, QStringLiteral("Size"), 0);
      auto* srcH = fieldAs<QDoubleSpinBox>(w, QStringLiteral("Size"), 1);
      REQUIRE(srcX);
      REQUIRE(srcY);
      REQUIRE(srcW);
      REQUIRE(srcH);
      CHECK(srcX->value() == Approx(0.0).margin(0.01));
      CHECK(srcW->value() == Approx(0.5).margin(0.01));

      srcX->setValue(0.25);
      srcY->setValue(0.10);
      srcW->setValue(0.40);
      srcH->setValue(0.30);

      const auto got = settingsOf(w);
      CHECK(got.outputs[0].sourceRect.x() == Approx(0.25).margin(0.01));
      CHECK(got.outputs[0].sourceRect.y() == Approx(0.10).margin(0.01));
      CHECK(got.outputs[0].sourceRect.width() == Approx(0.40).margin(0.01));
      CHECK(got.outputs[0].sourceRect.height() == Approx(0.30).margin(0.01));
    }

    SECTION("the blend spin boxes reach the mapping")
    {
      outputButton(w, 0)->click();
      struct Row
      {
        const char* label;
        float width;
        float gamma;
      };
      const Row rows[] = {
          {"Left", 0.05f, 1.1f},
          {"Right", 0.10f, 1.2f},
          {"Top", 0.15f, 1.3f},
          {"Bottom", 0.20f, 1.4f}};
      for(auto& r : rows)
      {
        auto* wsp = fieldAs<QDoubleSpinBox>(w, QString::fromUtf8(r.label), 0);
        auto* gsp = fieldAs<QDoubleSpinBox>(w, QString::fromUtf8(r.label), 1);
        REQUIRE(wsp);
        REQUIRE(gsp);
        wsp->setValue(r.width);
        gsp->setValue(r.gamma);
      }

      const auto& m = settingsOf(w).outputs[0];
      CHECK(m.blendLeft.width == Approx(0.05f).margin(0.001));
      CHECK(m.blendLeft.gamma == Approx(1.1f).margin(0.001));
      CHECK(m.blendRight.width == Approx(0.10f).margin(0.001));
      CHECK(m.blendRight.gamma == Approx(1.2f).margin(0.001));
      CHECK(m.blendTop.width == Approx(0.15f).margin(0.001));
      CHECK(m.blendTop.gamma == Approx(1.3f).margin(0.001));
      CHECK(m.blendBottom.width == Approx(0.20f).margin(0.001));
      CHECK(m.blendBottom.gamma == Approx(1.4f).margin(0.001));
    }

    SECTION("rotation and mirroring")
    {
      outputButton(w, 0)->click();
      auto* rot = comboStartingWith(w, QString::fromUtf8("0°"));
      auto* mx = fieldAs<QCheckBox>(w, QStringLiteral("Mirror X"));
      auto* my = fieldAs<QCheckBox>(w, QStringLiteral("Mirror Y"));
      REQUIRE(rot);
      REQUIRE(mx);
      REQUIRE(my);

      for(int i = 0; i < 4; ++i)
      {
        rot->setCurrentIndex(i);
        INFO("rotation index " << i);
        CHECK(settingsOf(w).outputs[0].rotation == i * 90);
      }

      mx->setChecked(true);
      my->setChecked(true);
      CHECK(settingsOf(w).outputs[0].mirrorX);
      CHECK(settingsOf(w).outputs[0].mirrorY);
      mx->setChecked(false);
      CHECK_FALSE(settingsOf(w).outputs[0].mirrorX);
      CHECK(settingsOf(w).outputs[0].mirrorY);
    }

    SECTION("the lock mode drives the window-size fields")
    {
      outputButton(w, 0)->click();
      auto* lock = comboStartingWith(w, QStringLiteral("Free"));
      REQUIRE(lock != nullptr);

      // Free: both size fields are editable and taken verbatim.
      lock->setCurrentIndex(int(OutputLockMode::Free));
      sizeW->setValue(333);
      sizeH->setValue(222);
      CHECK(sizeW->isEnabled());
      CHECK(sizeH->isEnabled());
      CHECK(settingsOf(w).outputs[0].windowSize == QSize{333, 222});

      // 1:1 pixel: the size becomes source-rect x input resolution
      // (0.5 x 1.0 of 1000x500), and both fields are locked.
      lock->setCurrentIndex(int(OutputLockMode::OneToOne));
      const auto oneToOne = settingsOf(w).outputs[0];
      CHECK_FALSE(sizeW->isEnabled());
      CHECK_FALSE(sizeH->isEnabled());
      CHECK(oneToOne.lockMode == OutputLockMode::OneToOne);
      CHECK(oneToOne.windowSize.width() == 500);
      CHECK(oneToOne.windowSize.height() == 500);

      // Aspect ratio: width stays editable, height is derived from it.
      lock->setCurrentIndex(int(OutputLockMode::AspectRatio));
      CHECK(sizeW->isEnabled());
      CHECK_FALSE(sizeH->isEnabled());
      sizeW->setValue(400);
      const auto aspect = settingsOf(w).outputs[0];
      CHECK(aspect.lockMode == OutputLockMode::AspectRatio);
      CHECK(aspect.windowSize.width() == 400);
      // source aspect = (0.5*1000) / (1.0*500) = 1.0
      CHECK(aspect.windowSize.height() == 400);

      lock->setCurrentIndex(int(OutputLockMode::FullLock));
      CHECK(settingsOf(w).outputs[0].lockMode == OutputLockMode::FullLock);
    }

    SECTION("the screen combo offsets by the Default entry")
    {
      outputButton(w, 0)->click();
      auto* screen = comboStartingWith(w, QStringLiteral("Default"));
      REQUIRE(screen != nullptr);
      REQUIRE(screen->count() >= 2);

      CHECK(settingsOf(w).outputs[0].screenIndex == -1);
      screen->setCurrentIndex(1);
      CHECK(settingsOf(w).outputs[0].screenIndex == 0);
      screen->setCurrentIndex(0);
      CHECK(settingsOf(w).outputs[0].screenIndex == -1);
    }
  });
}

TEST_CASE("WindowSettingsWidget input resolution", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {mapping({0, 0, 1, 1}, {0, 0}, {640, 480})};
    w.setSettings(makeSettings(ws));

    auto* inW = fieldAs<QSpinBox>(w, QStringLiteral("Resolution"), 0);
    auto* inH = fieldAs<QSpinBox>(w, QStringLiteral("Resolution"), 1);
    REQUIRE(inW);
    REQUIRE(inH);
    CHECK(inW->value() == 1920);
    CHECK(inH->value() == 1080);

    inW->setValue(3840);
    inH->setValue(2160);
    CHECK(settingsOf(w).inputWidth == 3840);
    CHECK(settingsOf(w).inputHeight == 2160);
  });
}

TEST_CASE("WindowSettingsWidget add and remove outputs", "[gfx][window][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    WindowSettingsWidget w;

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {mapping({0, 0, 0.5, 1}, {0, 0}, {640, 480})};
    w.setSettings(makeSettings(ws));
    REQUIRE(outputButtonCount(w) == 1);

    // The Add / Remove buttons are the non-checkable ones in the Outputs row.
    QPushButton* add{};
    QPushButton* remove{};
    for(auto* b : w.findChildren<QPushButton*>())
    {
      if(b->isCheckable())
        continue;
      if(b->text().contains(QStringLiteral("Add")))
        add = b;
      else if(b->text().contains(QStringLiteral("Remove")))
        remove = b;
    }
    REQUIRE(add != nullptr);
    REQUIRE(remove != nullptr);

    add->click();
    CHECK(outputButtonCount(w) == 2);
    CHECK(settingsOf(w).outputs.size() == 2);
    CHECK(visiblePreviewWindows() == 2);

    add->click();
    CHECK(outputButtonCount(w) == 3);
    CHECK(visiblePreviewWindows() == 3);

    // Remove acts on the selection.
    outputButton(w, 1)->click();
    remove->click();
    CHECK(outputButtonCount(w) == 2);
    CHECK(settingsOf(w).outputs.size() == 2);
    CHECK(visiblePreviewWindows() == 2);

    // With nothing selected it is a no-op.
    remove->click();
    CHECK(outputButtonCount(w) == 2);
  });
}

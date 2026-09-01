// Gfx/Libav/LibavSettingsWidget.cpp (781 lines, 0%) and Gfx/Libav/LibavPresets.cpp
// (301 lines, 0%): the FFmpeg input/output device's settings UI and the preset
// enumerators the device list offers.
//
// The widget is the only place a Gfx::LibavSettings is assembled, and most of
// its logic is libav introspection: choosing a muxer filters the encoder lists
// to the ones that muxer accepts, choosing an encoder repopulates the pixel /
// sample format lists from that codec's declared configuration, and the
// validation label is recomputed from av_guess_format /
// avcodec_find_encoder_by_name. All of that runs offscreen with no GPU.
//
// Controls are addressed by the label of the QFormLayout row they sit in, as in
// WindowSettingsUi.cpp.

#include <Gfx/Libav/LibavDevice.hpp>
#include <Gfx/Libav/LibavSettingsWidget.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>

#include <set>

#include <catch2/catch_test_macros.hpp>

using namespace Gfx;

namespace Catch
{
template <>
struct StringMaker<QString>
{
  static std::string convert(const QString& s) { return s.toStdString(); }
};
}

namespace
{
std::vector<QWidget*> fieldsForLabel(QWidget& root, const QString& label)
{
  std::vector<QWidget*> out;
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
      if(fieldItem)
        if(auto* w = fieldItem->widget())
          out.push_back(w);
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

LibavSettings settingsOf(const LibavSettingsWidget& w)
{
  return w.getSettings().deviceSpecificSettings.value<LibavSettings>();
}

/// Select `text` in a non-editable combo, or return false if it has no such
/// entry (the available muxers/codecs depend on how libav was built).
bool select(QComboBox* c, const QString& text)
{
  const int i = c->findText(text);
  if(i < 0)
    return false;
  c->setCurrentIndex(i);
  return true;
}

QStringList itemsOf(QComboBox* c)
{
  QStringList l;
  for(int i = 0; i < c->count(); ++i)
    l << c->itemText(i);
  return l;
}
}

TEST_CASE("LibavSettingsWidget direction", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;
    auto* dir = fieldAs<QComboBox>(w, QStringLiteral("Direction"));
    auto* stack = w.findChild<QStackedWidget*>();
    REQUIRE(dir != nullptr);
    REQUIRE(stack != nullptr);
    REQUIRE(dir->count() == 2);

    // Input is the default and page 0.
    CHECK(dir->currentData().toInt() == int(LibavSettings::Input));
    CHECK(stack->currentIndex() == 0);
    CHECK(settingsOf(w).direction == LibavSettings::Input);

    dir->setCurrentIndex(1);
    CHECK(stack->currentIndex() == 1);
    CHECK(settingsOf(w).direction == LibavSettings::Output);

    dir->setCurrentIndex(0);
    CHECK(stack->currentIndex() == 0);
    CHECK(settingsOf(w).direction == LibavSettings::Input);
  });
}

TEST_CASE("LibavSettingsWidget option text parsing", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;
    // Scope the lookup to the input page: both pages carry a "Path / URL" and
    // an "Options" row, and findChildren order is not the page order.
    auto* stack = w.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    auto& inPage = *stack->widget(0);
    auto* opts = fieldAs<QPlainTextEdit>(inPage, QStringLiteral("Options"));
    auto* path = fieldAs<QLineEdit>(inPage, QStringLiteral("Path / URL"));
    REQUIRE(opts != nullptr);
    REQUIRE(path != nullptr);

    path->setText(QStringLiteral("rtsp://example/stream"));
    opts->setPlainText(QStringLiteral(
        "framerate=30\n"
        "  start_number  =  4  \n"
        "\n"
        "no_equals_here\n"
        "a=b=c\n"
        "preset=fast"));

    const auto s = settingsOf(w);
    CHECK(s.path == QStringLiteral("rtsp://example/stream"));
    // Blank lines are skipped, a line with no '=' is dropped, and a line with
    // two '=' does not parse either (it splits into three parts).
    CHECK(s.options.size() == 3);
    REQUIRE(s.options.count("framerate") == 1);
    CHECK(s.options.at("framerate") == QStringLiteral("30"));
    // Both key and value are trimmed.
    REQUIRE(s.options.count("start_number") == 1);
    CHECK(s.options.at("start_number") == QStringLiteral("4"));
    CHECK(s.options.count("no_equals_here") == 0);
    CHECK(s.options.count("a") == 0);
    REQUIRE(s.options.count("preset") == 1);
    CHECK(s.options.at("preset") == QStringLiteral("fast"));

    // And the reverse direction: setSettings renders the map back into the box.
    Device::DeviceSettings ds;
    ds.name = QStringLiteral("ff");
    LibavSettings in;
    in.direction = LibavSettings::Input;
    in.path = QStringLiteral("/tmp/frames_%05d.png");
    in.options["framerate"] = QStringLiteral("25");
    ds.deviceSpecificSettings = QVariant::fromValue(in);
    w.setSettings(ds);

    CHECK(path->text() == QStringLiteral("/tmp/frames_%05d.png"));
    CHECK(opts->toPlainText() == QStringLiteral("framerate=25"));
    const auto back = settingsOf(w);
    CHECK(back.path == in.path);
    REQUIRE(back.options.count("framerate") == 1);
    CHECK(back.options.at("framerate") == QStringLiteral("25"));
  });
}

TEST_CASE("LibavSettingsWidget output fields", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;
    auto* dir = fieldAs<QComboBox>(w, QStringLiteral("Direction"));
    REQUIRE(dir);
    dir->setCurrentIndex(1);

    auto* stack = w.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    auto& outPage = *stack->widget(1);
    auto* path = fieldAs<QLineEdit>(outPage, QStringLiteral("Path / URL"));
    auto* width = fieldAs<QSpinBox>(w, QStringLiteral("Width"));
    auto* height = fieldAs<QSpinBox>(w, QStringLiteral("Height"));
    auto* rate = fieldAs<QSpinBox>(w, QStringLiteral("Rate"));
    auto* chans = fieldAs<QSpinBox>(w, QStringLiteral("Audio Channels"));
    auto* transfer = fieldAs<QComboBox>(w, QStringLiteral("Input Transfer"));
    REQUIRE(path);
    REQUIRE(width);
    REQUIRE(height);
    REQUIRE(rate);
    REQUIRE(chans);
    REQUIRE(transfer);

    path->setText(QStringLiteral("/tmp/out.mp4"));
    width->setValue(1920);
    height->setValue(1080);
    rate->setValue(60);
    chans->setValue(6);

    auto s = settingsOf(w);
    CHECK(s.direction == LibavSettings::Output);
    CHECK(s.path == QStringLiteral("/tmp/out.mp4"));
    CHECK(s.width == 1920);
    CHECK(s.height == 1080);
    CHECK(s.rate == 60);
    CHECK(s.audio_channels == 6);
    // The render format is fixed: the encoder always receives RGBA from the GPU.
    CHECK(s.video_render_pixfmt == QStringLiteral("rgba"));
    // sRGB is the default transfer.
    CHECK(s.input_transfer == 13);

    // Every transfer entry carries its AVColorTransferCharacteristic value.
    const std::vector<int> expected{13, 8, 16, 18, 2};
    REQUIRE(transfer->count() == int(expected.size()));
    for(int i = 0; i < transfer->count(); ++i)
    {
      transfer->setCurrentIndex(i);
      INFO("transfer entry " << i);
      CHECK(transfer->itemData(i).toInt() == expected[i]);
      CHECK(settingsOf(w).input_transfer == expected[i]);
    }
  });
}

TEST_CASE("LibavSettingsWidget encoder-driven format lists", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;
    fieldAs<QComboBox>(w, QStringLiteral("Direction"))->setCurrentIndex(1);

    auto* muxer = fieldAs<QComboBox>(w, QStringLiteral("Muxer"));
    auto* venc = fieldAs<QComboBox>(w, QStringLiteral("Video Encoder"));
    auto* aenc = fieldAs<QComboBox>(w, QStringLiteral("Audio Encoder"));
    auto* pixfmt = fieldAs<QComboBox>(w, QStringLiteral("Pixel Format"));
    auto* smpfmt = fieldAs<QComboBox>(w, QStringLiteral("Sample Format"));
    REQUIRE(muxer);
    REQUIRE(venc);
    REQUIRE(aenc);
    REQUIRE(pixfmt);
    REQUIRE(smpfmt);

    // The lists come from the libav build; a machine without these muxers /
    // encoders has nothing to say here.
    if(muxer->count() <= 1)
      SKIP("no muxers in this libav build");

    SECTION("choosing a muxer narrows the encoder lists")
    {
      const int allVideo = venc->count();
      const int allAudio = aenc->count();
      REQUIRE(allVideo > 1);
      REQUIRE(allAudio > 1);

      const auto allV = itemsOf(venc);
      const auto allA = itemsOf(aenc);

      if(!select(muxer, QStringLiteral("mp4")))
        SKIP("no mp4 muxer");

      // mp4 accepts a small subset of the encoders libav can build.
      CHECK(venc->count() < allVideo);
      CHECK(aenc->count() < allAudio);
      CHECK(venc->count() > 1);
      const auto v = itemsOf(venc);
      const auto a = itemsOf(aenc);
      // The empty "unset" entry is always kept as item 0...
      CHECK(v.front().isEmpty());
      CHECK(a.front().isEmpty());
      // ...and what remains is a subset of what the build offers.
      for(auto& e : v)
        CHECK(allV.contains(e));
      for(auto& e : a)
        CHECK(allA.contains(e));
      // Clearing the muxer removes the constraint, so the full lists come
      // back — matching validateOutput, which treats an empty muxer as
      // unconstrained.
      muxer->setCurrentIndex(0);
      CHECK(venc->count() == allVideo);
      CHECK(aenc->count() == allAudio);
      CHECK(itemsOf(venc) == allV);
      CHECK(itemsOf(aenc) == allA);
    }

    SECTION("choosing a video encoder repopulates the pixel formats")
    {
      if(!select(venc, QStringLiteral("mjpeg")))
        SKIP("no mjpeg encoder");
      const auto fmts = itemsOf(pixfmt);
      INFO("pixel formats: " << fmts.join(",").toStdString());
      CHECK(fmts.size() > 1);
      CHECK(fmts.front().isEmpty());
      // mjpeg declares only YUVJ/YUV planar formats — never RGBA.
      CHECK_FALSE(fmts.contains(QStringLiteral("rgba")));
      // The previous selection was the empty entry, and it still exists, so it
      // is kept rather than replaced by the codec's first format.
      CHECK(pixfmt->currentIndex() == 0);
      CHECK(settingsOf(w).video_converted_pixfmt.isEmpty());

      // An explicit choice survives a switch to another encoder that also
      // declares that format...
      REQUIRE(select(pixfmt, QStringLiteral("yuv420p")));
      CHECK(settingsOf(w).video_converted_pixfmt == QStringLiteral("yuv420p"));
      if(select(venc, QStringLiteral("mpeg4")))
      {
        CHECK(itemsOf(pixfmt).contains(QStringLiteral("yuv420p")));
        CHECK(pixfmt->currentText() == QStringLiteral("yuv420p"));
      }
      // ...and clearing the encoder empties the list entirely.
      venc->setCurrentIndex(0);
      CHECK(pixfmt->count() == 1);
      CHECK(pixfmt->itemText(0).isEmpty());
    }

    SECTION("an empty video encoder leaves only the unset entry")
    {
      select(venc, QStringLiteral("mjpeg"));
      REQUIRE(pixfmt->count() > 1);
      venc->setCurrentIndex(0);
      CHECK(pixfmt->count() == 1);
      CHECK(pixfmt->itemText(0).isEmpty());
    }

    SECTION("choosing an audio encoder repopulates the sample formats")
    {
      if(!select(aenc, QStringLiteral("flac")))
        SKIP("no flac encoder");
      const auto fmts = itemsOf(smpfmt);
      INFO("sample formats: " << fmts.join(",").toStdString());
      CHECK(fmts.size() > 1);
      CHECK(fmts.front().isEmpty());
      // FLAC is integer-only: no float sample format is offered.
      CHECK_FALSE(fmts.contains(QStringLiteral("flt")));
      CHECK_FALSE(fmts.contains(QStringLiteral("fltp")));
      CHECK(smpfmt->currentIndex() == 0);
      REQUIRE(select(smpfmt, fmts[1]));
      CHECK(settingsOf(w).audio_converted_smpfmt == fmts[1]);
    }

    SECTION("the validation label reports a good configuration")
    {
      QLabel* validation{};
      for(auto* l : w.findChildren<QLabel*>())
        if(l->wordWrap())
          validation = l;
      REQUIRE(validation != nullptr);
      CHECK(validation->text().isEmpty());

      if(!select(muxer, QStringLiteral("mp4")))
        SKIP("no mp4 muxer");
      INFO("validation: " << validation->text().toStdString());
      CHECK(validation->text().contains(QStringLiteral("valid")));
      CHECK_FALSE(validation->text().contains(QStringLiteral("not found")));
    }
  });
}

TEST_CASE("LibavSettingsWidget options dialog", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;
    fieldAs<QComboBox>(w, QStringLiteral("Direction"))->setCurrentIndex(1);

    auto* muxer = fieldAs<QComboBox>(w, QStringLiteral("Muxer"));
    auto* venc = fieldAs<QComboBox>(w, QStringLiteral("Video Encoder"));
    REQUIRE(muxer);
    REQUIRE(venc);

    QPushButton* show{};
    for(auto* b : w.findChildren<QPushButton*>())
      if(b->text().contains(QStringLiteral("options")))
        show = b;
    REQUIRE(show != nullptr);

    const auto dialogs = [] {
      int n = 0;
      for(auto* w : QApplication::topLevelWidgets())
        if(qobject_cast<QDialog*>(w))
          ++n;
      return n;
    };
    const int before = dialogs();

    // With nothing chosen the dialog still opens, with only the general tab.
    show->click();
    CHECK(dialogs() == before + 1);
    for(auto* t : QApplication::topLevelWidgets())
      if(auto* d = qobject_cast<QDialog*>(t))
        d->close();
    QApplication::processEvents();

    // With a muxer and an encoder chosen it gains their private-option tabs.
    if(select(muxer, QStringLiteral("mp4")) && select(venc, QStringLiteral("mjpeg")))
    {
      show->click();
      QDialog* dlg{};
      for(auto* t : QApplication::topLevelWidgets())
        if(auto* d = qobject_cast<QDialog*>(t))
          dlg = d;
      REQUIRE(dlg != nullptr);
      // At least the "General" tab, plus whichever of the two declare a
      // private AVClass.
      CHECK(dlg->findChildren<QTableWidget*>().size() >= 1);
      dlg->close();
      QApplication::processEvents();
    }
  });
}

TEST_CASE("LibavSettingsWidget settings round-trip", "[gfx][libav][settingswidget]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    LibavSettingsWidget w;

    Device::DeviceSettings ds;
    // A name that needs sanitizing on the way in.
    ds.name = QStringLiteral("  My FFmpeg Out  ");
    LibavSettings out;
    out.direction = LibavSettings::Output;
    out.path = QStringLiteral("/tmp/roundtrip.mkv");
    out.width = 640;
    out.height = 360;
    out.rate = 24;
    out.audio_channels = 1;
    out.input_transfer = 16;
    out.options["crf"] = QStringLiteral("23");
    ds.deviceSpecificSettings = QVariant::fromValue(out);

    w.setSettings(ds);

    const auto got = w.getSettings();
    CHECK(got.protocol == LibavProtocolFactory::static_concreteKey());
    // The device name is trimmed and sanitized into an address fragment.
    CHECK_FALSE(got.name.isEmpty());
    CHECK(got.name == got.name.trimmed());
    CHECK(got.name.contains(QStringLiteral("FFmpeg")));

    const auto s = got.deviceSpecificSettings.value<LibavSettings>();
    CHECK(s.direction == LibavSettings::Output);
    CHECK(s.path == QStringLiteral("/tmp/roundtrip.mkv"));
    CHECK(s.width == 640);
    CHECK(s.height == 360);
    CHECK(s.rate == 24);
    CHECK(s.audio_channels == 1);
    CHECK(s.input_transfer == 16);
    REQUIRE(s.options.count("crf") == 1);
    CHECK(s.options.at("crf") == QStringLiteral("23"));

    // toOutputSettings carries every field the encoder needs.
    const auto os = s.toOutputSettings();
    CHECK(os.path == s.path);
    CHECK(os.width == s.width);
    CHECK(os.height == s.height);
    CHECK(os.rate == s.rate);
    CHECK(os.audio_channels == s.audio_channels);
    CHECK(os.input_transfer == s.input_transfer);
    CHECK(os.video_render_pixfmt == QStringLiteral("rgba"));
    REQUIRE(os.options.count("crf") == 1);
  });
}

TEST_CASE("Libav preset enumerators", "[gfx][libav][presets]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    LibavProtocolFactory factory;
    auto enums = factory.getEnumerators(doc->context());

    // The enumerator set is built from what libav can actually do here, so an
    // exotic build may offer none; what must hold is that every preset it does
    // offer is a usable device settings record.
    int total = 0;
    std::set<QString> categories;
    for(auto& [category, e] : enums)
    {
      REQUIRE(e != nullptr);
      categories.insert(category);
      e->enumerate([&](const QString& label, const Device::DeviceSettings& s) {
        ++total;
        INFO("preset " << label.toStdString());
        CHECK_FALSE(label.isEmpty());
        CHECK(s.protocol == LibavProtocolFactory::static_concreteKey());
        CHECK_FALSE(s.name.isEmpty());
        REQUIRE(s.deviceSpecificSettings.canConvert<LibavSettings>());
        const auto ls = s.deviceSpecificSettings.value<LibavSettings>();
        CHECK_FALSE(ls.path.isEmpty());
        if(ls.direction == LibavSettings::Output)
        {
          // Every output preset is fully specified: an output the encoder
          // cannot configure is worse than no preset at all.
          CHECK(ls.width > 0);
          CHECK(ls.height > 0);
          CHECK(ls.rate > 0);
          CHECK_FALSE(ls.muxer.isEmpty());
          CHECK(ls.video_render_pixfmt == QStringLiteral("rgba"));
        }
      });
      // Empty enumerators must be dropped rather than published.
      int n = 0;
      e->enumerate([&](const QString&, const Device::DeviceSettings&) { ++n; });
      CHECK(n > 0);
    }

    INFO("categories: " << categories.size() << " presets: " << total);
    CHECK(total > 0);
    for(auto& c : categories)
      CHECK_FALSE(c.isEmpty());

    for(auto& [category, e] : enums)
      delete e;
  });
}

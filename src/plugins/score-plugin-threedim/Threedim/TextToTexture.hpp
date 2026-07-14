#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>

#include <string>

namespace Threedim
{

// Rasterize a text string into an RGBA texture via QPainter. Pipes into
// any node that consumes halp::gpu_texture — most commonly
// MaterialOverride (to show text on a mesh's base-color slot) or
// Instancer / a billboard renderer (for text sprites).
//
// Re-renders only when a control (text / font / size / color / canvas
// dimensions) changes — the update() hooks on each port fire recreate().
class TextToTexture
{
public:
  halp_meta(name, "Text to Texture")
  halp_meta(category, "Visuals/3D/Text")
  halp_meta(c_name, "text_to_texture")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/text-to-texture.html")
  halp_meta(uuid, "5d3a9b2f-7e6c-4a8d-b1f4-9c2e3d5a7b8f")

  struct ins
  {
    struct : halp::lineedit<"Text", "Hello, world">
    {
      void update(TextToTexture& self) { self.recreate(); }
    } text;
    struct : halp::lineedit<"Font family", "Sans">
    {
      void update(TextToTexture& self) { self.recreate(); }
    } font_family;
    struct : halp::spinbox_i32<"Font size", halp::irange{4, 512, 64}>
    {
      void update(TextToTexture& self) { self.recreate(); }
    } font_size;
    struct : halp::toggle<"Bold">
    {
      void update(TextToTexture& self) { self.recreate(); }
    } bold;
    struct : halp::toggle<"Italic">
    {
      void update(TextToTexture& self) { self.recreate(); }
    } italic;

    struct : halp::spinbox_i32<"Canvas width", halp::irange{16, 4096, 1024}>
    {
      void update(TextToTexture& self) { self.recreate(); }
    } canvas_w;
    struct : halp::spinbox_i32<"Canvas height", halp::irange{16, 4096, 256}>
    {
      void update(TextToTexture& self) { self.recreate(); }
    } canvas_h;

    // Colors are vec4 (r, g, b, a) in [0, 1]. A transparent background
    // is the useful default — drop on any mesh and you see only the
    // glyphs.
    struct : halp::hslider_f32<"Text R", halp::range{0., 1., 1.}> { void update(TextToTexture& s) { s.recreate(); } } fg_r;
    struct : halp::hslider_f32<"Text G", halp::range{0., 1., 1.}> { void update(TextToTexture& s) { s.recreate(); } } fg_g;
    struct : halp::hslider_f32<"Text B", halp::range{0., 1., 1.}> { void update(TextToTexture& s) { s.recreate(); } } fg_b;
    struct : halp::hslider_f32<"Text A", halp::range{0., 1., 1.}> { void update(TextToTexture& s) { s.recreate(); } } fg_a;
    struct : halp::hslider_f32<"BG R", halp::range{0., 1., 0.}> { void update(TextToTexture& s) { s.recreate(); } } bg_r;
    struct : halp::hslider_f32<"BG G", halp::range{0., 1., 0.}> { void update(TextToTexture& s) { s.recreate(); } } bg_g;
    struct : halp::hslider_f32<"BG B", halp::range{0., 1., 0.}> { void update(TextToTexture& s) { s.recreate(); } } bg_b;
    struct : halp::hslider_f32<"BG A", halp::range{0., 1., 0.}> { void update(TextToTexture& s) { s.recreate(); } } bg_a;

    // Text alignment inside the canvas: 0=left, 1=center, 2=right for h;
    // 0=top, 1=center, 2=bottom for v.
    struct : halp::spinbox_i32<"H align", halp::irange{0, 2, 1}>
    { void update(TextToTexture& s) { s.recreate(); } } h_align;
    struct : halp::spinbox_i32<"V align", halp::irange{0, 2, 1}>
    { void update(TextToTexture& s) { s.recreate(); } } v_align;
  } inputs;

  struct
  {
    halp::texture_output<"Output", halp::rgba_texture> main;
  } outputs;

  void recreate()
  {
    const int w = inputs.canvas_w.value;
    const int h = inputs.canvas_h.value;
    if(w <= 0 || h <= 0)
      return;

    // Qt renders with premultiplied alpha; we output straight RGBA8.
    // QImage::Format_RGBA8888 is non-premultiplied and matches what
    // gpu_texture expects when upload-bound as RGBA8.
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor::fromRgbF(
        inputs.bg_r.value, inputs.bg_g.value, inputs.bg_b.value,
        inputs.bg_a.value));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QFont f(QString::fromStdString(inputs.font_family.value));
    f.setPixelSize(inputs.font_size.value);
    f.setBold(inputs.bold.value);
    f.setItalic(inputs.italic.value);
    p.setFont(f);
    p.setPen(QColor::fromRgbF(
        inputs.fg_r.value, inputs.fg_g.value, inputs.fg_b.value,
        inputs.fg_a.value));

    int flags = 0;
    switch(inputs.h_align.value)
    {
      case 0: flags |= Qt::AlignLeft; break;
      case 2: flags |= Qt::AlignRight; break;
      default: flags |= Qt::AlignHCenter;
    }
    switch(inputs.v_align.value)
    {
      case 0: flags |= Qt::AlignTop; break;
      case 2: flags |= Qt::AlignBottom; break;
      default: flags |= Qt::AlignVCenter;
    }
    flags |= Qt::TextWordWrap;

    p.drawText(
        QRect(0, 0, w, h), flags,
        QString::fromStdString(inputs.text.value));
    p.end();

    outputs.main.create(w, h);
    std::memcpy(outputs.main.texture.bytes, img.constBits(), std::size_t(w) * h * 4);
    outputs.main.upload();
  }
};

}

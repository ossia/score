#pragma once
#include <LV2/EffectModel.hpp>

#include <QDialog>

#include <verdigris>

namespace LV2
{

class Window final : public QDialog
{
  W_OBJECT(Window)
public:
  Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent);

  ~Window() override;
  // TODO void resize(int w, int h);
  static bool is_resizable(LilvWorld* world, const LilvUI& ui);

private:
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  const Model& m_model;
  QWidget* m_widget{};
};

using LayerFactory
    = Process::EffectLayerFactory_T<Model, Process::DefaultEffectItem, Window>;
}

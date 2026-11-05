// The auto range of the Point2D View, while the points it is fed change.
//
// The auto range grows to every point received, and a double click on the
// view fits it again to the points it shows, so that a range stretched by
// points far away comes back to the ones near while playing. The view is
// built by its layer factory as in a document, fed through its inlet's
// execution values as playback feeds it, and double clicked.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <UI/2DView.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
constexpr auto point2d_uuid = "898078f0-b825-4c29-a997-dc856e351a97";

ossia::value xy(std::initializer_list<float> v)
{
  std::vector<ossia::value> l;
  for(float f : v)
    l.push_back(f);
  return l;
}

void doubleClick(uo::Point2DView::Layer& item)
{
  for(int i = 0; i < 2; i++)
  {
    QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMousePress};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(Qt::LeftButton);
    ev.setScreenPos(QPoint{100, 100});
    ev.setPos(QPointF{100, 100});
    item.mousePressEvent(&ev);
  }
}
}

TEST_CASE(
    "the Point2D View's auto range comes back to the points shown",
    "[nodes][views][monitoring][point2d]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& layers = ctx.interfaces<Process::LayerFactoryList>();
    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 640., 320.}};
    scene.addItem(root);
    auto& stack = doc->commandStack();

    //! A Point2D View in the document and its layer
    struct View
    {
      Process::ProcessModel* proc{};
      uo::Point2DView::Layer* layer{};
      Process::ControlInlet* control(int i) const
      {
        return static_cast<Process::ControlInlet*>(proc->inlets()[i]);
      }
      void feed(std::initializer_list<float> v) const
      {
        control(0)->executionValueChanged(xy(v));
      }
    };
    auto make = [&]() -> View {
      auto* proc = score::test::add_process(*doc, point2d_uuid, {});
      if(!proc)
        return {};
      REQUIRE(proc->inlets().size() >= 11);
      auto* lf = layers.findDefaultFactory(*proc);
      REQUIRE(lf);
      auto* view = lf->makeLayerView(*proc, pctx, root);
      REQUIRE(view);
      return {proc, static_cast<uo::Point2DView::Layer*>(view)};
    };

    {
      INFO("a double click fits the auto range to the points shown, while playing");
      auto v = make();
      if(!v.proc)
        SKIP("the Point2D View is not built");
      auto& layer = *v.layer;
      v.control(9)->setValue(true);
      v.proc->startExecution();

      v.feed({-100.f, -100.f, 100.f, 100.f});
      auto r = layer.range();
      CHECK(r.x0 < -100.);
      CHECK(r.x1 > 100.);

      // Points near: the range still spans the far ones
      v.feed({0.f, 0.f, 1.f, 1.f});
      r = layer.range();
      CHECK(r.x0 < -100.);
      CHECK(r.x1 > 100.);

      const int commands = stack.size();
      doubleClick(layer);
      r = layer.range();
      CHECK(r.x0 == Catch::Approx(-0.05));
      CHECK(r.x1 == Catch::Approx(1.05));
      CHECK(r.y0 == Catch::Approx(-0.05));
      CHECK(r.y1 == Catch::Approx(1.05));
      // The view's own state: the document is not edited
      CHECK(stack.size() == commands);

      // And it follows the points received from there
      v.feed({0.f, 0.f, 3.f, 2.f});
      r = layer.range();
      CHECK(r.x0 == Catch::Approx(-0.15));
      CHECK(r.x1 == Catch::Approx(3.15));
      CHECK(r.y1 == Catch::Approx(2.1));
    }

    {
      INFO("an auto range turned on fits the points shown, not those received before");
      auto v = make();
      v.control(9)->setValue(false);
      v.proc->startExecution();
      v.feed({-100.f, -100.f, 100.f, 100.f});
      v.feed({0.f, 0.f, 1.f, 1.f});

      v.control(9)->setValue(true);
      const auto r = v.layer->range();
      CHECK(r.x0 == Catch::Approx(-0.05));
      CHECK(r.x1 == Catch::Approx(1.05));
    }

    {
      INFO("without the auto range, a double click sets Min and Max, as one edit");
      auto v = make();
      v.control(9)->setValue(false);
      v.proc->startExecution();
      v.feed({2.f, 4.f, 12.f, 24.f});

      const int commands = stack.size();
      doubleClick(*v.layer);
      CHECK(stack.size() == commands + 1);
      CHECK(ossia::convert<float>(v.control(1)->value()) == Catch::Approx(1.5f));
      CHECK(ossia::convert<float>(v.control(3)->value()) == Catch::Approx(12.5f));
      const auto r = v.layer->range();
      CHECK(r.x0 == Catch::Approx(1.5));
      CHECK(r.x1 == Catch::Approx(12.5));
      CHECK(r.y0 == Catch::Approx(3.));
      CHECK(r.y1 == Catch::Approx(25.));
    }

    {
      INFO("far from zero, a single point or a tight cluster still gets a range");
      for(auto pts : {std::vector<float>{12345.f, -20000.f},
                      std::vector<float>{2e6f, 2e6f, 2e6f, 2e6f},
                      std::vector<float>{-3e7f, 5e6f}})
      {
        auto v = make();
        v.control(9)->setValue(false);
        v.proc->startExecution();
        v.control(0)->executionValueChanged([&] {
          std::vector<ossia::value> l;
          for(float f : pts)
            l.push_back(f);
          return ossia::value{l};
        }());

        const int commands = stack.size();
        doubleClick(*v.layer);
        CHECK(stack.size() == commands + 1);
        const float x0 = ossia::convert<float>(v.control(1)->value());
        const float y0 = ossia::convert<float>(v.control(2)->value());
        const float x1 = ossia::convert<float>(v.control(3)->value());
        const float y1 = ossia::convert<float>(v.control(4)->value());
        CHECK(x0 < pts[0]);
        CHECK(x1 > pts[0]);
        CHECK(y0 < pts[1]);
        CHECK(y1 > pts[1]);
        CHECK(uo::axisUsable(x0, x1));
        CHECK(uo::axisUsable(y0, y1));

        // The auto range shows it too
        v.control(9)->setValue(true);
        const auto r = v.layer->range();
        CHECK(uo::axisUsable(r.x0, r.x1));
        CHECK(uo::axisUsable(r.y0, r.y1));
      }
    }

    delete root;
    QApplication::processEvents();
  });
}

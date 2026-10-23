// The list, note matrix and channel mask widgets: their value, their text
// form, the clicks that edit them, and the layouts that follow a list's size.

#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/widgets/QGraphicsChannelMask.hpp>
#include <score/graphics/widgets/QGraphicsNoteMatrix.hpp>
#include <score/graphics/widgets/QGraphicsValueList.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <score_test/App.hpp>

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_all.hpp>

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

void click(Scene& scene, QGraphicsItem& item, QPointF at)
{
  for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(
        type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
    ev.setScenePos(item.mapToScene(at));
    ev.setPos(at);
    scene.sendEvent(&item, &ev);
  }
}

std::vector<int> ints(const ossia::value& v)
{
  std::vector<int> res;
  for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
    res.push_back(ossia::convert<int>(e));
  return res;
}

std::vector<std::vector<int>> rows(const ossia::value& v)
{
  std::vector<std::vector<int>> res;
  for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
    res.push_back(ints(e));
  return res;
}

score::ValueListColumn semitones()
{
  return {.name = "Semitones", .min = -12, .max = 12, .init = 0};
}
}

TEST_CASE("Value list: one column of numbers", "[graphics][value_list]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsValueList list{nullptr};
    list.setColumns({semitones()});
    list.setValue(std::vector<ossia::value>{4, 7});
    CHECK(ints(list.value()) == std::vector<int>{4, 7});
    CHECK(list.toText() == "4 7");

    // Clamped to the column, rounded
    list.setValue(std::vector<ossia::value>{40, -3.4f});
    CHECK(ints(list.value()) == std::vector<int>{12, -3});

    CHECK(list.setText("4, 7  -12"));
    CHECK(ints(list.value()) == std::vector<int>{4, 7, -12});
    // Text that does not parse leaves the list as it is
    CHECK(!list.setText("4 x"));
    CHECK(ints(list.value()) == std::vector<int>{4, 7, -12});
    CHECK(list.setText(""));
    CHECK(list.value() == ossia::value{std::vector<ossia::value>{}});
  });
}

TEST_CASE("Value list: bounded count", "[graphics][value_list]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsValueList list{nullptr};
    list.setColumns({semitones()});
    list.setCountRange(1, 3);
    CHECK(!list.setText(""));
    CHECK(!list.setText("1 2 3 4"));
    CHECK(list.setText("1 2 3"));
    // A longer value keeps its first elements
    list.setValue(std::vector<ossia::value>{1, 2, 3, 4, 5});
    CHECK(ints(list.value()) == std::vector<int>{1, 2, 3});
  });
}

TEST_CASE("Value list: + appends a copy of the last element", "[graphics][value_list]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto list = new score::QGraphicsValueList{nullptr};
    scene.addItem(list);
    list->setColumns({semitones()});
    list->setItemsPerLine(4);
    list->setValue(std::vector<ossia::value>{4, 7});

    int edits = 0;
    QObject::connect(list, &score::QGraphicsValueList::edited, [&] { edits++; });
    std::vector<QSizeF> sizes;
    QObject::connect(
        list, &score::QGraphicsValueList::sizeChanged, [&](QSizeF sz) { sizes.push_back(sz); });

    // The buttons follow the last cell: 2 cells of 40 and their gaps
    const QRectF before = list->boundingRect();
    click(scene, *list, {2 * 42. + 7., 10.});
    QCoreApplication::processEvents();
    CHECK(edits == 1);
    CHECK(ints(list->value()) == std::vector<int>{4, 7, 7});

    // A fifth element starts a second line: the item grows
    list->setValue(std::vector<ossia::value>{1, 2, 3, 4, 5});
    CHECK(list->boundingRect().height() > before.height());
    CHECK(!sizes.empty());

    // - removes the last element
    const QRectF minus{1 * 42. + 14. + 2., 22., 14., 20.};
    click(scene, *list, minus.center());
    QCoreApplication::processEvents();
    CHECK(ints(list->value()) == std::vector<int>{1, 2, 3, 4});
  });
}

TEST_CASE("Value list: rows of several columns", "[graphics][value_list]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsValueList list{nullptr};
    list.setColumns(
        {{.name = "Note", .min = 0, .max = 127, .init = 60},
         {.name = "Mode", .choices = {"Off", "Up", "Down"}},
         {.name = "Gain", .min = 0., .max = 1., .init = 0.5, .integral = false}});
    list.setValue(
        std::vector<ossia::value>{
            std::vector<ossia::value>{62, 1, 0.25f}, std::vector<ossia::value>{200, 9, 2.f}});
    const auto v = ossia::convert<std::vector<ossia::value>>(list.value());
    REQUIRE(v.size() == 2);
    CHECK(ints(v[0]) == std::vector<int>{62, 1, 0});
    CHECK(ossia::convert<float>(ossia::convert<std::vector<ossia::value>>(v[0])[2]) == 0.25f);
    CHECK(ints(v[1]) == std::vector<int>{127, 2, 1});

    CHECK(list.toText() == "62 Up 0.25, 127 Down 1");
    // Choices by name or index; missing trailing columns take their default
    CHECK(list.setText("1 down 0, 2 0"));
    CHECK(
        rows(list.value()) == std::vector<std::vector<int>>{{1, 2, 0}, {2, 0, 0}});
    const auto second = ossia::convert<std::vector<ossia::value>>(
        ossia::convert<std::vector<ossia::value>>(list.value())[1]);
    CHECK(ossia::convert<float>(second[2]) == 0.5f);
  });
}

TEST_CASE("Value list: the layouts around it follow its size", "[graphics][value_list]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto lay = new score::GraphicsVBoxLayout{nullptr};
    scene.addItem(lay);
    auto box = new score::EmptyRectItem{lay};
    auto list = new score::QGraphicsValueList{box};
    list->setColumns({semitones()});
    list->setItemsPerLine(2);
    list->setValue(std::vector<ossia::value>{1});
    box->fitChildrenRect();
    auto below = new score::EmptyRectItem{lay};
    below->setRect({0., 0., 20., 20.});
    lay->layout();
    const auto& cld = lay->childrenBoundingRect();
    lay->setRect({0., 0., cld.right(), cld.bottom()});
    lay->setFitMargins(0., 0.);

    const qreal y0 = below->y();
    const qreal h0 = lay->boundingRect().height();
    list->setValue(std::vector<ossia::value>{1, 2, 3, 4, 5});
    score::GraphicsLayout::relayoutAround(list);
    CHECK(below->y() > y0);
    CHECK(lay->boundingRect().height() > h0);
  });
}

TEST_CASE("Note matrix: clicks, mutes and text", "[graphics][note_matrix]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto m = new score::QGraphicsNoteMatrix{nullptr};
    scene.addItem(m);
    CHECK(ints(m->value()) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});

    int released = 0;
    QObject::connect(
        m, &score::QGraphicsNoteMatrix::sliderReleased, [&] { released++; });
    auto cell = [](int col, int row) {
      return QPointF{18. + col * 14. + 7., 11. + (12 - row) * 10. + 5.};
    };

    // The root to the octave above
    click(scene, *m, cell(0, 12));
    CHECK(ints(m->value())[0] == 12);
    // A click on the lit cell mutes the degree
    click(scene, *m, cell(3, 3));
    CHECK(ints(m->value())[3] == -1);
    CHECK(released == 2);
    CHECK(m->toText() == "12 1 2 x 4 5 6 7 8 9 10 11");

    CHECK(m->setText("0 11 10 9 8 7 6 5 4 3 2 1"));
    CHECK(ints(m->value()) == std::vector<int>{0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
    CHECK(!m->setText("0 1 2"));
    CHECK(!m->setText("0 1 2 3 4 5 6 7 8 9 10 13"));

    // Out of range values are muted degrees
    m->setValue(std::vector<ossia::value>{-5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 40});
    CHECK(ints(m->value())[0] == -1);
    CHECK(ints(m->value())[11] == -1);
  });
}

TEST_CASE("Channel mask: clicks, drags and text", "[graphics][channel_mask]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto m = new score::QGraphicsChannelMask{nullptr};
    scene.addItem(m);
    m->setValue(std::vector<ossia::value>(16, ossia::value{true}));
    CHECK(m->toText() == "1-16");

    auto cell = [](int channel) {
      const int c = channel - 1;
      return QPointF{(c % 8) * 17. + 8., (c / 8) * 15. + 7.};
    };
    click(scene, *m, cell(3));
    CHECK(m->toText() == "1-2, 4-16");
    click(scene, *m, cell(3));
    CHECK(m->toText() == "1-16");

    CHECK(m->setText("1-4, 10"));
    const auto v = ints(m->value());
    REQUIRE(v.size() == 16);
    CHECK(v == std::vector<int>{1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0});
    CHECK(!m->setText("0-4"));
    CHECK(!m->setText("17"));
    CHECK(m->toText() == "1-4, 10");
  });
}

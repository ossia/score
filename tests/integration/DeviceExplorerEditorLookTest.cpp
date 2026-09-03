// The value editors of a Device Explorer row, rendered.
//
// Two things can only be judged by rendering: an editor taller than its row,
// and one that does not paint its whole cell. Both are asserted here, and
// every editor is written out as a PNG under $SCORE_TEST_SHOT_DIR.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Keyboard.hpp>

#include <State/Widgets/Values/ExpandableTextEdit.hpp>

#include <State/ValueConversion.hpp>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerDelegate.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
QString shotDir()
{
  auto d = qEnvironmentVariable("SCORE_TEST_SHOT_DIR");
  if(d.isEmpty())
    d = QDir::currentPath() + "/shots";
  QDir{}.mkpath(d);
  return d;
}

Device::ProtocolFactory* oscFactory(const score::GUIApplicationContext& ctx)
{
  for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
    if(f.prettyName() == "OSC")
      return &f;
  return nullptr;
}

Device::AddressSettings
addr(const QString& name, ossia::value v, ossia::unit_t u = {})
{
  Device::AddressSettings as;
  as.name = name;
  as.value = std::move(v);
  as.unit = u;
  as.ioType = ossia::access_mode::BI;
  return as;
}

//! The tree as the panel builds it: same model, same delegate, real rows.
struct Tree
{
  Explorer::DeviceExplorerModel* model{};
  std::unique_ptr<QTreeView> view;

  explicit Tree(const score::GUIApplicationContext& ctx)
  {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* fact = oscFactory(ctx);
    REQUIRE(fact != nullptr);

    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    model = &devplug.explorer();

    auto settings = fact->defaultSettings();
    settings.name = "dev";
    Device::Node dev{settings, nullptr};
    dev.push_back(Device::Node{addr("int", 5), &dev});
    dev.push_back(Device::Node{addr("float", 0.5f), &dev});
    dev.push_back(Device::Node{addr("text", std::string{"hello"}), &dev});
    dev.push_back(
        Device::Node{addr("prose", std::string{"one\ntwo\nthree"}), &dev});
    dev.push_back(Device::Node{
        addr("tint", ossia::vec4f{{1.f, 0.5f, 0.f, 1.f}},
             ossia::unit_t{ossia::rgba_u{}}),
        &dev});
    dev.push_back(Device::Node{
        addr("pos", ossia::vec2f{{0.25f, -0.5f}},
             ossia::unit_t{ossia::cartesian_2d_u{}}),
        &dev});
    dev.push_back(Device::Node{
        addr("lst", ossia::value{std::vector<ossia::value>{1, 2, 3}}), &dev});
    dev.push_back(Device::Node{addr("flag", true), &dev});
    dev.push_back(Device::Node{addr("bang", ossia::impulse{}), &dev});
    dev.push_back(Device::Node{
        addr("blob",
             std::string{QByteArray::fromHex(
                            "89504e470d0a1a0a0000000d494844520000010000000100")
                            .toStdString()}),
        &dev});

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(dev)});

    view = std::make_unique<QTreeView>();
    view->setModel(model);
    view->setItemDelegate(new Explorer::DeviceExplorerDelegate{view.get()});
    view->setUniformRowHeights(true);
    view->resize(520, 340);
    view->expandAll();
    view->show();
    QApplication::processEvents();
  }

  QModelIndex valueIndex(int child) const
  {
    const auto devIdx = model->index(model->rootNode().childCount() - 1, 0, {});
    return model->index(child, (int)Explorer::Column::Value, devIdx);
  }
};

//! The editor the delegate put in the row, whatever kind it is.
QWidget* openEditor(QTreeView& v, const QModelIndex& idx)
{
  v.scrollTo(idx);
  v.setCurrentIndex(idx);
  v.edit(idx);
  QApplication::processEvents();

  for(auto* w : v.viewport()->findChildren<QWidget*>())
    if(w->isVisible() && w->parent() == v.viewport())
      return w;
  return nullptr;
}
}

TEST_CASE("a value editor fits the row it is opened in", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    static const char* names[]
        = {"int",  "float", "text", "prose", "tint",
           "pos",  "lst",   "flag", "bang",  "blob"};

    for(int i = 0; i < 10; i++)
    {
      // The bang has no editor at all: see the case below.
      if(i == 8)
        continue;

      const auto idx = t.valueIndex(i);
      REQUIRE(idx.isValid());

      const int rowH = t.view->visualRect(idx).height();
      REQUIRE(rowH > 0);

      auto* ed = openEditor(*t.view, idx);
      INFO(names[i]);
      REQUIRE(ed != nullptr);

      t.view->grab().save(
          shotDir() + QStringLiteral("/editor-%1-%2.png")
                          .arg(i, 2, 10, QChar('0'))
                          .arg(names[i]));

      // Taller than its row means it paints over the rows around it; the point
      // size is what the frames and arrows cost.
      WARN(
          names[i] << ": " << ed->height() << "px in " << rowH
                   << "px row, font " << ed->font().pointSizeF() << "pt");
      CHECK(ed->height() <= rowH);

      // A value the row cannot hold opens its popup on load; take it away
      // before the next row, or it sits over the tree.
      while(auto* p = QApplication::activePopupWidget())
      {
        p->close();
        QApplication::processEvents();
      }

      t.view->closePersistentEditor(idx);
      QApplication::processEvents();
    }
  });
}

// The Name column gets Qt's own editor, which never passes through
// make_value_widget and so keeps a dialog field's frame and padding.
TEST_CASE("the name editor is as legible as the value editors",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto devIdx
        = t.model->index(t.model->rootNode().childCount() - 1, 0, {});
    const auto idx = t.model->index(0, (int)Explorer::Column::Name, devIdx);
    REQUIRE(idx.isValid());

    const int rowH = t.view->visualRect(idx).height();
    auto* ed = openEditor(*t.view, idx);
    REQUIRE(ed != nullptr);

    t.view->grab().save(shotDir() + QStringLiteral("/editor-name.png"));

    WARN("name: " << ed->height() << "px in " << rowH << "px row, font "
                  << ed->font().pointSizeF() << "pt");
    CHECK(ed->height() <= rowH);
    CHECK(ed->font().pointSizeF() >= 8.0);
  });
}

// A boolean and an impulse are one click, not a double-click into an editor:
// the model offers a check state, and the delegate draws the button itself.
TEST_CASE("bool and impulse are always live in the row",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto flag = t.valueIndex(7);
    REQUIRE(flag.isValid());
    CHECK(t.model->flags(flag).testFlag(Qt::ItemIsUserCheckable));
    CHECK(flag.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked);
    // The check box is the value; "true" beside it would say it twice.
    CHECK(flag.data(Qt::DisplayRole).toString().isEmpty());

    // Ticking it writes the value, with no editor involved.
    CHECK(t.model->setData(flag, Qt::Unchecked, Qt::CheckStateRole));
    CHECK(flag.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Unchecked);

    // The impulse row is painted as a button; double-clicking it must not open
    // a second, identical one on top.
    const auto bang = t.valueIndex(8);
    REQUIRE(bang.isValid());
    CHECK(openEditor(*t.view, bang) == nullptr);

    t.view->grab().save(shotDir() + QStringLiteral("/row-live-controls.png"));
  });
}

// An editor that does not fill its cell leaves the painted value showing
// through behind it, which reads as two values at once.
TEST_CASE("a value editor covers the cell it replaces", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    // The colour editor is the widest of them: swatch plus field.
    const auto idx = t.valueIndex(4);
    const auto cell = t.view->visualRect(idx);

    auto* ed = openEditor(*t.view, idx);
    REQUIRE(ed != nullptr);

    INFO("editor " << ed->geometry().x() << "," << ed->geometry().y() << " "
                   << ed->width() << "x" << ed->height() << "  cell " << cell.x()
                   << "," << cell.y() << " " << cell.width() << "x"
                   << cell.height());
    CHECK(ed->geometry().contains(cell));
    CHECK(ed->autoFillBackground());
  });
}

// The two views are stacked, not tiled: each tab gets the whole panel.
TEST_CASE("the full editor gives each view the whole panel",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    for(auto [row, name] : {std::pair{3, "prose"}, std::pair{9, "blob"}})
    {
      const auto idx = t.valueIndex(row);
      REQUIRE(idx.isValid());

      // Opening the editor on a value the row cannot hold raises the popup.
      openEditor(*t.view, idx);
      QApplication::processEvents();

      auto* pop = QApplication::activePopupWidget();
      INFO(name);
      REQUIRE(pop != nullptr);

      pop->grab().save(
          shotDir() + QStringLiteral("/popup-%1.png").arg(QLatin1String(name)));

      // One view fills the panel; the other is not taking half the height.
      const auto edits = pop->findChildren<QPlainTextEdit*>();
      REQUIRE(!edits.isEmpty());
      int visible = 0, tallest = 0;
      for(auto* e : edits)
        if(e->isVisible())
        {
          visible++;
          tallest = std::max(tallest, e->height());
        }
      INFO(name << ": " << visible << " visible panes, tallest " << tallest
                << " of " << pop->height());
      CHECK(tallest > pop->height() / 2);

      pop->close();
      QApplication::processEvents();
      t.view->closePersistentEditor(idx);
      QApplication::processEvents();
    }
  });
}

// The hex column edits bytes, not text: a keystroke overwrites the nibble
// under the cursor and it can never hold half a byte.
TEST_CASE("the hex column cannot be made invalid", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digitsOf = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const int before = digitsOf().size();
    REQUIRE(before > 0);
    REQUIRE(before % 2 == 0);

    // Letters, punctuation and Return are not hex and never get in.
    for(const auto* keys : {"zz", "!!"})
      score::test::keyClicks(*hex, QLatin1String(keys));
    score::test::keyClick(*hex, Qt::Key_Return);
    QApplication::processEvents();

    CHECK(digitsOf() == QStringLiteral("89504e470d0a1a0a0000000d494844520000010000000100"));

    // A digit overwrites one nibble; the byte count does not move.
    hex->setFocus();
    score::test::keyClick(*hex, Qt::Key_Home);
    score::test::keyClicks(*hex, "ff");
    QApplication::processEvents();

    CHECK(digitsOf().size() == before);
    CHECK(digitsOf().startsWith("ff504e47"));

    // Space types a zero.
    score::test::keyClick(*hex, Qt::Key_Home);
    score::test::keyClicks(*hex, " 1");
    QApplication::processEvents();

    CHECK(digitsOf().size() == before);
    CHECK(digitsOf().startsWith("01504e47"));

    // Whatever was typed, it is always a whole number of bytes.
    CHECK(digitsOf().size() % 2 == 0);

    pop->close();
    QApplication::processEvents();
  });
}

// A blob has to be able to grow: the caret may sit one past the last byte, and
// a digit typed there starts a new one.
TEST_CASE("the hex column can grow and shrink", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digitsOf = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const int before = digitsOf().size();
    REQUIRE(before > 0);

    // Two bytes typed past the end of the data are two bytes added.
    hex->setFocus();
    score::test::keyClick(*hex, Qt::Key_End, Qt::ControlModifier);
    score::test::keyClicks(*hex, "abcd");
    QApplication::processEvents();

    CHECK(digitsOf().size() == before + 4);
    CHECK(digitsOf().endsWith("abcd"));

    // And Backspace takes the last one back off again.
    score::test::keyClick(*hex, Qt::Key_Backspace);
    QApplication::processEvents();

    CHECK(digitsOf().size() == before + 2);
    CHECK(digitsOf().endsWith("ab"));

    // Delete takes the first byte off, and the whole thing is still whole
    // bytes. (Adding one in the middle is insert mode's job, tested below.)
    score::test::keyClick(*hex, Qt::Key_Home, Qt::ControlModifier);
    score::test::keyClick(*hex, Qt::Key_Delete);
    QApplication::processEvents();

    CHECK(digitsOf().size() == before);
    CHECK(digitsOf().startsWith("504e47"));
    CHECK(digitsOf().size() % 2 == 0);

    pop->grab().save(shotDir() + QStringLiteral("/hex-grown.png"));

    pop->close();
    QApplication::processEvents();
  });
}

// Every way of getting the caret to the end has to append, clicking included.
TEST_CASE("the hex column appends however the caret got there",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digits = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    auto clickAt = [&](QPoint p) {
      const auto g = hex->viewport()->mapToGlobal(p);
      QMouseEvent press{QEvent::MouseButtonPress, QPointF(p),   QPointF(g),
                        Qt::LeftButton,           Qt::NoButton, Qt::NoModifier};
      QMouseEvent rel{QEvent::MouseButtonRelease, QPointF(p),   QPointF(g),
                      Qt::LeftButton,             Qt::NoButton, Qt::NoModifier};
      QCoreApplication::sendEvent(hex->viewport(), &press);
      QCoreApplication::sendEvent(hex->viewport(), &rel);
      QApplication::processEvents();
    };

    hex->setFocus();

    // Three ways a caret gets to the end of the data; each has to append,
    // and Backspace puts things back for the next one.
    enum How
    {
      EndKey,
      ClickPastLastByte,
      ClickBelowLastLine
    };
    for(How how : {EndKey, ClickPastLastByte, ClickBelowLastLine})
    {
      const int before = digits().size();

      switch(how)
      {
        case EndKey:
          // Only the last line's end is the end of the data.
          score::test::keyClick(*hex, Qt::Key_End, Qt::ControlModifier);
          score::test::keyClick(*hex, Qt::Key_End);
          break;

        case ClickPastLastByte:
        {
          QTextCursor c = hex->textCursor();
          c.movePosition(QTextCursor::End);
          const QRect r = hex->cursorRect(c);
          clickAt({r.right() + 60, r.center().y()});
          break;
        }

        case ClickBelowLastLine:
          clickAt({20, hex->viewport()->height() - 4});
          break;
      }

      score::test::keyClicks(*hex, "7f");
      QApplication::processEvents();

      INFO("way " << (int)how << ": caret at " << hex->textCursor().position()
                  << " of " << hex->document()->characterCount() - 1);
      CHECK(digits().size() == before + 2);
      CHECK(digits().endsWith("7f"));

      score::test::keyClick(*hex, Qt::Key_Backspace);
      QApplication::processEvents();
      CHECK(digits().size() == before);
    }

    pop->close();
    QApplication::processEvents();
  });
}

// The blob opens on the hex pane by itself; an ordinary string opens on text
// and gets there by the tab. Same column, different way in.
TEST_CASE("a plain string can be appended to in hex",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    for(int row : {2, 3}) // "hello", and the multi-line one
    {
    const auto idx = t.valueIndex(row);

    auto* ed = openEditor(*t.view, idx);
    REQUIRE(ed != nullptr);

    auto* field = ed->findChild<State::ExpandableTextEdit*>();
    if(!field)
      field = qobject_cast<State::ExpandableTextEdit*>(ed);
    REQUIRE(field != nullptr);

    field->expand();
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    // The user reaches hex through the tab.
    for(auto* b : pop->findChildren<QToolButton*>())
      if(b->text() == "Hex")
        b->click();
    QApplication::processEvents();

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digits = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const int before = digits().size();
    INFO("row " << row << ", " << before / 2 << " bytes");

    hex->setFocus();
    score::test::keyClick(*hex, Qt::Key_End, Qt::ControlModifier);
    score::test::keyClicks(*hex, "7f");
    QApplication::processEvents();

    CHECK(digits().size() == before + 2);
    CHECK(digits().endsWith("7f"));

    pop->close();
    QApplication::processEvents();
    t.view->closePersistentEditor(idx);
    QApplication::processEvents();
    }
  });
}

// Adding a byte in the middle is the one thing typing cannot do: a key and a
// button, the button naming the key.
TEST_CASE("a byte can be added anywhere, by key or by button",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    QToolButton* insert{};
    QToolButton* remove{};
    for(auto* b : pop->findChildren<QToolButton*>())
    {
      if(b->text() == "Insert")
        insert = b;
      else if(b->text() == "Remove")
        remove = b;
    }
    REQUIRE(insert != nullptr);
    REQUIRE(remove != nullptr);

    // Every action names its key.
    CHECK(insert->toolTip().contains("Ins"));
    CHECK(remove->toolTip().contains("Del"));

    auto digits = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const int before = digits().size();

    // At the front, nowhere near the end: a byte opens, two digits fill it,
    // and nothing that was there is lost.
    hex->setFocus();
    score::test::keyClick(*hex, Qt::Key_Home, Qt::ControlModifier);
    score::test::keyClick(*hex, Qt::Key_Insert);
    score::test::keyClicks(*hex, "7f");
    QApplication::processEvents();

    CHECK(digits().size() == before + 2);
    CHECK(digits().startsWith("7f89504e47"));

    // The button does the same and leaves the caret in the byte it opened.
    score::test::keyClick(*hex, Qt::Key_Home, Qt::ControlModifier);
    insert->click();
    score::test::keyClicks(*hex, "01");
    QApplication::processEvents();

    CHECK(digits().size() == before + 4);
    CHECK(digits().startsWith("017f89504e47"));

    // And Remove is Delete, on whichever byte the caret sits on.
    score::test::keyClick(*hex, Qt::Key_Home, Qt::ControlModifier);
    remove->click();
    QApplication::processEvents();

    CHECK(digits().size() == before + 2);
    CHECK(digits().startsWith("7f89504e47"));

    pop->grab().save(shotDir() + QStringLiteral("/hex-buttons.png"));

    pop->close();
    QApplication::processEvents();
  });
}

// The status line has to sit in the same place in both views.
TEST_CASE("the status line does not move between views",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    openEditor(*t.view, t.valueIndex(9)); // the blob, which opens on hex
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    QToolButton* asText{};
    QToolButton* asHex{};
    for(auto* b : pop->findChildren<QToolButton*>())
    {
      if(b->text() == "Text")
        asText = b;
      else if(b->text() == "Hex")
        asHex = b;
    }
    REQUIRE(asText != nullptr);
    REQUIRE(asHex != nullptr);

    // The two labels of the footer, whatever they happen to say.
    auto footer = [&] {
      QRect r;
      for(auto* l : pop->findChildren<QLabel*>())
        if(l->isVisible())
          r = r.united(l->geometry());
      return r;
    };

    const QRect inHex = footer();
    REQUIRE(inHex.isValid());

    asText->click();
    QApplication::processEvents();
    const QRect inText = footer();

    INFO("hex " << inHex.top() << ".." << inHex.bottom() << ", text "
                << inText.top() << ".." << inText.bottom());
    CHECK(inHex.top() == inText.top());
    CHECK(inHex.height() == inText.height());

    pop->close();
    QApplication::processEvents();
  });
}

// Delete on a selection removes the whole run, not the byte at the caret.
TEST_CASE("a selection in the hex column deletes as one",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digits = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const int before = digits().size();
    hex->setFocus();

    // The first four bytes, selected as the mouse would leave them.
    auto selectBytes = [&](int from, int to) {
      QTextCursor c = hex->textCursor();
      c.setPosition(3 * from);
      c.setPosition(3 * to - 1, QTextCursor::KeepAnchor);
      hex->setTextCursor(c);
    };

    selectBytes(0, 4);
    score::test::keyClick(*hex, Qt::Key_Delete);
    QApplication::processEvents();

    CHECK(digits().size() == before - 8);
    CHECK(digits().startsWith("0d0a1a0a"));

    // Backspace on a selection means the same thing, not "one more as well".
    selectBytes(0, 2);
    score::test::keyClick(*hex, Qt::Key_Backspace);
    QApplication::processEvents();

    CHECK(digits().size() == before - 12);
    CHECK(digits().startsWith("1a0a"));

    // Select everything and there is nothing left, with no crash on the way.
    score::test::keyClick(*hex, Qt::Key_A, Qt::ControlModifier);
    score::test::keyClick(*hex, Qt::Key_Delete);
    QApplication::processEvents();

    CHECK(digits().isEmpty());

    pop->close();
    QApplication::processEvents();
  });
}

// The column rewrites its own text on every keystroke, so the document's undo
// stack is useless; Ctrl+Z walks byte states instead.
TEST_CASE("the hex column undoes and redoes", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    auto digits = [&] {
      QString d;
      for(QChar c : hex->toPlainText())
        if(!c.isSpace())
          d += c;
      return d;
    };

    const QString before = digits();
    hex->setFocus();

    score::test::keyClick(*hex, Qt::Key_Home, Qt::ControlModifier);
    score::test::keyClicks(*hex, "ff");
    QApplication::processEvents();
    REQUIRE(digits().startsWith("ff"));

    // Two digits, two nibbles, two states: Ctrl+Z twice is where it started.
    score::test::keyClick(*hex, Qt::Key_Z, Qt::ControlModifier);
    score::test::keyClick(*hex, Qt::Key_Z, Qt::ControlModifier);
    QApplication::processEvents();
    CHECK(digits() == before);

    score::test::keyClick(*hex, Qt::Key_Y, Qt::ControlModifier);
    score::test::keyClick(*hex, Qt::Key_Y, Qt::ControlModifier);
    QApplication::processEvents();
    CHECK(digits().startsWith("ff"));

    // Undoing a removal brings the bytes back, not just the digits.
    score::test::keyClick(*hex, Qt::Key_Delete);
    QApplication::processEvents();
    REQUIRE(digits().size() == before.size() - 2);

    score::test::keyClick(*hex, Qt::Key_Z, Qt::ControlModifier);
    QApplication::processEvents();
    CHECK(digits().size() == before.size());

    pop->close();
    QApplication::processEvents();
  });
}

// The character column writes as well as reads: that is what it is for.
TEST_CASE("the character column writes bytes too", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto idx = t.valueIndex(9); // the blob, whose bytes 12-15 are IHDR
    openEditor(*t.view, idx);
    QApplication::processEvents();

    auto* pop = QApplication::activePopupWidget();
    REQUIRE(pop != nullptr);

    auto* ascii = pop->findChild<QPlainTextEdit*>("charColumn");
    REQUIRE(ascii != nullptr);
    REQUIRE(ascii->toPlainText().contains("IHDR"));

    ascii->setFocus();

    // Type over the H of IHDR: one byte changes, the dots around it do not.
    QTextCursor c = ascii->textCursor();
    c.setPosition(13);
    ascii->setTextCursor(c);
    score::test::keyClicks(*ascii, "X");
    QApplication::processEvents();

    CHECK(ascii->toPlainText().startsWith(".PNG........IXDR"));

    // The unprintable bytes are still unprintable, not the dots they show as.
    auto* hex = pop->findChild<QPlainTextEdit*>("hexColumn");
    REQUIRE(hex != nullptr);

    QString d;
    for(QChar ch : hex->toPlainText())
      if(!ch.isSpace())
        d += ch;
    CHECK(d.startsWith("89504e470d0a1a0a0000000d49584452"));

    pop->close();
    QApplication::processEvents();
  });
}

// An impulse row lights when a value arrives, not only when it is clicked: the
// parameter holds nothing, so a row that did not blink looks like one that
// never fired.
TEST_CASE("an arriving impulse lights its row", "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};

    const auto bang = t.valueIndex(8);
    REQUIRE(bang.isValid());

    auto rowShot = [&] {
      QApplication::processEvents();
      return t.view->viewport()->grab(t.view->visualRect(bang)).toImage();
    };

    auto settle = [&](int ms) {
      QElapsedTimer clock;
      clock.start();
      while(clock.elapsed() < ms)
        QApplication::processEvents(QEventLoop::AllEvents, 5);
    };

    const auto idle = rowShot();

    // What an incoming impulse looks like to the model.
    t.model->setData(
        bang, QVariant::fromValue(ossia::value{ossia::impulse{}}), Qt::EditRole);
    const auto lit = rowShot();

    t.view->viewport()->grab(t.view->visualRect(bang))
        .save(shotDir() + QStringLiteral("/bang-lit.png"));

    CHECK(lit != idle);

    // ... and it goes out again on its own.
    settle(300);
    CHECK(rowShot() == idle);
  });
}
// "commitData called with an editor that does not belong to this view" is only
// a warning, but the commit it names went nowhere: the typed value is dropped.
// Every way out of an editor, with the warnings captured.
namespace
{
QStringList g_warnings;
QtMessageHandler g_previous{};

void collectWarnings(QtMsgType t, const QMessageLogContext& c, const QString& m)
{
  if(m.contains("does not belong to this view"))
    g_warnings += m;
  if(g_previous)
    g_previous(t, c, m);
}

struct WarningWatch
{
  WarningWatch()
  {
    g_warnings.clear();
    g_previous = qInstallMessageHandler(collectWarnings);
  }
  ~WarningWatch() { qInstallMessageHandler(g_previous); }
};
}

TEST_CASE("leaving an editor does not confuse the view",
          "[integration][explorer][look]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Tree t{ctx};
    WarningWatch watch;

    // Reports which way out of the editor left the view confused.
    int row{};
    auto clean = [&](const char* step) {
      INFO("after " << step << " on row " << row);
      CHECK(g_warnings.join("; ").toStdString() == "");
      g_warnings.clear();
    };

    // Every editor the tree can build, left by every way out of one.
    for(int r : {0, 1, 2, 4, 5, 6, 7})
    {
      row = r;
      const auto idx = t.valueIndex(row);
      REQUIRE(idx.isValid());

      // 1. Return: commit and close from inside the editor.
      auto* ed = openEditor(*t.view, idx);
      REQUIRE(ed != nullptr);
      ed->setFocus();
      score::test::keyClick(*ed, Qt::Key_Return);
      QApplication::processEvents();

      clean("Return");

      // 2. Clicking away: the focus goes somewhere else entirely.
      ed = openEditor(*t.view, idx);
      REQUIRE(ed != nullptr);
      ed->setFocus();
      QApplication::processEvents();
      t.view->setFocus();
      QApplication::processEvents();

      clean("click away");
      g_warnings.clear();

      // 3. Escape: close without committing.
      ed = openEditor(*t.view, idx);
      REQUIRE(ed != nullptr);
      ed->setFocus();
      score::test::keyClick(*ed, Qt::Key_Escape);
      QApplication::processEvents();

      clean("escape");
      g_warnings.clear();

      // 4. Opening the next row's editor while this one is still up.
      openEditor(*t.view, idx);
      openEditor(*t.view, t.valueIndex(row == 0 ? 1 : 0));
      QApplication::processEvents();

      while(auto* p = QApplication::activePopupWidget())
      {
        p->close();
        QApplication::processEvents();
      }
      t.view->closePersistentEditor(idx);
      QApplication::processEvents();

      clean("reopen");
    }
  });
}

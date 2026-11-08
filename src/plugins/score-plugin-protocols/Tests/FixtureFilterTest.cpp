#include <ossia/detail/config.hpp>

#include <Protocols/Artnet/FixtureDatabase.hpp>

#include <catch2/catch_all.hpp>

using namespace Protocols;

namespace
{
class TestFixtureModel final : public TreeNodeBasedItemModel<FixtureNode>
{
public:
  FixtureNode root;

  FixtureNode& addManufacturer(QString name)
  {
    return root.emplace_back(FixtureData{.name = std::move(name)}, &root);
  }

  static void addFixture(FixtureNode& mf, QString name, QStringList tags = {})
  {
    mf.emplace_back(FixtureData{.name = std::move(name), .tags = std::move(tags)}, &mf);
  }

  FixtureNode& rootNode() override { return root; }
  const FixtureNode& rootNode() const override { return root; }
  int columnCount(const QModelIndex&) const override { return 2; }
  QVariant data(const QModelIndex& index, int role) const override
  {
    if(role != Qt::DisplayRole)
      return {};
    const auto& node = nodeFromModelIndex(index);
    return index.column() == 0 ? QVariant{node.name} : QVariant{node.tags.join(", ")};
  }
};

QStringList visibleNames(const FixtureFilterProxy& proxy, const QModelIndex& parent = {})
{
  QStringList names;
  for(int i = 0; i < proxy.rowCount(parent); i++)
  {
    const auto idx = proxy.index(i, 0, parent);
    names.push_back(proxy.nodeAt(idx)->name);
    names += visibleNames(proxy, idx);
  }
  return names;
}
}

TEST_CASE("The fixture search filters by fixture, manufacturer and tag", "[dmx][ui]")
{
  TestFixtureModel model;
  auto& martin = model.addManufacturer("Martin");
  TestFixtureModel::addFixture(martin, "MAC Aura", {"Moving Head"});
  TestFixtureModel::addFixture(martin, "Atomic 3000", {"Strobe"});
  auto& generic = model.addManufacturer("Generic");
  TestFixtureModel::addFixture(generic, "RGB Fader", {"Color Changer"});
  TestFixtureModel::addFixture(generic, "Dimmer");

  FixtureFilterProxy proxy;
  proxy.setSourceModel(&model);
  CHECK(proxy.columnCount({}) == 2);
  CHECK(visibleNames(proxy).size() == 6);

  proxy.setPattern("aura");
  CHECK(visibleNames(proxy) == QStringList{"Martin", "MAC Aura"});

  proxy.setPattern("GENERIC");
  CHECK(visibleNames(proxy) == QStringList{"Generic", "RGB Fader", "Dimmer"});

  proxy.setPattern("strobe");
  CHECK(visibleNames(proxy) == QStringList{"Martin", "Atomic 3000"});

  proxy.setPattern("nothing like this");
  CHECK(proxy.rowCount({}) == 0);

  proxy.setPattern("");
  CHECK(visibleNames(proxy).size() == 6);

  proxy.setPattern("fader");
  const auto mf = proxy.index(0, 0, {});
  const auto fixture = proxy.index(0, 0, mf);
  REQUIRE(proxy.nodeAt(fixture));
  CHECK(proxy.nodeAt(fixture) == &*generic.begin());
  CHECK(proxy.nodeAt({}) == nullptr);
}

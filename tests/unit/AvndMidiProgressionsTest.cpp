// Chord progressions (MidiCommon/Progressions.hpp, ProgressionTable.hpp): the
// roman-numeral parser, what a symbol means in a key (checked against the
// rules of python-mingus' progressions.to_chords and chords.py, which
// free-midi-chords renders its progressions with), chord names, voicing.

#include <Advanced/MidiScaler/MidiCommon/Progressions.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int pc_of(const std::string& n)
{
  static const char* names = "C D EF G A B";
  int pc = int(std::string(names).find(n[0]));
  for(std::size_t i = 1; i < n.size(); i++)
    pc += n[i] == '#' ? 1 : (n[i] == 'b' ? -1 : 0);
  return mtk::pitch_class(pc);
}

//! "Eb G Bb" -> {3, 7, 10}
std::set<int> pcs(const std::string& notes)
{
  std::set<int> r;
  std::istringstream s{notes};
  std::string n;
  while(s >> n)
    r.insert(pc_of(n));
  return r;
}

std::set<int> pcs(const mtk::realized_chord& c)
{
  std::set<int> r;
  for(int i : c.notes())
    r.insert(mtk::pitch_class(c.root + i));
  return r;
}

struct expected_chord
{
  std::string notes; //!< the chord's notes, root first
  std::string bass{};
};

void check_progression(
    const std::string& text, int key_root, mtk::mode m,
    const std::vector<expected_chord>& expected)
{
  INFO(text);
  mtk::parsed_progression p;
  const auto err = mtk::parse_progression(text, p);
  REQUIRE(!err);
  REQUIRE(p.count == int(expected.size()));
  for(int i = 0; i < p.count; i++)
  {
    INFO("chord " << i << ": " << p.chords[i].text(text));
    const auto c = mtk::realize(p.chords[i], key_root, m);
    REQUIRE(!c.rest);
    CHECK(pcs(c) == pcs(expected[i].notes));
    std::istringstream s{expected[i].notes};
    std::string root;
    s >> root;
    CHECK(c.root == pc_of(root));
    if(expected[i].bass.empty())
      CHECK(c.bass == -1);
    else
      CHECK(c.bass == pc_of(expected[i].bass));
  }
}

constexpr int C = 0;
constexpr int A = 9;
constexpr auto major = mtk::mode::ionian;
constexpr auto minor = mtk::mode::aeolian;

std::vector<int> pitches(const mtk::voicing& v)
{
  return {v.all().begin(), v.all().end()};
}

mtk::realized_chord chord_of(std::string_view sym, int key = C, mtk::mode m = major)
{
  mtk::chord_symbol s;
  REQUIRE(!mtk::parse_symbol(sym, s));
  return mtk::realize(s, key, m);
}
}

TEST_CASE("The table holds the 194 progressions of free-midi-chords", "[progressions]")
{
  REQUIRE(mtk::progression_count == 194);
  int fam[4]{};
  for(const auto& e : mtk::progression_table)
    fam[int(e.family)]++;
  CHECK(fam[0] == 50);
  CHECK(fam[1] == 60);
  CHECK(fam[2] == 79);
  CHECK(fam[3] == 5);

  std::set<std::string_view> labels;
  for(const auto& e : mtk::progression_table)
  {
    INFO(e.label);
    CHECK(labels.insert(e.label).second);
    CHECK(e.label.starts_with(e.symbols));
    CHECK(e.moods != 0);
    CHECK(e.label.find("New") == std::string_view::npos);
    for(char c : e.label)
      CHECK((c >= 32 && c < 127));
    // Null terminated: the binding hands .data() to Qt
    CHECK(e.label.data()[e.label.size()] == '\0');
  }
  CHECK(mtk::progression_table[18].label == "I V vi IV (Hopeful Romantic)");
}

TEST_CASE("Every progression of the table parses and realizes", "[progressions]")
{
  for(const auto& e : mtk::progression_table)
  {
    INFO(e.symbols);
    mtk::parsed_progression p;
    const auto err = mtk::parse_progression(e.symbols, p);
    CHECK(!err);
    CHECK(p.count >= 3);
    const auto m = e.family == mtk::progression_family::minor ? minor : major;
    for(const auto& s : p.symbols())
    {
      CHECK(!s.rest);
      const auto c = mtk::realize(s, C, m);
      CHECK(!c.rest);
      CHECK(c.count >= 2);
      // Every chord gets a name
      std::array<char, 32> buf{};
      const auto n = mtk::format_chord_name(c, true, buf);
      CHECK(n > 0);
      CHECK(std::string_view(buf.data(), n).find('?') == std::string_view::npos);
    }
  }
}

// Expected chords below are written from mingus' rules (see Progressions.hpp):
// - bare numeral: chords.triads(key)[degree], the diatonic triad;
// - "7": chords.sevenths(key)[degree], the diatonic seventh;
// - other suffix: chord_shorthand[suffix](root of the diatonic triad), the
//   numeral's case plays no part;
// - b / #: every note of the chord lowered / raised (notes.diminish/augment);
// - "/": the bass is the root of the second symbol (chords2midi c2m.py).
// Major and modal progressions in C major, minor ones in A natural minor
// (gen.py: the relative minor, keys.get_notes of a lower-case key).

TEST_CASE("Major progressions in C, as mingus renders them", "[progressions]")
{
  check_progression("I V vi IV", C, major, {{"C E G"}, {"G B D"}, {"A C E"}, {"F A C"}});
  check_progression("ii V I", C, major, {{"D F A"}, {"G B D"}, {"C E G"}});
  // I7 is chords.I7, the diatonic seventh (Cmaj7); dom7 is dominant_seventh
  check_progression(
      "I I7 Idom7 IV", C, major, {{"C E G"}, {"C E G B"}, {"C E G Bb"}, {"F A C"}});
  // vii: the diatonic triad of B, diminished
  check_progression(
      "I IV vii iii vi ii V I", C, major,
      {{"C E G"},
       {"F A C"},
       {"B D F"},
       {"E G B"},
       {"A C E"},
       {"D F A"},
       {"G B D"},
       {"C E G"}});
  check_progression(
      "iim7 V7 iiim7 vi7 iim7 V7", C, major,
      {{"D F A C"},
       {"G B D F"},
       {"E G B D"},
       {"A C E G"},
       {"D F A C"},
       {"G B D F"}});
  // viadd9 is major_add_ninth on A: the lower case does not make it minor
  check_progression(
      "I7 V7 viadd9 IV7", C, major,
      {{"C E G B"}, {"G B D F"}, {"A C# E B"}, {"F A C E"}});
  // M-5 is major_diminished_triad
  check_progression(
      "I I IM-5 IM-5 IV IV V Vsus2", C, major,
      {{"C E G"},
       {"C E G"},
       {"C E Gb"},
       {"C E Gb"},
       {"F A C"},
       {"F A C"},
       {"G B D"},
       {"G A D"}});
  // 6 is major_sixth, add9 major_add_ninth
  check_progression(
      "IV I IV6 Iadd9", C, major, {{"F A C"}, {"C E G"}, {"F A C D"}, {"C E G D"}});
  check_progression(
      "Isus2 I vi7 visus4", C, major, {{"C D G"}, {"C E G"}, {"A C E G"}, {"A D E"}});
  check_progression("ii7 Vadd9 I7", C, major, {{"D F A C"}, {"G B D A"}, {"C E G B"}});
}

TEST_CASE("Minor progressions in A minor, as mingus renders them", "[progressions]")
{
  check_progression("i VI III VII", A, minor, {{"A C E"}, {"F A C"}, {"C E G"}, {"G B D"}});
  // v7 is the diatonic seventh of E in A natural minor (Em7); 5 is root and
  // fifth
  check_progression(
      "i i iv iv v7 ii5 v v7", A, minor,
      {{"A C E"},
       {"A C E"},
       {"D F A"},
       {"D F A"},
       {"E G B D"},
       {"B F#"},
       {"E G B"},
       {"E G B D"}});
  check_progression("i vdim iv VI", A, minor, {{"A C E"}, {"E G Bb"}, {"D F A"}, {"F A C"}});
  // 69 is sixth_ninth; III7 the diatonic seventh of C in A minor (Cmaj7)
  check_progression(
      "i VI III VII i VI69 III7 VII", A, minor,
      {{"A C E"},
       {"F A C"},
       {"C E G"},
       {"G B D"},
       {"A C E"},
       {"F A C D G"},
       {"C E G B"},
       {"G B D"}});
  check_progression(
      "i7 VI III7 VII6 i i7 III7 iv7", A, minor,
      {{"A C E G"},
       {"F A C"},
       {"C E G B"},
       {"G B D E"},
       {"A C E"},
       {"A C E G"},
       {"C E G B"},
       {"D F A C"}});
  check_progression(
      "im7 ivsus4 v7 isus4", A, minor, {{"A C E G"}, {"D G A"}, {"E G B D"}, {"A D E"}});
  check_progression(
      "i VII VIadd9 VII i v VI VIIsus4", A, minor,
      {{"A C E"},
       {"G B D"},
       {"F A C G"},
       {"G B D"},
       {"A C E"},
       {"E G B"},
       {"F A C"},
       {"G C D"}});
  check_progression(
      "VI7 VII6 i VII6", A, minor, {{"F A C E"}, {"G B D E"}, {"A C E"}, {"G B D E"}});
  check_progression(
      "i III7 VI7 iv", A, minor, {{"A C E"}, {"C E G B"}, {"F A C E"}, {"D F A"}});
}

TEST_CASE("Modal progressions and cadences in C, as mingus renders them", "[progressions]")
{
  check_progression(
      "bIIIM ii bIIM I", C, major, {{"Eb G Bb"}, {"D F A"}, {"Db F Ab"}, {"C E G"}});
  // biii: the E minor triad lowered (Eb minor); bviim: B minor lowered
  check_progression(
      "bIIM bVIM biii bviim", C, major,
      {{"Db F Ab"}, {"Ab C Eb"}, {"Eb Gb Bb"}, {"Bb Db F"}});
  check_progression(
      "bviim I/bVII bviim I/bVII", C, major,
      {{"Bb Db F"}, {"C E G", "Bb"}, {"Bb Db F"}, {"C E G", "Bb"}});
  // bIdom7: C7 lowered, i.e. B7; III7: the diatonic seventh of E (Em7)
  check_progression(
      "I7 bIdom7 III7 ii7 V7", C, major,
      {{"C E G B"}, {"B D# F# A"}, {"E G B D"}, {"D F A C"}, {"G B D F"}});
  check_progression(
      "I5 iii II5 #IVm IV5 vi V5 viim", C, major,
      {{"C G"},
       {"E G B"},
       {"D A"},
       {"F# A C#"},
       {"F C"},
       {"A C E"},
       {"G D"},
       {"B D F#"}});
  // bi: the C major triad lowered, B major
  check_progression(
      "im VIM bi V", C, major, {{"C Eb G"}, {"A C# E"}, {"B D# F#"}, {"G B D"}});
  // bii: D minor lowered, Db minor; the bass after / is the root of i
  check_progression(
      "im viim/i im bii/i", C, major,
      {{"C Eb G"}, {"B D F#", "C"}, {"C Eb G"}, {"Db E Ab", "C"}});
  check_progression(
      "bVIM7 ivmadd9 I", C, major, {{"Ab C Eb G"}, {"F Ab C G"}, {"C E G"}});
  check_progression(
      "I bIIM7 bIIIM6 bIIM7 I im bVIIM bIIM", C, major,
      {{"C E G"},
       {"Db F Ab C"},
       {"Eb G Bb C"},
       {"Db F Ab C"},
       {"C E G"},
       {"C Eb G"},
       {"Bb D F"},
       {"Db F Ab"}});
  // biim6: D F A B lowered
  check_progression(
      "im bIIM biim6 ivm", C, major, {{"C Eb G"}, {"Db F Ab"}, {"Db E Ab Bb"}, {"F Ab C"}});
  // #IVdim: F Ab Cb raised
  check_progression(
      "vi viim V vi #IVdim V", C, major,
      {{"A C E"}, {"B D F#"}, {"G B D"}, {"A C E"}, {"F# A C"}, {"G B D"}});
  check_progression("im bIIIsus2 IV", C, major, {{"C Eb G"}, {"Eb F Bb"}, {"F A C"}});
  check_progression(
      "IIIM V VIsus4 VIM I IIM", C, major,
      {{"E G# B"}, {"G B D"}, {"A D E"}, {"A C# E"}, {"C E G"}, {"D F# A"}});
  // I9 is dominant_ninth
  check_progression(
      "I I7 I9 IV ivm", C, major,
      {{"C E G"}, {"C E G B"}, {"C E G Bb D"}, {"F A C"}, {"F Ab C"}});
  check_progression(
      "I I7 Idom7 IV ivm I ivm6 Vdom7", C, major,
      {{"C E G"},
       {"C E G B"},
       {"C E G Bb"},
       {"F A C"},
       {"F Ab C"},
       {"C E G"},
       {"F Ab C D"},
       {"G B D F"}});
  check_progression("bIIIM V7 I", C, major, {{"Eb G Bb"}, {"G B D F"}, {"C E G"}});
  check_progression(
      "ivm IIIM bIIM I", C, major, {{"F Ab C"}, {"E G# B"}, {"Db F Ab"}, {"C E G"}});
}

TEST_CASE("Symbols follow the key and the mode", "[progressions]")
{
  // D dorian: i is D minor, IV is G major, bare numerals take the mode's chords
  check_progression(
      "i IV v VII", 2, mtk::mode::dorian, {{"D F A"}, {"G B D"}, {"A C E"}, {"C E G"}});
  // E major
  check_progression("I V vi IV", 4, major, {{"E G# B"}, {"B D# F#"}, {"C# E G#"}, {"A C# E"}});
  // Harmonic minor: V is major, vii diminished
  check_progression("i V7 vii", A, mtk::mode::harmonic_minor, {{"A C E"}, {"E G# B D"}, {"G# B D"}});
}

TEST_CASE("Rests, moods and spacing", "[progressions]")
{
  mtk::parsed_progression p;
  REQUIRE(!mtk::parse_progression("I  V", p));
  REQUIRE(p.count == 3);
  CHECK(!p.chords[0].rest);
  CHECK(p.chords[1].rest);
  CHECK(p.chords[2].degree == 4);
  // n spaces are n - 1 rests, as gen.py's substitution gives
  REQUIRE(!mtk::parse_progression("I    V", p));
  CHECK(p.count == 5);

  REQUIRE(!mtk::parse_progression("I X x V", p));
  REQUIRE(p.count == 4);
  CHECK(p.chords[1].rest);
  CHECK(p.chords[2].rest);
  CHECK(mtk::realize(p.chords[1], C, major).rest);

  // A line of chords.py, moods included
  REQUIRE(!mtk::parse_progression("I V vi IV =Hopeful Romantic", p));
  CHECK(p.count == 4);

  // Leading and trailing spaces are ignored
  REQUIRE(!mtk::parse_progression("  ii V I  ", p));
  CHECK(p.count == 3);
  const std::string_view t = "  ii V I  ";
  CHECK(p.chords[0].text(t) == "ii");
  CHECK(p.chords[0].lower);
  CHECK(p.chords[1].text(t) == "V");
  CHECK(!p.chords[1].lower);
}

TEST_CASE("Malformed progressions give the offending token", "[progressions]")
{
  mtk::parsed_progression p;
  auto check = [&](std::string_view text, mtk::parse_status st, int pos, int len) {
    INFO(text);
    const auto e = mtk::parse_progression(text, p);
    CHECK(e.status == st);
    CHECK(e.position == pos);
    CHECK(e.length == len);
  };
  check("", mtk::parse_status::empty, 0, 0);
  check("   ", mtk::parse_status::empty, 0, 0);
  check("I VIII", mtk::parse_status::bad_numeral, 2, 4);
  check("I IIII V", mtk::parse_status::bad_numeral, 2, 4);
  check("I IIV", mtk::parse_status::bad_numeral, 2, 3);
  check("I IVxx V", mtk::parse_status::unknown_suffix, 2, 4);
  check("I / V", mtk::parse_status::missing_numeral, 2, 1);
  check("I V/", mtk::parse_status::bad_bass, 2, 2);
  check("I V/Q", mtk::parse_status::bad_bass, 2, 3);
  check("I V/IVxx", mtk::parse_status::bad_bass, 2, 6);
  check("bbII V", mtk::parse_status::double_accidental, 0, 4);
  check("#bII", mtk::parse_status::double_accidental, 0, 4);
  check("Q", mtk::parse_status::missing_numeral, 0, 1);
  check("ii V Cmaj7", mtk::parse_status::missing_numeral, 5, 5);
  // The suffixes with a "/" of mingus are taken as slash chords
  check("Im/M7", mtk::parse_status::bad_bass, 0, 5);

  std::string many;
  for(int i = 0; i < 65; i++)
    many += "I ";
  check(many, mtk::parse_status::too_many, 128, 1);

  // The error message names the token and its position
  const std::string_view text = "I IVxx V";
  const auto e = mtk::parse_progression(text, p);
  std::array<char, 64> buf{};
  const auto n = mtk::format_parse_error(e, text, buf);
  CHECK(std::string_view(buf.data(), n) == "Unknown chord type: IVxx (at 2)");
  // Truncated, never overflowing
  std::array<char, 5> small{};
  CHECK(mtk::format_parse_error(e, text, small) == 5);
}

TEST_CASE("Suffix lookup: mingus first, then Theory's names", "[progressions]")
{
  // mingus' M7+5 is augmented_minor_seventh, unlike Theory's M7+5
  REQUIRE(mtk::find_suffix("M7+5"));
  CHECK(mtk::find_suffix("M7+5")->count == 4);
  CHECK(mtk::find_suffix("M7+5")->intervals[3] == 10);
  // Theory's names, and "2"
  REQUIRE(mtk::find_suffix("maj7"));
  CHECK(mtk::find_suffix("maj7")->intervals[3] == 11);
  REQUIRE(mtk::find_suffix("2"));
  CHECK(mtk::find_suffix("2")->intervals[3] == 14);
  REQUIRE(mtk::find_suffix("m7-5"));
  // "7" never reaches the lookup: it is the diatonic seventh
  mtk::chord_symbol s7;
  REQUIRE(!mtk::parse_symbol("I7", s7));
  CHECK(s7.seventh);
  CHECK(s7.type == nullptr);
  CHECK(!mtk::find_suffix(""));
  CHECK(!mtk::find_suffix("xx"));
  // Every mingus suffix whose name Theory.hpp also uses has the same notes,
  // except M7+5
  for(const auto& s : mtk::mingus_suffixes)
  {
    INFO(s.name);
    if(auto t = mtk::find_chord_type(s.name); t && s.name != "M7+5")
    {
      CHECK(t->count == s.count);
      for(int i = 0; i < s.count; i++)
        CHECK(t->intervals[i] == s.intervals[i]);
    }
  }
}

TEST_CASE("Chord names", "[progressions]")
{
  auto name = [](std::string_view sym, int key = C, mtk::mode m = major) {
    const auto c = chord_of(sym, key, m);
    std::array<char, 32> buf{};
    const auto n
        = mtk::format_chord_name(c, mtk::key_prefers_flats(key, m) || sym[0] == 'b', buf);
    return std::string(buf.data(), n);
  };
  CHECK(name("vi7") == "Am7");
  CHECK(name("I") == "C");
  CHECK(name("I7") == "Cmaj7");
  CHECK(name("V7") == "G7");
  CHECK(name("vii7") == "Bm7-5");
  CHECK(name("vii") == "Bdim");
  CHECK(name("bVIIM") == "Bb");
  CHECK(name("I/bVII") == "C/Bb");
  CHECK(name("Iadd9") == "Cadd9");
  CHECK(name("IM-5") == "CM-5");
  CHECK(name("iii", 4) == "G#m");
  CHECK(name("IV", 5) == "Bb");
  CHECK(name("III", 0, minor) == "Eb");
  CHECK(name("v", 4, minor) == "Bm");
  CHECK(mtk::key_prefers_flats(2, mtk::mode::dorian));   // D dorian: C major
  CHECK(!mtk::key_prefers_flats(4, minor));               // E minor: G major
  CHECK(mtk::key_prefers_flats(5, minor));                // F minor: Ab major

  mtk::chord_symbol rest;
  REQUIRE(!mtk::parse_symbol("X", rest));
  std::array<char, 8> buf{};
  CHECK(mtk::format_chord_name(mtk::realize(rest, C, major), false, buf) == 0);
}

TEST_CASE("Voicing: ii V I leads each voice to the nearest note", "[progressions][voicing]")
{
  mtk::voicing_options o;
  mtk::voicing prev;
  std::vector<std::vector<int>> got;
  for(auto sym : {"ii", "V", "I"})
  {
    prev = mtk::voice(chord_of(sym), prev, o);
    got.push_back(pitches(prev));
  }
  CHECK(got[0] == std::vector<int>{62, 65, 69}); // D F A
  CHECK(got[1] == std::vector<int>{59, 62, 67}); // B D G
  CHECK(got[2] == std::vector<int>{60, 64, 67}); // C E G
}

TEST_CASE("Voicing: I V vi IV", "[progressions][voicing]")
{
  mtk::voicing_options o;
  mtk::voicing prev;
  std::vector<std::vector<int>> got;
  for(auto sym : {"I", "V", "vi", "IV", "I"})
  {
    prev = mtk::voice(chord_of(sym), prev, o);
    got.push_back(pitches(prev));
  }
  CHECK(got[0] == std::vector<int>{60, 64, 67});
  CHECK(got[1] == std::vector<int>{59, 62, 67});
  CHECK(got[2] == std::vector<int>{60, 64, 69});
  CHECK(got[3] == std::vector<int>{60, 65, 69});
  CHECK(got[4] == std::vector<int>{60, 64, 67});

  // Root position instead: every chord starts on its root near the center
  o.inversion = mtk::inversion_mode::root;
  prev = {};
  got.clear();
  for(auto sym : {"I", "V", "vi", "IV"})
  {
    prev = mtk::voice(chord_of(sym), prev, o);
    got.push_back(pitches(prev));
  }
  CHECK(got[0] == std::vector<int>{60, 64, 67});
  CHECK(got[1] == std::vector<int>{55, 59, 62});
  CHECK(got[2] == std::vector<int>{57, 60, 64});
  CHECK(got[3] == std::vector<int>{53, 57, 60});
}

TEST_CASE("Voicing: the register never drifts", "[progressions][voicing]")
{
  mtk::voicing_options o;
  o.center = 64;
  mtk::voicing prev;
  mtk::parsed_progression p;
  REQUIRE(!mtk::parse_progression("I IV vii iii vi ii V I", p));
  for(int round = 0; round < 50; round++)
    for(const auto& s : p.symbols())
    {
      prev = mtk::voice(mtk::realize(s, C, major), prev, o);
      double mean = 0;
      for(int x : prev.all())
        mean += x;
      mean /= prev.count;
      CHECK(std::abs(mean - 64) <= 6.0);
    }
}

TEST_CASE("Voicing: bass, slash chords, note count, range", "[progressions][voicing]")
{
  mtk::voicing_options o;
  o.inversion = mtk::inversion_mode::root;

  // Bass note: the root at most an octave below the center, below the chord
  o.bass = true;
  auto v = mtk::voice(chord_of("I"), {}, o);
  CHECK(v.has_bass);
  CHECK(pitches(v) == std::vector<int>{48, 60, 64, 67});
  CHECK(v.upper().size() == 3);
  v = mtk::voice(chord_of("V"), {}, o);
  CHECK(pitches(v) == std::vector<int>{43, 55, 59, 62});

  // Slash chord with a bass: the slash bass, not the root
  v = mtk::voice(chord_of("I/bVII"), {}, o);
  CHECK(v.notes[0] == 46);
  CHECK(v.has_bass);

  // Slash chord without the bass option: the bass is the lowest note
  o.bass = false;
  v = mtk::voice(chord_of("I/V"), {}, o); // G is a chord tone: second inversion
  CHECK(mtk::pitch_class(v.notes[0]) == 7);
  CHECK(v.count == 3);
  v = mtk::voice(chord_of("I/bVII"), {}, o); // Bb is not: added below
  CHECK(v.count == 4);
  CHECK(mtk::pitch_class(v.notes[0]) == 10);
  CHECK(v.notes[1] - v.notes[0] < 12);
  o.inversion = mtk::inversion_mode::closest;
  mtk::voicing prev = mtk::voice(chord_of("vi"), {}, o);
  v = mtk::voice(chord_of("viim/i"), prev, o);
  CHECK(mtk::pitch_class(v.notes[0]) == 0);
  CHECK(v.count == 4);

  // Fewer notes: root, third, seventh before the fifth
  o.inversion = mtk::inversion_mode::root;
  o.max_notes = 3;
  v = mtk::voice(chord_of("V7"), {}, o);
  std::set<int> s;
  for(int x : v.all())
    s.insert(mtk::pitch_class(x));
  CHECK(s == std::set<int>{7, 11, 5});
  o.max_notes = 6;
  v = mtk::voice(chord_of("I9"), {}, o);
  CHECK(v.count == 5);
  CHECK(v.notes[4] - v.notes[0] == 14); // the ninth stays compound

  // Extreme registers stay within 0 - 127
  for(int center : {0, 3, 120, 127})
  {
    o.center = center;
    o.bass = true;
    v = mtk::voice(chord_of("I13"), {}, o);
    CHECK(v.count >= 1);
    for(int x : v.all())
      CHECK((x >= 0 && x <= 127));
  }
  // A rest has no notes
  mtk::chord_symbol rest;
  REQUIRE(!mtk::parse_symbol("X", rest));
  CHECK(mtk::voice(mtk::realize(rest, C, major), {}, o).count == 0);
}

TEST_CASE("Nearest chord tone, scale tone, chord tones over octaves", "[progressions]")
{
  const auto am = chord_of("vi");
  CHECK(mtk::nearest_chord_tone(60, am) == 60);
  CHECK(mtk::nearest_chord_tone(62, am) == 60); // D: C and E are both 2 away, down wins
  CHECK(mtk::nearest_chord_tone(66, am) == 64);
  CHECK(mtk::nearest_chord_tone(68, am) == 69);
  CHECK(mtk::nearest_chord_tone(0, am) == 0);
  CHECK(mtk::nearest_chord_tone(127, am) == 124); // E, G is not a tone of Am

  CHECK(mtk::nearest_scale_tone(61, C, major) == 60);
  CHECK(mtk::nearest_scale_tone(66, C, major) == 65);
  CHECK(mtk::nearest_scale_tone(61, 2, major) == 61); // C# is in D major

  const auto g7 = chord_of("V7");
  CHECK(mtk::chord_tone(g7, 55, 0) == 55);
  CHECK(mtk::chord_tone(g7, 55, 1) == 59);
  CHECK(mtk::chord_tone(g7, 55, 2) == 62);
  CHECK(mtk::chord_tone(g7, 55, 3) == 65);
  CHECK(mtk::chord_tone(g7, 55, 4) == 67);
  CHECK(mtk::chord_tone(g7, 55, -1) == 53);
  const auto c = chord_of("I");
  CHECK(mtk::chord_tone(c, 60, 3) == 72);
  CHECK(mtk::chord_tone(c, 60, -3) == 48);
}

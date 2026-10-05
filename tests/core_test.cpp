#include "../src/core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

static void rejects(const std::vector<ll::Language>& list) {
    bool rejected = false;
    try { ll::validate(list); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
int main(int argc, char** argv) {
    for (unsigned count = 1; count <= ll::MaxLanguages; ++count) {
        std::set<unsigned> seen;
        for (unsigned page = 0; page < ll::pageCount(count); ++page) {
            auto menu = ll::makeMenu(count, page);
            unsigned rows = 0;
            bool back = false;
            for (const auto& e : menu) {
                if (!e.action) continue;
                ++rows;
                assert(std::memchr(e.key, 0, sizeof(e.key)));
                assert(e.y + ll::NativeHoverHeight < 448);
                for (int offset = 0; offset <= ll::NativeHoverHeight; ++offset)
                    assert(ll::hoverRow(menu, e.y + offset) == static_cast<int>(&e - menu.data()));
                if (e.action == ll::SkipAction) assert(!e.key[0]);
                if (e.action >= ll::FirstAction && e.action < ll::FirstAction + ll::PageSize) {
                    unsigned i = page * ll::PageSize + e.action - ll::FirstAction;
                    assert(i < count);
                    assert(e.key == ll::labelKey(i));
                    assert(seen.insert(i).second);
                }
                if (e.action == 2) { back = true; assert(e.destination == 33); }
            }
            assert(back && rows <= ll::MenuSlots);
            if (ll::pageCount(count) > 1) {
                assert(menu[ll::pageItems(count, page)].action == (page ? ll::PreviousAction : ll::SkipAction));
                assert(menu[ll::pageItems(count, page) + 1].action ==
                    (page + 1 < ll::pageCount(count) ? ll::NextAction : ll::SkipAction));
            }
        }
        assert(seen.size() == count);
    }
    for (unsigned i = 0; i < ll::MaxLanguages; ++i)
        assert(ll::labelIndex(ll::labelKey(i).c_str()) == static_cast<int>(i));
    for (const char* key : {"FEL_ENG", "LLTPREV", "LLTNEXT", "", "LLT00A0", "LLT00000"})
        assert(ll::labelIndex(key) == -1);
    assert(ll::labelIndex(nullptr) == -1);
    const auto first = ll::makeMenu(24, 0), middle = ll::makeMenu(24, 1), last = ll::makeMenu(24, 2);
    assert(ll::pageFocus(first, middle, 10, true) == 10);
    assert(ll::pageFocus(middle, last, 10, true) == 7); // stationary pointer over now-hidden Next
    assert(ll::pageFocus(last, middle, 6, true) == 9);
    assert(ll::pageFocus(middle, first, 9, true) == 9); // now-hidden Previous
    assert(ll::pageFocus(first, middle, 10, false) == 0);
    assert(ll::pageFocus(first, middle, -1, true) == 0);
    std::vector<ll::Language> languages(12, {"american.gxt", "FEL_ENG", "", 0, 0});
    for (unsigned i = 0; i < 5; ++i) languages[i].scriptLanguage = i;
    languages[11].scriptLanguage = 2;
    ll::validate(languages);
    for (unsigned i = 0; i < 5; ++i) assert(ll::scriptLanguage(languages, i) == i);
    for (unsigned i = 5; i < 11; ++i) assert(ll::scriptLanguage(languages, i) == 0);
    assert(ll::scriptLanguage(languages, 11) == 2);
    assert(ll::scriptLanguage(languages, 255) == 0);
    rejects({});
    rejects(std::vector<ll::Language>(65, languages[0]));
    for (const auto& filename : std::vector<std::string>{"../x.gxt", "x/y.gxt", "x\\y.gxt", "C:x.gxt", "", "a.txt", std::string(24, 'a') + ".gxt"}) {
        auto invalid = languages; invalid[0].filename = filename; rejects(invalid);
    }
    for (const std::string filename : {"arabic.gxt", "Arabic.gxt", "japanese.gxt", "korean.gxt"}) {
        auto invalid = languages; invalid[0].filename = filename; rejects(invalid);
    }
    auto removedProfile = languages; removedProfile[0].font = "arabic"; rejects(removedProfile);
    for (const std::string key : {"", "ABCDEFGH", "lower", "A/B"}) {
        auto invalid = languages; invalid[0].key = key; rejects(invalid);
    }
    auto invalid = languages; invalid[0].scriptLanguage = 5; rejects(invalid);
    invalid = languages; invalid[0].metric = 2; rejects(invalid);
    invalid = languages; invalid[0].name = "non-ASCII \xc3\xa9"; rejects(invalid);
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i], std::ios::binary);
        assert(f);
        std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
        ll::validateGxt(b);
        std::cout << "GXT structure passed: " << argv[i] << '\n';
        for (size_t size : {size_t(0), size_t(20), b.size() / 2}) {
            std::vector<uint8_t> truncated(b.begin(), b.begin() + size);
            bool rejected = false;
            try { ll::validateGxt(truncated); } catch (const std::exception&) { rejected = true; }
            assert(rejected);
        }
    }
    std::cout << "Core passed: every page for 1..64 languages; bounds, compatibility and config validation\n";
}

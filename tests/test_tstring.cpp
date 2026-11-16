#include "TString.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

namespace {

// TString's read API is the raw buffer; take a copy for comparing.
std::string Contents(const TString& s)
{
    return s.m_pData != NULL ? std::string(s.m_pData, static_cast<size_t>(s.m_nLen)) : std::string();
}

} // namespace

TEST_CASE("tstring delete removes a middle range", "[tstring]")
{
    TString s;
    s = "abcdefgh";

    TString& result = s.Delete(3, 2);

    CHECK(&result == &s);
    CHECK(Contents(s) == "abcfgh");
    CHECK(s.m_nLen == 6);
}

TEST_CASE("tstring delete clamps the count at the string end", "[tstring]")
{
    TString s;
    s = "abcdefgh";

    s.Delete(5, 100);

    CHECK(Contents(s) == "abcde");
    CHECK(s.m_nLen == 5);
}

TEST_CASE("tstring delete of everything empties the string", "[tstring]")
{
    TString s;
    s = "abc";

    s.Delete(0, 3);

    CHECK(Contents(s) == "");
    CHECK(s.m_nLen == 0);
    CHECK(s.m_pData[0] == 0);
}

TEST_CASE("tstring delete of zero characters keeps the string", "[tstring]")
{
    TString s;
    s = "abc";

    s.Delete(2, 0);

    CHECK(Contents(s) == "abc");
    CHECK(s.m_nLen == 3);
}

TEST_CASE("tstring delete past the end throws", "[tstring]")
{
    TString s;
    s = "abc";

    CHECK_THROWS_AS(s.Delete(4, 1), std::out_of_range);
    CHECK(Contents(s) == "abc");
}

TEST_CASE("tstring delete detaches a shared buffer", "[tstring]")
{
    TString original;
    original = "abcdefgh";
    TString copy(original);

    copy.Delete(2, 2);

    // The copy owns its own buffer now; the original keeps its text.
    CHECK(copy.m_pData != original.m_pData);
    CHECK(Contents(copy) == "abefgh");
    CHECK(Contents(original) == "abcdefgh");
}

TEST_CASE("tstring delete on a shared buffer keeps the sharing alive", "[tstring]")
{
    TString original;
    original = "abcd";
    TString copy(original);
    // A fresh buffer counts zero (sole owner); one copy bumps it to one.
    CHECK(static_cast<BYTE>(original.m_pData[-1]) == 1);
    CHECK(copy.m_pData == original.m_pData);

    original.Delete(1, 2);

    // Deleting everything worth sharing would drop the reference; a partial
    // delete detaches first, so the copy is untouched either way.
    CHECK(Contents(original) == "ad");
    CHECK(Contents(copy) == "abcd");
}

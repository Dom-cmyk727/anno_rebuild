#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include <anno/formats/json.hpp>
#include <anno/formats/sha256.hpp>

namespace js = anno::formats::json;
using anno::formats::Bytes;

namespace {

Bytes b(std::string_view s) {
    return Bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

bool accepts(std::string_view s) {
    return js::parse(b(s)).ok();
}

} // namespace

TEST_CASE("sha256: the FIPS 180-4 vectors") {
    CHECK(anno::formats::sha256_hex(b("")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(anno::formats::sha256_hex(b("abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(
        anno::formats::sha256_hex(b("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // The padding edge: 55, 56 and 64 bytes straddle the one- and two-block tails.
    CHECK(anno::formats::sha256_hex(b(std::string(55, 'a'))) ==
          "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(anno::formats::sha256_hex(b(std::string(56, 'a'))) ==
          "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(anno::formats::sha256_hex(b(std::string(64, 'a'))) ==
          "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(anno::formats::sha256_hex(b(std::string(1000000, 'a'))) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("json: values, kinds and the integer / real split") {
    auto v =
        js::parse(b(R"( {"a":[1,-2,0,-0,true,false,null,"x"],"b":{"c":1.5,"d":1e2,"e":-0.0}} )"));
    REQUIRE(v);
    const js::Value* a = v.value().find("a");
    REQUIRE(a != nullptr);
    REQUIRE(a->items.size() == 8);
    CHECK(a->items[0].kind == js::Kind::Integer);
    CHECK(a->items[1].integer == -2);
    CHECK(a->items[3].kind == js::Kind::Integer);
    CHECK(a->items[3].integer == 0);
    // A boolean is not an integer.
    CHECK(a->items[4].kind == js::Kind::Bool);
    CHECK(a->items[6].kind == js::Kind::Null);
    const js::Value* b_obj = v.value().find("b");
    REQUIRE(b_obj != nullptr);
    CHECK(b_obj->find("c")->kind == js::Kind::Real);
    CHECK(b_obj->find("d")->kind == js::Kind::Real);
    CHECK(b_obj->find("e")->kind == js::Kind::Real);
}

TEST_CASE("json: int64 bounds") {
    auto max = js::parse(b("9223372036854775807"));
    REQUIRE(max);
    CHECK(max.value().integer == INT64_MAX);
    auto min = js::parse(b("-9223372036854775808"));
    REQUIRE(min);
    CHECK(min.value().integer == INT64_MIN);
    CHECK_FALSE(accepts("9223372036854775808"));
    CHECK_FALSE(accepts("-9223372036854775809"));
    CHECK_FALSE(accepts("99999999999999999999999"));
}

TEST_CASE("json: the grammar rejects what RFC 8259 rejects") {
    for (const char* bad : {"",
                            " ",
                            "{",
                            "}",
                            "[",
                            "]",
                            "[1,]",
                            "{\"a\":1,}",
                            "{,}",
                            "[,1]",
                            "{\"a\"}",
                            "{\"a\":}",
                            "{a:1}",
                            "{'a':1}",
                            "01",
                            "-01",
                            "+1",
                            ".5",
                            "1.",
                            "1e",
                            "1e+",
                            "-",
                            "--1",
                            "NaN",
                            "Infinity",
                            "-Infinity",
                            "nul",
                            "tru",
                            "True",
                            "\"abc",
                            "\"a\nb\"",
                            "\"\\x\"",
                            "\"\\u12\"",
                            "\"\\u12g4\"",
                            "[1 2]",
                            "{\"a\":1 \"b\":2}",
                            "1 2",
                            "[] x",
                            "// c\n1",
                            "/* c */ 1",
                            "\xEF\xBB\xBF{}",
                            "\"\xC0\x80\"",
                            "\"\xED\xA0\x80\"",
                            "\"\xF5\x80\x80\x80\"",
                            "\"\xE2\x82\"",
                            "\"\x80\""}) {
        CAPTURE(bad);
        CHECK_FALSE(accepts(bad));
    }
    for (const char* good :
         {"{}", "[]", "null", "0", "-0", "1", "\"\"", " \t\r\n[ 1 , 2 ]\n", "{\"a\":{\"b\":[{}]}}",
          "1E5", "-1.25e-3", "\"\\/\\b\\f\\n\\r\\t\\\"\\\\\"", "\"\xC3\xA9\"",
          "\"\xF0\x9F\x98\x80\""}) {
        CAPTURE(good);
        CHECK(accepts(good));
    }
}

TEST_CASE("json: duplicate keys are an error at any depth") {
    CHECK_FALSE(accepts(R"({"a":1,"a":2})"));
    CHECK_FALSE(accepts(R"({"a":{"b":1,"b":1}})"));
    CHECK_FALSE(accepts(R"({"a":[{"b":1,"b":1}]})"));
    // The same key in two different objects is fine.
    CHECK(accepts(R"({"a":{"b":1},"c":{"b":1}})"));
    // Two spellings of one key are one key.
    CHECK_FALSE(accepts(R"({"a":1,"\u0061":2})"));
}

TEST_CASE("json: surrogate escapes") {
    auto pair = js::parse(b(R"("\ud83d\ude00")"));
    REQUIRE(pair);
    CHECK(pair.value().text == "\xF0\x9F\x98\x80");
    CHECK_FALSE(accepts(R"("\ud83d")"));
    CHECK_FALSE(accepts(R"("\ud83dx")"));
    CHECK_FALSE(accepts(R"("\ud83d\u0041")"));
    CHECK_FALSE(accepts(R"("\ude00")"));
    CHECK_FALSE(accepts(R"("\ude00\ud83d")"));
}

TEST_CASE("json: nesting is bounded") {
    std::string deep(65, '[');
    deep += std::string(65, ']');
    CHECK_FALSE(accepts(deep));
    std::string ok(64, '[');
    ok += std::string(64, ']');
    CHECK(accepts(ok));
    js::Limits wider;
    wider.max_depth = 100;
    CHECK(js::parse(b(deep), wider).ok());
    // A hostile depth does not recurse past the limit.
    CHECK_FALSE(accepts(std::string(1000000, '[')));
}

TEST_CASE("json: canonical form") {
    auto v = js::parse(b(
        R"({ "b" : [ 2 , 1 ] , "a" : {"z":null,"y":true} , "s":"\u00e9\ud83d\ude00\u007f\n\"\\/" })"));
    REQUIRE(v);
    CHECK(js::canonical(v.value()) ==
          R"({"a":{"y":true,"z":null},"b":[2,1],"s":"\u00e9\ud83d\ude00\u007f\n\"\\/"})");
    // The canonical form parses back to itself.
    const std::string once = js::canonical(v.value());
    auto again = js::parse(b(once));
    REQUIRE(again);
    CHECK(js::canonical(again.value()) == once);
}

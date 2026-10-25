#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/annowelt.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/test/game_dir.hpp>

namespace wt = anno::formats::annowelt;
using anno::formats::ByteBuffer;
using anno::formats::Bytes;

namespace {

void poke_u16(ByteBuffer& b, std::size_t o, std::uint16_t v) {
    // .at: GCC 13 -O3 -Wnull-dereference treats operator[] as a possible
    // null write on a freshly sized vector.
    b.at(o) = static_cast<std::uint8_t>(v & 0xFF);
    b.at(o + 1) = static_cast<std::uint8_t>(v >> 8);
}

void poke_u32(ByteBuffer& b, std::size_t o, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.at(o + static_cast<std::size_t>(i)) = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

struct Sec {
    std::uint32_t va = 0;
    ByteBuffer raw;
};

ByteBuffer make_pe(std::uint32_t stamp, std::uint32_t size_of_image, const std::vector<Sec>& secs) {
    constexpr std::uint16_t kOptSize = 0xE0;
    constexpr std::size_t kELfanew = 0x40;
    const std::size_t opt = kELfanew + 4 + 20;
    const std::size_t sec0 = opt + kOptSize;
    const auto nsec = static_cast<std::uint16_t>(secs.size());
    std::size_t ptr = 0x200;
    const std::size_t headers = sec0 + 40ull * nsec;
    if (ptr < headers) {
        ptr = headers;
    }
    std::vector<std::uint32_t> raw_ptr(nsec);
    std::size_t end = ptr;
    for (std::uint16_t i = 0; i < nsec; ++i) {
        raw_ptr[i] = static_cast<std::uint32_t>(end);
        end += secs[i].raw.size();
    }
    ByteBuffer b(end, 0);
    b.at(0) = 'M';
    b.at(1) = 'Z';
    poke_u32(b, 0x3C, static_cast<std::uint32_t>(kELfanew));
    b.at(kELfanew) = 'P';
    b.at(kELfanew + 1) = 'E';
    poke_u16(b, kELfanew + 4, 0x14C);
    poke_u16(b, kELfanew + 6, nsec);
    poke_u32(b, kELfanew + 8, stamp);
    poke_u16(b, kELfanew + 0x14, kOptSize);
    poke_u16(b, opt, 0x10B);
    poke_u32(b, opt + 0x1C, 0x10000000);
    poke_u32(b, opt + 0x38, size_of_image);
    for (std::uint16_t i = 0; i < nsec; ++i) {
        const std::size_t off = sec0 + 40ull * i;
        poke_u32(b, off + 0x0C, secs[i].va);
        poke_u32(b, off + 0x10, static_cast<std::uint32_t>(secs[i].raw.size()));
        poke_u32(b, off + 0x14, raw_ptr[i]);
        for (std::size_t k = 0; k < secs[i].raw.size(); ++k) {
            b.at(raw_ptr[i] + k) = secs[i].raw.at(k);
        }
    }
    return b;
}

ByteBuffer filled_rdata(const std::vector<std::uint16_t>& mapper,
                        const std::vector<std::array<wt::GoalEntry, wt::kGoalOrderWidth>>& goal) {
    ByteBuffer rdata(0x2000, 0);
    const std::size_t map_at = wt::kRvaMapper - 0xC1000;
    for (std::size_t i = 0; i < mapper.size(); ++i) {
        poke_u16(rdata, map_at + i * 2, mapper[i]);
    }
    const std::size_t goal_at = wt::kRvaGoalOrder - 0xC1000;
    for (std::size_t r = 0; r < goal.size(); ++r) {
        for (std::size_t k = 0; k < wt::kGoalOrderWidth; ++k) {
            const std::size_t off = goal_at + (r * wt::kGoalOrderWidth + k) * 4;
            poke_u16(rdata, off, goal[r][k].mask);
            rdata.at(off + 2) = goal[r][k].col;
            rdata.at(off + 3) = goal[r][k].row;
        }
    }
    return rdata;
}

ByteBuffer filled_data(const std::vector<std::vector<std::uint8_t>>& layers) {
    ByteBuffer data(0x9000, 0);
    const std::size_t base = wt::kRvaLayers - 0xD1000;
    for (std::size_t L = 0; L < layers.size(); ++L) {
        for (std::size_t i = 0; i < layers[L].size(); ++i) {
            data.at(base + L * wt::kLayerSize + i) = layers[L][i];
        }
    }
    return data;
}

wt::WalkTablesError err_of(Bytes dll) {
    auto t = wt::read_walk_tables(dll);
    REQUIRE_FALSE(t);
    return t.error();
}

} // namespace

TEST_CASE("synthetic pe round-trips every field") {
    std::vector<std::uint16_t> mapper(wt::kMapperCount);
    for (std::size_t i = 0; i < mapper.size(); ++i) {
        mapper[i] = static_cast<std::uint16_t>((i * 7u) & 0x0FFFu);
    }
    std::vector<std::vector<std::uint8_t>> layers(wt::kLayerCount,
                                                  std::vector<std::uint8_t>(wt::kLayerSize, 0));
    for (std::size_t L = 0; L < layers.size(); ++L) {
        for (std::size_t i = 0; i < layers[L].size(); ++i) {
            layers[L][i] = static_cast<std::uint8_t>((L * 13u + i) & 0xFFu);
        }
    }
    std::vector<std::array<wt::GoalEntry, wt::kGoalOrderWidth>> goal(wt::kGoalOrderRows);
    for (std::size_t r = 0; r < goal.size(); ++r) {
        for (std::size_t k = 0; k < wt::kGoalOrderWidth; ++k) {
            goal[r][k] = {static_cast<std::uint16_t>(r * 16u + k + 1u),
                          static_cast<std::uint8_t>(k % 3u), static_cast<std::uint8_t>(r % 3u)};
        }
    }
    const ByteBuffer pe =
        make_pe(wt::kGogTimeDateStamp, wt::kGogSizeOfImage,
                {{0xC1000, filled_rdata(mapper, goal)}, {0xD1000, filled_data(layers)}});
    auto t = wt::read_walk_tables(pe);
    REQUIRE(t);
    CHECK(t.value().time_date_stamp == wt::kGogTimeDateStamp);
    CHECK(t.value().size_of_image == wt::kGogSizeOfImage);
    CHECK(t.value().mapper == mapper);
    CHECK(t.value().layers == layers);
    REQUIRE(t.value().goal_order.size() == goal.size());
    for (std::size_t r = 0; r < goal.size(); ++r) {
        for (std::size_t k = 0; k < wt::kGoalOrderWidth; ++k) {
            CHECK(t.value().goal_order[r][k].mask == goal[r][k].mask);
            CHECK(t.value().goal_order[r][k].col == goal[r][k].col);
            CHECK(t.value().goal_order[r][k].row == goal[r][k].row);
        }
    }
}

TEST_CASE("not a PE") {
    CHECK(err_of({}) == wt::WalkTablesError::NotPe);
    CHECK(err_of(ByteBuffer{'M', 'Z'}) == wt::WalkTablesError::NotPe);
    ByteBuffer mz(0x80, 0);
    mz.at(0) = 'M';
    mz.at(1) = 'Z';
    poke_u32(mz, 0x3C, 0x40);
    CHECK(err_of(mz) == wt::WalkTablesError::NotPe);
}

TEST_CASE("wrong build") {
    const ByteBuffer pe =
        make_pe(wt::kGogTimeDateStamp + 1, wt::kGogSizeOfImage,
                {{0xC1000, ByteBuffer(0x2000, 0)}, {0xD1000, ByteBuffer(0x9000, 0)}});
    CHECK(err_of(pe) == wt::WalkTablesError::WrongBuild);
    const ByteBuffer pe2 =
        make_pe(wt::kGogTimeDateStamp, wt::kGogSizeOfImage + 1,
                {{0xC1000, ByteBuffer(0x2000, 0)}, {0xD1000, ByteBuffer(0x9000, 0)}});
    CHECK(err_of(pe2) == wt::WalkTablesError::WrongBuild);
}

TEST_CASE("RVA outside section") {
    const ByteBuffer pe =
        make_pe(wt::kGogTimeDateStamp, wt::kGogSizeOfImage, {{0x1000, ByteBuffer(0x200, 0)}});
    CHECK(err_of(pe) == wt::WalkTablesError::RvaOutsideSection);
}

TEST_CASE("short file") {
    ByteBuffer pe = make_pe(wt::kGogTimeDateStamp, wt::kGogSizeOfImage,
                            {{0xC1000, ByteBuffer(0x2000, 0)}, {0xD1000, ByteBuffer(0x9000, 0)}});
    pe.resize(0x400);
    CHECK(err_of(pe) == wt::WalkTablesError::ShortFile);
}

TEST_CASE("walk tables corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping walk tables corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "AnnoWelt.dll");
    auto t = wt::read_walk_tables(buf);
    REQUIRE(t);
    const wt::WalkTables& tables = t.value();
    CHECK(tables.time_date_stamp == wt::kGogTimeDateStamp);
    CHECK(tables.size_of_image == wt::kGogSizeOfImage);
    REQUIRE(tables.mapper.size() == wt::kMapperCount);
    std::set<std::uint16_t> distinct(tables.mapper.begin(), tables.mapper.end());
    CHECK(distinct.size() == 33);
    for (std::uint16_t v : tables.mapper) {
        CHECK((v & 0xF000u) == 0);
    }
    REQUIRE(tables.layers.size() == wt::kLayerCount);
    std::vector<std::size_t> admitted;
    for (std::size_t L = 0; L < tables.layers.size(); ++L) {
        REQUIRE(tables.layers[L].size() == wt::kLayerSize);
        CHECK(tables.layers[L][7] == 0);
        for (unsigned c = 0x80; c <= 0x86; ++c) {
            CHECK(tables.layers[L][c] == 0);
        }
        std::size_t n = 0;
        for (std::size_t c = 0; c < wt::kAdmissionCount; ++c) {
            n += tables.layers[L][c] != 0 ? 1u : 0u;
        }
        admitted.push_back(n);
    }
    CHECK(tables.layers[0][2] == 0xFF);
    CHECK(tables.layers[3][2] == 0);
    CHECK(tables.layers[0][wt::kAdmissionCount] == 10); // C_0(0) layer 0
    CHECK(tables.layers[3][wt::kAdmissionCount] == 10); // C_0(0) layer 3
    CHECK(tables.layers[3][4] == 0xFF);
    CHECK(tables.layers[3][wt::kAdmissionCount + wt::kCostStride * 4] == 40); // C_0(4) layer 3
    REQUIRE(tables.goal_order.size() == wt::kGoalOrderRows);
    MESSAGE("AnnoWelt.dll walk tables: mapper distinct=", distinct.size(),
            " L0..4 admitted=", admitted[0], "/", admitted[1], "/", admitted[2], "/", admitted[3],
            "/", admitted[4]);
}

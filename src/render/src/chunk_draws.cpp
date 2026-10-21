#include <algorithm>
#include <optional>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/scene.hpp>

namespace anno::render {

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void put_i32(std::vector<std::uint8_t>& out, std::int32_t v) {
    put_u32(out, static_cast<std::uint32_t>(v));
}

std::uint32_t get_u32(std::span<const std::uint8_t> b, std::size_t at) {
    return std::uint32_t{b[at]} | std::uint32_t{b[at + 1]} << 8 | std::uint32_t{b[at + 2]} << 16 |
           std::uint32_t{b[at + 3]} << 24;
}

std::int32_t get_i32(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::int32_t>(get_u32(b, at));
}

} // namespace

void append_object_record(std::vector<std::uint8_t>& out, const ObjectRecord& r) {
    put_i32(out, r.x);
    put_i32(out, r.y);
    put_u32(out, r.frame);
    put_u32(out, r.flags);
    put_u32(out, r.clip);
    put_u32(out, r.colour);
    put_i32(out, r.depth);
    put_u32(out, 0);
}

ObjectRecord read_object_record(std::span<const std::uint8_t> list, std::size_t index) {
    const std::size_t at = index * kObjectRecordBytes;
    ObjectRecord r;
    r.x = get_i32(list, at);
    r.y = get_i32(list, at + 4);
    r.frame = get_u32(list, at + 8);
    r.flags = get_u32(list, at + 12);
    r.clip = get_u32(list, at + 16);
    r.colour = get_u32(list, at + 20);
    r.depth = get_i32(list, at + 24);
    return r;
}

void append_ground_record(std::vector<std::uint8_t>& out, const GroundRecord& r) {
    put_i32(out, r.ox);
    put_i32(out, r.oy);
    put_u32(out, r.texture);
    put_u32(out, r.cell);
    put_u32(out, std::uint32_t{r.h_a} | std::uint32_t{r.h_c} << 16);
    put_u32(out, std::uint32_t{r.h_b} | std::uint32_t{r.h_d} << 16);
    put_u32(out, r.clip);
    put_u32(out, 0);
}

GroundRecord read_ground_record(std::span<const std::uint8_t> list, std::size_t index) {
    const std::size_t at = index * kGroundRecordBytes;
    GroundRecord r;
    r.ox = get_i32(list, at);
    r.oy = get_i32(list, at + 4);
    r.texture = get_u32(list, at + 8);
    r.cell = get_u32(list, at + 12);
    const std::uint32_t h01 = get_u32(list, at + 16), h23 = get_u32(list, at + 20);
    r.h_a = static_cast<std::uint16_t>(h01 & 0xFFFFU);
    r.h_c = static_cast<std::uint16_t>(h01 >> 16);
    r.h_b = static_cast<std::uint16_t>(h23 & 0xFFFFU);
    r.h_d = static_cast<std::uint16_t>(h23 >> 16);
    r.clip = get_u32(list, at + 24);
    return r;
}

std::vector<std::uint8_t> clip_table_bytes(std::span<const Rect> clips) {
    std::vector<std::uint8_t> out;
    out.reserve(clips.size() * 16);
    for (const Rect& c : clips) {
        put_i32(out, c.x);
        put_i32(out, c.y);
        put_i32(out, c.x + c.w);
        put_i32(out, c.y + c.h);
    }
    return out;
}

ChunkDrawsBuilder::ChunkDrawsBuilder(Rect rect, std::size_t group, bool bases, int base_band,
                                     std::uint32_t bin_px) {
    draws_.rect = rect;
    draws_.group = group;
    draws_.bases = bases;
    draws_.base_band = base_band;
    // A bin of 0 pixels would divide by zero; 1 is the finest bin there is.
    draws_.bin_px = std::max<std::uint32_t>(bin_px, 1);
}

void ChunkDrawsBuilder::begin_island(Rect clip) {
    pending_ = clip;
    has_pending_ = true;
    unclipped_ = false;
}

void ChunkDrawsBuilder::begin_unclipped() {
    has_pending_ = false;
    unclipped_ = true;
    clip_ = kClipNone;
}

void ChunkDrawsBuilder::add(ObjectRecord r) {
    if (has_pending_) {
        draws_.clips.push_back(pending_);
        clip_ = static_cast<std::uint32_t>(draws_.clips.size() - 1);
        has_pending_ = false;
    }
    r.clip = clip_;
    ids_.push_back(r.frame);
    append_object_record(draws_.objects, r);
}

ChunkDraws ChunkDrawsBuilder::finish(const FrameTable& frames) {
    ChunkDraws d = std::move(draws_);
    std::sort(ids_.begin(), ids_.end());
    ids_.erase(std::unique(ids_.begin(), ids_.end()), ids_.end());
    d.frame_set = std::move(ids_);
    const std::size_t n = d.object_count();
    if (n == 0 || d.rect.w <= 0 || d.rect.h <= 0) {
        return d;
    }
    const auto bin = static_cast<int>(d.bin_px);
    d.bins_x = static_cast<std::uint32_t>((d.rect.w + bin - 1) / bin);
    d.bins_y = static_cast<std::uint32_t>((d.rect.h + bin - 1) / bin);
    const std::size_t bins = std::size_t{d.bins_x} * d.bins_y;
    // The bins one record covers: its image box (the image may be wider than
    // the frame box) within its clip and the chunk; none when the box is out.
    struct Span {
        int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;
    };
    std::vector<Span> spans(n);
    std::vector<std::shared_ptr<const FrameData>> cache(d.frame_set.size());
    for (std::size_t i = 0; i < n; ++i) {
        const ObjectRecord r = d.object(i);
        const auto at = static_cast<std::size_t>(
            std::lower_bound(d.frame_set.begin(), d.frame_set.end(), r.frame) -
            d.frame_set.begin());
        if (!cache[at]) {
            cache[at] = frames.get(r.frame);
        }
        if (!cache[at] || !cache[at]->image) {
            continue;
        }
        const Rect box{r.x, r.y, static_cast<int>(cache[at]->width()),
                       static_cast<int>(cache[at]->height())};
        std::optional<Rect> in = box.intersect(d.rect);
        if (in && r.clip != kClipNone) {
            in = in->intersect(d.clips[r.clip]);
        }
        if (!in) {
            continue;
        }
        spans[i] = {(in->x - d.rect.x) / bin, (in->y - d.rect.y) / bin,
                    (in->x + in->w - 1 - d.rect.x) / bin, (in->y + in->h - 1 - d.rect.y) / bin};
    }
    d.bin_offsets.assign(bins + 1, 0);
    const auto bx = static_cast<std::size_t>(d.bins_x);
    for (const Span& s : spans) {
        for (int y = s.by0; y <= s.by1; ++y) {
            for (int x = s.bx0; x <= s.bx1; ++x) {
                ++d.bin_offsets[static_cast<std::size_t>(y) * bx + static_cast<std::size_t>(x) + 1];
            }
        }
    }
    for (std::size_t b = 0; b < bins; ++b) {
        d.bin_offsets[b + 1] += d.bin_offsets[b];
    }
    d.bin_indices.resize(d.bin_offsets[bins]);
    std::vector<std::uint32_t> next(d.bin_offsets.begin(), d.bin_offsets.end() - 1);
    for (std::size_t i = 0; i < n; ++i) {
        const Span& s = spans[i];
        for (int y = s.by0; y <= s.by1; ++y) {
            for (int x = s.bx0; x <= s.bx1; ++x) {
                const std::size_t b =
                    static_cast<std::size_t>(y) * bx + static_cast<std::size_t>(x);
                d.bin_indices[next[b]++] = static_cast<std::uint32_t>(i);
            }
        }
    }
    return d;
}

Canvas replay(const ChunkDraws& draws, const FrameTable& frames, std::size_t* missing) {
    Canvas canvas = blank(draws.rect);
    if (draws.bases) {
        canvas.base.assign(canvas.width * canvas.height, 255);
        canvas.base_band = draws.base_band;
    }
    std::vector<std::shared_ptr<const FrameData>> data(draws.frame_set.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = frames.get(draws.frame_set[i]);
    }
    // The island segment being drawn: its local canvas, composited at its end.
    std::unique_ptr<Canvas> local;
    std::uint32_t segment = kClipNone;
    auto close = [&] {
        if (local) {
            composite(canvas, *local, {0, 0});
            local.reset();
        }
    };
    const auto all = [](int, int, Px px) -> std::optional<Px> { return px; };
    const std::size_t n = draws.object_count();
    for (std::size_t i = 0; i < n; ++i) {
        const ObjectRecord r = draws.object(i);
        Canvas* target = &canvas;
        if (r.clip == kClipNone) {
            close();
        } else {
            if (!local || r.clip != segment) {
                close();
                const Rect& c = draws.clips.at(r.clip);
                local = std::make_unique<Canvas>(blank(c));
                if (draws.bases) {
                    local->base.assign(local->width * local->height, 255);
                    local->base_band = draws.base_band;
                }
                segment = r.clip;
            }
            target = &*local;
        }
        const auto at = static_cast<std::size_t>(
            std::lower_bound(draws.frame_set.begin(), draws.frame_set.end(), r.frame) -
            draws.frame_set.begin());
        const FrameData* f =
            at < data.size() && draws.frame_set[at] == r.frame ? data[at].get() : nullptr;
        if (f == nullptr || !f->image) {
            if (missing != nullptr) {
                ++*missing;
            }
            continue;
        }
        const bool shadow = (r.flags & object_flags::kShadow) != 0;
        const bool track = (r.flags & object_flags::kTrackBase) != 0 && !target->base.empty();
        blit_sprite_keel(*target, *f->image, f->header,
                         {r.x + f->header.anchor_x, r.y + f->header.anchor_y}, shadow, track,
                         f->keel, all);
    }
    close();
    return canvas;
}

} // namespace anno::render

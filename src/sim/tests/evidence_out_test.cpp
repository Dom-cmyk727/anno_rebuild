// MAP-BUILD-1 A: the checked evidence writer, at the stream seam.
#include <ios>
#include <sstream>
#include <streambuf>
#include <string>

#include <doctest/doctest.h>

#include "evidence_out.hpp"

namespace {

/// A buffer that accepts `limit` bytes and then fails every write, like a full disk.
class FullDisk : public std::streambuf {
  public:
    explicit FullDisk(std::streamsize limit) : left_(limit) {}

  protected:
    std::streamsize xsputn(const char*, std::streamsize n) override {
        const std::streamsize take = n < left_ ? n : left_;
        left_ -= take;
        return take;
    }
    int_type overflow(int_type) override { return traits_type::eof(); }

  private:
    std::streamsize left_;
};

/// Accepts every byte but fails the final flush, like a disk that reports the error late.
class FlushFails : public std::streambuf {
  protected:
    std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
    int_type overflow(int_type c) override { return c; }
    int sync() override { return -1; }
};

} // namespace

TEST_CASE("evidence writer: a stream that runs out of space is an error") {
    const std::string payload(64, 'x');
    {
        std::ostringstream ok;
        CHECK(anno::sim::test::write_stream_checked(ok, payload, "ok").empty());
        CHECK(ok.str() == payload);
    }
    FullDisk disk(10);
    std::ostream bad(&disk);
    const std::string err = anno::sim::test::write_stream_checked(bad, payload, "disk.bin");
    CHECK(err == "disk.bin: write failed");
}

TEST_CASE("evidence writer: a stream already in a failed state is an error") {
    std::ostringstream os;
    os.setstate(std::ios::badbit);
    CHECK(anno::sim::test::write_stream_checked(os, "abc", "x") == "x: write failed");
}

TEST_CASE("evidence writer: a flush that fails after accepted writes is an error") {
    FlushFails buf;
    std::ostream os(&buf);
    CHECK(anno::sim::test::write_stream_checked(os, "abc", "late.bin") == "late.bin: flush failed");
}

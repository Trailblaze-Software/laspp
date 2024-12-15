#include "laz/stream.hpp"

using namespace laspp;

int main() {
  std::stringstream ss;

  {
    OutStream out_stream(ss);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), std::numeric_limits<uint32_t>::max());

    out_stream.update_range(0, 1u << 31);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_EQ(ss.str(), "");

    out_stream.update_range(10, 20);
    LASPP_ASSERT_EQ(out_stream.length(), 10u);
    LASPP_ASSERT_EQ(out_stream.get_base(), 10u << 24);
    LASPP_ASSERT_EQ(out_stream.length(), 10u << 24);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0}));

    out_stream.update_range((10 << 24) - 1, (10 << 24));
    LASPP_ASSERT_EQ(out_stream.get_base(), 0xff000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 24);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255}));

    out_stream.update_range((1 << 24) - (1 << 16), 1 << 24);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0xff000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 24);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255}));

    out_stream.update_range(1 << 23, 1 << 24);

    LASPP_ASSERT_EQ(out_stream.length(), 1u << 23);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0x80000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255}));

    out_stream.update_range(0x4000FFFF, 0x80000000);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0xC000FFFF);
    LASPP_ASSERT_EQ(out_stream.length(), (1u << 31) - 0x4000FFFF);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255}));

    out_stream.update_range(0, 1 << 7);

    LASPP_ASSERT_EQ(out_stream.length(), 1u << 7);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0xFF000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 0, 0xFF}));

    out_stream.update_range(1 << 24, 1 << 25);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 24);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0}));

    out_stream.update_range(1 << 23, 1 << 24);

    LASPP_ASSERT_EQ(out_stream.length(), 1u << 23);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0x80000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0}));

    out_stream.update_range(0x40FFFFFF, 0x80000000);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0xC0FFFFFF);
    LASPP_ASSERT_EQ(out_stream.length(), (1u << 31) - 0x40FFFFFF);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0}));

    out_stream.update_range(0, 1 << 7);

    LASPP_ASSERT_EQ(out_stream.length(), 1u << 7);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0xFF000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 31);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC0, 255, 255}));

    out_stream.update_range(1 << 24, 1 << 25);

    LASPP_ASSERT_EQ(out_stream.get_base(), 0u);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 24);

    LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC1, 0, 0}));

    out_stream.update_range((1 << 24) - 1, 1 << 24);

    LASPP_ASSERT_EQ(out_stream.length(), 1u);
    LASPP_ASSERT_EQ(out_stream.get_base(), 0xFF000000);
    LASPP_ASSERT_EQ(out_stream.length(), 1u << 24);

    LASPP_ASSERT_RAW_STR_EQ(
        ss, ({0, 0, 0, 19, 255, 255, 255, 255, 0xC0, 1, 0, 0, 0xC1, 0, 0, 0, 255, 255}));
  }

  LASPP_ASSERT_RAW_STR_EQ(ss, ({0, 0,    0, 19, 255, 255, 255, 255, 0xC0, 1, 0,
                                0, 0xC1, 0, 0,  0,   255, 255, 255, 128,  0, 0}));
  LASPP_ASSERT_EQ(ss.tellg(), 0);

  return 0;
}

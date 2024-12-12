#pragma once

#ifdef __GNUC__
#define LASPP_PACKED __attribute__((packed))
#else
#define LASPP_PACKED
#endif

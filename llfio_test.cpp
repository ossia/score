#define LLFIO_HEADERS_ONLY 1
#define LLFIO_EXPERIMENTAL_STATUS_CODE 1
#define LLFIO_DISABLE_OPENSSL 1
#define QUICKCPPLIB_USE_STD_SPAN 1
// #define OUTCOME_USE_SYSTEM_STATUS_CODE 0

#if !defined(__has_feature)
#define __has_feature(T) 0
#endif
#if !defined(__has_extension)
#define __has_extension(T) 0
#endif

#include <llfio.hpp>

int main() {
    return 0;
}

#include <stdint.h>
#include <string.h>

namespace ErrorHandler {
namespace PHP {

enum class ErrorType : uint8_t { NONE = 0, FUNCTION, EXPECT_ID };
struct Err {
  uint16_t start;
  ErrorType type;
  uint8_t Len;
};

char ErrBuffer[4096];

// Minimum Err len is 32, maximum 256
Err ErrIdxPool[32];
static uint8_t FilledSlot = 0;
static uint16_t ErrIdx = 0;

// Max Slots per Type are 3
// Minimum Err len is 32, maximum 256
uint8_t setError(ErrorType e, const char *err, uint8_t errLen) {
  if (errLen + ErrIdx >= 4096)
    return 0;
  ErrIdxPool[ErrIdx] = {ErrIdx, e, errLen};

  memcpy(ErrBuffer, err, errLen);
  ErrIdx += errLen;
  FilledSlot += 1;

  return FilledSlot;
}

} // namespace PHP
} // namespace ErrorHandler

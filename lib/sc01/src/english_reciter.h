#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal spelling-rule converter, called serially from the firmware loop.
 * Accepts up to 253 input bytes. Returns zero if output does not fit.
 * SAM-derived text rules only; no SAM voice generation. See THIRD_PARTY.md. */
int reciteEnglish(const char *text, char *output, size_t outputSize);

#ifdef __cplusplus
}
#endif

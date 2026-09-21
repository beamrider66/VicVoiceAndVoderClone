#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* English -> SC-01 text-to-phoneme converter (a 1,288-entry Votrax SC-01
 * dictionary, a small exact-word override table, the NRL letter-to-sound rules
 * and the NRL/Votrax IPA-to-SC-01 mapping), called serially from the firmware
 * loop. Accepts up to 253 input bytes and writes space-separated SC-01 phoneme
 * names (no trailing STOP). Returns 1 on success, 0 if output does not fit.
 * See THIRD_PARTY.md. */
int reciteEnglish(const char *text, char *output, size_t outputSize);

#ifdef __cplusplus
}
#endif

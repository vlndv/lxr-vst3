#ifndef P16D_ORIGINAL_PARAMETER_LOCK_ADAPTER_H
#define P16D_ORIGINAL_PARAMETER_LOCK_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define P16D_ORIGINAL_TRACKS 7
#define P16D_ORIGINAL_SLOTS 2
#define P16D_ORIGINAL_PARAMETERS 228
#define P16D_ORIGINAL_MAX_WRITES 32

typedef struct P16DOriginalWrite
{
    uint16_t parameter;
    uint8_t value;
    uint8_t status;
} P16DOriginalWrite;

void p16d_original_init(const uint8_t base[P16D_ORIGINAL_PARAMETERS]);
void p16d_original_update_base(uint16_t parameter, uint8_t value);
void p16d_original_pair(uint8_t track,
                         uint8_t parameter1, uint8_t value1,
                         uint8_t parameter2, uint8_t value2);
uint8_t p16d_original_effective(uint16_t parameter);
uint16_t p16d_original_destination(uint8_t track, uint8_t slot);
size_t p16d_original_write_count(void);
P16DOriginalWrite p16d_original_write(size_t index);

#ifdef __cplusplus
}
#endif

#endif

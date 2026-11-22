#ifndef SW_VDYN_POLICY_H
#define SW_VDYN_POLICY_H
#include <stdbool.h>
#include <stdint.h>

// 2-level dynamic vertex/index buffer policy over #815 and #956:
//   level 0: read in place
//   level 1: marked by <= 4 KB dynamic data; copies fetches <= 64 KB
//   level 2: marked by strictly in-flight > 4 KB (<= 256 KB) dynamic data; copies fetches <= 256 KB
#define VDYN_SAMPLE_MAX 4096u
#define VDYN_COPY_MAX_SMALL (64u << 10)
#define VDYN_MID_MAX (256u << 10)
#define VDYN_HITS 3u
#define VDYN_N 16384u
#define VDYN_SMASK 15u

static inline bool vdyn_level_copies(uint8_t level, uint64_t size) {
    if (level == 1) return size <= VDYN_COPY_MAX_SMALL;
    if (level == 2) return size <= VDYN_MID_MAX;
    return false;
}

static inline bool vdyn_should_sample(uint64_t size, bool gw, bool first_seen, uint64_t *tick) {
    if (!size || size > VDYN_MID_MAX) return false;
    if (gw || first_seen) return true;
    return tick && (++*tick & VDYN_SMASK) == 0;
}

static inline uint8_t vdyn_record_hit(uint8_t cur_level, uint8_t *hits, uint64_t *last_frame,
                                      uint64_t frame, bool loading, uint64_t orig_size,
                                      bool still_inflight) {
    if (loading || !orig_size || orig_size > VDYN_MID_MAX) return cur_level;
    if (orig_size > VDYN_SAMPLE_MAX && !still_inflight) return cur_level;
    if (*hits && frame == *last_frame) return cur_level;
    *hits = (*hits && frame >= *last_frame && frame - *last_frame <= 60)
                ? (*hits < 255 ? (uint8_t)(*hits + 1) : 255)
                : 1;
    *last_frame = frame;
    if (*hits < VDYN_HITS) return cur_level;
    uint8_t target = orig_size <= VDYN_SAMPLE_MAX ? 1 : 2;
    return target > cur_level ? target : cur_level;
}
#endif

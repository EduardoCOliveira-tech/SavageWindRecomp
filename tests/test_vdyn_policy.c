#include <assert.h>
#include <stdio.h>
#include "nvn/vulkan/vdyn_policy.h"

static void test_level_copy_caps(void) {
    assert(!vdyn_level_copies(0, 0));
    assert(!vdyn_level_copies(0, 1));
    assert(!vdyn_level_copies(0, 4096));
    assert(!vdyn_level_copies(0, VDYN_COPY_MAX_SMALL));
    assert(!vdyn_level_copies(0, 67200));
    assert(!vdyn_level_copies(0, VDYN_MID_MAX));

    assert(vdyn_level_copies(1, 4096));
    assert(vdyn_level_copies(1, 54400));
    assert(vdyn_level_copies(1, VDYN_COPY_MAX_SMALL));
    assert(!vdyn_level_copies(1, VDYN_COPY_MAX_SMALL + 1));
    assert(!vdyn_level_copies(1, 67200));
    assert(!vdyn_level_copies(1, VDYN_MID_MAX));

    assert(vdyn_level_copies(2, 4096));
    assert(vdyn_level_copies(2, 54400));
    assert(vdyn_level_copies(2, 67200));
    assert(vdyn_level_copies(2, VDYN_MID_MAX));
    assert(!vdyn_level_copies(2, VDYN_MID_MAX + 1));

    assert(!vdyn_level_copies(3, 4096));
}

static void test_loading_and_post_completion_rejected(void) {
    uint8_t lvl = 0, hits = 0;
    uint64_t last = 0;

    for (uint64_t f = 10; f < 15; f++) {
        lvl = vdyn_record_hit(lvl, &hits, &last, f, true, 1840, true);
        assert(lvl == 0 && hits == 0 && last == 0);
        lvl = vdyn_record_hit(lvl, &hits, &last, f, true, 67200, true);
        assert(lvl == 0 && hits == 0 && last == 0);
    }

    for (uint64_t f = 20; f < 25; f++) {
        lvl = vdyn_record_hit(lvl, &hits, &last, f, false, 67200, false);
        assert(lvl == 0 && hits == 0 && last == 0);
        lvl = vdyn_record_hit(lvl, &hits, &last, f, false, VDYN_SAMPLE_MAX + 1, false);
        assert(lvl == 0 && hits == 0 && last == 0);
        lvl = vdyn_record_hit(lvl, &hits, &last, f, false, VDYN_MID_MAX, false);
        assert(lvl == 0 && hits == 0 && last == 0);
    }

    for (uint64_t f = 30; f < 35; f++) {
        lvl = vdyn_record_hit(lvl, &hits, &last, f, false, 0, true);
        assert(lvl == 0 && hits == 0 && last == 0);
        lvl = vdyn_record_hit(lvl, &hits, &last, f, false, VDYN_MID_MAX + 1, true);
        assert(lvl == 0 && hits == 0 && last == 0);
    }
}

static void test_small_and_mid_promotion_and_upgrade(void) {
    uint8_t lvl = 0, hits_small = 0, hits_mid = 0;
    uint64_t last_small = 0, last_mid = 0;

    lvl = vdyn_record_hit(lvl, &hits_small, &last_small, 10, false, 1840, false);
    assert(lvl == 0 && hits_small == 1);
    lvl = vdyn_record_hit(lvl, &hits_small, &last_small, 10, false, 1840, false);
    assert(lvl == 0 && hits_small == 1);
    lvl = vdyn_record_hit(lvl, &hits_small, &last_small, 11, false, 1840, false);
    assert(lvl == 0 && hits_small == 2);
    lvl = vdyn_record_hit(lvl, &hits_small, &last_small, 12, false, 1840, false);
    assert(lvl == 1 && hits_small == 3);
    assert(vdyn_level_copies(lvl, 1840));
    assert(!vdyn_level_copies(lvl, 67200));

    lvl = vdyn_record_hit(lvl, &hits_mid, &last_mid, 20, false, 67200, true);
    assert(lvl == 1 && hits_mid == 1);
    lvl = vdyn_record_hit(lvl, &hits_mid, &last_mid, 21, false, 67200, true);
    assert(lvl == 1 && hits_mid == 2);
    lvl = vdyn_record_hit(lvl, &hits_mid, &last_mid, 22, false, 67200, true);
    assert(lvl == 2 && hits_mid == 3);
    assert(vdyn_level_copies(lvl, 67200));

    for (uint64_t f = 23; f < 26; f++) {
        lvl = vdyn_record_hit(lvl, &hits_small, &last_small, f, false, 496, false);
        assert(lvl == 2);
    }

    uint8_t gap_lvl = 0, gap_hits = 0;
    uint64_t gap_last = 0;
    gap_lvl = vdyn_record_hit(gap_lvl, &gap_hits, &gap_last, 100, false, 54400, true);
    assert(gap_lvl == 0 && gap_hits == 1);
    gap_lvl = vdyn_record_hit(gap_lvl, &gap_hits, &gap_last, 101, false, 54400, true);
    assert(gap_lvl == 0 && gap_hits == 2);
    gap_lvl = vdyn_record_hit(gap_lvl, &gap_hits, &gap_last, 162, false, 54400, true);
    assert(gap_lvl == 0 && gap_hits == 1);
    gap_lvl = vdyn_record_hit(gap_lvl, &gap_hits, &gap_last, 222, false, 54400, true);
    assert(gap_lvl == 0 && gap_hits == 2);
    gap_lvl = vdyn_record_hit(gap_lvl, &gap_hits, &gap_last, 223, false, 54400, true);
    assert(gap_lvl == 2 && gap_hits == 3);

    uint8_t f0_lvl = 0, f0_hits = 0;
    uint64_t f0_last = 0;
    f0_lvl = vdyn_record_hit(f0_lvl, &f0_hits, &f0_last, 0, false, 496, false);
    assert(f0_lvl == 0 && f0_hits == 1);
    f0_lvl = vdyn_record_hit(f0_lvl, &f0_hits, &f0_last, 0, false, 496, false);
    assert(f0_lvl == 0 && f0_hits == 1);
    f0_lvl = vdyn_record_hit(f0_lvl, &f0_hits, &f0_last, 1, false, 496, false);
    assert(f0_lvl == 0 && f0_hits == 2);
    f0_lvl = vdyn_record_hit(f0_lvl, &f0_hits, &f0_last, 2, false, 496, false);
    assert(f0_lvl == 1 && f0_hits == 3);
}

static void test_gw_overlaps_sampling_bypass(void) {
    uint64_t tick = 0;

    for (int i = 0; i < 32; i++) {
        assert(vdyn_should_sample(67200, true, false, &tick));
        assert(tick == 0);
    }

    assert(vdyn_should_sample(67200, false, true, &tick));
    assert(tick == 0);

    int sampled = 0;
    for (int i = 0; i < 32; i++) {
        if (vdyn_should_sample(67200, false, false, &tick)) sampled++;
    }
    assert(tick == 32);
    assert(sampled == 2);

    assert(!vdyn_should_sample(0, true, true, &tick));
    assert(!vdyn_should_sample(VDYN_MID_MAX + 1, true, true, &tick));
    assert(tick == 32);
}

int main(void) {
    test_level_copy_caps();
    test_loading_and_post_completion_rejected();
    test_small_and_mid_promotion_and_upgrade();
    test_gw_overlaps_sampling_bypass();
    puts("vdyn policy tests: PASS");
    return 0;
}

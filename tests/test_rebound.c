#include "trackpoint_scroll/rebound.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void
assert_close(double actual, double expected)
{
    assert(fabs(actual - expected) <= 1e-12);
}

static struct tpsc_rebound_filter *
make_filter(void)
{
    struct tpsc_rebound_config cfg;
    struct tpsc_rebound_filter *filter;
    int status = 123;

    tpsc_rebound_config_defaults(&cfg);
    filter = tpsc_rebound_filter_create(&cfg, &status);
    assert(filter);
    assert(status == TPSC_OK);
    return filter;
}

static void
establish_positive_x(struct tpsc_rebound_filter *filter)
{
    assert(tpsc_rebound_filter_feed(
               filter, 1000, (struct tpsc_vec){3.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_feed(
               filter, 11000, (struct tpsc_vec){3.0, 0.0}) == TPSC_OK);
}

static void
test_small_terminal_reversal_is_undone_exactly(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    establish_positive_x(filter);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-2.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_pending(filter));

    assert(tpsc_rebound_filter_finish(filter, 100000, &correction) == TPSC_OK);
    assert_close(correction.x, 0.0);

    assert(tpsc_rebound_filter_finish(filter, 200000, &correction) == TPSC_OK);
    assert_close(correction.x, 2.0);
    assert_close(correction.y, 0.0);
    assert(!tpsc_rebound_filter_pending(filter));

    tpsc_rebound_filter_destroy(filter);
}

static void
test_large_reversal_is_immediately_intentional(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    establish_positive_x(filter);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-7.0, 0.0}) == TPSC_OK);
    assert(!tpsc_rebound_filter_pending(filter));

    assert(tpsc_rebound_filter_finish(filter, 500000, &correction) == TPSC_OK);
    assert_close(correction.x, 0.0);

    tpsc_rebound_filter_destroy(filter);
}

static void
test_long_reversal_is_intentional(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    establish_positive_x(filter);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-2.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_pending(filter));
    assert(tpsc_rebound_filter_feed(
               filter, 151001, (struct tpsc_vec){-2.0, 0.0}) == TPSC_OK);
    assert(!tpsc_rebound_filter_pending(filter));

    assert(tpsc_rebound_filter_finish(filter, 500000, &correction) == TPSC_OK);
    assert_close(correction.x, 0.0);

    tpsc_rebound_filter_destroy(filter);
}

static void
test_second_reversal_cancels_terminal_candidate(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    establish_positive_x(filter);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-2.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_pending(filter));

    assert(tpsc_rebound_filter_feed(
               filter, 31000, (struct tpsc_vec){1.0, 0.0}) == TPSC_OK);
    assert(!tpsc_rebound_filter_pending(filter));

    assert(tpsc_rebound_filter_finish(filter, 500000, &correction) == TPSC_OK);
    assert_close(correction.x, 0.0);

    tpsc_rebound_filter_destroy(filter);
}

static void
test_axes_classify_independently(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    assert(tpsc_rebound_filter_feed(
               filter, 1000, (struct tpsc_vec){3.0, 3.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_feed(
               filter, 11000, (struct tpsc_vec){3.0, 3.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-2.0, -7.0}) == TPSC_OK);

    assert(tpsc_rebound_filter_pending(filter));
    assert(tpsc_rebound_filter_finish(filter, 200000, &correction) == TPSC_OK);
    assert_close(correction.x, 2.0);
    assert_close(correction.y, 0.0);

    tpsc_rebound_filter_destroy(filter);
}

static void
test_reset_and_time_validation(void)
{
    struct tpsc_rebound_filter *filter = make_filter();
    struct tpsc_vec correction;

    establish_positive_x(filter);
    assert(tpsc_rebound_filter_feed(
               filter, 21000, (struct tpsc_vec){-2.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_pending(filter));

    tpsc_rebound_filter_reset(filter);
    assert(!tpsc_rebound_filter_pending(filter));
    assert(tpsc_rebound_filter_finish(filter, 500000, &correction) == TPSC_OK);
    assert_close(correction.x, 0.0);

    assert(tpsc_rebound_filter_feed(
               filter, 10000, (struct tpsc_vec){1.0, 0.0}) == TPSC_OK);
    assert(tpsc_rebound_filter_feed(
               filter, 9000, (struct tpsc_vec){1.0, 0.0}) ==
           TPSC_ERR_TIME_REVERSED);

    tpsc_rebound_filter_destroy(filter);
}

int
main(void)
{
    test_small_terminal_reversal_is_undone_exactly();
    test_large_reversal_is_immediately_intentional();
    test_long_reversal_is_intentional();
    test_second_reversal_cancels_terminal_candidate();
    test_axes_classify_independently();
    test_reset_and_time_validation();
    puts("rebound tests: PASS");
    return 0;
}

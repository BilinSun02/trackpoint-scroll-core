#include "trackpoint_scroll/engine.h"
#include "trackpoint_scroll/profiles.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void
assert_close(double actual, double expected, double eps)
{
    assert(fabs(actual - expected) <= eps);
}

static struct tpsc_engine *
make_engine(struct tpsc_engine_config *cfg)
{
    int status = 123;
    struct tpsc_engine *engine = tpsc_engine_create(cfg, &status);
    assert(engine);
    assert(status == TPSC_OK);
    assert(tpsc_engine_begin(engine, 0) == TPSC_OK);
    return engine;
}

static void
test_startup_componentwise(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);
    assert_close(out.x, 0.4, 1e-12);
    assert_close(out.y, 0.0, 1e-12);

    /* Orthogonal report inside 70 ms adds exactly one Y fixed step. */
    assert(tpsc_engine_feed(engine, 7000, (struct tpsc_vec){ 0.0, 2.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 9000, &out) == TPSC_OK);
    assert_close(out.x, 0.0, 1e-12);
    assert_close(out.y, 0.4, 1e-12);

    /* Opposite X inside the window is rebound and produces nothing. */
    assert(tpsc_engine_feed(engine, 10000, (struct tpsc_vec){ -1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 12000, &out) == TPSC_OK);
    assert(out.x == 0.0 && out.y == 0.0);

    tpsc_engine_destroy(engine);
}

static void
test_magnitude_erasure(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    engine = make_engine(&cfg);
    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 0.0, 17.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);
    assert_close(out.y, 0.4, 1e-12);
    tpsc_engine_destroy(engine);
}

static void
test_disable_startup_and_conserve(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;
    double total = 0.0;
    unsigned int i;

    tpsc_engine_config_defaults(&cfg);
    cfg.first_step_max_reports = 0;
    cfg.transform.apply = NULL; /* identity */
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    /* 240 ms / 2 ms = 120 identical shares. */
    for (i = 0; i < 120; i++) {
        assert(tpsc_engine_tick(engine, 3000 + i * 2000, &out) == TPSC_OK);
        total += out.y;
    }
    assert_close(total, 1.0, 1e-12);
    /* One zero-output cleanup tick expires the contribution. */
    assert(tpsc_engine_tick(engine, 243000, &out) == TPSC_OK);
    assert(out.x == 0.0 && out.y == 0.0);
    assert(!tpsc_engine_needs_ticks(engine));
    tpsc_engine_destroy(engine);
}

static void
test_report_limit_one(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    cfg.first_step_max_reports = 1;
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);
    assert_close(out.x, 0.4, 1e-12);

    /* A new axis no longer joins the fixed step after quota exhaustion. */
    assert(tpsc_engine_feed(engine, 7000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 9000, &out) == TPSC_OK);
    assert(out.y > 0.0 && out.y < 0.4);

    tpsc_engine_destroy(engine);
}

static void
test_idle_rearm(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    cfg.first_step_max_reports = 1;
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);

    assert(tpsc_engine_feed(engine, 334400, (struct tpsc_vec){ 0.0, -9.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 336400, &out) == TPSC_OK);
    assert_close(out.y, -0.4, 1e-12);

    tpsc_engine_destroy(engine);
}

static void
test_interval_median_and_overlap(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;
    double total_x = 0.0;
    double total_y = 0.0;
    unsigned int i;

    tpsc_engine_config_defaults(&cfg);
    cfg.first_step_max_reports = 0;
    cfg.transform.apply = NULL; /* identity */
    engine = make_engine(&cfg);

    /* First report uses 240 ms. Later gaps populate the five-sample median. */
    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_feed(engine, 11000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert_close(tpsc_engine_estimated_interval_ms(engine), 10.0, 1e-12);
    assert(tpsc_engine_feed(engine, 31000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert_close(tpsc_engine_estimated_interval_ms(engine), 15.0, 1e-12);
    assert(tpsc_engine_feed(engine, 61000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert_close(tpsc_engine_estimated_interval_ms(engine), 20.0, 1e-12);
    assert(tpsc_engine_feed(engine, 101000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert_close(tpsc_engine_estimated_interval_ms(engine), 25.0, 1e-12);
    assert(tpsc_engine_feed(engine, 151000, (struct tpsc_vec){ 0.0, 1.0 }) == TPSC_OK);
    assert_close(tpsc_engine_estimated_interval_ms(engine), 30.0, 1e-12);

    /* Drain every live contribution; overlapping shares conserve each axis. */
    for (i = 0; i < 200; i++) {
        assert(tpsc_engine_tick(engine, 153000 + i * 2000, &out) == TPSC_OK);
        total_x += out.x;
        total_y += out.y;
        if (!tpsc_engine_needs_ticks(engine))
            break;
    }
    assert(!tpsc_engine_needs_ticks(engine));
    assert_close(total_x, 1.0, 1e-12);
    assert_close(total_y, 5.0, 1e-12);

    tpsc_engine_destroy(engine);
}

static void
test_unlimited_startup_window(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    assert(cfg.first_step_max_reports < 0);
    engine = make_engine(&cfg);

    /* Any number of accepted reports may participate while the window is open. */
    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_feed(engine, 5000, (struct tpsc_vec){ 3.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_feed(engine, 9000, (struct tpsc_vec){ 0.0, -4.0 }) == TPSC_OK);
    assert(tpsc_engine_feed(engine, 13000, (struct tpsc_vec){ 2.0, -9.0 }) == TPSC_OK);

    assert(tpsc_engine_tick(engine, 15000, &out) == TPSC_OK);
    assert_close(out.x, 0.4, 1e-12);
    assert_close(out.y, -0.4, 1e-12);

    tpsc_engine_destroy(engine);
}

static void
test_end_discards_tail(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    cfg.first_step_max_reports = 0;
    cfg.transform.apply = NULL;
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 0.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);
    assert(out.x > 0.0);
    assert(tpsc_engine_needs_ticks(engine));

    /* Ending a gesture is an explicit cancellation boundary, not momentum. */
    assert(tpsc_engine_end(engine, 4000) == TPSC_OK);
    assert(!tpsc_engine_needs_ticks(engine));

    assert(tpsc_engine_begin(engine, 5000) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 7000, &out) == TPSC_OK);
    assert(out.x == 0.0 && out.y == 0.0);

    tpsc_engine_destroy(engine);
}

static void
test_hyperbolic_transform(void)
{
    struct tpsc_engine_config cfg;
    struct tpsc_profile profile;
    struct tpsc_engine *engine;
    struct tpsc_vec out;

    tpsc_engine_config_defaults(&cfg);
    tpsc_profile_defaults_hyperbolic(&profile);
    cfg.first_step_max_reports = 0;
    cfg.transform.apply = tpsc_profile_transform;
    cfg.transform.userdata = &profile;
    engine = make_engine(&cfg);

    assert(tpsc_engine_feed(engine, 1000, (struct tpsc_vec){ 1.0, 1.0 }) == TPSC_OK);
    assert(tpsc_engine_tick(engine, 3000, &out) == TPSC_OK);
    assert_close(out.x, out.y, 1e-12);
    assert(out.x > 0.0);

    tpsc_engine_destroy(engine);
}

int
main(void)
{
    test_startup_componentwise();
    test_magnitude_erasure();
    test_disable_startup_and_conserve();
    test_report_limit_one();
    test_idle_rearm();
    test_interval_median_and_overlap();
    test_unlimited_startup_window();
    test_end_discards_tail();
    test_hyperbolic_transform();
    puts("engine tests: PASS");
    return 0;
}

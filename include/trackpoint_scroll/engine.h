#ifndef TRACKPOINT_SCROLL_ENGINE_H
#define TRACKPOINT_SCROLL_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tpsc_vec {
    double x;
    double y;
};

struct tpsc_engine;

typedef struct tpsc_vec (*tpsc_transform_fn)(void *userdata,
                                             uint64_t time_us,
                                             struct tpsc_vec input);
typedef void (*tpsc_transform_reset_fn)(void *userdata, uint64_t time_us);

struct tpsc_transform {
    tpsc_transform_fn apply;
    tpsc_transform_reset_fn reset;
    void *userdata;
};

struct tpsc_engine_config {
    uint32_t tick_us;
    double initial_interval_ms;
    double interval_min_ms;
    double interval_max_ms;
    double idle_reset_ms;

    double first_step_distance;
    double first_step_axis_merge_ms;
    int first_step_max_reports;

    struct tpsc_transform transform;
};

enum tpsc_status {
    TPSC_OK = 0,
    TPSC_ERR_ARGUMENT = -1,
    TPSC_ERR_CONFIG = -2,
    TPSC_ERR_NOMEM = -3,
    TPSC_ERR_TIME_REVERSED = -4,
};

/* Fill cfg with the currently tested baseline parameters. */
void tpsc_engine_config_defaults(struct tpsc_engine_config *cfg);

/* Returns TPSC_OK when cfg satisfies all structural constraints. */
int tpsc_engine_config_validate(const struct tpsc_engine_config *cfg);

/* The engine owns no transform userdata. */
struct tpsc_engine *tpsc_engine_create(const struct tpsc_engine_config *cfg,
                                       int *status_out);
void tpsc_engine_destroy(struct tpsc_engine *engine);

/*
 * A gesture boundary discards pending reconstruction from the previous gesture.
 * The first motion report, not begin(), starts the startup merge window.
 */
int tpsc_engine_begin(struct tpsc_engine *engine, uint64_t time_us);
int tpsc_engine_end(struct tpsc_engine *engine, uint64_t time_us);

/*
 * Feed one timestamped raw relative displacement report. Reports must have
 * nondecreasing timestamps within a gesture.
 */
int tpsc_engine_feed(struct tpsc_engine *engine,
                     uint64_t time_us,
                     struct tpsc_vec delta);

/*
 * Advance one logical output tick. The caller chooses the scheduling mechanism;
 * logical processing remains fixed at cfg.tick_us.
 *
 * Returns TPSC_OK and writes the output for this tick. A zero vector is valid.
 */
int tpsc_engine_tick(struct tpsc_engine *engine,
                     uint64_t time_us,
                     struct tpsc_vec *output);

/* True while a fixed startup step is pending or reconstructed shares are live. */
bool tpsc_engine_needs_ticks(const struct tpsc_engine *engine);

/* Convenience accessors for schedulers and diagnostics. */
uint32_t tpsc_engine_tick_us(const struct tpsc_engine *engine);
double tpsc_engine_estimated_interval_ms(const struct tpsc_engine *engine);

#ifdef __cplusplus
}
#endif

#endif

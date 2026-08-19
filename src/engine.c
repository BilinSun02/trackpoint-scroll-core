#include "trackpoint_scroll/engine.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TPSC_INTERVAL_HISTORY 5u

struct tpsc_slot {
    struct tpsc_vec delta;
    unsigned int count;
};

struct tpsc_engine {
    struct tpsc_engine_config cfg;

    bool gesture_active;
    bool have_input;
    uint64_t last_time_us;

    bool first_step_window_active;
    uint64_t first_step_start_us;
    unsigned int first_step_report_count;
    int first_step_sign_x;
    int first_step_sign_y;
    struct tpsc_vec pending_fixed;

    struct tpsc_vec pending;
    struct tpsc_vec active;
    struct tpsc_slot *slots;
    size_t slot_count;
    size_t next_slot;
    unsigned int live_contributions;

    double intervals[TPSC_INTERVAL_HISTORY];
    unsigned int interval_next;
    unsigned int interval_count;
};

static bool
vec_is_zero(struct tpsc_vec v)
{
    return v.x == 0.0 && v.y == 0.0;
}

static double
clamp_double(double value, double low, double high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int
sign_double(double value)
{
    if (value > 0.0)
        return 1;
    if (value < 0.0)
        return -1;
    return 0;
}

static struct tpsc_vec
identity_transform(void *userdata, uint64_t time_us, struct tpsc_vec input)
{
    (void)userdata;
    (void)time_us;
    return input;
}

void
tpsc_engine_config_defaults(struct tpsc_engine_config *cfg)
{
    if (!cfg)
        return;

    *cfg = (struct tpsc_engine_config){
        .tick_us = 2000,
        .initial_interval_ms = 240.0,
        .interval_min_ms = 10.0,
        .interval_max_ms = 250.0,
        .idle_reset_ms = 333.3,
        .first_step_distance = 0.4,
        .first_step_axis_merge_ms = 70.0,
        .first_step_max_reports = -1,
        .transform = {
            .apply = NULL,
            .reset = NULL,
            .userdata = NULL,
        },
    };
}

int
tpsc_engine_config_validate(const struct tpsc_engine_config *cfg)
{
    if (!cfg)
        return TPSC_ERR_ARGUMENT;

    if (cfg->tick_us == 0 ||
        !isfinite(cfg->initial_interval_ms) ||
        !isfinite(cfg->interval_min_ms) ||
        !isfinite(cfg->interval_max_ms) ||
        !isfinite(cfg->idle_reset_ms) ||
        !isfinite(cfg->first_step_distance) ||
        !isfinite(cfg->first_step_axis_merge_ms))
        return TPSC_ERR_CONFIG;

    if (cfg->initial_interval_ms <= 0.0 ||
        cfg->interval_min_ms <= 0.0 ||
        cfg->interval_max_ms < cfg->interval_min_ms ||
        cfg->idle_reset_ms < 0.0 ||
        cfg->first_step_distance < 0.0 ||
        cfg->first_step_axis_merge_ms < 0.0)
        return TPSC_ERR_CONFIG;

    return TPSC_OK;
}

static size_t
required_slot_count(const struct tpsc_engine_config *cfg)
{
    double ticks = cfg->interval_max_ms * 1000.0 / (double)cfg->tick_us;
    size_t max_ticks = (size_t)ceil(ticks);

    /* One extra slot separates current expiration from the furthest removal. */
    return max_ticks + 2;
}

struct tpsc_engine *
tpsc_engine_create(const struct tpsc_engine_config *cfg, int *status_out)
{
    struct tpsc_engine *engine;
    int status = tpsc_engine_config_validate(cfg);

    if (status != TPSC_OK) {
        if (status_out)
            *status_out = status;
        return NULL;
    }

    engine = calloc(1, sizeof(*engine));
    if (!engine) {
        if (status_out)
            *status_out = TPSC_ERR_NOMEM;
        return NULL;
    }

    engine->cfg = *cfg;
    engine->slot_count = required_slot_count(cfg);
    engine->slots = calloc(engine->slot_count, sizeof(*engine->slots));
    if (!engine->slots) {
        free(engine);
        if (status_out)
            *status_out = TPSC_ERR_NOMEM;
        return NULL;
    }

    if (!engine->cfg.transform.apply)
        engine->cfg.transform.apply = identity_transform;

    if (status_out)
        *status_out = TPSC_OK;
    return engine;
}

void
tpsc_engine_destroy(struct tpsc_engine *engine)
{
    if (!engine)
        return;
    free(engine->slots);
    free(engine);
}

static void
clear_motion_state(struct tpsc_engine *engine)
{
    engine->first_step_window_active = false;
    engine->first_step_start_us = 0;
    engine->first_step_report_count = 0;
    engine->first_step_sign_x = 0;
    engine->first_step_sign_y = 0;
    engine->pending_fixed = (struct tpsc_vec){ 0.0, 0.0 };

    engine->pending = (struct tpsc_vec){ 0.0, 0.0 };
    engine->active = (struct tpsc_vec){ 0.0, 0.0 };
    memset(engine->slots, 0, engine->slot_count * sizeof(*engine->slots));
    engine->next_slot = 0;
    engine->live_contributions = 0;

    memset(engine->intervals, 0, sizeof(engine->intervals));
    engine->interval_next = 0;
    engine->interval_count = 0;
}

static void
reset_transform(struct tpsc_engine *engine, uint64_t time_us)
{
    if (engine->cfg.transform.reset)
        engine->cfg.transform.reset(engine->cfg.transform.userdata, time_us);
}

int
tpsc_engine_begin(struct tpsc_engine *engine, uint64_t time_us)
{
    if (!engine)
        return TPSC_ERR_ARGUMENT;

    clear_motion_state(engine);
    engine->gesture_active = true;
    engine->have_input = false;
    engine->last_time_us = time_us;
    reset_transform(engine, time_us);
    return TPSC_OK;
}

int
tpsc_engine_end(struct tpsc_engine *engine, uint64_t time_us)
{
    if (!engine)
        return TPSC_ERR_ARGUMENT;
    if (engine->gesture_active && time_us < engine->last_time_us)
        return TPSC_ERR_TIME_REVERSED;

    clear_motion_state(engine);
    engine->gesture_active = false;
    engine->have_input = false;
    engine->last_time_us = time_us;
    reset_transform(engine, time_us);
    return TPSC_OK;
}

static void
begin_burst(struct tpsc_engine *engine, uint64_t time_us)
{
    clear_motion_state(engine);
    engine->have_input = true;
    engine->last_time_us = time_us;
    engine->first_step_window_active = engine->cfg.first_step_max_reports != 0;
    engine->first_step_start_us = time_us;
    reset_transform(engine, time_us);
}

static void
add_interval(struct tpsc_engine *engine, double interval_ms)
{
    interval_ms = clamp_double(interval_ms,
                               engine->cfg.interval_min_ms,
                               engine->cfg.interval_max_ms);

    engine->intervals[engine->interval_next] = interval_ms;
    engine->interval_next =
        (engine->interval_next + 1) % TPSC_INTERVAL_HISTORY;
    if (engine->interval_count < TPSC_INTERVAL_HISTORY)
        engine->interval_count++;
}

double
tpsc_engine_estimated_interval_ms(const struct tpsc_engine *engine)
{
    double sorted[TPSC_INTERVAL_HISTORY];
    unsigned int i;

    if (!engine)
        return 0.0;
    if (engine->interval_count == 0)
        return engine->cfg.initial_interval_ms;

    for (i = 0; i < engine->interval_count; i++)
        sorted[i] = engine->intervals[i];

    for (i = 1; i < engine->interval_count; i++) {
        double value = sorted[i];
        unsigned int j = i;

        while (j > 0 && sorted[j - 1] > value) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = value;
    }

    if (engine->interval_count % 2 == 1)
        return sorted[engine->interval_count / 2];

    return (sorted[engine->interval_count / 2 - 1] +
            sorted[engine->interval_count / 2]) /
           2.0;
}

static bool
first_step_component(int incoming,
                     int *remembered,
                     double distance,
                     double *pending)
{
    if (incoming == 0)
        return false;

    if (*remembered == 0) {
        *remembered = incoming;
        *pending += incoming * distance;
        return true;
    }

    return *remembered == incoming;
}

static bool
handle_first_step(struct tpsc_engine *engine,
                  uint64_t time_us,
                  struct tpsc_vec delta,
                  struct tpsc_vec *profile_delta)
{
    double age_ms;
    int incoming_x;
    int incoming_y;
    bool rebound_x;
    bool rebound_y;
    bool quota_available;
    bool accepted_x;
    bool accepted_y;

    *profile_delta = delta;

    if (!engine->first_step_window_active)
        return false;

    age_ms = (double)(time_us - engine->first_step_start_us) / 1000.0;
    if (age_ms > engine->cfg.first_step_axis_merge_ms) {
        engine->first_step_window_active = false;
        return false;
    }

    incoming_x = sign_double(delta.x);
    incoming_y = sign_double(delta.y);
    rebound_x = incoming_x != 0 && engine->first_step_sign_x != 0 &&
                incoming_x != engine->first_step_sign_x;
    rebound_y = incoming_y != 0 && engine->first_step_sign_y != 0 &&
                incoming_y != engine->first_step_sign_y;

    if (rebound_x)
        profile_delta->x = 0.0;
    if (rebound_y)
        profile_delta->y = 0.0;

    quota_available = engine->cfg.first_step_max_reports < 0 ||
                      engine->first_step_report_count <
                          (unsigned int)engine->cfg.first_step_max_reports;

    if (quota_available) {
        accepted_x = !rebound_x && first_step_component(
            incoming_x,
            &engine->first_step_sign_x,
            engine->cfg.first_step_distance,
            &engine->pending_fixed.x);
        accepted_y = !rebound_y && first_step_component(
            incoming_y,
            &engine->first_step_sign_y,
            engine->cfg.first_step_distance,
            &engine->pending_fixed.y);

        if (accepted_x || accepted_y)
            engine->first_step_report_count++;

        /* While quota remains, the whole report belongs to the fixed step. */
        return true;
    }

    /* After quota exhaustion, rebound suppression lasts until window expiry. */
    return vec_is_zero(*profile_delta);
}

int
tpsc_engine_feed(struct tpsc_engine *engine,
                 uint64_t time_us,
                 struct tpsc_vec delta)
{
    struct tpsc_vec profile_delta;
    double elapsed_ms = 0.0;
    double interval_ms;
    unsigned int duration_ticks;
    size_t removal_index;
    struct tpsc_slot *removal;
    bool restarted = false;

    if (!engine)
        return TPSC_ERR_ARGUMENT;
    if (!engine->gesture_active)
        return TPSC_ERR_ARGUMENT;
    if (engine->have_input && time_us < engine->last_time_us)
        return TPSC_ERR_TIME_REVERSED;

    if (!engine->have_input) {
        begin_burst(engine, time_us);
        restarted = true;
    } else {
        elapsed_ms = (double)(time_us - engine->last_time_us) / 1000.0;
        if (elapsed_ms > engine->cfg.idle_reset_ms) {
            begin_burst(engine, time_us);
            restarted = true;
        } else {
            engine->last_time_us = time_us;
        }
    }

    if (handle_first_step(engine, time_us, delta, &profile_delta))
        return TPSC_OK;

    if (!restarted && elapsed_ms > 0.0)
        add_interval(engine, elapsed_ms);

    interval_ms = tpsc_engine_estimated_interval_ms(engine);
    duration_ticks = (unsigned int)(
        interval_ms * 1000.0 / (double)engine->cfg.tick_us + 0.5);
    if (duration_ticks == 0)
        duration_ticks = 1;
    if ((size_t)duration_ticks >= engine->slot_count)
        duration_ticks = (unsigned int)(engine->slot_count - 1);

    profile_delta.x /= duration_ticks;
    profile_delta.y /= duration_ticks;
    engine->pending.x += profile_delta.x;
    engine->pending.y += profile_delta.y;

    removal_index = (engine->next_slot + duration_ticks) % engine->slot_count;
    removal = &engine->slots[removal_index];
    removal->delta.x += profile_delta.x;
    removal->delta.y += profile_delta.y;
    removal->count++;
    engine->live_contributions++;

    return TPSC_OK;
}

int
tpsc_engine_tick(struct tpsc_engine *engine,
                 uint64_t time_us,
                 struct tpsc_vec *output)
{
    struct tpsc_slot *removal;
    struct tpsc_vec transformed;

    if (!engine || !output)
        return TPSC_ERR_ARGUMENT;
    if (!engine->gesture_active)
        return TPSC_ERR_ARGUMENT;
    if (engine->have_input && time_us < engine->last_time_us)
        return TPSC_ERR_TIME_REVERSED;

    *output = engine->pending_fixed;
    engine->pending_fixed = (struct tpsc_vec){ 0.0, 0.0 };

    removal = &engine->slots[engine->next_slot];
    if (removal->count > engine->live_contributions)
        return TPSC_ERR_CONFIG;

    engine->active.x -= removal->delta.x;
    engine->active.y -= removal->delta.y;
    engine->live_contributions -= removal->count;
    *removal = (struct tpsc_slot){ 0 };

    engine->active.x += engine->pending.x;
    engine->active.y += engine->pending.y;
    engine->pending = (struct tpsc_vec){ 0.0, 0.0 };
    engine->next_slot = (engine->next_slot + 1) % engine->slot_count;

    if (engine->live_contributions == 0)
        engine->active = (struct tpsc_vec){ 0.0, 0.0 };

    if (!vec_is_zero(engine->active)) {
        transformed = engine->cfg.transform.apply(
            engine->cfg.transform.userdata, time_us, engine->active);
        output->x += transformed.x;
        output->y += transformed.y;
    }

    return TPSC_OK;
}

bool
tpsc_engine_needs_ticks(const struct tpsc_engine *engine)
{
    if (!engine)
        return false;
    return !vec_is_zero(engine->pending_fixed) ||
           !vec_is_zero(engine->pending) ||
           engine->live_contributions > 0;
}

uint32_t
tpsc_engine_tick_us(const struct tpsc_engine *engine)
{
    return engine ? engine->cfg.tick_us : 0;
}

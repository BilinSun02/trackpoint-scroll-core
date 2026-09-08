#include "trackpoint_scroll/rebound.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

struct rebound_axis {
    int established_direction;
    double established_counts;
    uint64_t established_time_us;

    bool candidate;
    int candidate_direction;
    double candidate_delta;
    double candidate_counts;
    uint64_t candidate_start_us;
    uint64_t candidate_last_us;
};

struct tpsc_rebound_filter {
    struct tpsc_rebound_config cfg;
    struct rebound_axis x;
    struct rebound_axis y;

    bool have_last_motion_time;
    uint64_t last_motion_time_us;
    double recent_report_gap_ms;
};

static int
sign_double(double value)
{
    return value > 0.0 ? 1 : -1;
}

static void
clear_axis(struct rebound_axis *axis)
{
    *axis = (struct rebound_axis){0};
}

static void
clear_candidate(struct rebound_axis *axis)
{
    axis->candidate = false;
    axis->candidate_direction = 0;
    axis->candidate_delta = 0.0;
    axis->candidate_counts = 0.0;
    axis->candidate_start_us = 0;
    axis->candidate_last_us = 0;
}

void
tpsc_rebound_config_defaults(struct tpsc_rebound_config *cfg)
{
    if (!cfg)
        return;

    *cfg = (struct tpsc_rebound_config){
        .arm_ms = 400.0,
        .min_prior_counts = 6.0,
        .candidate_max_ms = 120.0,
        .candidate_max_counts = 6.0,
        .quiet_default_ms = 140.0,
        .quiet_min_ms = 100.0,
        .quiet_max_ms = 700.0,
        .quiet_gap_multiplier = 1.75,
        .report_gap_min_ms = 3.0,
        .report_gap_max_ms = 500.0,
        .recent_counts_cap = 4096.0,
    };
}

int
tpsc_rebound_config_validate(const struct tpsc_rebound_config *cfg)
{
    if (!cfg)
        return TPSC_ERR_ARGUMENT;

    if (!isfinite(cfg->arm_ms) ||
        !isfinite(cfg->min_prior_counts) ||
        !isfinite(cfg->candidate_max_ms) ||
        !isfinite(cfg->candidate_max_counts) ||
        !isfinite(cfg->quiet_default_ms) ||
        !isfinite(cfg->quiet_min_ms) ||
        !isfinite(cfg->quiet_max_ms) ||
        !isfinite(cfg->quiet_gap_multiplier) ||
        !isfinite(cfg->report_gap_min_ms) ||
        !isfinite(cfg->report_gap_max_ms) ||
        !isfinite(cfg->recent_counts_cap))
        return TPSC_ERR_CONFIG;

    if (cfg->arm_ms < 0.0 ||
        cfg->min_prior_counts < 0.0 ||
        cfg->candidate_max_ms < 0.0 ||
        cfg->candidate_max_counts < 0.0 ||
        cfg->quiet_default_ms < 0.0 ||
        cfg->quiet_min_ms < 0.0 ||
        cfg->quiet_max_ms < cfg->quiet_min_ms ||
        cfg->quiet_gap_multiplier < 0.0 ||
        cfg->report_gap_min_ms < 0.0 ||
        cfg->report_gap_max_ms < cfg->report_gap_min_ms ||
        cfg->recent_counts_cap < 0.0)
        return TPSC_ERR_CONFIG;

    return TPSC_OK;
}

struct tpsc_rebound_filter *
tpsc_rebound_filter_create(const struct tpsc_rebound_config *cfg,
                           int *status_out)
{
    struct tpsc_rebound_filter *filter;
    int status = tpsc_rebound_config_validate(cfg);

    if (status != TPSC_OK) {
        if (status_out)
            *status_out = status;
        return NULL;
    }

    filter = calloc(1, sizeof(*filter));
    if (!filter) {
        if (status_out)
            *status_out = TPSC_ERR_NOMEM;
        return NULL;
    }

    filter->cfg = *cfg;
    if (status_out)
        *status_out = TPSC_OK;
    return filter;
}

void
tpsc_rebound_filter_destroy(struct tpsc_rebound_filter *filter)
{
    free(filter);
}

void
tpsc_rebound_filter_reset(struct tpsc_rebound_filter *filter)
{
    if (!filter)
        return;

    clear_axis(&filter->x);
    clear_axis(&filter->y);
    filter->have_last_motion_time = false;
    filter->last_motion_time_us = 0;
    filter->recent_report_gap_ms = 0.0;
}

static void
note_established_motion(const struct tpsc_rebound_config *cfg,
                        struct rebound_axis *axis,
                        int direction,
                        double counts,
                        uint64_t time_us)
{
    double age_ms = 0.0;

    if (axis->established_direction != 0 &&
        time_us >= axis->established_time_us)
        age_ms = (double)(time_us - axis->established_time_us) / 1000.0;

    if (axis->established_direction == direction &&
        age_ms <= cfg->arm_ms) {
        axis->established_counts += counts;
        if (axis->established_counts > cfg->recent_counts_cap)
            axis->established_counts = cfg->recent_counts_cap;
    } else {
        axis->established_direction = direction;
        axis->established_counts = counts;
    }

    axis->established_time_us = time_us;
}

static void
commit_candidate(const struct tpsc_rebound_config *cfg,
                 struct rebound_axis *axis)
{
    int direction = axis->candidate_direction;
    double counts = axis->candidate_counts;
    uint64_t time_us = axis->candidate_last_us;

    clear_candidate(axis);
    note_established_motion(cfg, axis, direction, counts, time_us);
}

static bool
candidate_has_become_intentional(const struct tpsc_rebound_config *cfg,
                                 const struct rebound_axis *axis)
{
    double duration_ms;

    if (!axis->candidate ||
        axis->candidate_last_us < axis->candidate_start_us)
        return false;

    duration_ms =
        (double)(axis->candidate_last_us - axis->candidate_start_us) / 1000.0;

    /*
     * The classifier requires BOTH small displacement and short duration for a
     * rebound. Exceeding either threshold therefore proves live intentional
     * reversal and commits it immediately.
     */
    return duration_ms > cfg->candidate_max_ms ||
           axis->candidate_counts > cfg->candidate_max_counts;
}

static bool
candidate_is_rebound(const struct tpsc_rebound_config *cfg,
                     const struct rebound_axis *axis)
{
    double duration_ms;

    if (!axis->candidate ||
        axis->candidate_last_us < axis->candidate_start_us)
        return false;

    duration_ms =
        (double)(axis->candidate_last_us - axis->candidate_start_us) / 1000.0;

    return duration_ms <= cfg->candidate_max_ms &&
           axis->candidate_counts <= cfg->candidate_max_counts;
}

static void
observe_axis(const struct tpsc_rebound_config *cfg,
             struct rebound_axis *axis,
             double delta,
             uint64_t time_us)
{
    int direction;
    double counts;
    double established_age_ms = 0.0;

    if (delta == 0.0)
        return;

    direction = sign_double(delta);
    counts = fabs(delta);

    if (axis->candidate) {
        if (direction == axis->candidate_direction) {
            axis->candidate_delta += delta;
            axis->candidate_counts += counts;
            axis->candidate_last_us = time_us;

            if (candidate_has_become_intentional(cfg, axis))
                commit_candidate(cfg, axis);
            return;
        }

        /*
         * A second reversal before terminal quiet means the candidate was not
         * a terminal rebound episode.
         */
        clear_candidate(axis);
        note_established_motion(cfg, axis, direction, counts, time_us);
        return;
    }

    if (axis->established_direction != 0 &&
        time_us >= axis->established_time_us)
        established_age_ms =
            (double)(time_us - axis->established_time_us) / 1000.0;

    if (axis->established_direction != 0 &&
        direction != axis->established_direction &&
        established_age_ms <= cfg->arm_ms &&
        axis->established_counts >= cfg->min_prior_counts) {
        axis->candidate = true;
        axis->candidate_direction = direction;
        axis->candidate_delta = delta;
        axis->candidate_counts = counts;
        axis->candidate_start_us = time_us;
        axis->candidate_last_us = time_us;

        if (candidate_has_become_intentional(cfg, axis))
            commit_candidate(cfg, axis);
        return;
    }

    note_established_motion(cfg, axis, direction, counts, time_us);
}

int
tpsc_rebound_filter_feed(struct tpsc_rebound_filter *filter,
                         uint64_t time_us,
                         struct tpsc_vec delta)
{
    if (!filter)
        return TPSC_ERR_ARGUMENT;
    if (!isfinite(delta.x) || !isfinite(delta.y))
        return TPSC_ERR_ARGUMENT;
    if (delta.x == 0.0 && delta.y == 0.0)
        return TPSC_OK;

    if (filter->have_last_motion_time) {
        double gap_ms;

        if (time_us < filter->last_motion_time_us)
            return TPSC_ERR_TIME_REVERSED;

        gap_ms =
            (double)(time_us - filter->last_motion_time_us) / 1000.0;

        /*
         * Split-axis callbacks from one physical report can be effectively
         * simultaneous. Exclude those from sparse-report cadence estimation.
         */
        if (gap_ms >= filter->cfg.report_gap_min_ms &&
            gap_ms <= filter->cfg.report_gap_max_ms) {
            if (filter->recent_report_gap_ms <= 0.0 ||
                gap_ms > filter->recent_report_gap_ms)
                filter->recent_report_gap_ms = gap_ms;
            else
                filter->recent_report_gap_ms *= 0.92;
        }
    }

    filter->last_motion_time_us = time_us;
    filter->have_last_motion_time = true;

    observe_axis(&filter->cfg, &filter->x, delta.x, time_us);
    observe_axis(&filter->cfg, &filter->y, delta.y, time_us);
    return TPSC_OK;
}

bool
tpsc_rebound_filter_pending(const struct tpsc_rebound_filter *filter)
{
    return filter && (filter->x.candidate || filter->y.candidate);
}

static double
quiet_interval_ms(const struct tpsc_rebound_filter *filter)
{
    double quiet = filter->cfg.quiet_default_ms;

    if (filter->recent_report_gap_ms > 0.0)
        quiet =
            filter->recent_report_gap_ms * filter->cfg.quiet_gap_multiplier;

    if (quiet < filter->cfg.quiet_min_ms)
        quiet = filter->cfg.quiet_min_ms;
    if (quiet > filter->cfg.quiet_max_ms)
        quiet = filter->cfg.quiet_max_ms;
    return quiet;
}

uint64_t
tpsc_rebound_filter_deadline_us(const struct tpsc_rebound_filter *filter)
{
    double quiet_us;
    uint64_t offset;

    if (!filter ||
        !filter->have_last_motion_time ||
        !tpsc_rebound_filter_pending(filter))
        return 0;

    quiet_us = quiet_interval_ms(filter) * 1000.0;
    if (quiet_us >= (double)UINT64_MAX)
        return UINT64_MAX;

    offset = (uint64_t)ceil(quiet_us);
    if (offset > UINT64_MAX - filter->last_motion_time_us)
        return UINT64_MAX;

    return filter->last_motion_time_us + offset;
}

int
tpsc_rebound_filter_finish(struct tpsc_rebound_filter *filter,
                           uint64_t time_us,
                           struct tpsc_vec *correction)
{
    uint64_t deadline;

    if (!filter || !correction)
        return TPSC_ERR_ARGUMENT;

    *correction = (struct tpsc_vec){0.0, 0.0};

    if (!tpsc_rebound_filter_pending(filter) ||
        !filter->have_last_motion_time)
        return TPSC_OK;

    if (time_us < filter->last_motion_time_us)
        return TPSC_ERR_TIME_REVERSED;

    deadline = tpsc_rebound_filter_deadline_us(filter);
    if (time_us < deadline)
        return TPSC_OK;

    if (candidate_is_rebound(&filter->cfg, &filter->x))
        correction->x = -filter->x.candidate_delta;
    if (candidate_is_rebound(&filter->cfg, &filter->y))
        correction->y = -filter->y.candidate_delta;

    /*
     * Terminal quiet is a hard motion boundary. A later report starts fresh
     * rather than inheriting pre-idle direction as a rebound reference.
     */
    clear_axis(&filter->x);
    clear_axis(&filter->y);
    filter->have_last_motion_time = false;
    filter->last_motion_time_us = 0;

    return TPSC_OK;
}

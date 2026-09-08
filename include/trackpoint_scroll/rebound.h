#ifndef TRACKPOINT_SCROLL_REBOUND_H
#define TRACKPOINT_SCROLL_REBOUND_H

#include <stdbool.h>
#include <stdint.h>

#include "trackpoint_scroll/engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Retrospective terminal-rebound classifier.
 *
 * Live motion is never delayed or suppressed. The host feeds timestamped
 * relative displacement reports as they occur. If a short, small terminal
 * reversal is followed by a sufficient quiet interval, finish() returns the
 * exact inverse displacement needed to undo that reversal.
 *
 * The host owns scheduling and output policy. In particular, a host that can
 * observe other pointing devices should verify that applying a retrospective
 * correction is still appropriate before calling finish().
 */
struct tpsc_rebound_config {
    double arm_ms;
    double min_prior_counts;

    double candidate_max_ms;
    double candidate_max_counts;

    double quiet_default_ms;
    double quiet_min_ms;
    double quiet_max_ms;
    double quiet_gap_multiplier;

    double report_gap_min_ms;
    double report_gap_max_ms;

    double recent_counts_cap;
};

struct tpsc_rebound_filter;

void tpsc_rebound_config_defaults(struct tpsc_rebound_config *cfg);
int tpsc_rebound_config_validate(const struct tpsc_rebound_config *cfg);

struct tpsc_rebound_filter *
tpsc_rebound_filter_create(const struct tpsc_rebound_config *cfg,
                           int *status_out);
void tpsc_rebound_filter_destroy(struct tpsc_rebound_filter *filter);

/* Hard motion/gesture boundary. Discards all pending classification state. */
void tpsc_rebound_filter_reset(struct tpsc_rebound_filter *filter);

/*
 * Observe one live relative-motion report. Zero vectors are ignored.
 * Timestamps must be nondecreasing across nonzero reports between resets.
 */
int tpsc_rebound_filter_feed(struct tpsc_rebound_filter *filter,
                             uint64_t time_us,
                             struct tpsc_vec delta);

/* True while at least one axis has a terminal reversal candidate. */
bool tpsc_rebound_filter_pending(const struct tpsc_rebound_filter *filter);

/*
 * Earliest time at which finish() may classify the current candidate after
 * quiet. Returns 0 when there is no pending candidate.
 */
uint64_t
tpsc_rebound_filter_deadline_us(const struct tpsc_rebound_filter *filter);

/*
 * If the pending candidate has been quiet long enough, classify the episode,
 * clear its motion state, and return an exact retrospective correction.
 *
 * Before the quiet deadline, correction is zero and state is unchanged.
 * A zero correction is also valid when no candidate is pending.
 */
int tpsc_rebound_filter_finish(struct tpsc_rebound_filter *filter,
                               uint64_t time_us,
                               struct tpsc_vec *correction);

#ifdef __cplusplus
}
#endif

#endif

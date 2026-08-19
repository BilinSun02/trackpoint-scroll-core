#ifndef TRACKPOINT_SCROLL_PROFILES_H
#define TRACKPOINT_SCROLL_PROFILES_H

#include <stdbool.h>
#include "trackpoint_scroll/engine.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tpsc_profile_kind {
    TPSC_PROFILE_AFFINE = 0,
    TPSC_PROFILE_QUADRATIC,
    TPSC_PROFILE_HYPERBOLIC,
};

struct tpsc_affine_profile {
    double k;
    double b;
};

struct tpsc_quadratic_profile {
    double a;
    double h;
    double k;
};

struct tpsc_hyperbolic_profile {
    double a;
    double u;
    double k;
};

struct tpsc_profile {
    enum tpsc_profile_kind kind;
    bool clamp_negative_output;
    union {
        struct tpsc_affine_profile affine;
        struct tpsc_quadratic_profile quadratic;
        struct tpsc_hyperbolic_profile hyperbolic;
    } params;
};

void tpsc_profile_defaults_affine(struct tpsc_profile *profile);
void tpsc_profile_defaults_quadratic(struct tpsc_profile *profile);
void tpsc_profile_defaults_hyperbolic(struct tpsc_profile *profile);

/*
 * Apply a memoryless radial profile. The vector direction is preserved for a
 * positive scalar result. A negative scalar result reverses direction unless
 * clamp_negative_output is set. Exact zero input always yields exact zero.
 */
struct tpsc_vec tpsc_profile_apply(const struct tpsc_profile *profile,
                                   struct tpsc_vec input);

/* Adapter suitable for tpsc_transform.apply; userdata points to tpsc_profile. */
struct tpsc_vec tpsc_profile_transform(void *userdata,
                                       uint64_t time_us,
                                       struct tpsc_vec input);

#ifdef __cplusplus
}
#endif

#endif

#include "trackpoint_scroll/profiles.h"

#include <math.h>

void
tpsc_profile_defaults_affine(struct tpsc_profile *profile)
{
    if (!profile)
        return;
    *profile = (struct tpsc_profile){
        .kind = TPSC_PROFILE_AFFINE,
        .clamp_negative_output = false,
        .params.affine = { .k = 0.4, .b = 0.0 },
    };
}

void
tpsc_profile_defaults_quadratic(struct tpsc_profile *profile)
{
    if (!profile)
        return;
    *profile = (struct tpsc_profile){
        .kind = TPSC_PROFILE_QUADRATIC,
        .clamp_negative_output = false,
        .params.quadratic = { .a = 0.16, .h = 0.0, .k = 0.025 },
    };
}

void
tpsc_profile_defaults_hyperbolic(struct tpsc_profile *profile)
{
    if (!profile)
        return;
    *profile = (struct tpsc_profile){
        .kind = TPSC_PROFILE_HYPERBOLIC,
        .clamp_negative_output = false,
        .params.hyperbolic = { .a = 0.75, .u = 1.6, .k = -1.175 },
    };
}

static double
profile_scalar(const struct tpsc_profile *profile, double speed)
{
    switch (profile->kind) {
    case TPSC_PROFILE_AFFINE:
        return profile->params.affine.k * speed + profile->params.affine.b;
    case TPSC_PROFILE_QUADRATIC: {
        double d = speed - profile->params.quadratic.h;
        return profile->params.quadratic.a * d * d +
               profile->params.quadratic.k;
    }
    case TPSC_PROFILE_HYPERBOLIC:
        return profile->params.hyperbolic.a *
                   hypot(profile->params.hyperbolic.u, speed) +
               profile->params.hyperbolic.k;
    }

    return 0.0;
}

struct tpsc_vec
tpsc_profile_apply(const struct tpsc_profile *profile, struct tpsc_vec input)
{
    double speed;
    double output_speed;
    double scale;

    if (!profile)
        return (struct tpsc_vec){ 0.0, 0.0 };

    speed = hypot(input.x, input.y);
    if (speed == 0.0)
        return (struct tpsc_vec){ 0.0, 0.0 };

    output_speed = profile_scalar(profile, speed);
    if (profile->clamp_negative_output && output_speed < 0.0)
        output_speed = 0.0;

    scale = output_speed / speed;
    return (struct tpsc_vec){ input.x * scale, input.y * scale };
}

struct tpsc_vec
tpsc_profile_transform(void *userdata,
                       uint64_t time_us,
                       struct tpsc_vec input)
{
    (void)time_us;
    return tpsc_profile_apply((const struct tpsc_profile *)userdata, input);
}

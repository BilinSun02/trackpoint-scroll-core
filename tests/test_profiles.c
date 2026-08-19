#include "trackpoint_scroll/profiles.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void
assert_close(double actual, double expected, double eps)
{
    assert(fabs(actual - expected) <= eps);
}

int
main(void)
{
    struct tpsc_profile p;
    struct tpsc_vec out;

    tpsc_profile_defaults_affine(&p);
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 2.5, 0.0 });
    assert_close(out.x, 1.0, 1e-12);
    assert_close(out.y, 0.0, 1e-12);

    tpsc_profile_defaults_quadratic(&p);
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 2.5, 0.0 });
    assert_close(out.x, 1.025, 1e-12);

    tpsc_profile_defaults_hyperbolic(&p);
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 2.5, 0.0 });
    assert_close(out.x, 1.05112331195, 1e-10);

    /* Radial mapping preserves a 45-degree direction. */
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 1.0, 1.0 });
    assert_close(out.x, out.y, 1e-12);

    /* Exact zero remains exact zero even with a positive-side onset. */
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 0.0, 0.0 });
    assert(out.x == 0.0 && out.y == 0.0);

    /* Negative scalar output reverses direction unless clamped. */
    p = (struct tpsc_profile){
        .kind = TPSC_PROFILE_AFFINE,
        .clamp_negative_output = false,
        .params.affine = { .k = 0.0, .b = -1.0 },
    };
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 2.0, 0.0 });
    assert_close(out.x, -1.0, 1e-12);
    p.clamp_negative_output = true;
    out = tpsc_profile_apply(&p, (struct tpsc_vec){ 2.0, 0.0 });
    assert(out.x == 0.0 && out.y == 0.0);

    puts("profile tests: PASS");
    return 0;
}

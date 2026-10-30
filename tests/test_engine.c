/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zmk/spacemouse/engine.h>

static int failures = 0;
static int checks  = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                     \
        }                                                                   \
    } while (0)

int main(void)
{
    /* ---- T1: decay sequence with deadband ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 3, 10, 768);

        static const int expected[] = {30, 21, 15, 10, 7, 5, 4, 0};
        int n = sizeof expected / sizeof expected[0];

        for (int i = 0; i < n; i++) {
            int16_t out[SM_ENGINE_AXES];
            bool ok = sm_engine_tick(&e, 700, 3, out);

            /* First 7 ticks return true, 8th returns false. */
            if (i < n - 1)
                CHECK(ok == true);
            else
                CHECK(ok == false);

            CHECK(out[3] == expected[i]);

            /* Every other slot is 0. */
            for (int a = 0; a < SM_ENGINE_AXES; a++)
                if (a != 3)
                    CHECK(out[a] == 0);
        }

        CHECK(sm_engine_idle(&e));
    }

    /* ---- T1b: same add, deadband 0 ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 3, 10, 768);

        static const int expected[] = {30, 21, 15, 10, 7, 5, 4, 2, 2, 1, 1, 1, 0};
        int n = sizeof expected / sizeof expected[0];

        for (int i = 0; i < n; i++) {
            int16_t out[SM_ENGINE_AXES];
            bool ok = sm_engine_tick(&e, 700, 0, out);

            if (i < n - 1)
                CHECK(ok == true);
            else
                CHECK(ok == false);

            CHECK(out[3] == expected[i]);
        }
    }

    /* ---- T2: inverted axis ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 5 | SM_ENGINE_INV, 4, 256);

        static const int expected[] = {-4, -3, -2, -1, -1, -1, 0};
        int n = sizeof expected / sizeof expected[0];

        for (int i = 0; i < n; i++) {
            int16_t out[SM_ENGINE_AXES];
            bool ok = sm_engine_tick(&e, 700, 0, out);

            if (i < n - 1)
                CHECK(ok == true);
            else
                CHECK(ok == false);

            CHECK(out[5] == expected[i]);
        }
    }

    /* ---- T3: clamp at limit ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 0, 1000, 768);
        CHECK(e.acc[0] == 89600);

        int16_t out[SM_ENGINE_AXES];
        bool ok = sm_engine_tick(&e, 700, 3, out);
        CHECK(ok == true);
        CHECK(out[0] == 350);
    }

    /* ---- T4: invalid axis specs return false ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);

        CHECK(sm_engine_add(&e, SM_ENGINE_NONE, 5, 768) == false);
        CHECK(sm_engine_add(&e, 6, 5, 768) == false);
        CHECK(sm_engine_add(&e, 7 | SM_ENGINE_INV, 5, 768) == false);

        /* Accumulators still all zero. */
        for (int i = 0; i < SM_ENGINE_AXES; i++)
            CHECK(e.acc[i] == 0);

        /* Valid add after invalid ones. */
        CHECK(sm_engine_add(&e, 2 | SM_ENGINE_INV, 1, 256) == true);
        CHECK(e.acc[2] == -256);
    }

    /* ---- T5: small values with keep 1000 ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 1, 1, 128);
        int16_t out[SM_ENGINE_AXES];
        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[1] == 1);

        sm_engine_reset(&e);
        sm_engine_add(&e, 1 | SM_ENGINE_INV, 1, 128);
        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[1] == -1);

        sm_engine_reset(&e);
        sm_engine_add(&e, 1 | SM_ENGINE_INV, 5, 128);
        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[1] == -3);
    }

    /* ---- T6: value below deadband becomes zero immediately ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 2, 1, 768);
        int16_t out[SM_ENGINE_AXES];
        bool ok = sm_engine_tick(&e, 700, 3, out);
        CHECK(ok == false);
        for (int i = 0; i < SM_ENGINE_AXES; i++)
            CHECK(out[i] == 0);
        CHECK(sm_engine_idle(&e));
    }

    /* ---- T7: extreme counts clamp ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 0, INT32_MAX, 768);
        CHECK(e.acc[0] == 89600);

        sm_engine_reset(&e);
        sm_engine_add(&e, 0 | SM_ENGINE_INV, INT32_MIN, 768);
        CHECK(e.acc[0] == 89600);
    }

    /* ---- T8: keep 1000 (and capped 1500) preserves accumulator ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 4, 10, 768);
        int16_t out[SM_ENGINE_AXES];

        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[4] == 30);
        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[4] == 30);
        sm_engine_tick(&e, 1000, 0, out);
        CHECK(out[4] == 30);
        CHECK(e.acc[4] == 7680);

        /* Same with keep_per_mille > 1000 (capped). */
        sm_engine_reset(&e);
        sm_engine_add(&e, 4, 10, 768);
        sm_engine_tick(&e, 1500, 0, out);
        CHECK(out[4] == 30);
        sm_engine_tick(&e, 1500, 0, out);
        CHECK(out[4] == 30);
        sm_engine_tick(&e, 1500, 0, out);
        CHECK(out[4] == 30);
    }

    /* ---- T9: multiple axes, mixed adds ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        sm_engine_add(&e, 0, 10, 768);
        sm_engine_add(&e, 3 | SM_ENGINE_INV, 7, 768);
        sm_engine_add(&e, 0, -4, 768);

        int16_t out[SM_ENGINE_AXES];
        bool ok = sm_engine_tick(&e, 700, 3, out);
        CHECK(ok == true);
        CHECK(out[0] == 18);
        CHECK(out[1] == 0);
        CHECK(out[2] == 0);
        CHECK(out[3] == -21);
        CHECK(out[4] == 0);
        CHECK(out[5] == 0);

        CHECK(e.acc[0] == 3225);
        CHECK(e.acc[1] == 0);
        CHECK(e.acc[2] == 0);
        CHECK(e.acc[3] == -3763);
        CHECK(e.acc[4] == 0);
        CHECK(e.acc[5] == 0);
    }

    /* ---- T10: idle and reset ---- */
    {
        struct sm_engine e;
        sm_engine_reset(&e);
        CHECK(sm_engine_idle(&e));

        sm_engine_add(&e, 1, 1, 768);
        CHECK(sm_engine_idle(&e) == false);

        sm_engine_reset(&e);
        CHECK(sm_engine_idle(&e));
    }

    if (failures == 0)
        printf("PASS %d checks\n", checks);
    else
        printf("FAILED %d of %d\n", failures, checks);

    return failures ? 1 : 0;
}

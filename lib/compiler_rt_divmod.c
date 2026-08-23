typedef unsigned long long du_int;
typedef long long di_int;

static du_int uabsdi(di_int v) {
    du_int uv = (du_int)v;
    return (v < 0) ? (du_int)(0 - uv) : uv;
}

du_int __udivmoddi4(du_int a, du_int b, du_int *rem) {
    du_int div;
    du_int quot;
    unsigned shift;

    if (b == 0) {
        if (rem) *rem = 0;
        return 0;
    }
    if (a < b) {
        if (rem) *rem = a;
        return 0;
    }

    div = b;
    shift = 0;
    while ((div >> 63) == 0 && div <= (a >> 1)) {
        div <<= 1;
        shift++;
    }

    quot = 0;
    for (;;) {
        quot <<= 1;
        if (a >= div) {
            a -= div;
            quot |= 1;
        }
        if (shift == 0) break;
        div >>= 1;
        shift--;
    }

    if (rem) *rem = a;
    return quot;
}

du_int __udivdi3(du_int a, du_int b) {
    return __udivmoddi4(a, b, (du_int *)0);
}

du_int __umoddi3(du_int a, du_int b) {
    du_int r;
    (void)__udivmoddi4(a, b, &r);
    return r;
}

di_int __divdi3(di_int a, di_int b) {
    int neg;
    du_int ua;
    du_int ub;
    du_int uq;

    if (b == 0) return 0;
    neg = ((a < 0) ^ (b < 0)) != 0;
    ua = uabsdi(a);
    ub = uabsdi(b);
    uq = __udivdi3(ua, ub);
    return neg ? -(di_int)uq : (di_int)uq;
}

di_int __moddi3(di_int a, di_int b) {
    du_int ua;
    du_int ub;
    du_int ur;

    if (b == 0) return 0;
    ua = uabsdi(a);
    ub = uabsdi(b);
    ur = __umoddi3(ua, ub);
    return (a < 0) ? -(di_int)ur : (di_int)ur;
}

/* ---- 64-bit shift/multiply helpers ----
 * Variable-width i64 shifts soften to libcalls on this target, and there is
 * no native widening 64-bit multiply; provide them using 32-bit pieces so
 * freestanding guest code links without a full libgcc. */

static void di_split(du_int v, unsigned *lo, unsigned *hi) {
    *lo = (unsigned)(v & 0xFFFFFFFFu);
    *hi = (unsigned)(v >> 32);
}
static du_int di_join(unsigned lo, unsigned hi) {
    return (du_int)lo | ((du_int)hi << 32);
}

du_int __ashldi3(du_int a, int b) {
    unsigned lo, hi;
    di_split(a, &lo, &hi);
    if ((unsigned)b >= 64) return 0;
    if ((unsigned)b >= 32) {
        return di_join(0, lo << ((unsigned)b - 32));
    }
    if (b == 0) return a;
    return di_join(lo << b, (hi << b) | (lo >> (32 - b)));
}

du_int __lshrdi3(du_int a, int b) {
    unsigned lo, hi;
    di_split(a, &lo, &hi);
    if ((unsigned)b >= 64) return 0;
    if ((unsigned)b >= 32) {
        return di_join(hi >> ((unsigned)b - 32), 0);
    }
    if (b == 0) return a;
    return di_join((lo >> b) | (hi << (32 - b)), hi >> b);
}

di_int __ashrdi3(di_int a, int b) {
    unsigned lo, hi;
    di_split((du_int)a, &lo, &hi);
    if ((unsigned)b >= 64) return (hi & 0x80000000u) ? -1 : 0;
    if ((unsigned)b >= 32) {
        unsigned fill = (hi & 0x80000000u) ? 0xFFFFFFFFu : 0u;
        unsigned newlo = hi >> ((unsigned)b - 32);
        /* arithmetic fill */
        if (fill) {
            unsigned sh = (unsigned)b - 32;
            if (sh < 32) newlo |= fill << (32 - sh);
        }
        return (di_int)di_join(newlo, fill);
    }
    if (b == 0) return a;
    unsigned newlo = (lo >> b) | (hi << (32 - b));
    unsigned newhi = (hi >> b);
    /* sign-fill the bits shifted out of hi */
    if (hi & 0x80000000u) {
        unsigned sh = (unsigned)b;
        if (sh < 32) newhi |= (0xFFFFFFFFu << (32 - sh));
    }
    return (di_int)di_join(newlo, newhi);
}

du_int __muldi3(du_int a, du_int b) {
    /* shift-and-add; constant-width shifts keep this free of libcalls */
    du_int r = 0;
    while (b != 0) {
        if (b & 1) r += a;
        a <<= 1;
        b >>= 1;
    }
    return r;
}

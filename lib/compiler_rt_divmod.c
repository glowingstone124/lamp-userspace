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

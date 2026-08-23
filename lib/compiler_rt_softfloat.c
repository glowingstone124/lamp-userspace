/*
 * Binary64 soft-float runtime for the LAMP guest.
 *
 * The ISA has no double-precision instructions, so the backend softens f64
 * operations into compiler-rt style helper calls. Everything here runs on
 * plain unsigned 64-bit integers; the guest backend expands i64 add/sub/
 * shifts/compares inline and provides i64 divide helpers, so no floating
 * point C operations appear anywhere in this file.
 *
 * ABI note: i64 arguments are split into two consecutive i32 registers, so a
 * signature like (u64, u64) receives exactly what an (f64, f64) caller pushes.
 */

typedef unsigned long long dbits;
typedef long long sbits;
typedef unsigned int f32bits;

#define D_SIGN 0x8000000000000000ull
#define D_EXP  0x7FF0000000000000ull
#define D_FRAC 0x000FFFFFFFFFFFFFull
#define D_IMPL 0x0010000000000000ull
#define D_QUIET 0x0008000000000000ull
#define D_BIAS 1023

/* ================= compares ================= */

static int df_is_nan(dbits x) {
    unsigned hi = (unsigned)(x >> 32);
    if ((hi & 0x7FF00000u) != 0x7FF00000u) {
        return 0;
    }
    return ((hi & 0x000FFFFFu) != 0u) || ((unsigned)x != 0u);
}

static int df_cmp3way(dbits ua, dbits ub, int nan_result) {
    if (df_is_nan(ua) || df_is_nan(ub)) {
        return nan_result;
    }
    /* Signed zeros compare equal. */
    if ((ua << 1) == 0ull) {
        ua = 0;
    }
    if ((ub << 1) == 0ull) {
        ub = 0;
    }
    dbits ka;
    dbits kb;
    if (ua >> 63) {
        ka = ~ua;
    } else {
        ka = ua ^ D_SIGN;
    }
    if (ub >> 63) {
        kb = ~ub;
    } else {
        kb = ub ^ D_SIGN;
    }
    if (ka < kb) {
        return -1;
    }
    if (ka > kb) {
        return 1;
    }
    return 0;
}

int __eqdf2(dbits a, dbits b)    { return df_cmp3way(a, b, 1); }
int __nedf2(dbits a, dbits b)    { return df_cmp3way(a, b, 1); }
int __ltdf2(dbits a, dbits b)    { return df_cmp3way(a, b, 1); }
int __ledf2(dbits a, dbits b)    { return df_cmp3way(a, b, 1); }
int __gtdf2(dbits a, dbits b)    { return df_cmp3way(a, b, -1); }
int __gedf2(dbits a, dbits b)    { return df_cmp3way(a, b, -1); }
int __unorddf2(dbits a, dbits b) { return df_is_nan(a) || df_is_nan(b); }

/* ================= shared internals ================= */

/* Decompose a double into sign, scale exponent and 53-bit significand
 * (implicit bit set), such that value = sig * 2^exp. Returns 1 for zero. */
static int df_unpack(dbits x, unsigned *sign, int *exp, dbits *sig) {
    *sign = (unsigned)(x >> 63);
    int f = (int)((x >> 52) & 0x7FF);
    dbits m = x & D_FRAC;
    if (f == 0x7FF) {
        *exp = 0x7FFF; /* inf/nan marker */
        *sig = m;
        return 0;
    }
    if (f == 0) {
        if (m == 0) {
            *exp = 0;
            *sig = 0;
            return 1;
        }
        /* subnormal: value = m_raw * 2^-1074; normalize to implicit form */
        int k = 0;
        while ((m & D_IMPL) == 0ull) {
            m <<= 1;
            k++;
        }
        *sig = m;
        *exp = -1074 - k;
        return 0;
    }
    /* value = (1.f) * 2^(f-1023) = (m|impl) * 2^(f-1075) */
    *sig = m | D_IMPL;
    *exp = f - 1075;
    return 0;
}

/* Round-to-nearest-even packer. Value = Q * 2^(e - 52) with Q in [2^52, 2^53)
 * after rounding; G/S extend precision below Q. Handles overflow to infinity,
 * underflow through the subnormal range, and encodes the IEEE fields. */
static dbits df_pack(unsigned sign, sbits e, dbits q, int g, int s) {
    /* value = q * 2^e + g/2*ulp + s*smaller, with q in [2^52, 2^53).
     * IEEE exponent field = e + 1075 whenever the result stays normal. */
    int was_subnormal = 0;
    if (e < -1074) {
        /* Subnormal target: right-shift first so the guard ends up exactly
         * one bit below the final LSB, folding everything else (including
         * any caller-provided guard) into sticky. */
        sbits shift = -1074 - e;
        if (shift > 54) {
            /* value < a quarter of the smallest subnormal */
            return (dbits)sign << 63;
        }
        int ng = (int)((q >> (shift - 1)) & 1ull);
        dbits mask = (1ull << (shift - 1)) - 1ull;
        s |= g | (int)((q & mask) != 0ull);
        g = ng;
        q >>= shift;
        e += shift;
        was_subnormal = 1;
    }

    /* Round to nearest, ties to even. */
    if (g && (s || (q & 1ull))) {
        q++;
        if (q == (D_IMPL << 1)) {
            q >>= 1;
            e++;
        }
    }

    if (e > 971) {
        /* overflow -> infinity */
        return ((dbits)sign << 63) | D_EXP;
    }
    if (was_subnormal) {
        if (q == 0ull) {
            return (dbits)sign << 63;
        }
        if (q == D_IMPL) {
            /* rounded up into the smallest normal */
            return ((dbits)sign << 63) | D_IMPL;
        }
        return ((dbits)sign << 63) | q; /* exponent field stays zero */
    }
    return ((dbits)sign << 63) | ((dbits)(e + 1075) << 52) | (q & D_FRAC);
}

/* 128-bit product of two 64-bit values (schoolbook double-add-shift). */
static void df_umul128(dbits a, dbits b, dbits *hi, dbits *lo) {
    /* MSB-first shift-and-add: shift first, then conditionally add, so the
     * final product receives no spurious trailing doubling. */
    dbits h = 0;
    dbits l = 0;
    for (int i = 0; i < 64; i++) {
        dbits nh = (h << 1) | (l >> 63);
        dbits nl = l << 1;
        h = nh;
        l = nl;
        if ((b >> 63) & 1ull) {
            dbits old = l;
            l += a;
            if (l < old) {
                h++;
            }
        }
        b <<= 1;
    }
    *hi = h;
    *lo = l;
}

/* ================= arithmetic ================= */


static int df_msb(dbits v) {
    int t = -1;
    while (v) {
        t++;
        v >>= 1;
    }
    return t;
}

/* Normalize a 128-bit significand window down into the packer. */
static dbits df_norm_pack128(unsigned sign, dbits whi, dbits wlo, sbits e,
                             int s_extra) {
    int tp = whi ? (df_msb(whi) + 64) : df_msb(wlo);
    if (tp < 0) {
        return s_extra ? (((dbits)sign << 63) | 1ull)
                       : ((dbits)sign << 63);
    }
    int cut = tp - 52;
    int g = 0;
    int s = s_extra;
    dbits w;
    if (cut >= 64) {
        int sh2 = cut - 64;
        w = whi >> sh2;
        g = (int)((whi >> (sh2 - 1)) & 1ull);
        s |= ((whi & ((1ull << (sh2 - 1)) - 1ull)) != 0ull) || (wlo != 0ull);
    } else if (cut > 0) {
        w = (wlo >> cut) | ((whi & ((1ull << cut) - 1ull)) << (64 - cut));
        g = (int)((wlo >> (cut - 1)) & 1ull);
        s |= (int)((wlo & ((1ull << (cut - 1)) - 1ull)) != 0ull);
    } else {
        /* cut <= 0: normalize upward; there is no precision below the
         * original LSB, so only the caller's extra sticky applies. */
        w = wlo << (-cut);
        e += cut;
        g = 0;
        s = s_extra;
        return df_pack(sign, e, w, g, s);
    }
    return df_pack(sign, e + cut, w, g, s);
}

/* Normalize W so its most significant bit sits at bit 52, folding anything
 * shifted out into the guard/sticky pair, then round and encode. The value
 * represented is W * 2^E (plus half an ulp of G and a negative-power tail of
 * S below it). */
static dbits df_norm_pack(unsigned sign, dbits w, sbits e, int g, int s) {
    int t = df_msb(w);
    if (t < 0) {
        return (dbits)sign << 63;
    }
    if (t < 52) {
        w <<= (52 - t);
        e -= (52 - t);
    } else if (t > 52) {
        /* Right-shifting folds the previous guard into sticky and installs a
         * fresh guard from the last bit kept. */
        int sh = t - 52;
        dbits mask = (sh >= 64) ? ~0ull : ((1ull << (sh - 1)) - 1ull);
        int ng = (int)((w >> (sh - 1)) & 1ull);
        s |= g | (int)((w & mask) != 0ull);
        g = ng;
        w >>= sh;
        e += sh;
    }
    return df_pack(sign, e, w & (D_FRAC | D_IMPL), g, s);
}

dbits __adddf3(dbits a, dbits b) {
    unsigned sa, sb;
    int ea, eb;
    dbits ma, mb;
    int az = df_unpack(a, &sa, &ea, &ma);
    int bz = df_unpack(b, &sb, &eb, &mb);

    if (ea == 0x7FFF) {
        if (ma != 0) {
            return a | D_QUIET;
        }
        if (eb == 0x7FFF && sb != sa) {
            return D_EXP | D_QUIET; /* Inf - Inf */
        }
        return a;
    }
    if (eb == 0x7FFF) {
        if (mb != 0) {
            return b | D_QUIET;
        }
        return b;
    }
    if (az) {
        if (bz) {
            return ((dbits)(sa & sb)) << 63;
        }
        return b;
    }
    if (bz) {
        return a;
    }

    /* make |a| >= |b| so the aligned frame never goes negative */
    int swapped = 0;
    if (ea < eb || (ea == eb && ma < mb)) {
        unsigned ts = sa;
        sa = sb;
        sb = ts;
        int te = ea;
        ea = eb;
        eb = te;
        dbits tm = ma;
        ma = mb;
        mb = tm;
        swapped = 1;
    }
    unsigned rs = sa;
    int align = ea - eb;

    if (align > 122) {
        /* |b| sits far below any bit that can influence rounding: the sum
         * or difference rounds back to the larger operand exactly. */
        return swapped ? b : a;
    }

    /* Exact alignment in a 128-bit frame: A = ma<<6, B = (mb<<6)>>align.
     * B starts in the low word only, so right shifts never produce a high
     * word; whatever falls off the bottom becomes extra sticky. */
    dbits ga = ma << 6;
    dbits bl = mb << 6;
    int s_extra = 0;
    if (align >= 64) {
        s_extra = (bl != 0ull);
        bl = 0;
    } else if (align > 0) {
        dbits mask = (1ull << align) - 1ull;
        s_extra = (int)((bl & mask) != 0ull);
        bl >>= align;
    }

    dbits th = 0;
    dbits tl;
    int res_zero = 0;
    if (sa == sb) {
        /* Addition may carry into bit 59; the high word keeps that carry. */
        tl = ga + bl;
        th = (tl < ga) ? 1ull : 0ull;
    } else {
        if (ga < bl) {
            /* cannot happen after the magnitude swap */
            res_zero = 1;
        }
        tl = ga - bl;
        /* The dropped tail of b makes the exact difference sit strictly
         * between tl-1 and tl; decrementing here lets the guard/sticky
         * extraction below see the true bits (the sticky flag already
         * records that a nonzero fraction follows). */
        if (s_extra && tl != 0ull) {
            tl -= 1;
        } else if (tl == 0ull) {
            res_zero = 1;
        }
    }
    if (res_zero) {
        return 0; /* exact cancellation -> +0 */
    }
    /* th carries addition carry-out / subtraction high bits; dropping it
     * here would truncate every result whose frame exceeds 64 bits. */
    return df_norm_pack128(rs, th, tl, ea - 6, s_extra);
}

dbits __subdf3(dbits a, dbits b) {
    return __adddf3(a, b ^ D_SIGN);
}

dbits __muldf3(dbits a, dbits b) {
    unsigned sa, sb;
    int ea, eb;
    dbits ma, mb;
    int az = df_unpack(a, &sa, &ea, &ma);
    int bz = df_unpack(b, &sb, &eb, &mb);
    unsigned rs = sa ^ sb;

    if (ea == 0x7FFF || eb == 0x7FFF) {
        if ((ea == 0x7FFF && ma != 0) || (eb == 0x7FFF && mb != 0)) {
            return D_EXP | D_QUIET;
        }
        /* Inf * 0 -> NaN */
        if ((ea == 0x7FFF && bz) || (eb == 0x7FFF && az)) {
            return D_EXP | D_QUIET;
        }
        return ((dbits)rs << 63) | D_EXP;
    }
    if (az || bz) {
        return ((dbits)rs) << 63;
    }

    dbits hi;
    dbits lo;
    df_umul128(ma, mb, &hi, &lo);
    /* value = (hi:lo) * 2^(ea+eb); the unified normalizer extracts the
     * significand, guard and sticky directly from the wide product so the
     * subnormal reshift in the packer stays exact. */
    return df_norm_pack128(rs, hi, lo, ea + eb, 0);
}

dbits __divdf3(dbits a, dbits b) {
    unsigned sa, sb;
    int ea, eb;
    dbits ma, mb;
    int az = df_unpack(a, &sa, &ea, &ma);
    int bz = df_unpack(b, &sb, &eb, &mb);
    unsigned rs = sa ^ sb;

    if (ea == 0x7FFF || eb == 0x7FFF) {
        if ((ea == 0x7FFF && ma != 0) || (eb == 0x7FFF && mb != 0)) {
            return D_EXP | D_QUIET;
        }
        if (ea == 0x7FFF) {
            if (eb == 0x7FFF) return D_EXP | D_QUIET; /* Inf / Inf */
            return ((dbits)rs << 63) | D_EXP;
        }
        /* a finite, b inf -> zero; b zero -> NaN handled below */
        if (!bz) return ((dbits)rs) << 63;
        return D_EXP | D_QUIET;
    }
    if (bz) {
        if (az) return D_EXP | D_QUIET; /* 0 / 0 */
        return ((dbits)rs << 63) | D_EXP;
    }
    if (az) {
        return ((dbits)rs) << 63;
    }

    /* Restoring division on the ratio ma/mb in [0.5, 2): emit a leading
     * bit at weight 2^-1 relative to bit54, then 54 fractional bits, so the
     * final value is q * 2^(ea - eb - 54) with r < mb. */
    /* leading quotient bit at weight 2^54, then 54 fractional bits */
    dbits q = 0;
    dbits r = ma;
    if (r >= mb) {
        r -= mb;
        q = (1ull << 54);
    } else {
        q = 0;
    }
    for (int i = 53; i >= 0; i--) {
        r <<= 1;
        if (r >= mb) {
            r -= mb;
            q |= (1ull << i);
        }
    }
    int sticky = (r != 0);
    return df_norm_pack(rs, q, ea - eb - 54, 0, sticky);
}

/* ================= conversions ================= */

dbits __extendsfdf2(f32bits a) {
    unsigned sign = a & 0x80000000u;
    unsigned exp = (a >> 23) & 0xFFu;
    unsigned frac = a & 0x007FFFFFu;
    dbits r_sign = (dbits)sign << 32;

    if (exp == 0xFFu) {
        if (frac == 0u) {
            return r_sign | D_EXP; /* inf */
        }
        return r_sign | D_EXP | ((dbits)frac << 29) | D_QUIET; /* NaN */
    }
    if (exp == 0u && frac == 0u) {
        return r_sign; /* +-0 */
    }
    if (exp == 0u) {
        int shift = 0;
        while ((frac & 0x00800000u) == 0u) {
            frac <<= 1;
            shift++;
        }
        frac &= 0x007FFFFFu;
        sbits dexp = -126 - shift + 1023;
        dbits m = (dbits)frac << 29;
        return r_sign | ((dbits)dexp << 52) | m;
    }
    unsigned dexp = exp + (1023u - 127u);
    dbits m = (dbits)frac << 29;
    return r_sign | ((dbits)dexp << 52) | m;
}

f32bits __truncdfsf2(dbits a) {
    unsigned sign = (unsigned)(a >> 32) & 0x80000000u;
    int e = (int)((a >> 52) & 0x7FF);
    dbits m = a & D_FRAC;

    if (e == 0x7FF) {
        if (m == 0) {
            return sign | 0x7F800000u; /* inf */
        }
        /* NaN: keep the top payload bits, force quiet */
        f32bits keep = (f32bits)(((m | (a >> 32)) >> 21) & 0x003FFFFFu);
        return sign | 0x7FC00000u | (keep ? keep : 0x400000u);
    }
    if (e == 0 && m == 0) {
        return sign; /* +-0 */
    }

    unsigned rs = sign;
    int re;
    dbits q53;
    int g;
    int s;
    if (e == 0) {
        q53 = m;
        re = 1 - D_BIAS;
        /* normalize into implicit-bit form like unpack does */
        while ((q53 & D_IMPL) == 0ull) {
            q53 <<= 1;
            re--;
        }
    } else {
        q53 = m | D_IMPL;
        re = e - D_BIAS;
    }
    /* value = q53 * 2^(re-52); target f32 significand width is 24 bits:
       take top 24 bits of q53 with guard+sticky from the rest. */
    int t = df_msb(q53);
    /* desired: value = q24 * 2^(fe-23). Start from fe = re + (t-23) and cut
       q24 = q53 >> (t-23), rounding RNE. Subnormal targets need extra right
       shifts when fe < -126. */
    int sh = t - 23;
    dbits mask = (sh >= 64) ? ~0ull : ((1ull << sh) - 1ull);
    g = (int)((q53 >> (sh - 1)) & 1ull);
    s = (int)((q53 & mask) != 0ull);
    dbits q24 = q53 >> sh;
    int fe = re + sh - 29; /* value: q53*2^(re-52) == q24*2^(fe-23) */

    /* f32 min normal exponent is -126; smaller results become subnormal
     * outputs whose encoding carries no exponent field. */
    int subnormal_out = 0;
    if (fe < -126) {
        int extra = -126 - fe;
        for (int i = 0; i < extra; i++) {
            s |= g;
            g = (int)(q24 & 1ull);
            q24 >>= 1;
        }
        fe = -126;
        subnormal_out = 1;
    }
    if (g && (s || (q24 & 1ull))) {
        q24++;
        if (q24 == (1ull << 24)) {
            q24 >>= 1;
            if (subnormal_out) {
                /* rounded up into the smallest normal */
                subnormal_out = 0;
                fe = -126;
            } else {
                fe++;
            }
        }
    }
    if (fe > 127) {
        return sign | 0x7F800000u; /* overflow to inf */
    }
    if (subnormal_out) {
        if (q24 == 0ull) {
            return sign; /* rounded to zero */
        }
        return sign | (unsigned)q24;
    }
    if (q24 == 0ull) {
        return sign;
    }
    unsigned fexp = (unsigned)(fe + 127);
    return sign | (fexp << 23) | ((unsigned)(q24 & 0x007FFFFFull));
}

/* integer -> double */
static dbits df_from_mag(dbits mag, unsigned sign) {
    if (mag == 0ull) {
        return (dbits)sign << 63;
    }
    int tm = df_msb(mag);
    dbits q = mag << (52 - tm);
    return ((dbits)sign << 63) | ((dbits)(tm + D_BIAS) << 52) | (q & D_FRAC);
}

dbits __floatsidf(int v) {
    if (v < 0) {
        dbits mag = (dbits)(-(sbits)(v + 1)) + 1ull; /* safe for INT_MIN */
        return df_from_mag(mag, 1u);
    }
    return df_from_mag((dbits)v, 0u);
}

dbits __floatunsidf(unsigned v) {
    return df_from_mag((dbits)v, 0u);
}

/* double -> integer (truncate toward zero, saturate on overflow) */
static int df_to_i(dbits a, int is_signed) {
    unsigned sign = (unsigned)(a >> 63);
    int e = (int)((a >> 52) & 0x7FF);
    dbits m = a & D_FRAC;
    if (e == 0x7FF) {
        return is_signed ? (int)0x80000000 : 0x7FFFFFFF;
    }
    if (e == 0) {
        return 0; /* zero or subnormal below 1 */
    }
    dbits q = m | D_IMPL;
    int re = e - D_BIAS; /* |a| in [2^re, 2^(re+1)) */
    if (re < 0) {
        return 0;
    }
    dbits ip;
    if (re <= 52) {
        ip = q >> (52 - re); /* truncated integer magnitude */
    } else {
        int sh2 = re - 52;
        if (sh2 >= 32 || ((q << sh2) >> 32) != 0ull) {
            return is_signed ? (int)0x80000000 : 0x7FFFFFFF;
        }
        ip = q << sh2;
    }
    if (is_signed) {
        if (sign) {
            if (ip > (dbits)0x80000000ull) {
                return (int)0x80000000;
            }
            return (int)(-(sbits)ip); /* covers -2^31 exactly */
        }
        if (ip > (dbits)0x7FFFFFFFull) {
            return 0x7FFFFFFF;
        }
        return (int)ip;
    }
    if (sign || ip > (dbits)0xFFFFFFFFull) {
        return is_signed ? (int)0x80000000 : 0x7FFFFFFF;
    }
    return (int)ip;
}

int __fixdfsi(dbits a)           { return df_to_i(a, 1); }
unsigned __fixunsdfsi(dbits a)   { return (unsigned)df_to_i(a, 0); }

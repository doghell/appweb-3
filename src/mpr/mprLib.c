#include "mpr.h"

/******************************************************************************/
/* 
 *  This file is an amalgamation of all the individual source code files for
 *  Multithreaded Portable Runtime 3.1.4.
 *
 *  Catenating all the source into a single file makes embedding simpler and
 *  the resulting application faster, as many compilers can do whole file
 *  optimization.
 *
 *  If you want to modify mpr, you can still get the whole source
 *  as individual files if you need.
 */


/************************************************************************/
/*
 *  Start of file "../src/dtoa.c"
 */
/************************************************************************/

/* 
    Source from: http://www.netlib.org/fp/dtoa.c

    Changes wrapped with #if EMBEDTHIS
 */
/****************************************************************
 *
 * The author of this software is David M. Gay.
 *
 * Copyright (c) 1991, 2000, 2001 by Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 ***************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").  */

/* On a machine with IEEE extended-precision registers, it is
 * necessary to specify double-precision (53-bit) rounding precision
 * before invoking strtod or dtoa.  If the machine uses (the equivalent
 * of) Intel 80x87 arithmetic, the call
 *  _control87(PC_53, MCW_PC);
 * does this with many compilers.  Whether this or another call is
 * appropriate depends on the compiler; for this to work, it may be
 * necessary to #include "float.h" or another system-dependent header
 * file.
 */

/* strtod for IEEE-, VAX-, and IBM-arithmetic machines.
 *
 * This strtod returns a nearest machine number to the input decimal
 * string (or sets errno to ERANGE).  With IEEE arithmetic, ties are
 * broken by the IEEE round-even rule.  Otherwise ties are broken by
 * biased rounding (add half and chop).
 *
 * Inspired loosely by William D. Clinger's paper "How to Read Floating
 * Point Numbers Accurately" [Proc. ACM SIGPLAN '90, pp. 92-101].
 *
 * Modifications:
 *
 *  1. We only require IEEE, IBM, or VAX double-precision
 *      arithmetic (not IEEE double-extended).
 *  2. We get by with floating-point arithmetic in a case that
 *      Clinger missed -- when we're computing d * 10^n
 *      for a small integer d and the integer n is not too
 *      much larger than 22 (the maximum integer k for which
 *      we can represent 10^k exactly), we may be able to
 *      compute (d*10^k) * 10^(e-k) with just one roundoff.
 *  3. Rather than a bit-at-a-time adjustment of the binary
 *      result in the hard case, we use floating-point
 *      arithmetic to determine the adjustment to within
 *      one bit; only in really hard cases do we need to
 *      compute a second residual.
 *  4. Because of 3., we don't need a large table of powers of 10
 *      for ten-to-e (just some small tables, e.g. of 10^k
 *      for 0 <= k <= 22).
 */

/*
 * #define IEEE_8087 for IEEE-arithmetic machines where the least
 *  significant byte has the lowest address.
 * #define IEEE_MC68k for IEEE-arithmetic machines where the most
 *  significant byte has the lowest address.
 * #define Long int on machines with 32-bit ints and 64-bit longs.
 * #define IBM for IBM mainframe-style floating-point arithmetic.
 * #define VAX for VAX-style floating-point arithmetic (D_floating).
 * #define No_leftright to omit left-right logic in fast floating-point
 *  computation of dtoa.
 * #define Honor_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *  and strtod and dtoa should round accordingly.  Unless Trust_FLT_ROUNDS
 *  is also #defined, fegetround() will be queried for the rounding mode.
 *  Note that both FLT_ROUNDS and fegetround() are specified by the C99
 *  standard (and are specified to be consistent, with fesetround()
 *  affecting the value of FLT_ROUNDS), but that some (Linux) systems
 *  do not work correctly in this regard, so using fegetround() is more
 *  portable than using FLT_FOUNDS directly.
 * #define Check_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *  and Honor_FLT_ROUNDS is not #defined.
 * #define RND_PRODQUOT to use rnd_prod and rnd_quot (assembly routines
 *  that use extended-precision instructions to compute rounded
 *  products and quotients) with IBM.
 * #define ROUND_BIASED for IEEE-format with biased rounding.
 * #define Inaccurate_Divide for IEEE-format with correctly rounded
 *  products but inaccurate quotients, e.g., for Intel i860.
 * #define NO_LONG_LONG on machines that do not have a "long long"
 *  integer type (of >= 64 bits).  On such machines, you can
 *  #define Just_16 to store 16 bits per 32-bit Long when doing
 *  high-precision integer arithmetic.  Whether this speeds things
 *  up or slows things down depends on the machine and the number
 *  being converted.  If long long is available and the name is
 *  something other than "long long", #define Llong to be the name,
 *  and if "unsigned Llong" does not work as an unsigned version of
 *  Llong, #define #ULLong to be the corresponding unsigned type.
 * #define KR_headers for old-style C function headers.
 * #define Bad_float_h if your system lacks a float.h or if it does not
 *  define some or all of DBL_DIG, DBL_MAX_10_EXP, DBL_MAX_EXP,
 *  FLT_RADIX, FLT_ROUNDS, and DBL_MAX.
 * #define MALLOC your_malloc, where your_malloc(n) acts like malloc(n)
 *  if memory is available and otherwise does something you deem
 *  appropriate.  If MALLOC is undefined, malloc will be invoked
 *  directly -- and assumed always to succeed.  Similarly, if you
 *  want something other than the system's free() to be called to
 *  recycle memory acquired from MALLOC, #define FREE to be the
 *  name of the alternate routine.  (FREE or free is only called in
 *  pathological cases, e.g., in a dtoa call after a dtoa return in
 *  mode 3 with thousands of digits requested.)
 * #define Omit_Private_Memory to omit logic (added Jan. 1998) for making
 *  memory allocations from a private pool of memory when possible.
 *  When used, the private pool is PRIVATE_MEM bytes long:  2304 bytes,
 *  unless #defined to be a different length.  This default length
 *  suffices to get rid of MALLOC calls except for unusual cases,
 *  such as decimal-to-binary conversion of a very long string of
 *  digits.  The longest string dtoa can return is about 751 bytes
 *  long.  For conversions by strtod of strings of 800 digits and
 *  all dtoa conversions in single-threaded executions with 8-byte
 *  pointers, PRIVATE_MEM >= 7400 appears to suffice; with 4-byte
 *  pointers, PRIVATE_MEM >= 7112 appears adequate.
 * #define NO_INFNAN_CHECK if you do not wish to have INFNAN_CHECK
 *  #defined automatically on IEEE systems.  On such systems,
 *  when INFNAN_CHECK is #defined, strtod checks
 *  for Infinity and NaN (case insensitively).  On some systems
 *  (e.g., some HP systems), it may be necessary to #define NAN_WORD0
 *  appropriately -- to the most significant word of a quiet NaN.
 *  (On HP Series 700/800 machines, -DNAN_WORD0=0x7ff40000 works.)
 *  When INFNAN_CHECK is #defined and No_Hex_NaN is not #defined,
 *  strtod also accepts (case insensitively) strings of the form
 *  NaN(x), where x is a string of hexadecimal digits and spaces;
 *  if there is only one string of hexadecimal digits, it is taken
 *  for the 52 fraction bits of the resulting NaN; if there are two
 *  or more strings of hex digits, the first is for the high 20 bits,
 *  the second and subsequent for the low 32 bits, with intervening
 *  white space ignored; but if this results in none of the 52
 *  fraction bits being on (an IEEE Infinity symbol), then NAN_WORD0
 *  and NAN_WORD1 are used instead.
 * #define MULTIPLE_THREADS if the system offers preemptively scheduled
 *  multiple threads.  In this case, you must provide (or suitably
 *  #define) two locks, acquired by ACQUIRE_DTOA_LOCK(n) and freed
 *  by FREE_DTOA_LOCK(n) for n = 0 or 1.  (The second lock, accessed
 *  in pow5mult, ensures lazy evaluation of only one copy of high
 *  powers of 5; omitting this lock would introduce a small
 *  probability of wasting memory, but would otherwise be harmless.)
 *  You must also invoke freedtoa(s) to free the value s returned by
 *  dtoa.  You may do so whether or not MULTIPLE_THREADS is #defined.
 * #define NO_IEEE_Scale to disable new (Feb. 1997) logic in strtod that
 *  avoids underflows on inputs whose result does not underflow.
 *  If you #define NO_IEEE_Scale on a machine that uses IEEE-format
 *  floating-point numbers and flushes underflows to zero rather
 *  than implementing gradual underflow, then you must also #define
 *  Sudden_Underflow.
 * #define USE_LOCALE to use the current locale's decimal_point value.
 * #define SET_INEXACT if IEEE arithmetic is being used and extra
 *  computation should be done to set the inexact flag when the
 *  result is inexact and avoid setting inexact when the result
 *  is exact.  In this case, dtoa.c must be compiled in
 *  an environment, perhaps provided by #include "dtoa.c" in a
 *  suitable wrapper, that defines two functions,
 *      int get_inexact(void);
 *      void clear_inexact(void);
 *  such that get_inexact() returns a nonzero value if the
 *  inexact bit is already set, and clear_inexact() sets the
 *  inexact bit to 0.  When SET_INEXACT is #defined, strtod
 *  also does extra computations to set the underflow and overflow
 *  flags when appropriate (i.e., when the result is tiny and
 *  inexact or when it is a numeric value rounded to +-infinity).
 * #define NO_ERRNO if strtod should not assign errno = ERANGE when
 *  the result overflows to +-Infinity or underflows to 0.
 * #define NO_HEX_FP to omit recognition of hexadecimal floating-point
 *  values by strtod.
 * #define NO_STRTOD_BIGCOMP (on IEEE-arithmetic systems only for now)
 *  to disable logic for "fast" testing of very long input strings
 *  to strtod.  This testing proceeds by initially truncating the
 *  input string, then if necessary comparing the whole string with
 *  a decimal expansion to decide close cases. This logic is only
 *  used for input more than STRTOD_DIGLIM digits long (default 40).
 */
#if EMBEDTHIS || 1
    #include    "buildConfig.h"
#endif
#if BLD_FEATURE_FLOATING_POINT

#if EMBEDTHIS || 1
    #if WIN
        typedef int int32_t;
        typedef unsigned int uint32_t;
    #endif
    #define Long int32_t
    #define ULong uint32_t
#if BLD_HOST_CPU_ARCH == MPR_CPU_PPC
    #define IEEE_MC68k 1
#else
    #define IEEE_8087 1
#endif
#if VXWORKS
    #undef MALLOC
    #undef FREE
    #undef LSB
    #undef Bcopy
#endif
#endif

#ifndef Long
#define Long long
#endif
#ifndef ULong
typedef unsigned Long ULong;
#endif

#if !EMBEDTHIS
#ifdef DEBUG
 #include "stdio.h"
#define Bug(x) {fprintf(stderr, "%s\n", x); exit(1);}
#endif

 #include "stdlib.h"
 #include "string.h"

#if FREEBSD
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#ifdef USE_LOCALE
 #include "locale.h"
#endif

#ifdef Honor_FLT_ROUNDS
#ifndef Trust_FLT_ROUNDS
 #include <fenv.h>
#endif
#endif
#endif

#ifdef MALLOC
#ifdef KR_headers
extern char *MALLOC();
#else
extern void *MALLOC(size_t);
#endif
#else
#define MALLOC malloc
#endif

#ifndef Omit_Private_Memory
#ifndef PRIVATE_MEM
#define PRIVATE_MEM 2304
#endif
#define PRIVATE_mem ((PRIVATE_MEM+sizeof(double)-1)/sizeof(double))
static double private_mem[PRIVATE_mem], *pmem_next = private_mem;
#endif

#undef IEEE_Arith
#undef Avoid_Underflow
#ifdef IEEE_MC68k
#define IEEE_Arith
#endif
#ifdef IEEE_8087
#define IEEE_Arith
#endif

#ifdef IEEE_Arith
#ifndef NO_INFNAN_CHECK
#undef INFNAN_CHECK
#define INFNAN_CHECK
#endif
#else
#undef INFNAN_CHECK
#define NO_STRTOD_BIGCOMP
#endif

 #include "errno.h"

#ifdef Bad_float_h

#ifdef IEEE_Arith
#define DBL_DIG 15
#define DBL_MAX_10_EXP 308
#define DBL_MAX_EXP 1024
#define FLT_RADIX 2
#endif /*IEEE_Arith*/

#ifdef IBM
#define DBL_DIG 16
#define DBL_MAX_10_EXP 75
#define DBL_MAX_EXP 63
#define FLT_RADIX 16
#define DBL_MAX 7.2370055773322621e+75
#endif

#ifdef VAX
#define DBL_DIG 16
#define DBL_MAX_10_EXP 38
#define DBL_MAX_EXP 127
#define FLT_RADIX 2
#define DBL_MAX 1.7014118346046923e+38
#endif

#ifndef LONG_MAX
#define LONG_MAX 2147483647
#endif

#else /* ifndef Bad_float_h */
 #include "float.h"
#endif /* Bad_float_h */

#ifndef __MATH_H__
 #include "math.h"
#endif

#if !EMBEDTHIS
#define strtod UNUSED_strtod
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONST
#ifdef KR_headers
#define CONST /* blank */
#else
#define CONST const
#endif
#endif

#if defined(IEEE_8087) + defined(IEEE_MC68k) + defined(VAX) + defined(IBM) != 1
Exactly one of IEEE_8087, IEEE_MC68k, VAX, or IBM should be defined.
#endif

typedef union { double d; ULong L[2]; } U;

#ifdef IEEE_8087
#define word0(x) (x)->L[1]
#define word1(x) (x)->L[0]
#else
#define word0(x) (x)->L[0]
#define word1(x) (x)->L[1]
#endif
#define dval(x) (x)->d

#ifndef STRTOD_DIGLIM
#define STRTOD_DIGLIM 40
#endif

#ifdef DIGLIM_DEBUG
extern int strtod_diglim;
#else
#define strtod_diglim STRTOD_DIGLIM
#endif

/* The following definition of Storeinc is appropriate for MIPS processors.
 * An alternative that might be better on some machines is
 * #define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
 */
#if defined(IEEE_8087) + defined(VAX)
#define Storeinc(a,b,c) (((unsigned short *)a)[1] = (unsigned short)b, \
((unsigned short *)a)[0] = (unsigned short)c, a++)
#else
#define Storeinc(a,b,c) (((unsigned short *)a)[0] = (unsigned short)b, \
((unsigned short *)a)[1] = (unsigned short)c, a++)
#endif

/* #define P DBL_MANT_DIG */
/* Ten_pmax = floor(P*log(2)/log(5)) */
/* Bletch = (highest power of 2 < DBL_MAX_10_EXP) / 16 */
/* Quick_max = floor((P-1)*log(FLT_RADIX)/log(10) - 1) */
/* Int_max = floor(P*log(FLT_RADIX)/log(10) - 1) */

#ifdef IEEE_Arith
#define Exp_shift  20
#define Exp_shift1 20
#define Exp_msk1    0x100000
#define Exp_msk11   0x100000
#define Exp_mask  0x7ff00000
#define P 53
#define Nbits 53
#define BBias 1023
#define Emax 1023
#define Emin (-1022)
#define Exp_1  0x3ff00000
#define Exp_11 0x3ff00000
#define Ebits 11
#define Frac_mask  0xfffff
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Bletch 0x10
#define Bndry_mask  0xfffff
#define Bndry_mask1 0xfffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 1
#define Tiny0 0
#define Tiny1 1
#define Quick_max 14
#define Int_max 14
#ifndef NO_IEEE_Scale
#define Avoid_Underflow
#ifdef Flush_Denorm /* debugging option */
#undef Sudden_Underflow
#endif
#endif

#ifndef Flt_Rounds
#ifdef FLT_ROUNDS
#define Flt_Rounds FLT_ROUNDS
#else
#define Flt_Rounds 1
#endif
#endif /*Flt_Rounds*/

#ifdef Honor_FLT_ROUNDS
#undef Check_FLT_ROUNDS
#define Check_FLT_ROUNDS
#else
#define Rounding Flt_Rounds
#endif

#else /* ifndef IEEE_Arith */
#undef Check_FLT_ROUNDS
#undef Honor_FLT_ROUNDS
#undef SET_INEXACT
#undef  Sudden_Underflow
#define Sudden_Underflow
#ifdef IBM
#undef Flt_Rounds
#define Flt_Rounds 0
#define Exp_shift  24
#define Exp_shift1 24
#define Exp_msk1   0x1000000
#define Exp_msk11  0x1000000
#define Exp_mask  0x7f000000
#define P 14
#define Nbits 56
#define BBias 65
#define Emax 248
#define Emin (-260)
#define Exp_1  0x41000000
#define Exp_11 0x41000000
#define Ebits 8 /* exponent has 7 bits, but 8 is the right value in b2d */
#define Frac_mask  0xffffff
#define Frac_mask1 0xffffff
#define Bletch 4
#define Ten_pmax 22
#define Bndry_mask  0xefffff
#define Bndry_mask1 0xffffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 4
#define Tiny0 0x100000
#define Tiny1 0
#define Quick_max 14
#define Int_max 15
#else /* VAX */
#undef Flt_Rounds
#define Flt_Rounds 1
#define Exp_shift  23
#define Exp_shift1 7
#define Exp_msk1    0x80
#define Exp_msk11   0x800000
#define Exp_mask  0x7f80
#define P 56
#define Nbits 56
#define BBias 129
#define Emax 126
#define Emin (-129)
#define Exp_1  0x40800000
#define Exp_11 0x4080
#define Ebits 8
#define Frac_mask  0x7fffff
#define Frac_mask1 0xffff007f
#define Ten_pmax 24
#define Bletch 2
#define Bndry_mask  0xffff007f
#define Bndry_mask1 0xffff007f
#define LSB 0x10000
#define Sign_bit 0x8000
#define Log2P 1
#define Tiny0 0x80
#define Tiny1 0
#define Quick_max 15
#define Int_max 15
#endif /* IBM, VAX */
#endif /* IEEE_Arith */

#ifndef IEEE_Arith
#define ROUND_BIASED
#endif

#ifdef RND_PRODQUOT
#define rounded_product(a,b) a = rnd_prod(a, b)
#define rounded_quotient(a,b) a = rnd_quot(a, b)
#ifdef KR_headers
extern double rnd_prod(), rnd_quot();
#else
extern double rnd_prod(double, double), rnd_quot(double, double);
#endif
#else
#define rounded_product(a,b) a *= b
#define rounded_quotient(a,b) a /= b
#endif

#define Big0 (Frac_mask1 | Exp_msk1*(DBL_MAX_EXP+BBias-1))
#define Big1 0xffffffff

#ifndef Pack_32
#define Pack_32
#endif

typedef struct BCinfo BCinfo;
 struct
BCinfo { int dp0, dp1, dplen, dsign, e0, inexact, nd, nd0, rounding, scale, uflchk; };

#ifdef KR_headers
#define FFFFFFFF ((((unsigned long)0xffff)<<16)|(unsigned long)0xffff)
#else
#define FFFFFFFF 0xffffffffUL
#endif

#ifdef NO_LONG_LONG
#undef ULLong
#ifdef Just_16
#undef Pack_32
/* When Pack_32 is not defined, we store 16 bits per 32-bit Long.
 * This makes some inner loops simpler and sometimes saves work
 * during multiplications, but it often seems to make things slightly
 * slower.  Hence the default is now to store 32 bits per Long.
 */
#endif
#else   /* long long available */
#ifndef Llong
#define Llong long long
#endif
#ifndef ULLong
#define ULLong unsigned Llong
#endif
#endif /* NO_LONG_LONG */

#ifndef MULTIPLE_THREADS
#define ACQUIRE_DTOA_LOCK(n)    /*nothing*/
#define FREE_DTOA_LOCK(n)   /*nothing*/
#endif

#define Kmax 7

#ifdef __cplusplus
extern "C" double strtod(const char *s00, char **se);
extern "C" char *dtoa(double d, int mode, int ndigits,
            int *decpt, int *sign, char **rve);
#endif

 struct
Bigint {
    struct Bigint *next;
    int k, maxwds, sign, wds;
    ULong x[1];
    };

 typedef struct Bigint Bigint;

 static Bigint *freelist[Kmax+1];

 static Bigint *
Balloc
#ifdef KR_headers
    (k) int k;
#else
    (int k)
#endif
{
    int x;
    Bigint *rv;
#ifndef Omit_Private_Memory
    unsigned int len;
#endif

    ACQUIRE_DTOA_LOCK(0);
    /* The k > Kmax case does not need ACQUIRE_DTOA_LOCK(0), */
    /* but this case seems very unlikely. */
    if (k <= Kmax && (rv = freelist[k]))
        freelist[k] = rv->next;
    else {
        x = 1 << k;
#ifdef Omit_Private_Memory
        rv = (Bigint *)MALLOC(sizeof(Bigint) + (x-1)*sizeof(ULong));
#else
        len = (sizeof(Bigint) + (x-1)*sizeof(ULong) + sizeof(double) - 1)
            /sizeof(double);
        if (k <= Kmax && pmem_next - private_mem + len <= PRIVATE_mem) {
            rv = (Bigint*)pmem_next;
            pmem_next += len;
            }
        else
            rv = (Bigint*)MALLOC(len*sizeof(double));
#endif
        rv->k = k;
        rv->maxwds = x;
        }
    FREE_DTOA_LOCK(0);
    rv->sign = rv->wds = 0;
    return rv;
    }

 static void
Bfree
#ifdef KR_headers
    (v) Bigint *v;
#else
    (Bigint *v)
#endif
{
    if (v) {
        if (v->k > Kmax)
#ifdef FREE
            FREE((void*)v);
#else
            free((void*)v);
#endif
        else {
            ACQUIRE_DTOA_LOCK(0);
            v->next = freelist[v->k];
            freelist[v->k] = v;
            FREE_DTOA_LOCK(0);
            }
        }
    }

#define Bcopy(x,y) memcpy((char *)&x->sign, (char *)&y->sign, \
y->wds*sizeof(Long) + 2*sizeof(int))

 static Bigint *
multadd
#ifdef KR_headers
    (b, m, a) Bigint *b; int m, a;
#else
    (Bigint *b, int m, int a)   /* multiply by m and add a */
#endif
{
    int i, wds;
#ifdef ULLong
    ULong *x;
    ULLong carry, y;
#else
    ULong carry, *x, y;
#ifdef Pack_32
    ULong xi, z;
#endif
#endif
    Bigint *b1;

    wds = b->wds;
    x = b->x;
    i = 0;
    carry = a;
    do {
#ifdef ULLong
        y = *x * (ULLong)m + carry;
        carry = y >> 32;
        *x++ = (ULong) y & FFFFFFFF;
#else
#ifdef Pack_32
        xi = *x;
        y = (xi & 0xffff) * m + carry;
        z = (xi >> 16) * m + (y >> 16);
        carry = z >> 16;
        *x++ = (z << 16) + (y & 0xffff);
#else
        y = *x * m + carry;
        carry = y >> 16;
        *x++ = y & 0xffff;
#endif
#endif
        }
        while(++i < wds);
    if (carry) {
        if (wds >= b->maxwds) {
            b1 = Balloc(b->k+1);
            Bcopy(b1, b);
            Bfree(b);
            b = b1;
            }
        b->x[wds++] = (ULong) carry;
        b->wds = wds;
        }
    return b;
    }

 static Bigint *
s2b
#ifdef KR_headers
    (s, nd0, nd, y9, dplen) CONST char *s; int nd0, nd, dplen; ULong y9;
#else
    (CONST char *s, int nd0, int nd, ULong y9, int dplen)
#endif
{
    Bigint *b;
    int i, k;
    Long x, y;

    x = (nd + 8) / 9;
    for(k = 0, y = 1; x > y; y <<= 1, k++) ;
#ifdef Pack_32
    b = Balloc(k);
    b->x[0] = y9;
    b->wds = 1;
#else
    b = Balloc(k+1);
    b->x[0] = y9 & 0xffff;
    b->wds = (b->x[1] = y9 >> 16) ? 2 : 1;
#endif

    i = 9;
    if (9 < nd0) {
        s += 9;
        do b = multadd(b, 10, *s++ - '0');
            while(++i < nd0);
        s += dplen;
        }
    else
        s += dplen + 9;
    for(; i < nd; i++)
        b = multadd(b, 10, *s++ - '0');
    return b;
    }

 static int
hi0bits
#ifdef KR_headers
    (x) ULong x;
#else
    (ULong x)
#endif
{
    int k = 0;

    if (!(x & 0xffff0000)) {
        k = 16;
        x <<= 16;
        }
    if (!(x & 0xff000000)) {
        k += 8;
        x <<= 8;
        }
    if (!(x & 0xf0000000)) {
        k += 4;
        x <<= 4;
        }
    if (!(x & 0xc0000000)) {
        k += 2;
        x <<= 2;
        }
    if (!(x & 0x80000000)) {
        k++;
        if (!(x & 0x40000000))
            return 32;
        }
    return k;
    }

 static int
lo0bits
#ifdef KR_headers
    (y) ULong *y;
#else
    (ULong *y)
#endif
{
    int k;
    ULong x = *y;

    if (x & 7) {
        if (x & 1)
            return 0;
        if (x & 2) {
            *y = x >> 1;
            return 1;
            }
        *y = x >> 2;
        return 2;
        }
    k = 0;
    if (!(x & 0xffff)) {
        k = 16;
        x >>= 16;
        }
    if (!(x & 0xff)) {
        k += 8;
        x >>= 8;
        }
    if (!(x & 0xf)) {
        k += 4;
        x >>= 4;
        }
    if (!(x & 0x3)) {
        k += 2;
        x >>= 2;
        }
    if (!(x & 1)) {
        k++;
        x >>= 1;
        if (!x)
            return 32;
        }
    *y = x;
    return k;
    }

 static Bigint *
i2b
#ifdef KR_headers
    (i) int i;
#else
    (int i)
#endif
{
    Bigint *b;

    b = Balloc(1);
    b->x[0] = i;
    b->wds = 1;
    return b;
    }

 static Bigint *
mult
#ifdef KR_headers
    (a, b) Bigint *a, *b;
#else
    (Bigint *a, Bigint *b)
#endif
{
    Bigint *c;
    int k, wa, wb, wc;
    ULong *x, *xa, *xae, *xb, *xbe, *xc, *xc0;
    ULong y;
#ifdef ULLong
    ULLong carry, z;
#else
    ULong carry, z;
#ifdef Pack_32
    ULong z2;
#endif
#endif

    if (a->wds < b->wds) {
        c = a;
        a = b;
        b = c;
        }
    k = a->k;
    wa = a->wds;
    wb = b->wds;
    wc = wa + wb;
    if (wc > a->maxwds)
        k++;
    c = Balloc(k);
    for(x = c->x, xa = x + wc; x < xa; x++)
        *x = 0;
    xa = a->x;
    xae = xa + wa;
    xb = b->x;
    xbe = xb + wb;
    xc0 = c->x;
#ifdef ULLong
    for(; xb < xbe; xc0++) {
        if ((y = *xb++)) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = *x++ * (ULLong)y + *xc + carry;
                carry = z >> 32;
                *xc++ = (ULong) (z & FFFFFFFF);
                }
                while(x < xae);
            *xc = (ULong) carry;
            }
        }
#else
#ifdef Pack_32
    for(; xb < xbe; xb++, xc0++) {
        if (y = *xb & 0xffff) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = (*x & 0xffff) * y + (*xc & 0xffff) + carry;
                carry = z >> 16;
                z2 = (*x++ >> 16) * y + (*xc >> 16) + carry;
                carry = z2 >> 16;
                Storeinc(xc, z2, z);
                }
                while(x < xae);
            *xc = carry;
            }
        if (y = *xb >> 16) {
            x = xa;
            xc = xc0;
            carry = 0;
            z2 = *xc;
            do {
                z = (*x & 0xffff) * y + (*xc >> 16) + carry;
                carry = z >> 16;
                Storeinc(xc, z, z2);
                z2 = (*x++ >> 16) * y + (*xc & 0xffff) + carry;
                carry = z2 >> 16;
                }
                while(x < xae);
            *xc = z2;
            }
        }
#else
    for(; xb < xbe; xc0++) {
        if (y = *xb++) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = *x++ * y + *xc + carry;
                carry = z >> 16;
                *xc++ = (ULong) (z & 0xffff);
                }
                while(x < xae);
            *xc = carry;
            }
        }
#endif
#endif
    for(xc0 = c->x, xc = xc0 + wc; wc > 0 && !*--xc; --wc) ;
    c->wds = wc;
    return c;
    }

 static Bigint *p5s;

 static Bigint *
pow5mult
#ifdef KR_headers
    (b, k) Bigint *b; int k;
#else
    (Bigint *b, int k)
#endif
{
    Bigint *b1, *p5, *p51;
    int i;
    static int p05[3] = { 5, 25, 125 };

    if ((i = k & 3))
        b = multadd(b, p05[i-1], 0);

    if (!(k >>= 2))
        return b;
    if (!(p5 = p5s)) {
        /* first time */
#ifdef MULTIPLE_THREADS
        ACQUIRE_DTOA_LOCK(1);
        if (!(p5 = p5s)) {
            p5 = p5s = i2b(625);
            p5->next = 0;
            }
        FREE_DTOA_LOCK(1);
#else
        p5 = p5s = i2b(625);
        p5->next = 0;
#endif
        }
    for(;;) {
        if (k & 1) {
            b1 = mult(b, p5);
            Bfree(b);
            b = b1;
            }
        if (!(k >>= 1))
            break;
        if (!(p51 = p5->next)) {
#ifdef MULTIPLE_THREADS
            ACQUIRE_DTOA_LOCK(1);
            if (!(p51 = p5->next)) {
                p51 = p5->next = mult(p5,p5);
                p51->next = 0;
                }
            FREE_DTOA_LOCK(1);
#else
            p51 = p5->next = mult(p5,p5);
            p51->next = 0;
#endif
            }
        p5 = p51;
        }
    return b;
    }

 static Bigint *
lshift
#ifdef KR_headers
    (b, k) Bigint *b; int k;
#else
    (Bigint *b, int k)
#endif
{
    int i, k1, n, n1;
    Bigint *b1;
    ULong *x, *x1, *xe, z;

#ifdef Pack_32
    n = k >> 5;
#else
    n = k >> 4;
#endif
    k1 = b->k;
    n1 = n + b->wds + 1;
    for(i = b->maxwds; n1 > i; i <<= 1)
        k1++;
    b1 = Balloc(k1);
    x1 = b1->x;
    for(i = 0; i < n; i++)
        *x1++ = 0;
    x = b->x;
    xe = x + b->wds;
#ifdef Pack_32
    if (k &= 0x1f) {
        k1 = 32 - k;
        z = 0;
        do {
            *x1++ = *x << k | z;
            z = *x++ >> k1;
            }
            while(x < xe);
        if ((*x1 = z))
            ++n1;
        }
#else
    if (k &= 0xf) {
        k1 = 16 - k;
        z = 0;
        do {
            *x1++ = *x << k  & 0xffff | z;
            z = *x++ >> k1;
            }
            while(x < xe);
        if (*x1 = z)
            ++n1;
        }
#endif
    else do
        *x1++ = *x++;
        while(x < xe);
    b1->wds = n1 - 1;
    Bfree(b);
    return b1;
    }

 static int
cmp
#ifdef KR_headers
    (a, b) Bigint *a, *b;
#else
    (Bigint *a, Bigint *b)
#endif
{
    ULong *xa, *xa0, *xb, *xb0;
    int i, j;

    i = a->wds;
    j = b->wds;
#ifdef DEBUG
    if (i > 1 && !a->x[i-1])
        Bug("cmp called with a->x[a->wds-1] == 0");
    if (j > 1 && !b->x[j-1])
        Bug("cmp called with b->x[b->wds-1] == 0");
#endif
    if (i -= j)
        return i;
    xa0 = a->x;
    xa = xa0 + j;
    xb0 = b->x;
    xb = xb0 + j;
    for(;;) {
        if (*--xa != *--xb)
            return *xa < *xb ? -1 : 1;
        if (xa <= xa0)
            break;
        }
    return 0;
    }

 static Bigint *
diff
#ifdef KR_headers
    (a, b) Bigint *a, *b;
#else
    (Bigint *a, Bigint *b)
#endif
{
    Bigint *c;
    int i, wa, wb;
    ULong *xa, *xae, *xb, *xbe, *xc;
#ifdef ULLong
    ULLong borrow, y;
#else
    ULong borrow, y;
#ifdef Pack_32
    ULong z;
#endif
#endif

    i = cmp(a,b);
    if (!i) {
        c = Balloc(0);
        c->wds = 1;
        c->x[0] = 0;
        return c;
        }
    if (i < 0) {
        c = a;
        a = b;
        b = c;
        i = 1;
        }
    else
        i = 0;
    c = Balloc(a->k);
    c->sign = i;
    wa = a->wds;
    xa = a->x;
    xae = xa + wa;
    wb = b->wds;
    xb = b->x;
    xbe = xb + wb;
    xc = c->x;
    borrow = 0;
#ifdef ULLong
    do {
        y = (ULLong)*xa++ - *xb++ - borrow;
        borrow = y >> 32 & (ULong)1;
        *xc++ = (ULong) (y & FFFFFFFF);
        }
        while(xb < xbe);
    while(xa < xae) {
        y = *xa++ - borrow;
        borrow = y >> 32 & (ULong)1;
        *xc++ = (ULong) (y & FFFFFFFF);
        }
#else
#ifdef Pack_32
    do {
        y = (*xa & 0xffff) - (*xb & 0xffff) - borrow;
        borrow = (y & 0x10000) >> 16;
        z = (*xa++ >> 16) - (*xb++ >> 16) - borrow;
        borrow = (z & 0x10000) >> 16;
        Storeinc(xc, z, y);
        }
        while(xb < xbe);
    while(xa < xae) {
        y = (*xa & 0xffff) - borrow;
        borrow = (y & 0x10000) >> 16;
        z = (*xa++ >> 16) - borrow;
        borrow = (z & 0x10000) >> 16;
        Storeinc(xc, z, y);
        }
#else
    do {
        y = *xa++ - *xb++ - borrow;
        borrow = (y & 0x10000) >> 16;
        *xc++ = (ULong) (y & 0xffff);
        }
        while(xb < xbe);
    while(xa < xae) {
        y = *xa++ - borrow;
        borrow = (y & 0x10000) >> 16;
        *xc++ = (ULong) (y & 0xffff);
        }
#endif
#endif
    while(!*--xc)
        wa--;
    c->wds = wa;
    return c;
    }

 static double
ulp
#ifdef KR_headers
    (x) U *x;
#else
    (U *x)
#endif
{
    Long L;
    U u;

    L = (word0(x) & Exp_mask) - (P-1)*Exp_msk1;
#ifndef Avoid_Underflow
#ifndef Sudden_Underflow
    if (L > 0) {
#endif
#endif
#ifdef IBM
        L |= Exp_msk1 >> 4;
#endif
        word0(&u) = L;
        word1(&u) = 0;
#ifndef Avoid_Underflow
#ifndef Sudden_Underflow
        }
    else {
        L = -L >> Exp_shift;
        if (L < Exp_shift) {
            word0(&u) = 0x80000 >> L;
            word1(&u) = 0;
            }
        else {
            word0(&u) = 0;
            L -= Exp_shift;
            word1(&u) = L >= 31 ? 1 : 1 << 31 - L;
            }
        }
#endif
#endif
    return dval(&u);
    }

 static double
b2d
#ifdef KR_headers
    (a, e) Bigint *a; int *e;
#else
    (Bigint *a, int *e)
#endif
{
    ULong *xa, *xa0, w, y, z;
    int k;
    U d;
#ifdef VAX
    ULong d0, d1;
#else
#define d0 word0(&d)
#define d1 word1(&d)
#endif

    xa0 = a->x;
    xa = xa0 + a->wds;
    y = *--xa;
#ifdef DEBUG
    if (!y) Bug("zero y in b2d");
#endif
    k = hi0bits(y);
    *e = 32 - k;
#ifdef Pack_32
    if (k < Ebits) {
        d0 = Exp_1 | y >> (Ebits - k);
        w = xa > xa0 ? *--xa : 0;
        d1 = y << ((32-Ebits) + k) | w >> (Ebits - k);
        goto ret_d;
        }
    z = xa > xa0 ? *--xa : 0;
    if (k -= Ebits) {
        d0 = Exp_1 | y << k | z >> (32 - k);
        y = xa > xa0 ? *--xa : 0;
        d1 = z << k | y >> (32 - k);
        }
    else {
        d0 = Exp_1 | y;
        d1 = z;
        }
#else
    if (k < Ebits + 16) {
        z = xa > xa0 ? *--xa : 0;
        d0 = Exp_1 | y << k - Ebits | z >> Ebits + 16 - k;
        w = xa > xa0 ? *--xa : 0;
        y = xa > xa0 ? *--xa : 0;
        d1 = z << k + 16 - Ebits | w << k - Ebits | y >> 16 + Ebits - k;
        goto ret_d;
        }
    z = xa > xa0 ? *--xa : 0;
    w = xa > xa0 ? *--xa : 0;
    k -= Ebits + 16;
    d0 = Exp_1 | y << k + 16 | z << k | w >> 16 - k;
    y = xa > xa0 ? *--xa : 0;
    d1 = w << k + 16 | y << k;
#endif
 ret_d:
#ifdef VAX
    word0(&d) = d0 >> 16 | d0 << 16;
    word1(&d) = d1 >> 16 | d1 << 16;
#else
#undef d0
#undef d1
#endif
    return dval(&d);
    }

 static Bigint *
d2b
#ifdef KR_headers
    (d, e, bits) U *d; int *e, *bits;
#else
    (U *d, int *e, int *bits)
#endif
{
    Bigint *b;
    int de, k;
    ULong *x, y, z;
#ifndef Sudden_Underflow
    int i;
#endif
#ifdef VAX
    ULong d0, d1;
    d0 = word0(d) >> 16 | word0(d) << 16;
    d1 = word1(d) >> 16 | word1(d) << 16;
#else
#define d0 word0(d)
#define d1 word1(d)
#endif

#ifdef Pack_32
    b = Balloc(1);
#else
    b = Balloc(2);
#endif
    x = b->x;

    z = d0 & Frac_mask;
    d0 &= 0x7fffffff;   /* clear sign bit, which we ignore */
#ifdef Sudden_Underflow
    de = (int)(d0 >> Exp_shift);
#ifndef IBM
    z |= Exp_msk11;
#endif
#else
    if ((de = (int)(d0 >> Exp_shift)))
        z |= Exp_msk1;
#endif
#ifdef Pack_32
    if ((y = d1)) {
        if ((k = lo0bits(&y))) {
            x[0] = y | z << (32 - k);
            z >>= k;
            }
        else
            x[0] = y;
#ifndef Sudden_Underflow
        i =
#endif
            b->wds = (x[1] = z) ? 2 : 1;
        }
    else {
        k = lo0bits(&z);
        x[0] = z;
#ifndef Sudden_Underflow
        i =
#endif
            b->wds = 1;
        k += 32;
        }
#else
    if (y = d1) {
        if (k = lo0bits(&y))
            if (k >= 16) {
                x[0] = y | z << 32 - k & 0xffff;
                x[1] = z >> k - 16 & 0xffff;
                x[2] = z >> k;
                i = 2;
                }
            else {
                x[0] = y & 0xffff;
                x[1] = y >> 16 | z << 16 - k & 0xffff;
                x[2] = z >> k & 0xffff;
                x[3] = z >> k+16;
                i = 3;
                }
        else {
            x[0] = y & 0xffff;
            x[1] = y >> 16;
            x[2] = z & 0xffff;
            x[3] = z >> 16;
            i = 3;
            }
        }
    else {
#ifdef DEBUG
        if (!z)
            Bug("Zero passed to d2b");
#endif
        k = lo0bits(&z);
        if (k >= 16) {
            x[0] = z;
            i = 0;
            }
        else {
            x[0] = z & 0xffff;
            x[1] = z >> 16;
            i = 1;
            }
        k += 32;
        }
    while(!x[i])
        --i;
    b->wds = i + 1;
#endif
#ifndef Sudden_Underflow
    if (de) {
#endif
#ifdef IBM
        *e = (de - BBias - (P-1) << 2) + k;
        *bits = 4*P + 8 - k - hi0bits(word0(d) & Frac_mask);
#else
        *e = de - BBias - (P-1) + k;
        *bits = P - k;
#endif
#ifndef Sudden_Underflow
        }
    else {
        *e = de - BBias - (P-1) + 1 + k;
#ifdef Pack_32
        *bits = 32*i - hi0bits(x[i-1]);
#else
        *bits = (i+2)*16 - hi0bits(x[i]);
#endif
        }
#endif
    return b;
    }
#undef d0
#undef d1

 static double
ratio
#ifdef KR_headers
    (a, b) Bigint *a, *b;
#else
    (Bigint *a, Bigint *b)
#endif
{
    U da, db;
    int k, ka, kb;

    dval(&da) = b2d(a, &ka);
    dval(&db) = b2d(b, &kb);
#ifdef Pack_32
    k = ka - kb + 32*(a->wds - b->wds);
#else
    k = ka - kb + 16*(a->wds - b->wds);
#endif
#ifdef IBM
    if (k > 0) {
        word0(&da) += (k >> 2)*Exp_msk1;
        if (k &= 3)
            dval(&da) *= 1 << k;
        }
    else {
        k = -k;
        word0(&db) += (k >> 2)*Exp_msk1;
        if (k &= 3)
            dval(&db) *= 1 << k;
        }
#else
    if (k > 0)
        word0(&da) += k*Exp_msk1;
    else {
        k = -k;
        word0(&db) += k*Exp_msk1;
        }
#endif
    return dval(&da) / dval(&db);
    }

 static CONST double
tens[] = {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
        1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
        1e20, 1e21, 1e22
#ifdef VAX
        , 1e23, 1e24
#endif
        };

 static CONST double
#ifdef IEEE_Arith
bigtens[] = { 1e16, 1e32, 1e64, 1e128, 1e256 };
static CONST double tinytens[] = { 1e-16, 1e-32, 1e-64, 1e-128,
#ifdef Avoid_Underflow
        9007199254740992.*9007199254740992.e-256
        /* = 2^106 * 1e-256 */
#else
        1e-256
#endif
        };
/* The factor of 2^53 in tinytens[4] helps us avoid setting the underflow */
/* flag unnecessarily.  It leads to a song and dance at the end of strtod. */
#define Scale_Bit 0x10
#define n_bigtens 5
#else
#ifdef IBM
bigtens[] = { 1e16, 1e32, 1e64 };
static CONST double tinytens[] = { 1e-16, 1e-32, 1e-64 };
#define n_bigtens 3
#else
bigtens[] = { 1e16, 1e32 };
static CONST double tinytens[] = { 1e-16, 1e-32 };
#define n_bigtens 2
#endif
#endif

#undef Need_Hexdig
#ifdef INFNAN_CHECK
#ifndef No_Hex_NaN
#define Need_Hexdig
#endif
#endif

#ifndef Need_Hexdig
#ifndef NO_HEX_FP
#define Need_Hexdig
#endif
#endif

#ifdef Need_Hexdig /*{*/
static unsigned char hexdig[256];

 static void
#ifdef KR_headers
htinit(h, s, inc) unsigned char *h; unsigned char *s; int inc;
#else
htinit(unsigned char *h, unsigned char *s, int inc)
#endif
{
    int i, j;
    for(i = 0; (j = s[i]) !=0; i++)
        h[j] = i + inc;
    }

 static void
#ifdef KR_headers
hexdig_init()
#else
hexdig_init(void)
#endif
{
#define USC (unsigned char *)
    htinit(hexdig, USC "0123456789", 0x10);
    htinit(hexdig, USC "abcdef", 0x10 + 10);
    htinit(hexdig, USC "ABCDEF", 0x10 + 10);
    }
#endif /* } Need_Hexdig */

#ifdef INFNAN_CHECK

#ifndef NAN_WORD0
#define NAN_WORD0 0x7ff80000
#endif

#ifndef NAN_WORD1
#define NAN_WORD1 0
#endif

 static int
match
#ifdef KR_headers
    (sp, t) char **sp, *t;
#else
    (CONST char **sp, char *t)
#endif
{
    int c, d;
    CONST char *s = *sp;

    while((d = *t++)) {
        if ((c = *++s) >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != d)
            return 0;
        }
    *sp = s + 1;
    return 1;
    }

#ifndef No_Hex_NaN
 static void
hexnan
#ifdef KR_headers
    (rvp, sp) U *rvp; CONST char **sp;
#else
    (U *rvp, CONST char **sp)
#endif
{
    ULong c, x[2];
    CONST char *s;
    int c1, havedig, udx0, xshift;

    if (!hexdig['0'])
        hexdig_init();
    x[0] = x[1] = 0;
    havedig = xshift = 0;
    udx0 = 1;
    s = *sp;
    /* allow optional initial 0x or 0X */
    while((c = *(CONST unsigned char*)(s+1)) && c <= ' ')
        ++s;
    if (s[1] == '0' && (s[2] == 'x' || s[2] == 'X'))
        s += 2;
    while((c = *(CONST unsigned char*)++s)) {
        if ((c1 = hexdig[c]))
            c  = c1 & 0xf;
        else if (c <= ' ') {
            if (udx0 && havedig) {
                udx0 = 0;
                xshift = 1;
                }
            continue;
            }
#ifdef GDTOA_NON_PEDANTIC_NANCHECK
        else if (/*(*/ c == ')' && havedig) {
            *sp = s + 1;
            break;
            }
        else
            return; /* invalid form: don't change *sp */
#else
        else {
            do {
                if (/*(*/ c == ')') {
                    *sp = s + 1;
                    break;
                    }
                } while((c = *++s));
            break;
            }
#endif
        havedig = 1;
        if (xshift) {
            xshift = 0;
            x[0] = x[1];
            x[1] = 0;
            }
        if (udx0)
            x[0] = (x[0] << 4) | (x[1] >> 28);
        x[1] = (x[1] << 4) | c;
        }
    if ((x[0] &= 0xfffff) || x[1]) {
        word0(rvp) = Exp_mask | x[0];
        word1(rvp) = x[1];
        }
    }
#endif /*No_Hex_NaN*/
#endif /* INFNAN_CHECK */

#ifdef Pack_32
#define ULbits 32
#define kshift 5
#define kmask 31
#else
#define ULbits 16
#define kshift 4
#define kmask 15
#endif
#ifndef NO_HEX_FP /*{*/

 static void
#ifdef KR_headers
rshift(b, k) Bigint *b; int k;
#else
rshift(Bigint *b, int k)
#endif
{
    ULong *x, *x1, *xe, y;
    int n;

    x = x1 = b->x;
    n = k >> kshift;
    if (n < b->wds) {
        xe = x + b->wds;
        x += n;
        if (k &= kmask) {
            n = 32 - k;
            y = *x++ >> k;
            while(x < xe) {
                *x1++ = (y | (*x << n)) & 0xffffffff;
                y = *x++ >> k;
                }
            if ((*x1 = y) !=0)
                x1++;
            }
        else
            while(x < xe)
                *x1++ = *x++;
        }
    if ((b->wds = x1 - b->x) == 0)
        b->x[0] = 0;
    }

 static ULong
#ifdef KR_headers
any_on(b, k) Bigint *b; int k;
#else
any_on(Bigint *b, int k)
#endif
{
    int n, nwds;
    ULong *x, *x0, x1, x2;

    x = b->x;
    nwds = b->wds;
    n = k >> kshift;
    if (n > nwds)
        n = nwds;
    else if (n < nwds && (k &= kmask)) {
        x1 = x2 = x[n];
        x1 >>= k;
        x1 <<= k;
        if (x1 != x2)
            return 1;
        }
    x0 = x;
    x += n;
    while(x > x0)
        if (*--x)
            return 1;
    return 0;
    }

enum {  /* rounding values: same as FLT_ROUNDS */
    Round_zero = 0,
    Round_near = 1,
    Round_up = 2,
    Round_down = 3
    };

 static Bigint *
#ifdef KR_headers
increment(b) Bigint *b;
#else
increment(Bigint *b)
#endif
{
    ULong *x, *xe;
    Bigint *b1;

    x = b->x;
    xe = x + b->wds;
    do {
        if (*x < (ULong)0xffffffffL) {
            ++*x;
            return b;
            }
        *x++ = 0;
        } while(x < xe);
    {
        if (b->wds >= b->maxwds) {
            b1 = Balloc(b->k+1);
            Bcopy(b1,b);
            Bfree(b);
            b = b1;
            }
        b->x[b->wds++] = 1;
        }
    return b;
    }

 void
#ifdef KR_headers
gethex(sp, rvp, rounding, sign)
    CONST char **sp; U *rvp; int rounding, sign;
#else
gethex( CONST char **sp, U *rvp, int rounding, int sign)
#endif
{
    Bigint *b;
    CONST unsigned char *decpt, *s0, *s, *s1;
    Long e, e1;
    ULong L, lostbits, *x;
    int big, denorm, esign, havedig, k, n, nbits, up, zret;
#ifdef IBM
    int j;
#endif
    enum {
#ifdef IEEE_Arith /*{{*/
        emax = 0x7fe - BBias - P + 1,
        emin = Emin - P + 1
#else /*}{*/
        emin = Emin - P,
#ifdef VAX
        emax = 0x7ff - BBias - P + 1
#endif
#ifdef IBM
        emax = 0x7f - BBias - P
#endif
#endif /*}}*/
        };
#ifdef USE_LOCALE
    int i;
#ifdef NO_LOCALE_CACHE
    const unsigned char *decimalpoint = (unsigned char*)
        localeconv()->decimal_point;
#else
    const unsigned char *decimalpoint;
    static unsigned char *decimalpoint_cache;
    if (!(s0 = decimalpoint_cache)) {
        s0 = (unsigned char*)localeconv()->decimal_point;
        if ((decimalpoint_cache = (unsigned char*)
                MALLOC(strlen((CONST char*)s0) + 1))) {
            strcpy((char*)decimalpoint_cache, (CONST char*)s0);
            s0 = decimalpoint_cache;
            }
        }
    decimalpoint = s0;
#endif
#endif

    if (!hexdig['0'])
        hexdig_init();
    havedig = 0;
    s0 = *(CONST unsigned char **)sp + 2;
    while(s0[havedig] == '0')
        havedig++;
    s0 += havedig;
    s = s0;
    decpt = 0;
    zret = 0;
    e = 0;
    if (hexdig[*s])
        havedig++;
    else {
        zret = 1;
#ifdef USE_LOCALE
        for(i = 0; decimalpoint[i]; ++i) {
            if (s[i] != decimalpoint[i])
                goto pcheck;
            }
        decpt = s += i;
#else
        if (*s != '.')
            goto pcheck;
        decpt = ++s;
#endif
        if (!hexdig[*s])
            goto pcheck;
        while(*s == '0')
            s++;
        if (hexdig[*s])
            zret = 0;
        havedig = 1;
        s0 = s;
        }
    while(hexdig[*s])
        s++;
#ifdef USE_LOCALE
    if (*s == *decimalpoint && !decpt) {
        for(i = 1; decimalpoint[i]; ++i) {
            if (s[i] != decimalpoint[i])
                goto pcheck;
            }
        decpt = s += i;
#else
    if (*s == '.' && !decpt) {
        decpt = ++s;
#endif
        while(hexdig[*s])
            s++;
        }/*}*/
    if (decpt)
        e = -(((Long)(s-decpt)) << 2);
 pcheck:
    s1 = s;
    big = esign = 0;
    switch(*s) {
      case 'p':
      case 'P':
        switch(*++s) {
          case '-':
            esign = 1;
            /* no break */
          case '+':
            s++;
          }
        if ((n = hexdig[*s]) == 0 || n > 0x19) {
            s = s1;
            break;
            }
        e1 = n - 0x10;
        while((n = hexdig[*++s]) !=0 && n <= 0x19) {
            if (e1 & 0xf8000000)
                big = 1;
            e1 = 10*e1 + n - 0x10;
            }
        if (esign)
            e1 = -e1;
        e += e1;
      }
    *sp = (char*)s;
    if (!havedig)
        *sp = (char*)s0 - 1;
    if (zret)
        goto retz1;
    if (big) {
        if (esign) {
#ifdef IEEE_Arith
            switch(rounding) {
              case Round_up:
                if (sign)
                    break;
                goto ret_tiny;
              case Round_down:
                if (!sign)
                    break;
                goto ret_tiny;
              }
#endif
            goto retz;
#ifdef IEEE_Arith
 ret_tiny:
#ifndef NO_ERRNO
            errno = ERANGE;
#endif
            word0(rvp) = 0;
            word1(rvp) = 1;
            return;
#endif /* IEEE_Arith */
            }
        switch(rounding) {
          case Round_near:
            goto ovfl1;
          case Round_up:
            if (!sign)
                goto ovfl1;
            goto ret_big;
          case Round_down:
            if (sign)
                goto ovfl1;
            goto ret_big;
          }
 ret_big:
        word0(rvp) = Big0;
        word1(rvp) = Big1;
        return;
        }
    n = s1 - s0 - 1;
    for(k = 0; n > (1 << (kshift-2)) - 1; n >>= 1)
        k++;
    b = Balloc(k);
    x = b->x;
    n = 0;
    L = 0;
#ifdef USE_LOCALE
    for(i = 0; decimalpoint[i+1]; ++i);
#endif
    while(s1 > s0) {
#ifdef USE_LOCALE
        if (*--s1 == decimalpoint[i]) {
            s1 -= i;
            continue;
            }
#else
        if (*--s1 == '.')
            continue;
#endif
        if (n == ULbits) {
            *x++ = L;
            L = 0;
            n = 0;
            }
        L |= (hexdig[*s1] & 0x0f) << n;
        n += 4;
        }
    *x++ = L;
    b->wds = n = x - b->x;
    n = ULbits*n - hi0bits(L);
    nbits = Nbits;
    lostbits = 0;
    x = b->x;
    if (n > nbits) {
        n -= nbits;
        if (any_on(b,n)) {
            lostbits = 1;
            k = n - 1;
            if (x[k>>kshift] & 1 << (k & kmask)) {
                lostbits = 2;
                if (k > 0 && any_on(b,k))
                    lostbits = 3;
                }
            }
        rshift(b, n);
        e += n;
        }
    else if (n < nbits) {
        n = nbits - n;
        b = lshift(b, n);
        e -= n;
        x = b->x;
        }
    if (e > Emax) {
 ovfl:
        Bfree(b);
 ovfl1:
#ifndef NO_ERRNO
        errno = ERANGE;
#endif
        word0(rvp) = Exp_mask;
        word1(rvp) = 0;
        return;
        }
    denorm = 0;
    if (e < emin) {
        denorm = 1;
        n = emin - e;
        if (n >= nbits) {
#ifdef IEEE_Arith /*{*/
            switch (rounding) {
              case Round_near:
                if (n == nbits && (n < 2 || any_on(b,n-1)))
                    goto ret_tiny;
                break;
              case Round_up:
                if (!sign)
                    goto ret_tiny;
                break;
              case Round_down:
                if (sign)
                    goto ret_tiny;
              }
#endif /* } IEEE_Arith */
            Bfree(b);
 retz:
#ifndef NO_ERRNO
            errno = ERANGE;
#endif
 retz1:
            rvp->d = 0.;
            return;
            }
        k = n - 1;
        if (lostbits)
            lostbits = 1;
        else if (k > 0)
            lostbits = any_on(b,k);
        if (x[k>>kshift] & 1 << (k & kmask))
            lostbits |= 2;
        nbits -= n;
        rshift(b,n);
        e = emin;
        }
    if (lostbits) {
        up = 0;
        switch(rounding) {
          case Round_zero:
            break;
          case Round_near:
            if (lostbits & 2
             && (lostbits & 1) | (x[0] & 1))
                up = 1;
            break;
          case Round_up:
            up = 1 - sign;
            break;
          case Round_down:
            up = sign;
          }
        if (up) {
            k = b->wds;
            b = increment(b);
            x = b->x;
            if (denorm) {
#if 0
                if (nbits == Nbits - 1
                 && x[nbits >> kshift] & 1 << (nbits & kmask))
                    denorm = 0; /* not currently used */
#endif
                }
            else if (b->wds > k
             || ((n = nbits & kmask) !=0
                 && hi0bits(x[k-1]) < 32-n)) {
                rshift(b,1);
                if (++e > Emax)
                    goto ovfl;
                }
            }
        }
#ifdef IEEE_Arith
    if (denorm)
        word0(rvp) = b->wds > 1 ? b->x[1] & ~0x100000 : 0;
    else
        word0(rvp) = (b->x[1] & ~0x100000) | ((e + 0x3ff + 52) << 20);
    word1(rvp) = b->x[0];
#endif
#ifdef IBM
    if ((j = e & 3)) {
        k = b->x[0] & ((1 << j) - 1);
        rshift(b,j);
        if (k) {
            switch(rounding) {
              case Round_up:
                if (!sign)
                    increment(b);
                break;
              case Round_down:
                if (sign)
                    increment(b);
                break;
              case Round_near:
                j = 1 << (j-1);
                if (k & j && ((k & (j-1)) | lostbits))
                    increment(b);
              }
            }
        }
    e >>= 2;
    word0(rvp) = b->x[1] | ((e + 65 + 13) << 24);
    word1(rvp) = b->x[0];
#endif
#ifdef VAX
    /* The next two lines ignore swap of low- and high-order 2 bytes. */
    /* word0(rvp) = (b->x[1] & ~0x800000) | ((e + 129 + 55) << 23); */
    /* word1(rvp) = b->x[0]; */
    word0(rvp) = ((b->x[1] & ~0x800000) >> 16) | ((e + 129 + 55) << 7) | (b->x[1] << 16);
    word1(rvp) = (b->x[0] >> 16) | (b->x[0] << 16);
#endif
    Bfree(b);
    }
#endif /*}!NO_HEX_FP*/

 static int
#ifdef KR_headers
dshift(b, p2) Bigint *b; int p2;
#else
dshift(Bigint *b, int p2)
#endif
{
    int rv = hi0bits(b->x[b->wds-1]) - 4;
    if (p2 > 0)
        rv -= p2;
    return rv & kmask;
    }

 static int
quorem
#ifdef KR_headers
    (b, S) Bigint *b, *S;
#else
    (Bigint *b, Bigint *S)
#endif
{
    int n;
    ULong *bx, *bxe, q, *sx, *sxe;
#ifdef ULLong
    ULLong borrow, carry, y, ys;
#else
    ULong borrow, carry, y, ys;
#ifdef Pack_32
    ULong si, z, zs;
#endif
#endif

    n = S->wds;
#ifdef DEBUG
    /*debug*/ if (b->wds > n)
    /*debug*/   Bug("oversize b in quorem");
#endif
    if (b->wds < n)
        return 0;
    sx = S->x;
    sxe = sx + --n;
    bx = b->x;
    bxe = bx + n;
    q = *bxe / (*sxe + 1);  /* ensure q <= true quotient */
#ifdef DEBUG
    /*debug*/ if (q > 9)
    /*debug*/   Bug("oversized quotient in quorem");
#endif
    if (q) {
        borrow = 0;
        carry = 0;
        do {
#ifdef ULLong
            ys = *sx++ * (ULLong)q + carry;
            carry = ys >> 32;
            y = *bx - (ys & FFFFFFFF) - borrow;
            borrow = y >> 32 & (ULong)1;
            *bx++ = (ULong) (y & FFFFFFFF);
#else
#ifdef Pack_32
            si = *sx++;
            ys = (si & 0xffff) * q + carry;
            zs = (si >> 16) * q + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#else
            ys = *sx++ * q + carry;
            carry = ys >> 16;
            y = *bx - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            *bx++ = (Ulong) (y & 0xffff);
#endif
#endif
            }
            while(sx <= sxe);
        if (!*bxe) {
            bx = b->x;
            while(--bxe > bx && !*bxe)
                --n;
            b->wds = n;
            }
        }
    if (cmp(b, S) >= 0) {
        q++;
        borrow = 0;
        carry = 0;
        bx = b->x;
        sx = S->x;
        do {
#ifdef ULLong
            ys = *sx++ + carry;
            carry = ys >> 32;
            y = *bx - (ys & FFFFFFFF) - borrow;
            borrow = y >> 32 & (ULong)1;
            *bx++ = (ULong) (y & FFFFFFFF);
#else
#ifdef Pack_32
            si = *sx++;
            ys = (si & 0xffff) + carry;
            zs = (si >> 16) + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#else
            ys = *sx++ + carry;
            carry = ys >> 16;
            y = *bx - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            *bx++ = (ULong) (y & 0xffff);
#endif
#endif
            }
            while(sx <= sxe);
        bx = b->x;
        bxe = bx + n;
        if (!*bxe) {
            while(--bxe > bx && !*bxe)
                --n;
            b->wds = n;
            }
        }
    return q;
    }

#ifndef NO_STRTOD_BIGCOMP

 static double
sulp
#ifdef KR_headers
    (x, bc) U *x; BCinfo *bc;
#else
    (U *x, BCinfo *bc)
#endif
{
    U u;
    double rv;
    int i;

    rv = ulp(x);
    if (!bc->scale)
        return rv;
    i = 2*P + 1 - ((word0(x) & Exp_mask) >> Exp_shift);
    word0(&u) = Exp_1 + (i << Exp_shift);
    word1(&u) = 0;
    return rv * u.d;
    }

 static void
bigcomp
#ifdef KR_headers
    (rv, s0, bc)
    U *rv; CONST char *s0; BCinfo *bc;
#else
    (U *rv, CONST char *s0, BCinfo *bc)
#endif
{
    Bigint *b, *d;
    int b2, bbits, d2, dd, dig, dsign, i, j, nd, nd0, p2, p5, speccase;

    dd = 0;
    dsign = bc->dsign;
    nd = bc->nd;
    nd0 = bc->nd0;
    p5 = nd + bc->e0 - 1;
    speccase = 0;
#ifndef Sudden_Underflow
    if (rv->d == 0.) {  /* special case: value near underflow-to-zero */
                /* threshold was rounded to zero */
        b = i2b(1);
        p2 = Emin - P + 1;
        bbits = 1;
#ifdef Avoid_Underflow
        word0(rv) = (P+2) << Exp_shift;
#else
        word1(rv) = 1;
#endif
        i = 0;
#ifdef Honor_FLT_ROUNDS
        if (bc->rounding == 1)
#endif
            {
            speccase = 1;
            --p2;
            dsign = 0;
            goto have_i;
            }
        }
    else
#endif
        b = d2b(rv, &p2, &bbits);
#ifdef Avoid_Underflow
    p2 -= bc->scale;
#endif
    /* floor(log2(rv)) == bbits - 1 + p2 */
    /* Check for denormal case. */
    i = P - bbits;
    if (i > (j = P - Emin - 1 + p2)) {
#ifdef Sudden_Underflow
        Bfree(b);
        b = i2b(1);
        p2 = Emin;
        i = P - 1;
#ifdef Avoid_Underflow
        word0(rv) = (1 + bc->scale) << Exp_shift;
#else
        word0(rv) = Exp_msk1;
#endif
        word1(rv) = 0;
#else
        i = j;
#endif
        }
#ifdef Honor_FLT_ROUNDS
    if (bc->rounding != 1) {
        if (i > 0)
            b = lshift(b, i);
        if (dsign)
            b = increment(b);
        }
    else
#endif
        {
        b = lshift(b, ++i);
        b->x[0] |= 1;
        }
#ifndef Sudden_Underflow
 have_i:
#endif
    p2 -= p5 + i;
    d = i2b(1);
    /* Arrange for convenient computation of quotients:
     * shift left if necessary so divisor has 4 leading 0 bits.
     */
    if (p5 > 0)
        d = pow5mult(d, p5);
    else if (p5 < 0)
        b = pow5mult(b, -p5);
    if (p2 > 0) {
        b2 = p2;
        d2 = 0;
        }
    else {
        b2 = 0;
        d2 = -p2;
        }
    i = dshift(d, d2);
    if ((b2 += i) > 0)
        b = lshift(b, b2);
    if ((d2 += i) > 0)
        d = lshift(d, d2);

    /* Now b/d = exactly half-way between the two floating-point values */
    /* on either side of the input string.  Compute first digit of b/d. */

    if (!(dig = quorem(b,d))) {
        b = multadd(b, 10, 0);  /* very unlikely */
        dig = quorem(b,d);
        }

    /* Compare b/d with s0 */

    for(i = 0; i < nd0; ) {
        if ((dd = s0[i++] - '0' - dig))
            goto ret;
        if (!b->x[0] && b->wds == 1) {
            if (i < nd)
                dd = 1;
            goto ret;
            }
        b = multadd(b, 10, 0);
        dig = quorem(b,d);
        }
    for(j = bc->dp1; i++ < nd;) {
        if ((dd = s0[j++] - '0' - dig))
            goto ret;
        if (!b->x[0] && b->wds == 1) {
            if (i < nd)
                dd = 1;
            goto ret;
            }
        b = multadd(b, 10, 0);
        dig = quorem(b,d);
        }
    if (b->x[0] || b->wds > 1)
        dd = -1;
 ret:
    Bfree(b);
    Bfree(d);
#ifdef Honor_FLT_ROUNDS
    if (bc->rounding != 1) {
        if (dd < 0) {
            if (bc->rounding == 0) {
                if (!dsign)
                    goto retlow1;
                }
            else if (dsign)
                goto rethi1;
            }
        else if (dd > 0) {
            if (bc->rounding == 0) {
                if (dsign)
                    goto rethi1;
                goto ret1;
                }
            if (!dsign)
                goto rethi1;
            dval(rv) += 2.*sulp(rv,bc);
            }
        else {
            bc->inexact = 0;
            if (dsign)
                goto rethi1;
            }
        }
    else
#endif
    if (speccase) {
        if (dd <= 0)
            rv->d = 0.;
        }
    else if (dd < 0) {
        if (!dsign) /* does not happen for round-near */
retlow1:
            dval(rv) -= sulp(rv,bc);
        }
    else if (dd > 0) {
        if (dsign) {
 rethi1:
            dval(rv) += sulp(rv,bc);
            }
        }
    else {
        /* Exact half-way case:  apply round-even rule. */
        if (word1(rv) & 1) {
            if (dsign)
                goto rethi1;
            goto retlow1;
            }
        }

#ifdef Honor_FLT_ROUNDS
 ret1:
#endif
    return;
    }
#endif /* NO_STRTOD_BIGCOMP */

 double
strtod
#ifdef KR_headers
    (s00, se) CONST char *s00; char **se;
#else
    (CONST char *s00, char **se)
#endif
{
    int bb2, bb5, bbe, bd2, bd5, bbbits, bs2, c, e, e1;
    int esign, i, j, k, nd, nd0, nf, nz, nz0, sign;
    CONST char *s, *s0, *s1;
    double aadj, aadj1;
    Long L;
    U aadj2, adj, rv, rv0;
    ULong y, z;
    BCinfo bc;
    Bigint *bb, *bb1, *bd, *bd0, *bs, *delta;
#ifdef SET_INEXACT
    int oldinexact;
#endif
#ifdef Honor_FLT_ROUNDS /*{*/
#ifdef Trust_FLT_ROUNDS /*{{ only define this if FLT_ROUNDS really works! */
    bc.rounding = Flt_Rounds;
#else /*}{*/
    bc.rounding = 1;
    switch(fegetround()) {
      case FE_TOWARDZERO:   bc.rounding = 0; break;
      case FE_UPWARD:   bc.rounding = 2; break;
      case FE_DOWNWARD: bc.rounding = 3;
      }
#endif /*}}*/
#endif /*}*/
#ifdef USE_LOCALE
    CONST char *s2;
#endif

    sign = nz0 = nz = bc.dplen = bc.uflchk = 0;
    dval(&rv) = 0.;
    for(s = s00;;s++) switch(*s) {
        case '-':
            sign = 1;
            /* no break */
        case '+':
            if (*++s)
                goto break2;
            /* no break */
        case 0:
            goto ret0;
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
        case ' ':
            continue;
        default:
            goto break2;
        }
 break2:
    if (*s == '0') {
#ifndef NO_HEX_FP /*{*/
        switch(s[1]) {
          case 'x':
          case 'X':
#ifdef Honor_FLT_ROUNDS
            gethex(&s, &rv, bc.rounding, sign);
#else
            gethex(&s, &rv, 1, sign);
#endif
            goto ret;
          }
#endif /*}*/
        nz0 = 1;
        while(*++s == '0') ;
        if (!*s)
            goto ret;
        }
    s0 = s;
    y = z = 0;
    for(nd = nf = 0; (c = *s) >= '0' && c <= '9'; nd++, s++)
        if (nd < 9)
            y = 10*y + c - '0';
        else if (nd < 16)
            z = 10*z + c - '0';
    nd0 = nd;
    bc.dp0 = bc.dp1 = s - s0;
#ifdef USE_LOCALE
    s1 = localeconv()->decimal_point;
    if (c == *s1) {
        c = '.';
        if (*++s1) {
            s2 = s;
            for(;;) {
                if (*++s2 != *s1) {
                    c = 0;
                    break;
                    }
                if (!*++s1) {
                    s = s2;
                    break;
                    }
                }
            }
        }
#endif
    if (c == '.') {
        c = *++s;
        bc.dp1 = s - s0;
        bc.dplen = bc.dp1 - bc.dp0;
        if (!nd) {
            for(; c == '0'; c = *++s)
                nz++;
            if (c > '0' && c <= '9') {
                s0 = s;
                nf += nz;
                nz = 0;
                goto have_dig;
                }
            goto dig_done;
            }
        for(; c >= '0' && c <= '9'; c = *++s) {
 have_dig:
            nz++;
            if (c -= '0') {
                nf += nz;
                for(i = 1; i < nz; i++)
                    if (nd++ < 9)
                        y *= 10;
                    else if (nd <= DBL_DIG + 1)
                        z *= 10;
                if (nd++ < 9)
                    y = 10*y + c;
                else if (nd <= DBL_DIG + 1)
                    z = 10*z + c;
                nz = 0;
                }
            }
        }
 dig_done:
    e = 0;
    if (c == 'e' || c == 'E') {
        if (!nd && !nz && !nz0) {
            goto ret0;
            }
        s00 = s;
        esign = 0;
        switch(c = *++s) {
            case '-':
                esign = 1;
            case '+':
                c = *++s;
            }
        if (c >= '0' && c <= '9') {
            while(c == '0')
                c = *++s;
            if (c > '0' && c <= '9') {
                L = c - '0';
                s1 = s;
                while((c = *++s) >= '0' && c <= '9')
                    L = 10*L + c - '0';
                if (s - s1 > 8 || L > 19999)
                    /* Avoid confusion from exponents
                     * so large that e might overflow.
                     */
                    e = 19999; /* safe for 16 bit ints */
                else
                    e = (int)L;
                if (esign)
                    e = -e;
                }
            else
                e = 0;
            }
        else
            s = s00;
        }
    if (!nd) {
        if (!nz && !nz0) {
#ifdef INFNAN_CHECK
            /* Check for Nan and Infinity */
            if (!bc.dplen)
             switch(c) {
              case 'i':
              case 'I':
                if (match(&s,"nf")) {
                    --s;
                    if (!match(&s,"inity"))
                        ++s;
                    word0(&rv) = 0x7ff00000;
                    word1(&rv) = 0;
                    goto ret;
                    }
                break;
              case 'n':
              case 'N':
                if (match(&s, "an")) {
                    word0(&rv) = NAN_WORD0;
                    word1(&rv) = NAN_WORD1;
#ifndef No_Hex_NaN
                    if (*s == '(') /*)*/
                        hexnan(&rv, &s);
#endif
                    goto ret;
                    }
              }
#endif /* INFNAN_CHECK */
 ret0:
            s = s00;
            sign = 0;
            }
        goto ret;
        }
    bc.e0 = e1 = e -= nf;

    /* Now we have nd0 digits, starting at s0, followed by a
     * decimal point, followed by nd-nd0 digits.  The number we're
     * after is the integer represented by those digits times
     * 10**e */

    if (!nd0)
        nd0 = nd;
    k = nd < DBL_DIG + 1 ? nd : DBL_DIG + 1;
    dval(&rv) = y;
    if (k > 9) {
#ifdef SET_INEXACT
        if (k > DBL_DIG)
            oldinexact = get_inexact();
#endif
        dval(&rv) = tens[k - 9] * dval(&rv) + z;
        }
    bd0 = 0;
    if (nd <= DBL_DIG
#ifndef RND_PRODQUOT
#ifndef Honor_FLT_ROUNDS
        && Flt_Rounds == 1
#endif
#endif
            ) {
        if (!e)
            goto ret;
        if (e > 0) {
            if (e <= Ten_pmax) {
#ifdef VAX
                goto vax_ovfl_check;
#else
#ifdef Honor_FLT_ROUNDS
                /* round correctly FLT_ROUNDS = 2 or 3 */
                if (sign) {
                    rv.d = -rv.d;
                    sign = 0;
                    }
#endif
                /* rv = */ rounded_product(dval(&rv), tens[e]);
                goto ret;
#endif
                }
            i = DBL_DIG - nd;
            if (e <= Ten_pmax + i) {
                /* A fancier test would sometimes let us do
                 * this for larger i values.
                 */
#ifdef Honor_FLT_ROUNDS
                /* round correctly FLT_ROUNDS = 2 or 3 */
                if (sign) {
                    rv.d = -rv.d;
                    sign = 0;
                    }
#endif
                e -= i;
                dval(&rv) *= tens[i];
#ifdef VAX
                /* VAX exponent range is so narrow we must
                 * worry about overflow here...
                 */
 vax_ovfl_check:
                word0(&rv) -= P*Exp_msk1;
                /* rv = */ rounded_product(dval(&rv), tens[e]);
                if ((word0(&rv) & Exp_mask)
                 > Exp_msk1*(DBL_MAX_EXP+BBias-1-P))
                    goto ovfl;
                word0(&rv) += P*Exp_msk1;
#else
                /* rv = */ rounded_product(dval(&rv), tens[e]);
#endif
                goto ret;
                }
            }
#ifndef Inaccurate_Divide
        else if (e >= -Ten_pmax) {
#ifdef Honor_FLT_ROUNDS
            /* round correctly FLT_ROUNDS = 2 or 3 */
            if (sign) {
                rv.d = -rv.d;
                sign = 0;
                }
#endif
            /* rv = */ rounded_quotient(dval(&rv), tens[-e]);
            goto ret;
            }
#endif
        }
    e1 += nd - k;

#ifdef IEEE_Arith
#ifdef SET_INEXACT
    bc.inexact = 1;
    if (k <= DBL_DIG)
        oldinexact = get_inexact();
#endif
#ifdef Avoid_Underflow
    bc.scale = 0;
#endif
#ifdef Honor_FLT_ROUNDS
    if (bc.rounding >= 2) {
        if (sign)
            bc.rounding = bc.rounding == 2 ? 0 : 2;
        else
            if (bc.rounding != 2)
                bc.rounding = 0;
        }
#endif
#endif /*IEEE_Arith*/

    /* Get starting approximation = rv * 10**e1 */

    if (e1 > 0) {
        if ((i = e1 & 15))
            dval(&rv) *= tens[i];
        if (e1 &= ~15) {
            if (e1 > DBL_MAX_10_EXP) {
 ovfl:
#ifndef NO_ERRNO
                errno = ERANGE;
#endif
                /* Can't trust HUGE_VAL */
#ifdef IEEE_Arith
#ifdef Honor_FLT_ROUNDS
                switch(bc.rounding) {
                  case 0: /* toward 0 */
                  case 3: /* toward -infinity */
                    word0(&rv) = Big0;
                    word1(&rv) = Big1;
                    break;
                  default:
                    word0(&rv) = Exp_mask;
                    word1(&rv) = 0;
                  }
#else /*Honor_FLT_ROUNDS*/
                word0(&rv) = Exp_mask;
                word1(&rv) = 0;
#endif /*Honor_FLT_ROUNDS*/
#ifdef SET_INEXACT
                /* set overflow bit */
                dval(&rv0) = 1e300;
                dval(&rv0) *= dval(&rv0);
#endif
#else /*IEEE_Arith*/
                word0(&rv) = Big0;
                word1(&rv) = Big1;
#endif /*IEEE_Arith*/
                goto ret;
                }
            e1 >>= 4;
            for(j = 0; e1 > 1; j++, e1 >>= 1)
                if (e1 & 1)
                    dval(&rv) *= bigtens[j];
        /* The last multiplication could overflow. */
            word0(&rv) -= P*Exp_msk1;
            dval(&rv) *= bigtens[j];
            if ((z = word0(&rv) & Exp_mask)
             > Exp_msk1*(DBL_MAX_EXP+BBias-P))
                goto ovfl;
            if (z > Exp_msk1*(DBL_MAX_EXP+BBias-1-P)) {
                /* set to largest number */
                /* (Can't trust DBL_MAX) */
                word0(&rv) = Big0;
                word1(&rv) = Big1;
                }
            else
                word0(&rv) += P*Exp_msk1;
            }
        }
    else if (e1 < 0) {
        e1 = -e1;
        if ((i = e1 & 15))
            dval(&rv) /= tens[i];
        if (e1 >>= 4) {
            if (e1 >= 1 << n_bigtens)
                goto undfl;
#ifdef Avoid_Underflow
            if (e1 & Scale_Bit)
                bc.scale = 2*P;
            for(j = 0; e1 > 0; j++, e1 >>= 1)
                if (e1 & 1)
                    dval(&rv) *= tinytens[j];
            if (bc.scale && (j = 2*P + 1 - ((word0(&rv) & Exp_mask)
                        >> Exp_shift)) > 0) {
                /* scaled rv is denormal; clear j low bits */
                if (j >= 32) {
                    if (j > 53)
                        goto undfl;
                    word1(&rv) = 0;
                    if (j >= 53)
                     word0(&rv) = (P+2)*Exp_msk1;
                    else
                     word0(&rv) &= 0xffffffff << (j-32);
                    }
                else
                    word1(&rv) &= 0xffffffff << j;
                }
#else
            for(j = 0; e1 > 1; j++, e1 >>= 1)
                if (e1 & 1)
                    dval(&rv) *= tinytens[j];
            /* The last multiplication could underflow. */
            dval(&rv0) = dval(&rv);
            dval(&rv) *= tinytens[j];
            if (!dval(&rv)) {
                dval(&rv) = 2.*dval(&rv0);
                dval(&rv) *= tinytens[j];
#endif
                if (!dval(&rv)) {
 undfl:
                    dval(&rv) = 0.;
#ifndef NO_ERRNO
                    errno = ERANGE;
#endif
                    goto ret;
                    }
#ifndef Avoid_Underflow
                word0(&rv) = Tiny0;
                word1(&rv) = Tiny1;
                /* The refinement below will clean
                 * this approximation up.
                 */
                }
#endif
            }
        }

    /* Now the hard part -- adjusting rv to the correct value.*/

    /* Put digits into bd: true value = bd * 10^e */

    bc.nd = nd;
#ifndef NO_STRTOD_BIGCOMP
    bc.nd0 = nd0;   /* Only needed if nd > strtod_diglim, but done here */
            /* to silence an erroneous warning about bc.nd0 */
            /* possibly not being initialized. */
    if (nd > strtod_diglim) {
        /* ASSERT(strtod_diglim >= 18); 18 == one more than the */
        /* minimum number of decimal digits to distinguish double values */
        /* in IEEE arithmetic. */
        i = j = 18;
        if (i > nd0)
            j += bc.dplen;
        for(;;) {
            if (--j <= bc.dp1 && j >= bc.dp0)
                j = bc.dp0 - 1;
            if (s0[j] != '0')
                break;
            --i;
            }
        e += nd - i;
        nd = i;
        if (nd0 > nd)
            nd0 = nd;
        if (nd < 9) { /* must recompute y */
            y = 0;
            for(i = 0; i < nd0; ++i)
                y = 10*y + s0[i] - '0';
            for(j = bc.dp1; i < nd; ++i)
                y = 10*y + s0[j++] - '0';
            }
        }
#endif
    bd0 = s2b(s0, nd0, nd, y, bc.dplen);

    for(;;) {
        bd = Balloc(bd0->k);
        Bcopy(bd, bd0);
        bb = d2b(&rv, &bbe, &bbbits);   /* rv = bb * 2^bbe */
        bs = i2b(1);

        if (e >= 0) {
            bb2 = bb5 = 0;
            bd2 = bd5 = e;
            }
        else {
            bb2 = bb5 = -e;
            bd2 = bd5 = 0;
            }
        if (bbe >= 0)
            bb2 += bbe;
        else
            bd2 -= bbe;
        bs2 = bb2;
#ifdef Honor_FLT_ROUNDS
        if (bc.rounding != 1)
            bs2++;
#endif
#ifdef Avoid_Underflow
        j = bbe - bc.scale;
        i = j + bbbits - 1; /* logb(rv) */
        if (i < Emin)   /* denormal */
            j += P - Emin;
        else
            j = P + 1 - bbbits;
#else /*Avoid_Underflow*/
#ifdef Sudden_Underflow
#ifdef IBM
        j = 1 + 4*P - 3 - bbbits + ((bbe + bbbits - 1) & 3);
#else
        j = P + 1 - bbbits;
#endif
#else /*Sudden_Underflow*/
        j = bbe;
        i = j + bbbits - 1; /* logb(rv) */
        if (i < Emin)   /* denormal */
            j += P - Emin;
        else
            j = P + 1 - bbbits;
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
        bb2 += j;
        bd2 += j;
#ifdef Avoid_Underflow
        bd2 += bc.scale;
#endif
        i = bb2 < bd2 ? bb2 : bd2;
        if (i > bs2)
            i = bs2;
        if (i > 0) {
            bb2 -= i;
            bd2 -= i;
            bs2 -= i;
            }
        if (bb5 > 0) {
            bs = pow5mult(bs, bb5);
            bb1 = mult(bs, bb);
            Bfree(bb);
            bb = bb1;
            }
        if (bb2 > 0)
            bb = lshift(bb, bb2);
        if (bd5 > 0)
            bd = pow5mult(bd, bd5);
        if (bd2 > 0)
            bd = lshift(bd, bd2);
        if (bs2 > 0)
            bs = lshift(bs, bs2);
        delta = diff(bb, bd);
        bc.dsign = delta->sign;
        delta->sign = 0;
        i = cmp(delta, bs);
#ifndef NO_STRTOD_BIGCOMP
        if (bc.nd > nd && i <= 0) {
            if (bc.dsign)
                break;  /* Must use bigcomp(). */
#ifdef Honor_FLT_ROUNDS
            if (bc.rounding != 1) {
                if (i < 0)
                    break;
                }
            else
#endif
                {
                bc.nd = nd;
                i = -1; /* Discarded digits make delta smaller. */
                }
            }
#endif
#ifdef Honor_FLT_ROUNDS
        if (bc.rounding != 1) {
            if (i < 0) {
                /* Error is less than an ulp */
                if (!delta->x[0] && delta->wds <= 1) {
                    /* exact */
#ifdef SET_INEXACT
                    bc.inexact = 0;
#endif
                    break;
                    }
                if (bc.rounding) {
                    if (bc.dsign) {
                        adj.d = 1.;
                        goto apply_adj;
                        }
                    }
                else if (!bc.dsign) {
                    adj.d = -1.;
                    if (!word1(&rv)
                     && !(word0(&rv) & Frac_mask)) {
                        y = word0(&rv) & Exp_mask;
#ifdef Avoid_Underflow
                        if (!bc.scale || y > 2*P*Exp_msk1)
#else
                        if (y)
#endif
                          {
                          delta = lshift(delta,Log2P);
                          if (cmp(delta, bs) <= 0)
                            adj.d = -0.5;
                          }
                        }
 apply_adj:
#ifdef Avoid_Underflow
                    if (bc.scale && (y = word0(&rv) & Exp_mask)
                        <= 2*P*Exp_msk1)
                      word0(&adj) += (2*P+1)*Exp_msk1 - y;
#else
#ifdef Sudden_Underflow
                    if ((word0(&rv) & Exp_mask) <=
                            P*Exp_msk1) {
                        word0(&rv) += P*Exp_msk1;
                        dval(&rv) += adj.d*ulp(dval(&rv));
                        word0(&rv) -= P*Exp_msk1;
                        }
                    else
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
                    dval(&rv) += adj.d*ulp(&rv);
                    }
                break;
                }
            adj.d = ratio(delta, bs);
            if (adj.d < 1.)
                adj.d = 1.;
            if (adj.d <= 0x7ffffffe) {
                /* adj = rounding ? ceil(adj) : floor(adj); */
                y = (ULong) adj.d;
                if (y != adj.d) {
                    if (!((bc.rounding>>1) ^ bc.dsign))
                        y++;
                    adj.d = y;
                    }
                }
#ifdef Avoid_Underflow
            if (bc.scale && (y = word0(&rv) & Exp_mask) <= 2*P*Exp_msk1)
                word0(&adj) += (2*P+1)*Exp_msk1 - y;
#else
#ifdef Sudden_Underflow
            if ((word0(&rv) & Exp_mask) <= P*Exp_msk1) {
                word0(&rv) += P*Exp_msk1;
                adj.d *= ulp(dval(&rv));
                if (bc.dsign)
                    dval(&rv) += adj.d;
                else
                    dval(&rv) -= adj.d;
                word0(&rv) -= P*Exp_msk1;
                goto cont;
                }
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
            adj.d *= ulp(&rv);
            if (bc.dsign) {
                if (word0(&rv) == Big0 && word1(&rv) == Big1)
                    goto ovfl;
                dval(&rv) += adj.d;
                }
            else
                dval(&rv) -= adj.d;
            goto cont;
            }
#endif /*Honor_FLT_ROUNDS*/

        if (i < 0) {
            /* Error is less than half an ulp -- check for
             * special case of mantissa a power of two.
             */
            if (bc.dsign || word1(&rv) || word0(&rv) & Bndry_mask
#ifdef IEEE_Arith
#ifdef Avoid_Underflow
             || (word0(&rv) & Exp_mask) <= (2*P+1)*Exp_msk1
#else
             || (word0(&rv) & Exp_mask) <= Exp_msk1
#endif
#endif
                ) {
#ifdef SET_INEXACT
                if (!delta->x[0] && delta->wds <= 1)
                    bc.inexact = 0;
#endif
                break;
                }
            if (!delta->x[0] && delta->wds <= 1) {
                /* exact result */
#ifdef SET_INEXACT
                bc.inexact = 0;
#endif
                break;
                }
            delta = lshift(delta,Log2P);
            if (cmp(delta, bs) > 0)
                goto drop_down;
            break;
            }
        if (i == 0) {
            /* exactly half-way between */
            if (bc.dsign) {
                if ((word0(&rv) & Bndry_mask1) == Bndry_mask1
                 &&  word1(&rv) == (
#ifdef Avoid_Underflow
            (bc.scale && (y = word0(&rv) & Exp_mask) <= 2*P*Exp_msk1)
        ? (0xffffffff & (0xffffffff << (2*P+1-(y>>Exp_shift)))) :
#endif
                           0xffffffff)) {
                    /*boundary case -- increment exponent*/
                    word0(&rv) = (word0(&rv) & Exp_mask)
                        + Exp_msk1
#ifdef IBM
                        | Exp_msk1 >> 4
#endif
                        ;
                    word1(&rv) = 0;
#ifdef Avoid_Underflow
                    bc.dsign = 0;
#endif
                    break;
                    }
                }
            else if (!(word0(&rv) & Bndry_mask) && !word1(&rv)) {
 drop_down:
                /* boundary case -- decrement exponent */
#ifdef Sudden_Underflow /*{{*/
                L = word0(&rv) & Exp_mask;
#ifdef IBM
                if (L <  Exp_msk1)
#else
#ifdef Avoid_Underflow
                if (L <= (bc.scale ? (2*P+1)*Exp_msk1 : Exp_msk1))
#else
                if (L <= Exp_msk1)
#endif /*Avoid_Underflow*/
#endif /*IBM*/
                    {
                    if (bc.nd >nd) {
                        bc.uflchk = 1;
                        break;
                        }
                    goto undfl;
                    }
                L -= Exp_msk1;
#else /*Sudden_Underflow}{*/
#ifdef Avoid_Underflow
                if (bc.scale) {
                    L = word0(&rv) & Exp_mask;
                    if (L <= (2*P+1)*Exp_msk1) {
                        if (L > (P+2)*Exp_msk1)
                            /* round even ==> */
                            /* accept rv */
                            break;
                        /* rv = smallest denormal */
                        if (bc.nd >nd) {
                            bc.uflchk = 1;
                            break;
                            }
                        goto undfl;
                        }
                    }
#endif /*Avoid_Underflow*/
                L = (word0(&rv) & Exp_mask) - Exp_msk1;
#endif /*Sudden_Underflow}}*/
                word0(&rv) = L | Bndry_mask1;
                word1(&rv) = 0xffffffff;
#ifdef IBM
                goto cont;
#else
                break;
#endif
                }
#ifndef ROUND_BIASED
            if (!(word1(&rv) & LSB))
                break;
#endif
            if (bc.dsign)
                dval(&rv) += ulp(&rv);
#ifndef ROUND_BIASED
            else {
                dval(&rv) -= ulp(&rv);
#ifndef Sudden_Underflow
                if (!dval(&rv)) {
                    if (bc.nd >nd) {
                        bc.uflchk = 1;
                        break;
                        }
                    goto undfl;
                    }
#endif
                }
#ifdef Avoid_Underflow
            bc.dsign = 1 - bc.dsign;
#endif
#endif
            break;
            }
        if ((aadj = ratio(delta, bs)) <= 2.) {
            if (bc.dsign)
                aadj = aadj1 = 1.;
            else if (word1(&rv) || word0(&rv) & Bndry_mask) {
#ifndef Sudden_Underflow
                if (word1(&rv) == Tiny1 && !word0(&rv)) {
                    if (bc.nd >nd) {
                        bc.uflchk = 1;
                        break;
                        }
                    goto undfl;
                    }
#endif
                aadj = 1.;
                aadj1 = -1.;
                }
            else {
                /* special case -- power of FLT_RADIX to be */
                /* rounded down... */

                if (aadj < 2./FLT_RADIX)
                    aadj = 1./FLT_RADIX;
                else
                    aadj *= 0.5;
                aadj1 = -aadj;
                }
            }
        else {
            aadj *= 0.5;
            aadj1 = bc.dsign ? aadj : -aadj;
#ifdef Check_FLT_ROUNDS
            switch(bc.rounding) {
                case 2: /* towards +infinity */
                    aadj1 -= 0.5;
                    break;
                case 0: /* towards 0 */
                case 3: /* towards -infinity */
                    aadj1 += 0.5;
                }
#else
            if (Flt_Rounds == 0)
                aadj1 += 0.5;
#endif /*Check_FLT_ROUNDS*/
            }
        y = word0(&rv) & Exp_mask;

        /* Check for overflow */

        if (y == Exp_msk1*(DBL_MAX_EXP+BBias-1)) {
            dval(&rv0) = dval(&rv);
            word0(&rv) -= P*Exp_msk1;
            adj.d = aadj1 * ulp(&rv);
            dval(&rv) += adj.d;
            if ((word0(&rv) & Exp_mask) >=
                    Exp_msk1*(DBL_MAX_EXP+BBias-P)) {
                if (word0(&rv0) == Big0 && word1(&rv0) == Big1)
                    goto ovfl;
                word0(&rv) = Big0;
                word1(&rv) = Big1;
                goto cont;
                }
            else
                word0(&rv) += P*Exp_msk1;
            }
        else {
#ifdef Avoid_Underflow
            if (bc.scale && y <= 2*P*Exp_msk1) {
                if (aadj <= 0x7fffffff) {
                    if ((z = (ULong) aadj) <= 0)
                        z = 1;
                    aadj = z;
                    aadj1 = bc.dsign ? aadj : -aadj;
                    }
                dval(&aadj2) = aadj1;
                word0(&aadj2) += (2*P+1)*Exp_msk1 - y;
                aadj1 = dval(&aadj2);
                }
            adj.d = aadj1 * ulp(&rv);
            dval(&rv) += adj.d;
#else
#ifdef Sudden_Underflow
            if ((word0(&rv) & Exp_mask) <= P*Exp_msk1) {
                dval(&rv0) = dval(&rv);
                word0(&rv) += P*Exp_msk1;
                adj.d = aadj1 * ulp(&rv);
                dval(&rv) += adj.d;
#ifdef IBM
                if ((word0(&rv) & Exp_mask) <  P*Exp_msk1)
#else
                if ((word0(&rv) & Exp_mask) <= P*Exp_msk1)
#endif
                    {
                    if (word0(&rv0) == Tiny0
                     && word1(&rv0) == Tiny1) {
                        if (bc.nd >nd) {
                            bc.uflchk = 1;
                            break;
                            }
                        goto undfl;
                        }
                    word0(&rv) = Tiny0;
                    word1(&rv) = Tiny1;
                    goto cont;
                    }
                else
                    word0(&rv) -= P*Exp_msk1;
                }
            else {
                adj.d = aadj1 * ulp(&rv);
                dval(&rv) += adj.d;
                }
#else /*Sudden_Underflow*/
            /* Compute adj so that the IEEE rounding rules will
             * correctly round rv + adj in some half-way cases.
             * If rv * ulp(rv) is denormalized (i.e.,
             * y <= (P-1)*Exp_msk1), we must adjust aadj to avoid
             * trouble from bits lost to denormalization;
             * example: 1.2e-307 .
             */
            if (y <= (P-1)*Exp_msk1 && aadj > 1.) {
                aadj1 = (double)(int)(aadj + 0.5);
                if (!bc.dsign)
                    aadj1 = -aadj1;
                }
            adj.d = aadj1 * ulp(&rv);
            dval(&rv) += adj.d;
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
            }
        z = word0(&rv) & Exp_mask;
#ifndef SET_INEXACT
        if (bc.nd == nd) {
#ifdef Avoid_Underflow
        if (!bc.scale)
#endif
        if (y == z) {
            /* Can we stop now? */
            L = (Long)aadj;
            aadj -= L;
            /* The tolerances below are conservative. */
            if (bc.dsign || word1(&rv) || word0(&rv) & Bndry_mask) {
                if (aadj < .4999999 || aadj > .5000001)
                    break;
                }
            else if (aadj < .4999999/FLT_RADIX)
                break;
            }
        }
#endif
 cont:
        Bfree(bb);
        Bfree(bd);
        Bfree(bs);
        Bfree(delta);
        }
    Bfree(bb);
    Bfree(bd);
    Bfree(bs);
    Bfree(bd0);
    Bfree(delta);
#ifndef NO_STRTOD_BIGCOMP
    if (bc.nd > nd)
        bigcomp(&rv, s0, &bc);
#endif
#ifdef SET_INEXACT
    if (bc.inexact) {
        if (!oldinexact) {
            word0(&rv0) = Exp_1 + (70 << Exp_shift);
            word1(&rv0) = 0;
            dval(&rv0) += 1.;
            }
        }
    else if (!oldinexact)
        clear_inexact();
#endif
#ifdef Avoid_Underflow
    if (bc.scale) {
        word0(&rv0) = Exp_1 - 2*P*Exp_msk1;
        word1(&rv0) = 0;
        dval(&rv) *= dval(&rv0);
#ifndef NO_ERRNO
        /* try to avoid the bug of testing an 8087 register value */
#ifdef IEEE_Arith
        if (!(word0(&rv) & Exp_mask))
#else
        if (word0(&rv) == 0 && word1(&rv) == 0)
#endif
            errno = ERANGE;
#endif
        }
#endif /* Avoid_Underflow */
#ifdef SET_INEXACT
    if (bc.inexact && !(word0(&rv) & Exp_mask)) {
        /* set underflow bit */
        dval(&rv0) = 1e-300;
        dval(&rv0) *= dval(&rv0);
        }
#endif
 ret:
    if (se)
        *se = (char *)s;
    return sign ? -dval(&rv) : dval(&rv);
    }

#ifndef MULTIPLE_THREADS
 static char *dtoa_result;
#endif

 static char *
#ifdef KR_headers
rv_alloc(i) int i;
#else
rv_alloc(int i)
#endif
{
    int j, k, *r;

    j = sizeof(ULong);
    for(k = 0;
        ((int) (sizeof(Bigint) - sizeof(ULong) - sizeof(int) + j)) <= i;
        j <<= 1)
            k++;
    r = (int*)Balloc(k);
    *r = k;
    return
#ifndef MULTIPLE_THREADS
    dtoa_result =
#endif
        (char *)(r+1);
    }

 static char *
#ifdef KR_headers
nrv_alloc(s, rve, n) char *s, **rve; int n;
#else
nrv_alloc(char *s, char **rve, int n)
#endif
{
    char *rv, *t;

    t = rv = rv_alloc(n);
    while((*t = *s++)) t++;
    if (rve)
        *rve = t;
    return rv;
    }

/* freedtoa(s) must be used to free values s returned by dtoa
 * when MULTIPLE_THREADS is #defined.  It should be used in all cases,
 * but for consistency with earlier versions of dtoa, it is optional
 * when MULTIPLE_THREADS is not defined.
 */

 void
#ifdef KR_headers
freedtoa(s) char *s;
#else
freedtoa(char *s)
#endif
{
    Bigint *b = (Bigint *)((int *)s - 1);
    b->maxwds = 1 << (b->k = *(int*)b);
    Bfree(b);
#ifndef MULTIPLE_THREADS
    if (s == dtoa_result)
        dtoa_result = 0;
#endif
    }

/* dtoa for IEEE arithmetic (dmg): convert double to ASCII string.
 *
 * Inspired by "How to Print Floating-Point Numbers Accurately" by
 * Guy L. Steele, Jr. and Jon L. White [Proc. ACM SIGPLAN '90, pp. 112-126].
 *
 * Modifications:
 *  1. Rather than iterating, we use a simple numeric overestimate
 *     to determine k = floor(log10(d)).  We scale relevant
 *     quantities using O(log2(k)) rather than O(k) multiplications.
 *  2. For some modes > 2 (corresponding to ecvt and fcvt), we don't
 *     try to generate digits strictly left to right.  Instead, we
 *     compute with fewer bits and propagate the carry if necessary
 *     when rounding the final digit up.  This is often faster.
 *  3. Under the assumption that input will be rounded nearest,
 *     mode 0 renders 1e23 as 1e23 rather than 9.999999999999999e22.
 *     That is, we allow equality in stopping tests when the
 *     round-nearest rule will give the same floating-point value
 *     as would satisfaction of the stopping test with strict
 *     inequality.
 *  4. We remove common factors of powers of 2 from relevant
 *     quantities.
 *  5. When converting floating-point integers less than 1e16,
 *     we use floating-point arithmetic rather than resorting
 *     to multiple-precision integers.
 *  6. When asked to produce fewer than 15 digits, we first try
 *     to get by with floating-point arithmetic; we resort to
 *     multiple-precision integer arithmetic only if we cannot
 *     guarantee that the floating-point calculation has given
 *     the correctly rounded result.  For k requested digits and
 *     "uniformly" distributed input, the probability is
 *     something like 10^(k-15) that we must resort to the Long
 *     calculation.
 */

 char *
dtoa
#ifdef KR_headers
    (dd, mode, ndigits, decpt, sign, rve)
    double dd; int mode, ndigits, *decpt, *sign; char **rve;
#else
    (double dd, int mode, int ndigits, int *decpt, int *sign, char **rve)
#endif
{
 /* Arguments ndigits, decpt, sign are similar to those
    of ecvt and fcvt; trailing zeros are suppressed from
    the returned string.  If not null, *rve is set to point
    to the end of the return value.  If d is +-Infinity or NaN,
    then *decpt is set to 9999.

    mode:
        0 ==> shortest string that yields d when read in
            and rounded to nearest.
        1 ==> like 0, but with Steele & White stopping rule;
            e.g. with IEEE P754 arithmetic , mode 0 gives
            1e23 whereas mode 1 gives 9.999999999999999e22.
        2 ==> max(1,ndigits) significant digits.  This gives a
            return value similar to that of ecvt, except
            that trailing zeros are suppressed.
        3 ==> through ndigits past the decimal point.  This
            gives a return value similar to that from fcvt,
            except that trailing zeros are suppressed, and
            ndigits can be negative.
        4,5 ==> similar to 2 and 3, respectively, but (in
            round-nearest mode) with the tests of mode 0 to
            possibly return a shorter string that rounds to d.
            With IEEE arithmetic and compilation with
            -DHonor_FLT_ROUNDS, modes 4 and 5 behave the same
            as modes 2 and 3 when FLT_ROUNDS != 1.
        6-9 ==> Debugging modes similar to mode - 4:  don't try
            fast floating-point estimate (if applicable).

        Values of mode other than 0-9 are treated as mode 0.

        Sufficient space is allocated to the return value
        to hold the suppressed trailing zeros.
    */

    int bbits, b2, b5, be, dig, i, ieps, ilim, ilim0, ilim1,
        j, j1, k, k0, k_check, leftright, m2, m5, s2, s5,
        spec_case, try_quick;
    Long L;
#ifndef Sudden_Underflow
    int denorm;
    ULong x;
#endif
    Bigint *b, *b1, *delta, *mlo, *mhi, *S;
    U d2, eps, u;
    double ds;
    char *s, *s0;
#ifdef SET_INEXACT
    int inexact, oldinexact;
#endif
#ifdef Honor_FLT_ROUNDS /*{*/
    int Rounding;
#ifdef Trust_FLT_ROUNDS /*{{ only define this if FLT_ROUNDS really works! */
    Rounding = Flt_Rounds;
#else /*}{*/
    Rounding = 1;
    switch(fegetround()) {
      case FE_TOWARDZERO:   Rounding = 0; break;
      case FE_UPWARD:   Rounding = 2; break;
      case FE_DOWNWARD: Rounding = 3;
      }
#endif /*}}*/
#endif /*}*/

#ifndef MULTIPLE_THREADS
    if (dtoa_result) {
        freedtoa(dtoa_result);
        dtoa_result = 0;
        }
#endif

    u.d = dd;
    if (word0(&u) & Sign_bit) {
        /* set sign for everything, including 0's and NaNs */
        *sign = 1;
        word0(&u) &= ~Sign_bit; /* clear sign bit */
        }
    else
        *sign = 0;

#if defined(IEEE_Arith) + defined(VAX)
#ifdef IEEE_Arith
    if ((word0(&u) & Exp_mask) == Exp_mask)
#else
    if (word0(&u)  == 0x8000)
#endif
        {
        /* Infinity or NaN */
        *decpt = 9999;
#ifdef IEEE_Arith
        if (!word1(&u) && !(word0(&u) & 0xfffff))
            return nrv_alloc("Infinity", rve, 8);
#endif
        return nrv_alloc("NaN", rve, 3);
        }
#endif
#ifdef IBM
    dval(&u) += 0; /* normalize */
#endif
    if (!dval(&u)) {
        *decpt = 1;
        return nrv_alloc("0", rve, 1);
        }

#ifdef SET_INEXACT
    try_quick = oldinexact = get_inexact();
    inexact = 1;
#endif
#ifdef Honor_FLT_ROUNDS
    if (Rounding >= 2) {
        if (*sign)
            Rounding = Rounding == 2 ? 0 : 2;
        else
            if (Rounding != 2)
                Rounding = 0;
        }
#endif

    b = d2b(&u, &be, &bbits);
#ifdef Sudden_Underflow
    i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1));
#else
    if ((i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1)))) {
#endif
        dval(&d2) = dval(&u);
        word0(&d2) &= Frac_mask1;
        word0(&d2) |= Exp_11;
#ifdef IBM
        if (j = 11 - hi0bits(word0(&d2) & Frac_mask))
            dval(&d2) /= 1 << j;
#endif

        /* log(x)   ~=~ log(1.5) + (x-1.5)/1.5
         * log10(x)  =  log(x) / log(10)
         *      ~=~ log(1.5)/log(10) + (x-1.5)/(1.5*log(10))
         * log10(d) = (i-BBias)*log(2)/log(10) + log10(d2)
         *
         * This suggests computing an approximation k to log10(d) by
         *
         * k = (i - BBias)*0.301029995663981
         *  + ( (d2-1.5)*0.289529654602168 + 0.176091259055681 );
         *
         * We want k to be too large rather than too small.
         * The error in the first-order Taylor series approximation
         * is in our favor, so we just round up the constant enough
         * to compensate for any error in the multiplication of
         * (i - BBias) by 0.301029995663981; since |i - BBias| <= 1077,
         * and 1077 * 0.30103 * 2^-52 ~=~ 7.2e-14,
         * adding 1e-13 to the constant term more than suffices.
         * Hence we adjust the constant term to 0.1760912590558.
         * (We could get a more accurate k by invoking log10,
         *  but this is probably not worthwhile.)
         */

        i -= BBias;
#ifdef IBM
        i <<= 2;
        i += j;
#endif
#ifndef Sudden_Underflow
        denorm = 0;
        }
    else {
        /* d is denormalized */

        i = bbits + be + (BBias + (P-1) - 1);
        x = i > 32  ? word0(&u) << (64 - i) | word1(&u) >> (i - 32)
                : word1(&u) << (32 - i);
        dval(&d2) = x;
        word0(&d2) -= 31*Exp_msk1; /* adjust exponent */
        i -= (BBias + (P-1) - 1) + 1;
        denorm = 1;
        }
#endif
    ds = (dval(&d2)-1.5)*0.289529654602168 + 0.1760912590558 + i*0.301029995663981;
    k = (int)ds;
    if (ds < 0. && ds != k)
        k--;    /* want k = floor(ds) */
    k_check = 1;
    if (k >= 0 && k <= Ten_pmax) {
        if (dval(&u) < tens[k])
            k--;
        k_check = 0;
        }
    j = bbits - i - 1;
    if (j >= 0) {
        b2 = 0;
        s2 = j;
        }
    else {
        b2 = -j;
        s2 = 0;
        }
    if (k >= 0) {
        b5 = 0;
        s5 = k;
        s2 += k;
        }
    else {
        b2 -= k;
        b5 = -k;
        s5 = 0;
        }
    if (mode < 0 || mode > 9)
        mode = 0;

#ifndef SET_INEXACT
#ifdef Check_FLT_ROUNDS
    try_quick = Rounding == 1;
#else
    try_quick = 1;
#endif
#endif /*SET_INEXACT*/

    if (mode > 5) {
        mode -= 4;
        try_quick = 0;
        }
    leftright = 1;
    ilim = ilim1 = -1;  /* Values for cases 0 and 1; done here to */
                /* silence erroneous "gcc -Wall" warning. */
    switch(mode) {
        case 0:
        case 1:
            i = 18;
            ndigits = 0;
            break;
        case 2:
            leftright = 0;
            /* no break */
        case 4:
            if (ndigits <= 0)
                ndigits = 1;
            ilim = ilim1 = i = ndigits;
            break;
        case 3:
            leftright = 0;
            /* no break */
        case 5:
            i = ndigits + k + 1;
            ilim = i;
            ilim1 = i - 1;
            if (i <= 0)
                i = 1;
        }
    s = s0 = rv_alloc(i);

#ifdef Honor_FLT_ROUNDS
    if (mode > 1 && Rounding != 1)
        leftright = 0;
#endif

    if (ilim >= 0 && ilim <= Quick_max && try_quick) {

        /* Try to get by with floating-point arithmetic. */

        i = 0;
        dval(&d2) = dval(&u);
        k0 = k;
        ilim0 = ilim;
        ieps = 2; /* conservative */
        if (k > 0) {
            ds = tens[k&0xf];
            j = k >> 4;
            if (j & Bletch) {
                /* prevent overflows */
                j &= Bletch - 1;
                dval(&u) /= bigtens[n_bigtens-1];
                ieps++;
                }
            for(; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    ds *= bigtens[i];
                    }
            dval(&u) /= ds;
            }
        else if ((j1 = -k)) {
            dval(&u) *= tens[j1 & 0xf];
            for(j = j1 >> 4; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    dval(&u) *= bigtens[i];
                    }
            }
        if (k_check && dval(&u) < 1. && ilim > 0) {
            if (ilim1 <= 0)
                goto fast_failed;
            ilim = ilim1;
            k--;
            dval(&u) *= 10.;
            ieps++;
            }
        dval(&eps) = ieps*dval(&u) + 7.;
        word0(&eps) -= (P-1)*Exp_msk1;
        if (ilim == 0) {
            S = mhi = 0;
            dval(&u) -= 5.;
            if (dval(&u) > dval(&eps))
                goto one_digit;
            if (dval(&u) < -dval(&eps))
                goto no_digits;
            goto fast_failed;
            }
#ifndef No_leftright
        if (leftright) {
            /* Use Steele & White method of only
             * generating digits needed.
             */
            dval(&eps) = 0.5/tens[ilim-1] - dval(&eps);
            for(i = 0;;) {
                L = (Long) dval(&u);
                dval(&u) -= L;
                *s++ = '0' + (int)L;
                if (dval(&u) < dval(&eps))
                    goto ret1;
                if (1. - dval(&u) < dval(&eps))
                    goto bump_up;
                if (++i >= ilim)
                    break;
                dval(&eps) *= 10.;
                dval(&u) *= 10.;
                }
            }
        else {
#endif
            /* Generate ilim digits, then fix them up. */
            dval(&eps) *= tens[ilim-1];
            for(i = 1;; i++, dval(&u) *= 10.) {
                L = (Long)(dval(&u));
                if (!(dval(&u) -= L))
                    ilim = i;
                *s++ = '0' + (int)L;
                if (i == ilim) {
                    if (dval(&u) > 0.5 + dval(&eps))
                        goto bump_up;
                    else if (dval(&u) < 0.5 - dval(&eps)) {
                        while(*--s == '0');
                        s++;
                        goto ret1;
                        }
                    break;
                    }
                }
#ifndef No_leftright
            }
#endif
 fast_failed:
        s = s0;
        dval(&u) = dval(&d2);
        k = k0;
        ilim = ilim0;
        }

    /* Do we have a "small" integer? */

    if (be >= 0 && k <= Int_max) {
        /* Yes. */
        ds = tens[k];
        if (ndigits < 0 && ilim <= 0) {
            S = mhi = 0;
            if (ilim < 0 || dval(&u) <= 5*ds)
                goto no_digits;
            goto one_digit;
            }
        for(i = 1;; i++, dval(&u) *= 10.) {
            L = (Long)(dval(&u) / ds);
            dval(&u) -= L*ds;
#ifdef Check_FLT_ROUNDS
            /* If FLT_ROUNDS == 2, L will usually be high by 1 */
            if (dval(&u) < 0) {
                L--;
                dval(&u) += ds;
                }
#endif
            *s++ = '0' + (int)L;
            if (!dval(&u)) {
#ifdef SET_INEXACT
                inexact = 0;
#endif
                break;
                }
            if (i == ilim) {
#ifdef Honor_FLT_ROUNDS
                if (mode > 1)
                switch(Rounding) {
                  case 0: goto ret1;
                  case 2: goto bump_up;
                  }
#endif
                dval(&u) += dval(&u);
                if (dval(&u) > ds || (dval(&u) == ds && L & 1)) {
 bump_up:
                    while(*--s == '9')
                        if (s == s0) {
                            k++;
                            *s = '0';
                            break;
                            }
                    ++*s++;
                    }
                break;
                }
            }
        goto ret1;
        }

    m2 = b2;
    m5 = b5;
    mhi = mlo = 0;
    if (leftright) {
        i =
#ifndef Sudden_Underflow
            denorm ? be + (BBias + (P-1) - 1 + 1) :
#endif
#ifdef IBM
            1 + 4*P - 3 - bbits + ((bbits + be - 1) & 3);
#else
            1 + P - bbits;
#endif
        b2 += i;
        s2 += i;
        mhi = i2b(1);
        }
    if (m2 > 0 && s2 > 0) {
        i = m2 < s2 ? m2 : s2;
        b2 -= i;
        m2 -= i;
        s2 -= i;
        }
    if (b5 > 0) {
        if (leftright) {
            if (m5 > 0) {
                mhi = pow5mult(mhi, m5);
                b1 = mult(mhi, b);
                Bfree(b);
                b = b1;
                }
            if ((j = b5 - m5))
                b = pow5mult(b, j);
            }
        else
            b = pow5mult(b, b5);
        }
    S = i2b(1);
    if (s5 > 0)
        S = pow5mult(S, s5);

    /* Check for special case that d is a normalized power of 2. */

    spec_case = 0;
    if ((mode < 2 || leftright)
#ifdef Honor_FLT_ROUNDS
            && Rounding == 1
#endif
                ) {
        if (!word1(&u) && !(word0(&u) & Bndry_mask)
#ifndef Sudden_Underflow
         && word0(&u) & (Exp_mask & ~Exp_msk1)
#endif
                ) {
            /* The special case */
            b2 += Log2P;
            s2 += Log2P;
            spec_case = 1;
            }
        }

    /* Arrange for convenient computation of quotients:
     * shift left if necessary so divisor has 4 leading 0 bits.
     *
     * Perhaps we should just compute leading 28 bits of S once
     * and for all and pass them and a shift to quorem, so it
     * can do shifts and ors to compute the numerator for q.
     */
#ifdef Pack_32
    if ((i = ((s5 ? 32 - hi0bits(S->x[S->wds-1]) : 1) + s2) & 0x1f))
        i = 32 - i;
#define iInc 28
#else
    if (i = ((s5 ? 32 - hi0bits(S->x[S->wds-1]) : 1) + s2) & 0xf)
        i = 16 - i;
#define iInc 12
#endif
    i = dshift(S, s2);
    b2 += i;
    m2 += i;
    s2 += i;
    if (b2 > 0)
        b = lshift(b, b2);
    if (s2 > 0)
        S = lshift(S, s2);
    if (k_check) {
        if (cmp(b,S) < 0) {
            k--;
            b = multadd(b, 10, 0);  /* we botched the k estimate */
            if (leftright)
                mhi = multadd(mhi, 10, 0);
            ilim = ilim1;
            }
        }
    if (ilim <= 0 && (mode == 3 || mode == 5)) {
        if (ilim < 0 || cmp(b,S = multadd(S,5,0)) <= 0) {
            /* no digits, fcvt style */
 no_digits:
            k = -1 - ndigits;
            goto ret;
            }
 one_digit:
        *s++ = '1';
        k++;
        goto ret;
        }
    if (leftright) {
        if (m2 > 0)
            mhi = lshift(mhi, m2);

        /* Compute mlo -- check for special case
         * that d is a normalized power of 2.
         */

        mlo = mhi;
        if (spec_case) {
            mhi = Balloc(mhi->k);
            Bcopy(mhi, mlo);
            mhi = lshift(mhi, Log2P);
            }

        for(i = 1;;i++) {
            dig = quorem(b,S) + '0';
            /* Do we yet have the shortest decimal string
             * that will round to d?
             */
            j = cmp(b, mlo);
            delta = diff(S, mhi);
            j1 = delta->sign ? 1 : cmp(b, delta);
            Bfree(delta);
#ifndef ROUND_BIASED
            if (j1 == 0 && mode != 1 && !(word1(&u) & 1)
#ifdef Honor_FLT_ROUNDS
                && Rounding >= 1
#endif
                                   ) {
                if (dig == '9')
                    goto round_9_up;
                if (j > 0)
                    dig++;
#ifdef SET_INEXACT
                else if (!b->x[0] && b->wds <= 1)
                    inexact = 0;
#endif
                *s++ = dig;
                goto ret;
                }
#endif
            if (j < 0 || (j == 0 && mode != 1
#ifndef ROUND_BIASED
                            && !(word1(&u) & 1)
#endif
                    )) {
                if (!b->x[0] && b->wds <= 1) {
#ifdef SET_INEXACT
                    inexact = 0;
#endif
                    goto accept_dig;
                    }
#ifdef Honor_FLT_ROUNDS
                if (mode > 1)
                 switch(Rounding) {
                  case 0: goto accept_dig;
                  case 2: goto keep_dig;
                  }
#endif /*Honor_FLT_ROUNDS*/
                if (j1 > 0) {
                    b = lshift(b, 1);
                    j1 = cmp(b, S);
                    if ((j1 > 0 || (j1 == 0 && dig & 1))
                    && dig++ == '9')
                        goto round_9_up;
                    }
 accept_dig:
                *s++ = dig;
                goto ret;
                }
            if (j1 > 0) {
#ifdef Honor_FLT_ROUNDS
                if (!Rounding)
                    goto accept_dig;
#endif
                if (dig == '9') { /* possible if i == 1 */
 round_9_up:
                    *s++ = '9';
                    goto roundoff;
                    }
                *s++ = dig + 1;
                goto ret;
                }
#ifdef Honor_FLT_ROUNDS
 keep_dig:
#endif
            *s++ = dig;
            if (i == ilim)
                break;
            b = multadd(b, 10, 0);
            if (mlo == mhi)
                mlo = mhi = multadd(mhi, 10, 0);
            else {
                mlo = multadd(mlo, 10, 0);
                mhi = multadd(mhi, 10, 0);
                }
            }
        }
    else
        for(i = 1;; i++) {
            *s++ = dig = quorem(b,S) + '0';
            if (!b->x[0] && b->wds <= 1) {
#ifdef SET_INEXACT
                inexact = 0;
#endif
                goto ret;
                }
            if (i >= ilim)
                break;
            b = multadd(b, 10, 0);
            }

    /* Round off last digit */

#ifdef Honor_FLT_ROUNDS
    switch(Rounding) {
      case 0: goto trimzeros;
      case 2: goto roundoff;
      }
#endif
    b = lshift(b, 1);
    j = cmp(b, S);
    if (j > 0 || (j == 0 && dig & 1)) {
 roundoff:
        while(*--s == '9')
            if (s == s0) {
                k++;
                *s++ = '1';
                goto ret;
                }
        ++*s++;
        }
    else {
#ifdef Honor_FLT_ROUNDS
 trimzeros:
#endif
        while(*--s == '0');
        s++;
        }
 ret:
    Bfree(S);
    if (mhi) {
        if (mlo && mlo != mhi)
            Bfree(mlo);
        Bfree(mhi);
        }
 ret1:
#ifdef SET_INEXACT
    if (inexact) {
        if (!oldinexact) {
            word0(&u) = Exp_1 + (70 << Exp_shift);
            word1(&u) = 0;
            dval(&u) += 1.;
            }
        }
    else if (!oldinexact)
        clear_inexact();
#endif
    Bfree(b);
    *s = 0;
    *decpt = k + 1;
    if (rve)
        *rve = s;
    return s0;
    }
#ifdef __cplusplus
}
#endif
/* EMBEDTHIS */
#endif /* BLD_FEATURE_FLOATING_POINT */
/************************************************************************/
/*
 *  End of file "../src/dtoa.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mpr.c"
 */
/************************************************************************/

/**
 *  mpr.c - Michael's Portable Runtime (MPR). Initialization, start/stop and control of the MPR.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */




static void memoryFailure(MprCtx ctx, int64 size, int64 total, bool granted);
static int  mprDestructor(Mpr *mpr);

#if BLD_FEATURE_MULTITHREAD
static void serviceEvents(void *data, MprThread *tp);
#endif

/*
 *  Create the MPR service. This routine is the first call an MPR application must do. It creates the top 
 *  level memory context.
 */

Mpr *mprCreate(int argc, char **argv, MprAllocNotifier cback)
{
    return mprCreateEx(argc, argv, cback, NULL);
}


/*
 *  Add a shell parameter then do the regular init
 */
Mpr *mprCreateEx(int argc, char **argv, MprAllocNotifier cback, void *shell)
{
    MprFileSystem   *fs;
    Mpr             *mpr;
    char            *cp;

    if (cback == 0) {
        cback = memoryFailure;
    }
    mpr = (Mpr*) mprCreateAllocService(cback, (MprDestructor) mprDestructor);

    if (mpr == 0) {
        mprAssert(mpr);
        return 0;
    }
    
    /*
     *  Wince and Vxworks passes an arg via argc, and the program name in argv. NOTE: this will only work on 32-bit systems.
     */
#if WINCE
    mprMakeArgv(mpr, (char*) argv, mprToAsc(mpr, (uni*) argc), &argc, &argv);
#elif VXWORKS
    mprMakeArgv(mpr, NULL, (char*) argc, &argc, &argv);
#endif
    mpr->argc = argc;
    mpr->argv = argv;

    mpr->name = mprStrdup(mpr, BLD_PRODUCT);
    mpr->title = mprStrdup(mpr, BLD_NAME);
    mpr->version = mprStrdup(mpr, BLD_VERSION);
    mpr->idleCallback = mprServicesAreIdle;

    if (mprCreateTimeService(mpr) < 0) {
        goto error;
    }
    if ((mpr->osService = mprCreateOsService(mpr)) < 0) {
        goto error;
    }

    /*
     *  See if any of the preceeding allocations failed and mark all blocks allocated so far as required.
     *  They will then be omitted from leak reports.
     */
    if (mprHasAllocError(mpr)) {
        goto error;
    }

#if BREW
    mprSetShell(mpr, shell);
#endif

#if BLD_FEATURE_MULTITHREAD
    mpr->multiThread = 1;
    if ((mpr->threadService = mprCreateThreadService(mpr)) == 0) {
        goto error;
    }
    mpr->mutex = mprCreateLock(mpr);
    mpr->spin = mprCreateSpinLock(mpr);
#endif

    if ((fs = mprCreateFileSystem(mpr, "/")) == 0) {
        goto error;
    }
    mprAddFileSystem(mpr, fs);

    if ((mpr->moduleService = mprCreateModuleService(mpr)) == 0) {
        goto error;
    }
    if ((mpr->dispatcher = mprCreateDispatcher(mpr)) == 0) {
        goto error;
    }
    if ((mpr->cmdService = mprCreateCmdService(mpr)) == 0) {
        goto error;
    }
#if BLD_FEATURE_MULTITHREAD
    if ((mpr->workerService = mprCreateWorkerService(mpr)) == 0) {
        goto error;
    }
#endif
    if ((mpr->waitService = mprCreateWaitService(mpr)) == 0) {
        goto error;
    }
    if ((mpr->socketService = mprCreateSocketService(mpr)) == 0) {
        goto error;
    }
#if BLD_FEATURE_HTTP
    if ((mpr->httpService = mprCreateHttpService(mpr)) == 0) {
        goto error;
    }
#endif

    if (mpr->argv && mpr->argv[0] && *mpr->argv[0]) {
        mprFree(mpr->name);
        mpr->name = mprGetPathBase(mpr, mpr->argv[0]);
        if ((cp = strchr(mpr->name, '.')) != 0) {
            *cp = '\0';
        }
    }

    /*
     *  Now catch all memory allocation errors up to this point. Should be none.
     */
    if (mprHasAllocError(mpr)) {
        goto error;
    }
    return mpr;

/*
 *  Error return
 */
error:
    mprFree(mpr);
    return 0;
}


static int mprDestructor(Mpr *mpr)
{
    if ((mpr->flags & MPR_STARTED) && !(mpr->flags & MPR_STOPPED)) {
        if (!mprStop(mpr)) {
            return 1;
        }
    }
    return 0;

}


/*
 *  Start the Mpr and all services
 */
int mprStart(Mpr *mpr, int startEventsThread)
{
    int     rc;

    rc = mprStartOsService(mpr->osService);
    rc += mprStartModuleService(mpr->moduleService);
#if BLD_FEATURE_MULTITHREAD
    rc += mprStartWorkerService(mpr->workerService);
#endif
    rc += mprStartSocketService(mpr->socketService);
#if BLD_FEATURE_HTTP
    rc += mprStartHttpService(mpr->httpService);
#endif

    if (rc != 0) {
        mprUserError(mpr, "Can't start MPR services");
        return MPR_ERR_CANT_INITIALIZE;
    }
    mpr->flags |= MPR_STARTED;
    mprLog(mpr, MPR_INFO, "MPR services are ready");
#if BLD_FEATURE_MULTITHREAD
    if (startEventsThread) {
        mprStartEventsThread(mpr);
    }
#endif
    return 0;
}


bool mprStop(Mpr *mpr)
{
    int     stopped;

    stopped = 1;

    mprLock(mpr->mutex);
    if (! (mpr->flags & MPR_STARTED) || (mpr->flags & MPR_STOPPED)) {
        mprUnlock(mpr->mutex);
        return 0;
    }
    mpr->flags |= MPR_STOPPED;

    /*
        Trigger graceful termination. This will prevent further tasks and events being created.
     */
    mprTerminate(mpr, 1);

#if BLD_FEATURE_HTTP
    mprStopHttpService(mpr->httpService);
#endif
    mprStopSocketService(mpr->socketService);
#if BLD_FEATURE_MULTITHREAD
    if (!mprStopWorkerService(mpr->workerService, MPR_TIMEOUT_STOP_TASK)) {
        stopped = 0;
    }
    if (!mprStopThreadService(mpr->threadService, MPR_TIMEOUT_STOP_TASK)) {
        stopped = 0;
    }
#endif
    mprStopModuleService(mpr->moduleService);
    mprStopOsService(mpr->osService);
    return stopped;
}


#if BLD_FEATURE_MULTITHREAD
/*
 *  Thread to service the event queue. Used if the user does not have their own main event loop.
 */
int mprStartEventsThread(Mpr *mpr)
{
    MprThread   *tp;

    mprLog(mpr, MPR_CONFIG, "Starting service thread");

    if ((tp = mprCreateThread(mpr, "events", serviceEvents, 0, MPR_NORMAL_PRIORITY, 0)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    mpr->hasDedicatedService = 1;
    mprStartThread(tp);
    return 0;
}


/*
 *  Thread main for serviceEvents
 */
static void serviceEvents(void *data, MprThread *tp)
{
    Mpr     *mpr;

    mpr = mprGetMpr(tp);
    mpr->serviceThread = tp->osThread;
    mprServiceEvents(mpr->dispatcher, -1, MPR_SERVICE_EVENTS | MPR_SERVICE_IO);
    mpr->serviceThread = 0;
    mpr->hasDedicatedService = 1;
}


void mprSetServiceThread(MprCtx ctx, MprThread *thread)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mpr->serviceThread = thread->osThread;
}


/*
 *  If this thread is not responsibile for running the Mpr dispatcher then return true.
 */
bool mprMustWakeDispatcher(MprCtx ctx)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);

#if BLD_FEATURE_MULTITHREAD
    return mprGetCurrentOsThread() != mpr->serviceThread;
#else
    return 0;
#endif
}
#endif /* BLD_FEATURE_MULTITHREAD */


/*
 *  Exit the mpr gracefully. Instruct the event loop to exit.
 */
void mprTerminate(MprCtx ctx, bool graceful)
{
    if (! graceful) {
        exit(0);
    }
    mprSignalExit(ctx);
}


bool mprIsExiting(MprCtx ctx)
{
    Mpr *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr == 0) {
        return 1;
    }
    return mpr->flags & MPR_EXITING;
}


bool mprIsComplete(MprCtx ctx)
{
    Mpr *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr == 0) {
        return 1;
    }
    return (mpr->flags & MPR_EXITING) && mprIsIdle(ctx);
}


/*
    Just the Mpr services are idle. Use mprIsIdle to determine if the entire process is idle
 */
bool mprServicesAreIdle(MprCtx ctx)
{
    Mpr     *mpr;
    
    mpr = mprGetMpr(ctx);
#if BLD_FEATURE_MULTITHREAD
    return mprGetListCount(mpr->workerService->busyThreads) == 0 && mprGetListCount(mpr->cmdService->cmds) == 0 && 
       !(mpr->dispatcher->flags & MPR_DISPATCHER_DO_EVENT);
#else
    return mprGetListCount(mpr->cmdService->cmds) == 0 && !(mpr->dispatcher->flags & MPR_DISPATCHER_DO_EVENT);
#endif
}


bool mprIsIdle(MprCtx ctx)
{
    return (mprGetMpr(ctx)->idleCallback)(ctx);
}


MprIdleCallback mprSetIdleCallback(MprCtx ctx, MprIdleCallback idleCallback)
{
    MprIdleCallback old;
    Mpr             *mpr;
    
    mpr = mprGetMpr(ctx);
    old = mpr->idleCallback;
    mpr->idleCallback = idleCallback;
    return old;
}


void mprSignalExit(MprCtx ctx)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);

    mprSpinLock(mpr->spin);
    mpr->flags |= MPR_EXITING;
    mprSpinUnlock(mpr->spin);
    mprWakeWaitService(mpr);
}


int mprSetAppName(MprCtx ctx, cchar *name, cchar *title, cchar *version)
{
    Mpr     *mpr;
    char    *cp;

    mpr = mprGetMpr(ctx);

    if (name) {
        mprFree(mpr->name);
        if ((mpr->name = (char*) mprGetPathBase(mpr, name)) == 0) {
            return MPR_ERR_CANT_ALLOCATE;
        }
        if ((cp = strrchr(mpr->name, '.')) != 0) {
            *cp = '\0';
        }
    }

    if (title) {
        mprFree(mpr->title);
        if ((mpr->title = mprStrdup(ctx, title)) == 0) {
            return MPR_ERR_CANT_ALLOCATE;
        }
    }

    if (version) {
        mprFree(mpr->version);
        if ((mpr->version = mprStrdup(ctx, version)) == 0) {
            return MPR_ERR_CANT_ALLOCATE;
        }
    }
    return 0;
}


cchar *mprGetAppName(MprCtx ctx)
{
    return mprGetMpr(ctx)->name;
}


cchar *mprGetAppTitle(MprCtx ctx)
{
    return mprGetMpr(ctx)->title;
}


/*
 *  Full host name with domain. E.g. "server.domain.com"
 */
void mprSetHostName(MprCtx ctx, cchar *s)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mprLock(mpr->mutex);
    mprFree(mpr->hostName);
    mpr->hostName = mprStrdup(mpr, s);
    mprUnlock(mpr->mutex);
    return;
}


/*
 *  Return the fully qualified host name
 */
cchar *mprGetHostName(MprCtx ctx)
{
    return mprGetMpr(ctx)->hostName;
}


/*
 *  Server name portion (no domain name)
 */
void mprSetServerName(MprCtx ctx, cchar *s)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr->serverName) {
        mprFree(mpr->serverName);
    }
    mpr->serverName = mprStrdup(mpr, s);
    return;
}


/*
 *  Return the server name
 */
cchar *mprGetServerName(MprCtx ctx)
{
    return mprGetMpr(ctx)->serverName;
}


/*
 *  Set the domain name
 */
void mprSetDomainName(MprCtx ctx, cchar *s)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr->domainName) {
        mprFree(mpr->domainName);
    }
    mpr->domainName = mprStrdup(mpr, s);
    return;
}


/*
 *  Return the domain name
 */
cchar *mprGetDomainName(MprCtx ctx)
{
    return mprGetMpr(ctx)->domainName;
}


/*
 *  Set the IP address
 */
void mprSetIpAddr(MprCtx ctx, cchar *s)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr->ipAddr) {
        mprFree(mpr->ipAddr);
    }
    mpr->ipAddr = mprStrdup(mpr, s);
    return;
}


/*
 *  Return the IP address
 */
cchar *mprGetIpAddr(MprCtx ctx)
{
    return mprGetMpr(ctx)->ipAddr;
}


cchar *mprGetAppVersion(MprCtx ctx)
{
    Mpr *mpr;

    mpr = mprGetMpr(ctx);
    return mpr->version;
}


bool mprGetDebugMode(MprCtx ctx)
{
    return mprGetMpr(ctx)->debugMode;
}


void mprSetDebugMode(MprCtx ctx, bool on)
{
    mprGetMpr(ctx)->debugMode = on;
}


void mprSetLogHandler(MprCtx ctx, MprLogHandler handler, void *handlerData)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);

    mpr->logHandler = handler;
    mpr->logHandlerData = handlerData;
}


MprLogHandler mprGetLogHandler(MprCtx ctx)
{
    return mprGetMpr(ctx)->logHandler;
}


cchar *mprCopyright()
{
    return  "Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.\n"
            "Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.";
}


int mprGetEndian(MprCtx ctx)
{
    char    *probe;
    int     test;

    test = 1;
    probe = (char*) &test;
    return (*probe == 1) ? MPR_LITTLE_ENDIAN : MPR_BIG_ENDIAN;
}


/*
 *  Default memory handler
 */
static void memoryFailure(MprCtx ctx, int64 size, int64 total, bool granted)
{
    if (!granted) {
        mprPrintfError(ctx, "Can't allocate memory block of size %d\n", size);
        mprPrintfError(ctx, "Total memory used %d\n", total);
        exit(255);
    }
    mprPrintfError(ctx, "Memory request for %d bytes exceeds memory red-line\n", size);
    mprPrintfError(ctx, "Total memory used %d\n", total);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mpr.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprAlloc.c"
 */
/************************************************************************/

/**
 *  mprAlloc.c - Memory Allocator. This is a layer above malloc providing memory services including: virtual memory mapping,
 *               slab based and arena based allocations.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */


/*
 *  This is a memory "turbo-charger" that sits above malloc. It provides arena and slab based allocations. The goal is
 *  to provide a scalable memory allocator that supports hierarchical allocations and performs well in multi-threaded apps.
 *  It suports arena-based and slab-based allocations.
 * 
 *  This module uses several preprocessor directives to control features:
 *
 *      BLD_FEATURE_MEMORY_DEBUG            Enable checks for block integrity. Fills blocks on allocation and free.
 *      BLD_FEATURE_MEMORY_STATS            Enables accumulation of memory stats.
 *      BLD_FEATURE_MONITOR_STACK           Monitor stack use
 *      BLD_FEATURE_MULTITHREAD             Defined if building a multi-threaded application.
 *      BLD_FEATURE_VERIFY                  Adds deep and slow integrity tests.
 *      BLD_CC_MMU                          Enabled if the system has a memory management unit supporting virtual memory.
 */



/*
 *  Convert from user pointers to memory blocks and back again.
 */
#define GET_BLK(ptr)            ((MprBlk*) (((char*) (ptr)) - MPR_ALLOC_HDR_SIZE))
#define GET_PTR(bp)             ((char*) (((char*) (bp)) + MPR_ALLOC_HDR_SIZE))
#define GET_USIZE(bp)           ((bp->size) - MPR_ALLOC_HDR_SIZE)
#define DESTRUCTOR_PTR(bp)      (((char*) bp) + bp->size - sizeof(MprDestructor))
#define GET_DESTRUCTOR(bp)      ((bp->flags & MPR_ALLOC_HAS_DESTRUCTOR) ? \
                                    (MprDestructor) (*(MprDestructor*) (DESTRUCTOR_PTR(bp))) : 0)
#define SET_DESTRUCTOR(bp, d)   if (d) { bp->flags |= MPR_ALLOC_HAS_DESTRUCTOR; \
                                    *((MprDestructor*) DESTRUCTOR_PTR(bp)) = d; } else
#if BLD_FEATURE_MEMORY_DEBUG
#define VALID_BLK(bp)           ((bp)->magic == MPR_ALLOC_MAGIC)
#define VALID_CTX(ptr)          (VALID_BLK(GET_BLK(ptr)))
#define SET_MAGIC(bp)           (bp)->magic = MPR_ALLOC_MAGIC

/*
 *  Set this address to break when this address is allocated or freed. This is a block address (not a user ptr).
 */
static MprBlk *stopAlloc;
static int stopSeqno = -1;
static int allocCount = 1;

#else
#define VALID_BLK(bp)           (1)
#define VALID_CTX(ptr)          (1)
#define SET_MAGIC(bp)
#endif

/*
 *  Heaps may be "thread-safe" such that lock and unlock requests on a single heap can come from different threads.
 *  The lock and unlock macros will use spin locks because we expect contention to be very low.
 */
#if BLD_FEATURE_MULTITHREAD
#define lockHeap(heap)               if (unlikely(heap->flags & MPR_ALLOC_THREAD_SAFE)) { mprSpinLock(&heap->spin); }
#define unlockHeap(heap)             if (unlikely(heap->flags & MPR_ALLOC_THREAD_SAFE)) { mprSpinUnlock(&heap->spin); }
#else
#define lockHeap(ctx)
#define unlockHeap(ctx)
#endif

#if BLD_HAS_GLOBAL_MPR || BLD_WIN_LIKE
/*
 *  Mpr control and root memory context. This is a constant and a permissible global.
 */
    #if BLD_WIN_LIKE
        static Mpr  *_globalMpr;
    #else
        #undef _globalMpr
        Mpr  *_globalMpr;
    #endif
#endif


static void allocException(MprBlk *bp, uint size, bool granted);
static void *allocMemory(uint size);
static void allocError(MprBlk *parent, uint size);
static void freeBlock(Mpr *mpr, MprHeap *heap, MprBlk *bp);
static void freeMemory(MprBlk *bp);
static void initHeap(MprHeap *heap, cchar *name, bool threadSafe);
static void linkBlock(MprBlk *parent, MprBlk *bp);
static void sysinit(Mpr *mpr);
static void unlinkBlock(MprBlk *bp);

#if BLD_CC_MMU
static MprRegion *createRegion(MprCtx ctx, MprHeap *heap, uint size);
#endif
#if BLD_FEATURE_MEMORY_STATS
static void incStats(MprHeap *heap, MprBlk *bp);
static void decStats(MprHeap *heap, MprBlk *bp);
#else
#define incStats(heap, parent, bp)
#define decStats(heap, bp)
#endif
#if BLD_FEATURE_MONITOR_STACK
static void monitorStack(Mpr *mpr);
#endif
#if BLD_WIN_LIKE
static int mapProt(int flags);
#endif

/*
 *  Initialize the memory subsystem
 */
Mpr *mprCreateAllocService(MprAllocNotifier cback, MprDestructor destructor)
{
    Mpr             *mpr;
    MprBlk          *bp;
    uint            usize, size;

    /*
     *  Hand-craft the first block to optimize subsequent use of mprAlloc. Layout is:
     *      HDR
     *      Mpr
     *          MprHeap
     *      Destructor
     */
    usize = sizeof(Mpr) + sizeof(MprDestructor);
    size = MPR_ALLOC_ALIGN(MPR_ALLOC_HDR_SIZE + usize);
    usize = size - MPR_ALLOC_HDR_SIZE;

    bp = (MprBlk*) allocMemory(size);
    if (bp == 0) {
        if (cback) {
            (*cback)(0, sizeof(Mpr), 0, 0);
        }
        return 0;
    }
    memset(bp, 0, size);
    mpr = (Mpr*) GET_PTR(bp);

    /*
     *  Set initial defaults to no memory limit. Redline at 90%.
     */
    mpr->alloc.maxMemory = INT_MAX;
    mpr->alloc.redLine = INT_MAX / 100 * 99;

#if BLD_HAS_GLOBAL_MPR || BLD_WIN_LIKE
    _globalMpr = mpr;
#endif

    bp->parent = 0;
    bp->size = size;
    SET_DESTRUCTOR(bp, destructor);

    SET_MAGIC(bp);
    bp->flags |= MPR_ALLOC_IS_HEAP;

    mpr->alloc.bytesAllocated += size;
    mpr->alloc.peakAllocated = mpr->alloc.bytesAllocated;
    mpr->alloc.stackStart = (void*) &mpr;

    sysinit(mpr);
    initHeap(&mpr->pageHeap, "page", 1);
    mpr->pageHeap.flags = MPR_ALLOC_PAGE_HEAP | MPR_ALLOC_THREAD_SAFE;
    initHeap(&mpr->heap, "mpr", 1);

    mpr->heap.notifier = cback;
    mpr->heap.notifierCtx = mpr;

#if BLD_FEATURE_MEMORY_DEBUG
    stopAlloc = 0;
    stopSeqno = -1;
#endif
    return mpr;
}


static MprCtx allocHeap(MprCtx ctx, cchar *name, uint heapSize, bool threadSafe, MprDestructor destructor)
{
    Mpr         *mpr;
    MprHeap     *pageHeap, *heap;
    MprRegion   *region;
    MprBlk      *bp, *parent;
    int         headersSize, usize, size;

    mprAssert(ctx);
    mprAssert(VALID_CTX(ctx));

    mpr = mprGetMpr(ctx);

    /*
     *  Allocate the full arena/slab out of one memory allocation. This includes the user object, heap object and 
     *  heap memory. Do this because heaps should generally be initially sized to be sufficient for the apps needs 
     *  (they are virtual with MMUs)
     *
     *  Layout is:
     *      HDR
     *      MprHeap structure
     *      MprRegion structure
     *      Heap data (>= heapSize)
     *
     *  The MprHeap and MprRegion structures are aligned. This may result in the size allocated being bigger 
     *  than the requested heap size.
     */
    headersSize = MPR_ALLOC_ALIGN(sizeof(MprHeap) + sizeof(MprRegion));
    usize = headersSize + heapSize;
    size = MPR_PAGE_ALIGN(MPR_ALLOC_HDR_SIZE + usize, mpr->alloc.pageSize);
    usize = (size - MPR_ALLOC_HDR_SIZE);
    heapSize = usize - headersSize;

    parent = GET_BLK(ctx);
    mprAssert(parent);

    /*
     *  All heaps are allocated from the page heap
     */
    pageHeap = &mpr->pageHeap;
    mprAssert(pageHeap);

    if (unlikely((bp = _mprAllocBlock(ctx, pageHeap, NULL, usize)) == 0)) {
        allocError(parent, usize);
        unlockHeap(pageHeap);
        return 0;
    }

    lockHeap(pageHeap);
    bp->flags |= MPR_ALLOC_IS_HEAP;
    linkBlock(parent, bp);
    incStats(pageHeap, bp);
    unlockHeap(pageHeap);

    heap = (MprHeap*) GET_PTR(bp);
    heap->destructor = destructor;
    initHeap((MprHeap*) heap, name, threadSafe);

    region = (MprRegion*) ((char*) heap + sizeof(MprHeap));
    region->next = 0;
    region->memory = (char*) heap + headersSize;
    region->nextMem = region->memory;
    region->vmSize = MPR_ALLOC_ALIGN(MPR_ALLOC_HDR_SIZE + usize);
    region->size = heapSize;
    heap->region = region;
    return GET_PTR(bp);
}


/*
 *  Create an arena memory context. An arena context is a memory heap which allocates all child requests from a 
 *  single (logical) memory block. Allocations are done like simple salami slices. Arenas may be created thread-safe, 
 *  and are not thread-safe by default for speed.
 */
MprHeap *mprAllocArena(MprCtx ctx, cchar *name, uint arenaSize, bool threadSafe, MprDestructor destructor)
{
    MprHeap     *heap;

    mprAssert(ctx);
    mprAssert(VALID_CTX(ctx));
    mprAssert(arenaSize > 0);

    heap = (MprHeap*) allocHeap(ctx, name, arenaSize, threadSafe, destructor);
    if (heap == 0) {
        return 0;
    }
    heap->flags = MPR_ALLOC_ARENA_HEAP;
    return heap;
}


/*
 *  Create standard (malloc) heap. 
 */
MprHeap *mprAllocHeap(MprCtx ctx, cchar *name, uint arenaSize, bool threadSafe, MprDestructor destructor)
{
    MprHeap     *heap;

    mprAssert(ctx);
    mprAssert(VALID_CTX(ctx));
    mprAssert(arenaSize > 0);

    heap = (MprHeap*) allocHeap(ctx, name, arenaSize, threadSafe, destructor);
    if (heap == 0) {
        return 0;
    }
    heap->flags = MPR_ALLOC_MALLOC_HEAP;
    return heap;
}


/*
 *  Create an object slab memory context. An object slab context is a memory heap which allocates constant size objects 
 *  from a single (logical) memory block. The object slab keeps a free list of freed blocks. Object slabs may be created 
 *  thread-safe, but are thread insensitive by default and will allocate memory without any locking. Hence allocations 
 *  will be fast and scalable.
 *
 *  This call is typically made via the macro mprCreateSlab. ObjSize is the size of objects to create from the slab heap.
 *  The count parameter indicates how may objects the slab heap will initially contain. MaxCount is the the maximum the 
 *  heap will ever support. If maxCount is greater than count, then the slab is growable.
 *
 *  NOTE: Currently not being used
 */
MprHeap *mprAllocSlab(MprCtx ctx, cchar *name, uint objSize, uint count, bool threadSafe, MprDestructor destructor)
{
    MprHeap     *heap;
    uint        size;

    mprAssert(ctx);
    mprAssert(VALID_CTX(ctx));
    mprAssert(objSize > 0);
    mprAssert(count > 0);

    size = MPR_ALLOC_ALIGN(objSize) * count;
    heap = (MprHeap*) allocHeap(ctx, name, size, threadSafe, destructor);
    if (heap == 0) {
        return 0;
    }
    heap->flags = MPR_ALLOC_SLAB_HEAP;
    return heap;
}


/*
 *  Allocate a block. Not used to allocate heaps.
 */
void *_mprAlloc(MprCtx ctx, uint usize)
{
    MprBlk      *bp, *parent;
    MprHeap     *heap;

    mprAssert(ctx);
    mprAssert(usize >= 0);
    mprAssert(VALID_CTX(ctx));

    parent = GET_BLK(ctx);
    mprAssert(parent);
    heap = mprGetHeap(parent);
    mprAssert(heap);

    if (unlikely((bp = _mprAllocBlock(ctx, heap, parent, usize)) == 0)) {
        allocError(parent, usize);
        return 0;
    }
    return GET_PTR(bp);
}


/*
 *  Allocate and zero a block
 */
void *_mprAllocZeroed(MprCtx ctx, uint size)
{
    void    *newBlock;

    newBlock = _mprAlloc(ctx, size);
    mprAssert(newBlock);

    if (newBlock) {
        memset(newBlock, 0, size);
    }
    return newBlock;
}


/*
 *  Allocate an object. Typically used via the macro: mprAllocObj
 */
void *_mprAllocWithDestructor(MprCtx ctx, uint size, MprDestructor destructor)
{
    MprBlk      *bp;
    void        *ptr;

    mprAssert(VALID_CTX(ctx));
    mprAssert(size > 0);

    ptr = _mprAlloc(ctx, size + sizeof(MprDestructor));
    mprAssert(ptr);
    if (ptr == 0) {
        return 0;
    }
    bp = GET_BLK(ptr);
    SET_DESTRUCTOR(bp, destructor);
    return ptr;
}


void mprSetDestructor(void *ptr, MprDestructor destructor)
{
    MprBlk      *bp;

    bp = GET_BLK(ptr);
    SET_DESTRUCTOR(bp, destructor);
}


#if BLD_DEBUG
cchar *mprGetName(void *ptr)
{
    MprBlk      *bp;

    bp = GET_BLK(ptr);
    return bp->name;
}


void *mprSetName(void *ptr, cchar *name)
{
    MprBlk      *bp;

    if (ptr) {
        bp = GET_BLK(ptr);
        if (bp) {
            bp->name = (char*) name;
        }
    }
    return ptr;
}


void *mprSetDynamicName(void *ptr, cchar *name)
{
    MprBlk      *bp;

    if (ptr) {
        bp = GET_BLK(ptr);
        if (bp) {
            bp->name = malloc(strlen(name) + 1);
            if (bp->name) {
                strcpy(bp->name, name);
            }
        }
    }
    return ptr;
}
#else
#undef mprSetName
#undef mprSetDynamicName
#undef mprGetName
void *mprSetName(void *ptr, cchar *name) { return ptr; }
void *mprSetDynamicName(void *ptr, cchar *name) { return ptr; }
cchar *mprGetName(void *ptr) { return ""; }
#endif


void mprInitBlock(MprCtx ctx, void *ptr, uint size)
{
    MprBlk      *bp;

    bp = GET_BLK(ptr);
    memset(ptr, 0, size);
    bp->parent = MPR_GET_BLK(mprGetMpr(ctx));
    bp->children = 0;
    bp->next = 0;
    bp->prev = 0;
    bp->size = 0;
    bp->flags = 0;
    SET_MAGIC(bp);
}


/*
 *  Allocate and zero a block
 */
void *_mprAllocWithDestructorZeroed(MprCtx ctx, uint size, MprDestructor destructor)
{
    void    *newBlock;

    newBlock = _mprAllocWithDestructor(ctx, size, destructor);
    if (newBlock) {
        memset(newBlock, 0, size);
    }
    return newBlock;
}


/*
 *  Free a block of memory. Free all children recursively. Return 0 if the memory was freed. A destructor may prevent
 *  memory being deleted by returning non-zero.
 */
int mprFree(void *ptr)
{
    Mpr         *mpr;
    MprHeap     *heap, *hp;
    MprBlk      *bp, *parent;

    if (unlikely(ptr == 0)) {
        return 0;
    }
    mpr = mprGetMpr(ptr);
    bp = GET_BLK(ptr);
    mprAssert(VALID_BLK(bp));
    mprAssert(bp->size > 0);

#if BLD_FEATURE_MEMORY_DEBUG
    if (bp == stopAlloc || bp->seqno == stopSeqno) {
        mprBreakpoint();
    }
    /*
     *  Test if already freed
     */
    if (unlikely(bp->parent == 0 && ptr != mpr)) {
        mprAssert(bp->parent);
        return 0;
    }
#endif

    /*
     *  We need to run destructors first if there is a destructor and it isn't a heap
     */
    if (unlikely(bp->flags & MPR_ALLOC_HAS_DESTRUCTOR)) {
        if ((GET_DESTRUCTOR(bp))(ptr) != 0) {
            /*
             *  Destructor aborted the free. Reparent to the top level.
             */
            mprStealBlock(mpr, ptr);
            return 1;
        }
    }
    
    mprFreeChildren(ptr);
    parent = bp->parent;

    if (unlikely(bp->flags & MPR_ALLOC_IS_HEAP)) {
        hp = (MprHeap*) ptr;
        if (hp->destructor) {
            hp->destructor(ptr);
        }
        heap = &mpr->pageHeap;

    } else {
        mprAssert(VALID_BLK(parent));
        heap = mprGetHeap(parent);
        mprAssert(heap);
    }

    lockHeap(heap);
    decStats(heap, bp);
    unlinkBlock(bp);
    freeBlock(mpr, heap, bp);
    if (ptr != mpr) {
        unlockHeap(heap);
    }
    return 0;
}


/*
 *  Free the children of a block of memory
 */
void mprFreeChildren(MprCtx ptr)
{
    MprBlk      *bp, *child, *next;

    if (unlikely(ptr == 0)) {
        return;
    }

    bp = GET_BLK(ptr);
    mprAssert(VALID_BLK(bp));

    /*
     *  Free the children. They are linked in LIFO order. So free from the start and it will actually free in reverse order.
     *  ie. last allocated will be first freed.
     */
    if (likely((child = bp->children) != NULL)) {
        do {
            mprAssert(VALID_BLK(child));
            next = child->next;
            mprFree(GET_PTR(child));
        } while ((child = next) != 0);
        bp->children = 0;
    }
}


/*
 *  Rallocate a block
 */
void *_mprRealloc(MprCtx ctx, void *ptr, uint usize)
{
    MprHeap     *heap;
    MprBlk      *parent, *bp, *newbp, *child;
    Mpr         *mpr;
    void        *newPtr;

    mprAssert(VALID_CTX(ctx));
    mprAssert(usize > 0);

    if (ptr == 0) {
        return _mprAlloc(ctx, usize);
    }
    mpr = mprGetMpr(ctx);
    mprAssert(VALID_CTX(ptr));
    bp = GET_BLK(ptr);
    mprAssert(bp);
    mprAssert(bp->parent);

    if (usize < GET_USIZE(bp)) {
        return ptr;
    }
    parent = GET_BLK(ctx);
    mprAssert(parent);

    newPtr = _mprAlloc(ctx, usize);
    if (newPtr == 0) {
        return 0;
    }

    newbp = GET_BLK(newPtr);
    mprAssert(newbp->parent == parent);
    memcpy(GET_PTR(newbp), GET_PTR(bp), GET_USIZE(bp));

    heap = mprGetHeap(parent);
    mprAssert(heap);
    lockHeap(heap);

    /*
     *  Remove old block
     */
    decStats(heap, bp);
    unlinkBlock(bp);

    /*
     *  Fix the parent pointer of all children
     */
    for (child = bp->children; child; child = child->next) {
        child->parent = newbp;
    }
    newbp->children = bp->children;
    unlockHeap(heap);
    freeBlock(mpr, heap, bp);
    return newPtr;
}


static int getBlockSize(MprBlk *bp) 
{
    MprBlk      *child;
    int         size;
    
    size = bp->size;
    for (child = bp->children; child; child = child->next) {
        size += getBlockSize(child);
    }
    return size;
}


/*
 *  Steal a block from one context and insert in a new context. Ptr is inserted into the Ctx context.
 */
int mprStealBlock(MprCtx ctx, cvoid *ptr)
{
    MprHeap     *heap, *newHeap;
    MprBlk      *bp, *parent, *newParent;
    int         total;

    if (ptr == 0) {
        return 0;
    }
    mprAssert(VALID_CTX(ctx));
    mprAssert(VALID_CTX(ptr));
    bp = GET_BLK(ptr);

#if BLD_FEATURE_MEMORY_VERIFY
    /*
     *  Ensure bp is not a parent of the nominated context.
     */
    for (parent = GET_BLK(ctx); parent; parent = parent->parent) {
        mprAssert(parent != bp);
    }
#endif

    parent = bp->parent;
    mprAssert(VALID_BLK(parent));
    heap = mprGetHeap(parent);
    mprAssert(heap);

    newParent = GET_BLK(ctx);
    mprAssert(VALID_BLK(newParent));
    newHeap = mprGetHeap(newParent);
    mprAssert(newHeap);

    if (heap == newHeap) {
        lockHeap(heap);
        unlinkBlock(bp);
        linkBlock(newParent, bp);
        unlockHeap(heap);
    } else {
        lockHeap(heap);
#if BLD_FEATURE_MEMORY_STATS
        /* Remove all child blocks from the heap */
        total = getBlockSize(bp) - bp->size;
        heap->allocBytes -= total;
        newHeap->allocBytes += total;
#endif
        decStats(heap, bp);
        unlinkBlock(bp);
        unlockHeap(heap);

        lockHeap(newHeap);
        linkBlock(newParent, bp);
        incStats(newHeap, bp);
        unlockHeap(newHeap);
    }
    return 0;
}


/*
 *  Fast unlocked steal within a single heap. WARNING: no locking!
 */
void mprReparentBlock(MprCtx ctx, cvoid *ptr)
{
    MprBlk      *bp;

    bp = GET_BLK(ptr);
    unlinkBlock(bp);
    linkBlock(GET_BLK(ctx), bp);
}


char *_mprStrdup(MprCtx ctx, cchar *str)
{
    char    *newp;
    int     len;

    mprAssert(VALID_CTX(ctx));

    if (str == 0) {
        str = "";
    }
    len = (int) strlen(str) + 1;
    newp = (char*) _mprAlloc(ctx, len);
    if (newp) {
        memcpy(newp, str, len);
    }
    return newp;
}


char *_mprStrndup(MprCtx ctx, cchar *str, uint usize)
{
    char    *newp;
    uint    len;

    mprAssert(VALID_CTX(ctx));

    if (str == 0) {
        str = "";
    }
    len = (int) strlen(str) + 1;
    len = min(len, usize);
    newp = (char*) _mprAlloc(ctx, len);
    if (newp) {
        memcpy(newp, str, len);
    }
    return newp;
}


void *_mprMemdup(MprCtx ctx, cvoid *ptr, uint usize)
{
    char    *newp;

    mprAssert(VALID_CTX(ctx));

    newp = (char*) _mprAlloc(ctx, usize);
    if (newp) {
        memcpy(newp, ptr, usize);
    }
    return newp;
}


/*
 *  Allocate a block from a heap. Must be heap locked when called.
 */
MprBlk *_mprAllocBlock(MprCtx ctx, MprHeap *heap, MprBlk *parent, uint usize)
{
    MprBlk      *bp;
    Mpr         *mpr;
    uint        size;
#if BLD_CC_MMU
    MprRegion   *region;
#endif

    size = MPR_ALLOC_ALIGN(MPR_ALLOC_HDR_SIZE + usize);
    usize = size - MPR_ALLOC_HDR_SIZE;
    mpr = mprGetMpr(ctx);

    /*
     *  Check a memory allocation request against configured maximums and redlines. We do this so that 
     *  the application does not need to check the result of every little memory allocation. Rather, an 
     *  application-wide memory allocation failure can be invoked proactively when a memory redline is 
     *  exceeded. It is the application's responsibility to set the red-line value suitable for the system.
     */
    if (parent) {
#if UNUSED
        if (size >= MPR_ALLOC_BIGGEST) {
            /* Block is too big */
            allocException(parent, size, 0);
            return 0;

        } else 
#endif
        if ((size + mpr->alloc.bytesAllocated) > mpr->alloc.maxMemory) {
            /*
             *  Prevent allocation as over the maximum memory limit.
             */
            allocException(parent, size, 0);
            return 0;

        } else if ((size + mpr->alloc.bytesAllocated) > mpr->alloc.redLine) {
            /*
             *  Warn if allocation puts us over the red line. Then continue to grant the request.
             */
            allocException(parent, size, 1);
        }
    }

    lockHeap(heap);
#if BLD_CC_MMU
    if (likely(heap->flags & MPR_ALLOC_ARENA_HEAP)) {
        /*
         *  Allocate a block from an arena heap
         */
        region = heap->region;
        if ((region->nextMem + size) > &region->memory[region->size]) {
            if ((region = createRegion(ctx, heap, size)) == NULL) {
                unlockHeap(heap);
                return 0;
            }
        }
        bp = (MprBlk*) region->nextMem;
        bp->flags = 0;
        region->nextMem += size;

    } else if (likely(heap->flags & MPR_ALLOC_SLAB_HEAP)) {
        /*
         *  Allocate a block from a slab heap
         */
        region = heap->region;
        if ((bp = heap->freeList) != 0) {
            heap->freeList = bp->next;
            heap->reuseCount++;
        } else {
            if ((region->nextMem + size) > &region->memory[region->size]) {
                if ((region = createRegion(ctx, heap, size)) == NULL) {
                    unlockHeap(heap);
                    return 0;
                }
            }
            bp = (MprBlk*) region->nextMem;
            mprAssert(bp);
            region->nextMem += size;
        }
        bp->flags = 0;

    } else if (heap->flags & MPR_ALLOC_PAGE_HEAP) {
        if ((bp = (MprBlk*) mprMapAlloc(mpr, size, MPR_MAP_READ | MPR_MAP_WRITE)) == 0) {
            unlockHeap(heap);
            return 0;
        }
        bp->flags = 0;

    } else {
#endif
        if ((bp = (MprBlk*) allocMemory(size)) == 0) {
            unlockHeap(heap);
            return 0;
        }
        bp->flags = MPR_ALLOC_FROM_MALLOC;
#if BLD_CC_MMU
    }
#endif

    bp->children = 0;
    bp->parent = 0;
    bp->next = 0;
    bp->prev = 0;
    bp->size = size;
    SET_MAGIC(bp);

    if (parent) {
        linkBlock(parent, bp);
        incStats(heap, bp);

        if (heap != (MprHeap*) mpr) {
            mprSpinLock(&mpr->heap.spin);
            mpr->alloc.bytesAllocated += size;
            if (mpr->alloc.bytesAllocated > mpr->alloc.peakAllocated) {
                mpr->alloc.peakAllocated = mpr->alloc.bytesAllocated;
            }
            mprSpinUnlock(&mpr->heap.spin);
        } else {
            mpr->alloc.bytesAllocated += size;
            if (mpr->alloc.bytesAllocated > mpr->alloc.peakAllocated) {
                mpr->alloc.peakAllocated = mpr->alloc.bytesAllocated;
            }
        }
    }
    unlockHeap(heap);

#if BLD_FEATURE_MEMORY_DEBUG
    /*
     *  Catch uninitialized use
     */
    memset(GET_PTR(bp), 0xf7, usize);
    bp->seqno = allocCount++;
    if (bp == stopAlloc || bp->seqno == stopSeqno) {
        mprBreakpoint();
    }
#endif
#if BLD_FEATURE_MONITOR_STACK
    monitorStack(mpr);
#endif
    return bp;
}


/*
 *  Free a block back to a heap
 */
static void freeBlock(Mpr *mpr, MprHeap *heap, MprBlk *bp)
{
    int         size;
#if BLD_CC_MMU
    MprHeap     *hp;
    MprRegion   *region, *next;
#endif

    if (bp->flags & MPR_ALLOC_IS_HEAP && bp != GET_BLK(mpr)) {
#if BLD_CC_MMU
        hp = (MprHeap*) GET_PTR(bp);
        if (hp->depleted) {
            /*
             *  If there are depleted blocks, then the region contained in the heap memory block will be on 
             *  the depleted list. Must not free it here. Also, the region pointer for the original heap 
             *  block does not point to the start of the memory block to free.
             */
            region = hp->depleted;
            while (region) {
                next = region->next;
                if ((char*) region != ((char*) hp + sizeof(MprHeap))) {
                    /*
                     *  Don't free the initial region which is part of the heap (hp) structure
                     */
                    mprMapFree(region, region->vmSize);
                }
                region = next;
            }
            mprMapFree(hp->region, hp->region->vmSize);
        }
        mprMapFree(bp, bp->size);
#else
        freeMemory(bp);
#endif
        return;
    }
    size = bp->size;

    if (heap != (MprHeap*) mpr) {
        mprSpinLock(&mpr->heap.spin);
        mpr->alloc.bytesAllocated -= size;
        mprAssert(mpr->alloc.bytesAllocated >= 0);
        mprSpinUnlock(&mpr->heap.spin);
    } else {
        mpr->alloc.bytesAllocated -= size;
    }

#if BLD_CC_MMU
    if (!(bp->flags & MPR_ALLOC_FROM_MALLOC)) {
        if (heap->flags & MPR_ALLOC_ARENA_HEAP) {
            /*
             *  Just drop the memory. It will be reclaimed when the arena is freed.
             */
#if BLD_FEATURE_MEMORY_DEBUG
            bp->parent = 0;
            bp->next = 0;
            bp->prev = 0;
#endif
            return;

        } else if (heap->flags & MPR_ALLOC_SLAB_HEAP) {
            bp->next = heap->freeList;
            bp->prev = 0;
            bp->parent = 0;
            heap->freeList = bp;
            heap->freeListCount++;
            if (heap->freeListCount > heap->peakFreeListCount) {
                heap->peakFreeListCount = heap->freeListCount;
            }
            return;
        }
    }
#endif
    freeMemory(bp);
}


#if BLD_CC_MMU
/*
 *  Create a new region to satify the request if no memory exists in any depleted regions. 
 */
static MprRegion *createRegion(MprCtx ctx, MprHeap *heap, uint usize)
{
    MprRegion   *region;
    Mpr         *mpr;
    uint        size, regionSize, regionStructSize;

    mpr = mprGetMpr(ctx);

    /*
     *  Scavenge the depleted regions for scraps. We don't expect there to be many of these.
     */
    if (usize < 512) {
        for (region = heap->depleted; region; region = region->next) {
            if ((region->nextMem + usize) < &region->memory[region->size]) {
                return region;
            }
        }
    }

    /*
     *  Each time we grow the heap, double the size of the next region of memory. Use 30MB so we don't double regions
     *  that are just under 32MB.
     */
    if (heap->region->size <= (30 * 1024 * 1024)) {
        regionSize = heap->region->size * 2;
    } else {
        regionSize = heap->region->size;
    }

    regionStructSize = MPR_ALLOC_ALIGN(sizeof(MprRegion));
    size = max(usize, (regionStructSize + regionSize));
    size = MPR_PAGE_ALIGN(size, mpr->alloc.pageSize);
    usize = size - regionStructSize;

    if ((region = (MprRegion*) mprMapAlloc(mpr, size, MPR_MAP_READ | MPR_MAP_WRITE)) == 0) {
        return 0;
    }
    region->memory = (char*) region + regionStructSize;
    region->nextMem = region->memory;
    region->vmSize = size;
    region->size = usize;

    /*
     *  Move old region to depleted and install new region as the current heap region
     */
    heap->region->next = heap->depleted;
    heap->depleted = heap->region;
    heap->region = region;

    return region;
}
#endif


static void linkBlock(MprBlk *parent, MprBlk *bp)
{
#if BLD_FEATURE_MEMORY_VERIFY
    MprBlk      *sibling;

    /*
     *  Test that bp is not already in the list
     */
    mprAssert(bp != parent);
    for (sibling = parent->children; sibling; sibling = sibling->next) {
        mprAssert(sibling != bp);
    }
#endif

    /*
     *  Add to the front of the children
     */
    bp->parent = parent;
    if (parent->children) {
        parent->children->prev = bp;
    }
    bp->next = parent->children;
    parent->children = bp;
    bp->prev = 0;
}


static void unlinkBlock(MprBlk *bp)
{
    MprBlk      *parent;

    mprAssert(bp);

    parent = bp->parent;
    if (parent) {
        if (bp->prev) {
            bp->prev->next = bp->next;
        } else {
            parent->children = bp->next;
        }
        if (bp->next) {
            bp->next->prev = bp->prev;
        }
        bp->next = 0;
        bp->prev = 0;
        bp->parent = 0;
    }
}


#if BLD_FEATURE_MEMORY_STATS
static void incStats(MprHeap *heap, MprBlk *bp)
{
    if (unlikely(bp->flags & MPR_ALLOC_IS_HEAP)) {
        heap->reservedBytes += bp->size;
    } else {
        heap->totalAllocCalls++;
        heap->allocBlocks++;
        if (heap->allocBlocks > heap->peakAllocBlocks) {
            heap->peakAllocBlocks = heap->allocBlocks;
        }
        heap->allocBytes += bp->size;
        if (heap->allocBytes > heap->peakAllocBytes) {
            heap->peakAllocBytes = heap->allocBytes;
        }
    }
}


static void decStats(MprHeap *heap, MprBlk *bp)
{
    mprAssert(bp);

    if (unlikely(bp->flags & MPR_ALLOC_IS_HEAP)) {
        heap->reservedBytes += bp->size;
    } else {
        heap->allocBytes -= bp->size;
        heap->allocBlocks--;
    }
    mprAssert(heap->allocBytes >= 0);
}
#endif


#if BLD_FEATURE_MONITOR_STACK
static void monitorStack(Mpr *mpr)
{
    /*
     *  Monitor stack usage
     */
    int diff = (int) ((char*) mpr->alloc.stackStart - (char*) &diff);
    if (diff < 0) {
        mpr->alloc.peakStack -= diff;
        mpr->alloc.stackStart = (void*) &diff;
        diff = 0;
    }
    if (diff > mpr->alloc.peakStack) {
        mpr->alloc.peakStack = diff;
    }
}
#endif


static void initHeap(MprHeap *heap, cchar *name, bool threadSafe)
{
    heap->name = name;
    heap->region = 0;
    heap->depleted = 0;
    heap->flags = 0;
    heap->objSize = 0;
    heap->freeList = 0;
    heap->freeListCount = 0;
    heap->reuseCount = 0;

#if BLD_FEATURE_MEMORY_STATS
    heap->allocBlocks = 0;
    heap->peakAllocBlocks = 0;
    heap->allocBytes = 0;
    heap->peakAllocBytes = 0;
    heap->totalAllocCalls = 0;
    heap->peakFreeListCount = 0;
#endif

    heap->notifier = 0;
    heap->notifierCtx = 0;

#if BLD_FEATURE_MULTITHREAD
    if (threadSafe) {
        mprInitSpinLock(heap, &heap->spin);
        heap->flags |= MPR_ALLOC_THREAD_SAFE;
    }
#endif
}


/*
 *  Find the heap from which a block has been allocated. Chase up the parent chain.
 */
MprHeap *mprGetHeap(MprBlk *bp)
{
    mprAssert(bp);
    mprAssert(VALID_BLK(bp));

    while (!(bp->flags & MPR_ALLOC_IS_HEAP)) {
        bp = bp->parent;
        mprAssert(bp);
    }
    return (MprHeap*) GET_PTR(bp);
}


void mprSetAllocNotifier(MprCtx ctx, MprAllocNotifier cback)
{
    MprHeap     *heap;

    heap = mprGetHeap(GET_BLK(ctx));
    heap->notifier = cback;
    heap->notifierCtx = ctx;
}


/*
 *  Monitor stack usage. Return true if the stack has grown. Uses no locking and thus yields approximate results.
 */
bool mprStackCheck(MprCtx ptr)
{
    Mpr     *mpr;
    int     size;

    mprAssert(VALID_CTX(ptr));

    mpr = mprGetMpr(ptr);
    size = (int) ((char*) mpr->alloc.stackStart - (char*) &size);
    if (size < 0) {
        mpr->alloc.peakStack -= size;
        mpr->alloc.stackStart = (void*) &size;
        size = 0;
    }
    if (size > mpr->alloc.peakStack) {
        mpr->alloc.peakStack = size;
        return 1;
    }
    return 0;
}


void mprSetAllocLimits(MprCtx ctx, int redLine, int maxMemory)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    if (redLine > 0) {
        mpr->alloc.redLine = redLine;
    }
    if (maxMemory > 0) {
        mpr->alloc.maxMemory = maxMemory;
    }
}


void *mprGetParent(MprCtx ptr)
{
    MprBlk  *bp;

    if (ptr == 0) {
        return 0;
    }

    mprAssert(VALID_CTX(ptr));

    bp = GET_BLK(ptr);
    mprAssert(VALID_BLK(bp));
    mprAssert(bp->parent);

    return GET_PTR(bp->parent);
}


MprAlloc *mprGetAllocStats(MprCtx ctx)
{
    MprAlloc    *ap;

#if LINUX
    struct rusage   rusage;
    char            buf[1024], *cp;
    int             fd, len;

    getrusage(RUSAGE_SELF, &rusage);
    ap = &mprGetMpr(ctx)->alloc;
    ap->rss = rusage.ru_maxrss;

    ap->ram = MAXINT64;
    if ((fd = open("/proc/meminfo", O_RDONLY)) >= 0) {
        if ((len = read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[len] = '\0';
            if ((cp = strstr(buf, "MemTotal:")) != 0) {
                for (; *cp && !isdigit((int) *cp); cp++) {}
                ap->ram = ((int64) atoi(cp) * 1024);
            }
        }
        close(fd);
    }
#elif MACOSX || FREEBSD
    struct rusage   rusage;
    int64           ram, usermem;
    size_t          len;
    int             mib[2];

    getrusage(RUSAGE_SELF, &rusage);
    ap = &mprGetMpr(ctx)->alloc;
    ap->rss = rusage.ru_maxrss;

    mib[0] = CTL_HW;
#if MACOSX
    mib[1] = HW_MEMSIZE;
#else
    mib[1] = HW_PHYSMEM;
#endif
    len = sizeof(ram);
    sysctl(mib, 2, &ram, &len, NULL, 0);
    ap->ram = ram;

    mib[0] = CTL_HW;
    mib[1] = HW_USERMEM;
    len = sizeof(usermem);
    sysctl(mib, 2, &usermem, &len, NULL, 0);
    ap->user = usermem;
#else
    ap = &mprGetMpr(ctx)->alloc;
#endif
    return ap;
}


int64 mprGetUsedMemory(MprCtx ctx)
{
    return mprGetMpr(ctx)->alloc.bytesAllocated;
}


int mprIsValid(MprCtx ptr)
{
    MprBlk  *bp;

    bp = GET_BLK(ptr);
    return (bp && VALID_BLK(bp));
}


#if !BLD_HAS_GLOBAL_MPR || BLD_WIN_LIKE
/*
 *  Get the ultimate block parent
 */
Mpr *mprGetMpr(MprCtx ctx)
{
#if BLD_WIN_LIKE
    /*  Windows can use globalMpr but must have a function to solve linkage issues */
    return (Mpr*) _globalMpr;
#else
    MprBlk  *bp = GET_BLK(ctx);

    while (bp && bp->parent) {
        bp = bp->parent;
    }
    return (Mpr*) GET_PTR(bp);
#endif
}
#endif



bool mprHasAllocError(MprCtx ctx)
{
    MprBlk  *bp;

    bp = GET_BLK(ctx);
    return (bp->flags & MPR_ALLOC_HAS_ERROR) ? 1 : 0;
}


/*
 *  Reset the allocation error flag at this block and all parent blocks
 */
void mprResetAllocError(MprCtx ctx)
{
    MprBlk  *bp;

    bp = GET_BLK(ctx);
    while (bp) {
        bp->flags &= ~MPR_ALLOC_HAS_ERROR;
        bp = bp->parent;
    }
}



/*
 *  Set the allocation error flag at this block and all parent blocks
 */
void mprSetAllocError(MprCtx ctx)
{
    MprBlk  *bp;

    bp = GET_BLK(ctx);
    while (bp) {
        bp->flags |= MPR_ALLOC_HAS_ERROR;
        bp = bp->parent;
    }
}


/*
 *  Called to invoke the memory failure handler on a memory allocation error
 */
static void allocException(MprBlk *parent, uint size, bool granted)
{
    MprHeap     *hp;
    Mpr         *mpr;

    mprAssert(VALID_BLK(parent));

    mpr = mprGetMpr(parent);
    mprSpinLock(&mpr->heap.spin);
    if (mpr->alloc.inAllocException == 0) {
        mpr->alloc.inAllocException = 1;
        mprSpinUnlock(&mpr->heap.spin);

        /*
         *  Notify all the heaps up the chain
         */
        for (hp = mprGetHeap(parent); hp; hp = mprGetHeap(parent)) {
            if (hp->notifier) {
                (hp->notifier)(hp->notifierCtx, size, (int) mpr->alloc.bytesAllocated, granted);
                break;
            }
            parent = parent->parent;
            if (parent == 0) {
                break;
            }
        }
        mpr->alloc.inAllocException = 0;
    } else {
        mprSpinUnlock(&mpr->heap.spin);
    }
}


/*
 *  Handle an allocation error
 */
static void allocError(MprBlk *parent, uint size)
{
    Mpr     *mpr;

    mpr = mprGetMpr(parent);
    mpr->alloc.errors++;
    mprSetAllocError(GET_PTR(parent));
    allocException(parent, size, 0);
}


/*
 *  Get information about the system. Get page size and number of CPUs.
 */
static void sysinit(Mpr *mpr)
{
    MprAlloc    *ap;

    ap = &mpr->alloc;

    ap->numCpu = 1;

#if MACOSX
    #ifdef _SC_NPROCESSORS_ONLN
        ap->numCpu = sysconf(_SC_NPROCESSORS_ONLN);
    #else
        ap->numCpu = 1;
    #endif
    ap->pageSize = sysconf(_SC_PAGESIZE);
#elif SOLARIS
{
    FILE *ptr;
    if  ((ptr = popen("psrinfo -p", "r")) != NULL) {
        fscanf(ptr, "%d", &alloc.numCpu);
        (void) pclose(ptr);
    }
    alloc.pageSize = sysconf(_SC_PAGESIZE);
}
#elif BLD_WIN_LIKE
{
    SYSTEM_INFO     info;

    GetSystemInfo(&info);
    ap->numCpu = info.dwNumberOfProcessors;
    ap->pageSize = info.dwPageSize;

}
#elif FREEBSD
    {
        int     cmd[2];
        size_t  len;

        /*
         *  Get number of CPUs
         */
        cmd[0] = CTL_HW;
        cmd[1] = HW_NCPU;
        len = sizeof(ap->numCpu);
        if (sysctl(cmd, 2, &ap->numCpu, &len, 0, 0) < 0) {
            ap->numCpu = 1;
        }

        /*
         *  Get page size
         */
        ap->pageSize = sysconf(_SC_PAGESIZE);
    }
#elif LINUX
    {
        static const char processor[] = "processor\t:";
        char    c;
        int     fd, col, match;

        fd = open("/proc/cpuinfo", O_RDONLY);
        if (fd < 0) {
            return;
        }
        match = 1;
        for (col = 0; read(fd, &c, 1) == 1; ) {
            if (c == '\n') {
                col = 0;
                match = 1;
            } else {
                if (match && col < (sizeof(processor) - 1)) {
                    if (c != processor[col]) {
                        match = 0;
                    }
                    col++;

                } else if (match) {
                    ap->numCpu++;
                    match = 0;
                }
            }
        }
        --ap->numCpu;
        close(fd);

        /*
         *  Get page size
         */
        ap->pageSize = sysconf(_SC_PAGESIZE);
    }
#else
        ap->pageSize = 4096;
#endif
    if (ap->pageSize <= 0 || ap->pageSize >= (16 * 1024)) {
        ap->pageSize = 4096;
    }
}


int mprGetPageSize(MprCtx ctx)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    return mpr->alloc.pageSize;
}


/*
 *  Virtual memory support. Map virutal memory into the address space and commit.
 */
void *mprMapAlloc(MprCtx ctx, uint size, int mode)
{
    Mpr     *mpr;
    void    *ptr;

    mpr = mprGetMpr(ctx);
    size = MPR_PAGE_ALIGN(size, mpr->alloc.pageSize);

#if BLD_CC_MMU
    /*
     *  Has virtual memory
     */
    #if BLD_UNIX_LIKE
        ptr = mmap(0, size, mode, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (ptr == (void*) -1) {
            ptr = 0;
        }
    #elif BLD_WIN_LIKE
        ptr = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, mapProt(mode));
    #else
        ptr = malloc(size);
    #endif
#else
    /*
     *  No MMU
     */
    ptr = malloc(size);
#endif

    if (ptr == 0) {
        return 0;
    }
    return ptr;
}


void mprMapFree(void *ptr, uint size)
{
#if BLD_CC_MMU
    /*
     *  Has virtual memory
     */
    #if BLD_UNIX_LIKE
        if (munmap(ptr, size) != 0) {
            mprAssert(0);
        }
    #elif BLD_WIN_LIKE
        VirtualFree(ptr, 0, MEM_RELEASE);
    #else
        free(ptr);
    #endif
#else
    /*
     *  Has no MMU
     */
    free(ptr);
#endif
}


#if BLD_WIN_LIKE
static int mapProt(int flags)
{
    if (flags & MPR_MAP_EXECUTE) {
        return PAGE_EXECUTE_READWRITE;
    } else if (flags & MPR_MAP_WRITE) {
        return PAGE_READWRITE;
    }
    return PAGE_READONLY;
}
#endif


/*
 *  Actually allocate memory. Just use ordinary malloc. Arenas and slabs will use MapAlloc instead.
 */
static void *allocMemory(uint size)
{
    return malloc(size);
}


static void freeMemory(MprBlk *bp)
{
#if BLD_FEATURE_MEMORY_DEBUG
    int     size;
    
    /*
     *  Free with unique signature to catch block-reuse
     */
    size = bp->size;
    memset(bp, 0xF1, size);
#endif
    free(bp);
}


void mprValidateBlock(MprCtx ctx)
{
#if BLD_FEATURE_MEMORY_DEBUG
    Mpr         *mpr;
    MprBlk      *bp, *parent, *sibling, *child;

    mprAssert(VALID_CTX(ctx));

    bp = GET_BLK(ctx);
    mpr = mprGetMpr(ctx);

    if (bp == GET_BLK(mpr)) {
        return;
    }

    mprAssert(bp->parent);
    mprAssert(VALID_BLK(bp->parent));
    parent = bp->parent;

    /*
     *  Find this block in the parent chain
     */
    for (sibling = parent->children; sibling; sibling = sibling->next) {
        mprAssert(VALID_BLK(sibling));
        mprAssert(sibling != parent);
        mprAssert(sibling->parent == parent);
        if (sibling->children) {
            mprAssert(VALID_BLK(sibling->children));
        }
        if (sibling == bp) {
            break;
        }
    }
    mprAssert(sibling);

    /*
     *  Check the rest of the siblings
     */
    if (sibling) {
        for (sibling = sibling->next; sibling; sibling = sibling->next) {
            mprAssert(VALID_BLK(sibling));
            mprAssert(sibling != parent);
            mprAssert(sibling->parent == parent);
            if (sibling->children) {
                mprAssert(VALID_BLK(sibling->children));
            }
            mprAssert(sibling != bp);
        }
    }

    /*
     *  Validate children (recursively)
     */
    for (child = bp->children; child; child = child->next) {
        mprAssert(child != bp);
        mprValidateBlock(GET_PTR(child));
    }
#endif
}


#if BLD_FEATURE_MEMORY_STATS

#define percent(a,b) ((a / 1000) * 100 / (b / 1000))

/*
 *  Traverse all blocks and look for heaps
 */
static void printMprHeaps(MprCtx ctx)
{
    MprAlloc    *ap;
    MprBlk      *bp, *child;
    MprHeap     *heap;
    MprRegion   *region;
    cchar       *kind;
    int         available, total, remaining;

    bp = MPR_GET_BLK(ctx);

    if (bp->size & MPR_ALLOC_IS_HEAP) {
        ap = mprGetAllocStats(ctx);
        heap = (MprHeap*) ctx;
        if (heap->flags & MPR_ALLOC_PAGE_HEAP) {
            kind = "page";
        } else if (heap->flags & MPR_ALLOC_ARENA_HEAP) {
            kind = "arena";
        } else if (heap->flags & MPR_ALLOC_SLAB_HEAP) {
            kind = "slab";
        } else {
            kind = "general";
        }
        mprLog(ctx, 0, "\n    Heap                     %10s (%s)",       heap->name, kind);

        available = 0;
        total = 0;
        for (region = heap->depleted; region; region = region->next) {
            available += (region->size - (region->nextMem - region->memory));
            total += region->size;
        }
        remaining = 0;
        if (heap->region) {
            total += heap->region->size;
            remaining = (region->size - (region->nextMem - region->memory));
        }

        mprLog(ctx, 0, "    Allocated memory         %,10d K",          heap->allocBytes / 1024);
        mprLog(ctx, 0, "    Peak heap memory         %,10d K",          heap->peakAllocBytes / 1024);
        mprLog(ctx, 0, "    Allocated blocks         %,10d",            heap->allocBlocks);
        mprLog(ctx, 0, "    Peak heap blocks         %,10d",            heap->peakAllocBlocks);
        mprLog(ctx, 0, "    Alloc calls              %,10d",            heap->totalAllocCalls);

        if (heap->flags & (MPR_ALLOC_PAGE_HEAP | MPR_ALLOC_ARENA_HEAP | MPR_ALLOC_SLAB_HEAP)) {
            mprLog(ctx, 0, "    Heap Regions             %,10d K",      total / 1024);
            mprLog(ctx, 0, "    Depleted regions         %,10d K",      available / 1024);
            if (heap->region) {
                mprLog(ctx, 0, "    Unallocated memory       %,10d K",  remaining / 1024);
            }            
        }
            
        if (heap->flags & MPR_ALLOC_PAGE_HEAP) {
            mprLog(ctx, 0, "    Page size                %,10d",         ap->pageSize);

        } else if (heap->flags & MPR_ALLOC_ARENA_HEAP) {

        } else if (heap->flags & MPR_ALLOC_SLAB_HEAP) {
            mprLog(ctx, 0, "    Heap object size         %,10d bytes",   heap->objSize);
            mprLog(ctx, 0, "    Heap free list count     %,10d",         heap->freeListCount);
            mprLog(ctx, 0, "    Heap peak free list      %,10d",         heap->peakFreeListCount);
            mprLog(ctx, 0, "    Heap reuse count         %,10d",         heap->reuseCount);
        }
    }
    for (child = bp->children; child; child = child->next) {
        printMprHeaps(MPR_GET_PTR(child));
    }
}
#endif


void mprPrintAllocReport(MprCtx ctx, cchar *msg)
{
#if BLD_FEATURE_MEMORY_STATS
    MprAlloc    *ap;

    ap = &mprGetMpr(ctx)->alloc;

    /*
     *  MPR stats
     */
    mprLog(ctx, 0, "\n\n\nMPR Memory Report %s", msg);
    mprLog(ctx, 0, "------------------------------------------------------------------------------------------\n");
    mprLog(ctx, 0, "  Current heap memory  %,14d K",              ap->bytesAllocated / 1024);
    mprLog(ctx, 0, "  Peak heap memory     %,14d K",              ap->peakAllocated / 1024);
    mprLog(ctx, 0, "  Peak stack size      %,14d K",              ap->peakStack / 1024);
    mprLog(ctx, 0, "  Allocation errors    %,14d",                ap->errors);
    
    /*
     *  Limits
     */
    mprLog(ctx, 0, "  Memory limit         %,14d MB (%d %%)",    ap->maxMemory / (1024 * 1024), 
           percent(ap->bytesAllocated, ap->maxMemory));
    mprLog(ctx, 0, "  Memory redline       %,14d MB (%d %%)",    ap->redLine / (1024 * 1024), 
           percent(ap->bytesAllocated, ap->redLine));

    /*
     *  Heaps
     */
    mprLog(ctx, 0, "\n  Heaps");
    mprLog(ctx, 0, "  -----");
    printMprHeaps(ctx);
#endif /* BLD_FEATURE_MEMORY_STATS */
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

/************************************************************************/
/*
 *  End of file "../src/mprAlloc.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprAsyncSelectWait.c"
 */
/************************************************************************/

/**
 *  mprAsyncSelectWait.c - Wait for I/O on Windows.
 *
 *  This module provides io management for sockets on Windows like systems. 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_WIN_LIKE && !WINCE


static LRESULT msgProc(HWND hwnd, uint msg, uint wp, long lp);


int mprInitSelectWait(MprWaitService *ws)
{   
    mprGetMpr(ws)->waitService = ws;
    mprInitWindow(mprGetMpr(ws)->waitService);
    return 0;
}


/*
 *  Wait for I/O on a single descriptor. Return the number of I/O events found. Mask is the events of interest.
 *  Timeout is in milliseconds.
 */
int mprWaitForSingleIO(MprCtx ctx, int fd, int desiredMask, int timeout)
{
    HANDLE      h;
    int         winMask;

    winMask = 0;
    if (desiredMask & MPR_READABLE) {
        winMask |= /* FD_CONNECT | */ FD_CLOSE | FD_READ;
    }
    if (desiredMask & MPR_WRITABLE) {
        winMask |= FD_WRITE;
    }
    h = CreateEvent(NULL, FALSE, FALSE, "mprWaitForSingleIO");
    WSAEventSelect(fd, h, winMask);
    if (WaitForSingleObject(h, timeout) == WAIT_OBJECT_0) {
        CloseHandle(h);
        return MPR_READABLE | MPR_WRITABLE;
    }
    CloseHandle(h);
    return 0;
}


/*
 *  Wait for I/O on all registered descriptors. Timeout is in milliseconds. Return the number of events serviced.
 */
int mprWaitForIO(MprWaitService *ws, int timeout)
{
    MSG             msg;
    int             count, rc;

    mprAssert(ws->hwnd);

#if BLD_DEBUG
    if (mprGetDebugMode(ws) && timeout > 30000) {
        timeout = 30000;
    }
#endif
    rc = SetTimer(ws->hwnd, 0, timeout, NULL);
    mprAssert(rc != 0);

    count = 0;
    if (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        count++;
    } else {
        mprTerminate(ws, 1);
    }
    return count;
}


void mprServiceWinIO(MprWaitService *ws, int sockFd, int winMask)
{
    MprWaitHandler      *wp;
    int                 index, mask;

    mprLock(ws->mutex);
    ws->flags &= ~MPR_BREAK_REQUESTED;

    for (index = 0; (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &index)) != 0; ) {
        if (wp->fd == sockFd) {
            break;
        }
    }
    if (wp == 0) {
        /*
         *  If the server forcibly closed the socket, we may still get a read event. Just ignore it.
         */
        mprUnlock(ws->mutex);
        return;
    }

    /*
     *  disableMask will be zero if we are already servicing an event
     */
    mask = wp->desiredMask & wp->disableMask;
    if (mask == 0 || wp->inUse > 0) {
        /*
         *  Already have an event scheduled so we must not schedule another yet. We should have disabled events, 
         *  but a message may already be in the message queue.
         */
        mprUnlock(ws->mutex);
        return;
    }

    /*
     *  Mask values: READ==1, WRITE=2, ACCEPT=8, CONNECT=10, CLOSE=20
     */
    wp->presentMask = 0;
    if (winMask & (FD_READ | FD_ACCEPT | FD_CLOSE)) {
        wp->presentMask |= MPR_READABLE;
    }
    if (winMask & (FD_WRITE | FD_CONNECT)) {
        wp->presentMask |= MPR_WRITABLE;
    }

    if (wp->presentMask) {
#if BLD_FEATURE_MULTITHREAD
        wp->disableMask = 0;
        ws->maskGeneration++;
        wp->inUse++;
#endif
        mprUnlock(ws->mutex);
        mprInvokeWaitCallback(wp);
    } else {
        mprUnlock(ws->mutex);
    }
}


#if BLD_FEATURE_MULTITHREAD
/*
 *  Wake the wait service
 */
void mprWakeOsWaitService(MprCtx ctx)
{
    MprWaitService  *ws;
   
    ws = mprGetMpr(ctx)->waitService;
    mprLock(ws->mutex);
    if (!(ws->flags & MPR_BREAK_REQUESTED)) {
        ws->flags |= MPR_BREAK_REQUESTED;
        if (ws->hwnd) {
            PostMessage(ws->hwnd, WM_NULL, 0, 0L);
        }
    }
    mprUnlock(ws->mutex);
}
#endif


/*
 *  Apply wait handler updates that occurred while the wait handler was in use
 */
void mprUpdateWaitHandler(MprWaitHandler *wp, bool wakeup)
{
    MprWaitService  *ws;
    int             eligible, winMask;

    if (!wp->inUse && wp->flags & (MPR_WAIT_RECALL_HANDLER | MPR_WAIT_MASK_CHANGED)) {
        ws = wp->waitService;
        if (wp->flags & MPR_WAIT_RECALL_HANDLER) {
            PostMessage(ws->hwnd, ws->socketMessage, wp->fd, FD_READ);
            wp->flags &= ~MPR_WAIT_RECALL_HANDLER;
            return;
        }
        winMask = 0;
        eligible = wp->desiredMask & wp->disableMask;
        if (eligible & MPR_READABLE) {
            winMask |= FD_ACCEPT | FD_CONNECT | FD_CLOSE | FD_READ;
        }
        if (eligible & MPR_WRITABLE) {
            winMask |= FD_WRITE;
        }
        WSAAsyncSelect(wp->fd, ws->hwnd, ws->socketMessage, winMask);
    }
}


/*
 *  Create a default window if the application has not already created one.
 */ 
int mprInitWindow(MprWaitService *ws)
{
    Mpr         *mpr;
    WNDCLASS    wc;
    HWND        hwnd;
    int         rc;

    mpr = mprGetMpr(ws);

    if (ws->hwnd) {
        return 0;
    }

    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground    = (HBRUSH) (COLOR_WINDOW+1);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = 0;
    wc.hIcon            = NULL;
    wc.lpfnWndProc      = (WNDPROC) msgProc;

    wc.lpszMenuName     = wc.lpszClassName = mprGetAppName(mpr);

    rc = RegisterClass(&wc);
    if (rc == 0) {
        mprError(mpr, "Can't register windows class");
        return MPR_ERR_CANT_INITIALIZE;
    }

    hwnd = CreateWindow(mprGetAppName(mpr), mprGetAppTitle(mpr), WS_OVERLAPPED, CW_USEDEFAULT, 0, 0, 0, NULL, NULL, 0, NULL);
    if (!hwnd) {
        mprError(mpr, "Can't create window");
        return -1;
    }
    ws->hwnd = hwnd;
    ws->socketMessage = MPR_SOCKET_MESSAGE;
    return 0;
}


/*
 *  Windows message processing loop for wakeup and socket messages
 */
static LRESULT msgProc(HWND hwnd, uint msg, uint wp, long lp)
{
    Mpr                 *mpr;
    MprWaitService      *ws;
    int                 sock, winMask;

    mpr = mprGetMpr(0);
    ws = mpr->waitService;

    if (msg == WM_DESTROY || msg == WM_QUIT) {
        mprTerminate(mpr, 1);

    } else if (msg && msg == ws->socketMessage) {
        sock = wp;
        winMask = LOWORD(lp);
        mprServiceWinIO(mpr->waitService, sock, winMask);

    } else if (ws->msgCallback) {
        ws->msgCallback(hwnd, msg, wp, lp);

    } else {
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}


void mprSetWinMsgCallback(MprWaitService *ws, MprMsgCallback callback)
{
    ws->msgCallback = callback;
}


#else
void __mprAsyncDummy() {}
#endif /* BLD_WIN_LIKE */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprAsyncSelectWait.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprBrew.c"
 */
/************************************************************************/

/**
 *  mprBrew.c - Brew specific adaptions
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BREW

static void closeFile(MprFile *file);
static int  consoleWrite(MprFile *file, const void *buf, uint count);
static int  brewFileMode(int omode);
static int fileSystemDestructor(MprDiskFileSystem *fs)


MprOsService *mprCreateOsService(MprCtx ctx)
{
    return mprAllocObj(ctx, MprOsService);
}


static int osDestructor(MprOsService *os)
{
    return 0;
}


int mprStartOsService(MprOsService *os)
{
    return 0;
}


void mprStopOsService(MprOsService *os)
{
}


int mprGetRandomBytes(MprCtx ctx, char *buf, int length, int block)
{
    MprTime now;
    int     i;

    for (i = 0; i < length; i++) {
        now = mprGetTime(ctx);
        buf[i] = (uchar) (now >> i);
    }
    return 0;
}


void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
}


void mprSetShell(MprCtx ctx, void *shell)
{
    mprGetMpr(ctx)->shell = shell;
}


void *mprGetShell(MprCtx ctx)
{
    return mprGetMpr(ctx)->shell;
}


void mprSetClassId(MprCtx ctx, uint classId)
{
    mprGetMpr(ctx)->classId = classId;
}


uint mprGetClassId(MprCtx ctx)
{
    return mprGetMpr(ctx)->classId;
}


void mprSetDisplay(MprCtx ctx, void *display)
{
    mprGetMpr(ctx)->display = display;
}


void *mprGetDisplay(MprCtx ctx)
{
    return mprGetMpr(ctx)->display;
}


/*
 *  Sleep. Period given in milliseconds.
 *  WARNING: not a good idea to call this as it will hang the phone !!!!
 */
void mprSleep(MprCtx ctx, int milliseconds)
{
    MprTime     then;

    then = mprGetTime(ctx) + milliseconds;

    while (mprCompareTime(mprGetTime(ctx), then) < 0) {
        ;
    }
}


struct hostent *mprGetHostByName(MprCtx ctx, cchar *name)
{
    return 0;
}


int getpid()
{
    return 0;
}


int isalnum(int c)
{
    return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'));
} 


int isalpha(int c)
{
    return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
} 


int isdigit(int c)
{
    return ('0' <= c && c <= '9');
}
 

int islower(int c)
{
    return ('a' <= c && c <= 'z');
}
 

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}
 

int isupper(int c)
{
    return ('A' <= c && c <= 'Z');
}
 

int isxdigit(int c)
{
    return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}
 

char *strpbrk(cchar *str, cchar *set)
{
    uchar   charMap[32];
    int     i;

    for (i = 0; i < 32; i++) {
        charMap[i] = 0;
    }

    while (*set) {
        charMap[*set >> 0x3] |= (1 << (*set & 0x7));
        set++;
    }

    while (*str) {
        if (charMap[*str >> 3] & (1 << (*str & 7)))
            return (char*) str;
        str++;
    }
    return 0;
} 


uint strspn(cchar *str, cchar *set)
{
    uchar   charMap[32];
    int     i;

    for (i = 0; i < 32; i++) {
        charMap[i] = 0;
    }

    while (*set) {
        charMap[*set >> 0x3] |= (1 << (*set & 0x7));
        set++;
    }

    if (*str) {
        i = 0;
        while (charMap[*str >> 0x3] & (1 << (*str & 0x7))) {
            i++;
            str++;
        }
        return i;
    }

    return 0;
}
 

char *strstr(cchar *str, cchar *subString)
{
    char *cp, *s1, *s2;

    if (subString == 0 || *subString == '\0') {
        return (char*) str;
    }

    for (cp = (char*) str; *cp; cp++) {
        s1 = cp;
        s2 = (char *) subString;

        while (*s1 && *s2 && (*s1 == *s2)) {
            s1++;
            s2++;
        }

        if (*s2 == '\0') {
            return cp;
        }
    }
    return 0;
}


#if !BREWSIM

uint strlen(cchar *str)
{
    return STRLEN(str);
}


void *memset(const void *dest, int c, uint count)
{
    return MEMSET((void*) dest, c, count);
}


int toupper(int c)
{
    if (islower(c)) {
        c = 'A' + c - 'a';
    }
    return c;
}


void *memcpy(void *dest, const void *src, uint count)
{
    return MEMCPY(dest, src, count);
}


/*
 *  Copy memory supporting overlapping regions
 */

void *memmove(void *destPtr, const void *srcPtr, uint count)
{
    char    *dest, *src;

    dest = (char*) destPtr;
    src = (char*) srcPtr;

    if (dest <= src || dest >= &src[count]) {
        /*
         *  Disjoint
         */
        while (count--) {
            *dest++ = *src++;
        }

    } else {
        /*
         * Overlapping region
         */
        dest = &dest[count - 1];
        src = &src[count - 1];

        while (count--) {
            *dest-- = *src--;
        }
    }
    return destPtr;
}


char *strrchr(cchar *str, int c)
{
    return STRRCHR(str, c);
}


char *strcat(char *dest, cchar *src)
{
    return STRCAT(dest, src);
}


int strcmp(cchar *s1, cchar *s2)
{
    return STRCMP(s1, s2);
}


int strncmp(cchar *s1, cchar *s2, uint count)
{
    return STRNCMP(s1, s2, count);
}


char *strcpy(char *dest, cchar *src)
{
    return STRCPY(dest, src);
}


char *strncpy(char *dest, cchar *src, uint count)
{
    return STRNCPY(dest, src, count);
}


char *strchr(cchar *str, int c)
{
    return STRCHR(str, c);
}


int atoi(cchar *str)
{
    return ATOI(str);
}


int tolower(int c)
{
    if (isupper(c)) {
        c = 'a' + c - 'A';
    }
    return c;
}


void *malloc(uint size)
{
    void    *ptr;
    ptr = MALLOC(size);
    if (ptr == 0) {
        mprAssert(0);
    }
    return ptr; 
}


void *realloc(void *ptr, uint size)
{
    void    *newPtr;

    newPtr = REALLOC(ptr, size);
    if (newPtr == 0) {
        mprAssert(0);
    }
    return newPtr;
}


void free(void *ptr)
{
    if (ptr) {
        FREE(ptr);
    }
}
#endif /* ! BREWSIM */


static MprFile *openFile(MprCtx ctx, MprFileSystem *fileSystem, cchar *path, int omode, int perms)
{
    MprBrewFileSystem   *bfs;
    MprFile             *file;

    mprAssert(path && *path);

    bfs = (MprBrewFileSystem*) fileSystem;
    file = mprAllocObjWithDestructorZeroed(ctx, MprFile, closeFile);
    if (file == 0) {
        mprAssert(file);
        return 0;
    }
    file->mode = omode;

    if (omode & O_CREAT) {
        IFILEMGR_Remove(fileSystem->fileMgr, path);
    }
    file->fd = IFILEMGR_OpenFile(fileSystem->fileMgr, path, brewFileMode(omode));
    if (file->fd == 0) {
        /* int err = IFILEMGR_GetLastError(fileSystem->fileMgr); */
        return 0;
    }

    return file;
}


static int closeFile(MprFile *file)
{
    mprAssert(file);

    if (file == 0) {
        return;
    }
    mprFlush(file);
    IFILE_Release(file->fd);
    return 0;
}
 

static int readFile(MprFile *file, void *buf, uint size)
{
    mprAssert(file);
    mprAssert(buf);

    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }
    return IFILE_Read(file->fd, buf, size);
}


static int writeFile(MprFile *file, const void *buf, uint count)
{
    mprAssert(file);
    mprAssert(buf);

    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }

    /*
     *  Handle == 1 is for console writes
     */
    if (file->fd == (IFile*) 1) {
        return consoleWrite(file, buf, count);
    }
    return IFILE_Write(file->fd, buf, count);
}


static int seekFile(MprFile *file, int seekType, long distance)
{
    int     type;

    mprAssert(file);

    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }

    if (seekType == SEEK_SET) {
        type = _SEEK_START;
    } else if (seekType == SEEK_END) {
        type = _SEEK_END;
    } else {
        type = _SEEK_CURRENT;
    }
    return IFILE_Seek(file->fd, type, distance);
}


static bool accessPath(MprBrewFileSystem *fileSystem, cchar *path, int omode)
{
    return getPathInfo(fileSystem, path);
}


static int deletePath(MprBrewFileSystem *fileSystem, cchar *path)
{
    FileInfo    info;

    getPathInfo(fileSystem, path, &info);
    if (info.valid && info.isDir) {
        if (IFILEMGR_RmDir(fileSystem->fileMgr, path) == EFAILED) {
            mprError(ctx, "Can't remove directory %s, error %d", path, IFILEMGR_GetLastError(fileSystem->fileMgr));
            return MPR_ERR_CANT_ACCESS;
        }
    } else {
        if (IFILEMGR_Remove(fileSystem->fileMgr, path) == EFAILED) {
            return MPR_ERR_CANT_ACCESS;
        }
    }
    return 0;
}
 

static int makeDir(MprBrewFileSystem *fileSystem, cchar *path, int perms)
{
    if (IFILEMGR_MkDir(fileSystem->fileMgr, path) == EFAILED) {
        mprError(ctx, "Can't make directory %s, error %d", path, IFILEMGR_GetLastError(fileSystem->fileMgr));
        return MPR_ERR_CANT_ACCESS;
    }
    return 0;
}
 

static int getPathInfo(MprBrewFileSystem *fileSystem, cchar *path, MprPath *info)
{
    FileInfo        brewFileInfo;

    mprAssert(path && *path);
    mprAssert(info);

    info->checked = 1;
    info->valid = 0;
    if (IFILEMGR_GetInfo(fileSystem->fileMgr, path, &brewFileInfo) == EFAILED) {
        mprError(ctx, "Can't get file info for %s, error %d", path, 
            IFILEMGR_GetLastError(fileSystem->fileMgr));
        return -1;
    }

    info->size = brewFileInfo.dwSize;
    info->ctime = brewFileInfo.dwCreationDate;
    info->isDir = brewFileInfo.attrib & _FA_DIR;
    info->isReg = brewFileInfo.attrib & _FA_NORMAL;
    info->valid = 1;
    info->checked = 1;

    return 0;
}
 

static int consoleWrite(MprFile *file, const void *writeBuf, uint count)
{
    MprBuf  *bp;
    char    *start, *cp, *end, *np, *buf;
    int     total, bytes;

    mprAssert(file);
    mprAssert(writeBuf);

    /*
     *  Buffer output and flush on a '\n'. This is necesary because 
     *  BREW appends a new line to all calls to DBGPRINTF.
     */
    if (file->buf == 0) {
#if BREWSIM
        file->buf = mprCreateBuf(file, 128, 128);
#else
        file->buf = mprCreateBuf(file, 35, 35);
#endif
    }
    bp = file->buf;

    if (mprGetBufLength(bp) > 0 && mprGetBufSpace(bp) < (int) count) {
        printf(" MP: %s", mprGetBufStart(bp));
        mprFlushBuf(bp);
    }

    total = 0;
    buf = (char*) writeBuf;

    while (count > 0) {
        bytes = mprPutBlockToBuf(bp, buf, count);
        buf += bytes;
        count -= bytes;
        total += bytes;

        /*
         *  Output the line if we find a newline or the line is too long to 
         *  buffer (count > 0).
         */
        if (strchr(mprGetBufStart(bp), '\n') || count > 0) {
            end = cp = mprGetBufEnd(bp);
            start = cp = mprGetBufStart(bp);

            /*
             *  Brew can't handle tabs
             */
            for (; cp < end && *cp; cp++) {
                if (*cp == '\t') {
                    *cp = ' ';
                }
            }

            cp = start;
            for (np = cp; np < end; np++) {
                if (*np == '\n') {
                    *np = '\0';
                    /* BREW appends '\n' */
                    if (count > 0) {
                        printf("_MP: %s", cp);
                    } else {
                        printf(" MP: %s", cp);
                    }
                    cp = np + 1;
                }
            }
            if (cp < np) {
                if (cp == start) {
                    /* Nothing output. Line must be too long */
                    printf("_MP: %s", cp);
                    mprFlushBuf(bp);

                } else if (count > 0) {
                    /* We did output text, but there is more of this line */
                    mprAdjustBufStart(bp, (int) (cp - start));
                    mprCompactBuf(bp);

                } else {
                    printf(" MP: %s", cp);
                    mprFlushBuf(bp);
                }
            } else {
                mprFlushBuf(bp);
            }
        }
    }
    return total;
}


void mprSetFileMgr(MprCtx ctx, void *fileMgr)
{
    MprFileSystem       *fs;

    fs = mprLookupFileSystem(ctx, "/");
    fs->fileMgr = fileMgr;
}


void *mprGetFileMgr(MprCtx ctx)
{
    MprFileSystem       *fs;

    fs = mprLookupFileSystem(ctx, "/");
    return fs->fileMgr;
}


static int brewFileMode(int omode)
{
    uint        mode;

    mode = 0;

    if (omode & (O_RDONLY | O_RDWR)) {
        mode |= _OFM_READ;
    }
    if (omode & (O_RDWR)) {
        mode |= _OFM_READWRITE;
    }
    if (omode & (O_CREAT)) {
        mode |= _OFM_CREATE;
    }
    if (omode & (O_APPEND)) {
        mode |= _OFM_APPEND;
    }
    return mode;
}


int *mprSetBrewFileSystem(MprCtx ctx)
{
    MprBrewFileSystem  *bfs;

    /*
     *  Assume that STDOUT is 1 and STDERR is 2
     */
    bfs->stdOutput = mprAllocObjWithDestructorZeroed(bfs, MprFile);
    bfs->stdError = mprAllocObjWithDestructorZeroed(bfs, MprFile);

    bfs->stdOutput->fd = (IFile*) 1;
    bfs->error->fd = (IFile*) 2;

    bfs->stdOutput->fs = (MprFileSystem*) bfs;
    bfs->stdError->fs = (MprFileSystem*) bfs;

    bfs->stdOutput->mode = O_WRONLY;
    bfs->stdError->mode = O_WRONLY;

    mprAssert(bfs->fileMgr);
    if (ISHELL_CreateInstance(mprGetMpr(bfs)->shell, AEECLSID_FILEMGR, (void**) &bfs->fileMgr) != SUCCESS) {
        mprError(fs, "Can't open file manager.");
        return MPR_ERR_CANT_OPEN;
    }
    return fs;
}


MprBrewFileSystem *mprCreateBrewFileSystem(MprCtx ctx, cchar *path)
{
    MprBrewFileSystem  *bfs;

    bfs = mprAllocObjWithDestructorZeroed(ctx, MprBrewFileSystem, fileSystemDestructor);
    if (bfs == 0) {
        return 0;
    }

    bfs->accessPath = accessPath;
    bfs->deletePath = deletePath;
    bfs->getPathInfo = getPathInfo;
    bfs->makeDir = makeDir;
    bfs->openFile = openFile;
    bfs->closeFile = closeFile;
    bfs->readFile = readFile;
    bfs->seekFile = seekFile;
    bfs->writeFile = writeFile;

    return bfs;
}


static int fileSystemDestructor(MprDiskFileSystem *fs)
{
    IFILEMGR_Release(fs->fileMgr);
    return 0;
}


#else
void __dummyBrew() {}
#endif /* !BREW */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprBrew.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprBuf.c"
 */
/************************************************************************/

/**
 *  mprBuf.c - Dynamic buffer module
 *
 *  This module is not thread-safe for performance. Callers must do their own locking.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Create a new buffer. "maxsize" is the limit to which the buffer can ever grow. -1 means no limit. "initialSize" is 
 *  used to define the amount to increase the size of the buffer each time if it becomes full. (Note: mprGrowBuf() will 
 *  exponentially increase this number for performance.)
 */
MprBuf *mprCreateBuf(MprCtx ctx, int initialSize, int maxSize)
{
    MprBuf      *bp;
    
    if (initialSize <= 0) {
        initialSize = MPR_DEFAULT_ALLOC;
    }
    if ((bp = mprAllocObjZeroed(ctx, MprBuf)) == 0) {
        return 0;
    }
    bp->growBy = MPR_BUFSIZE;
    mprSetBufSize(bp, initialSize, maxSize);
    return bp;
}


/*
 *  Set the current buffer size and maximum size limit.
 */
int mprSetBufSize(MprBuf *bp, int initialSize, int maxSize)
{
    mprAssert(bp);

    if (initialSize <= 0) {
        if (maxSize > 0) {
            bp->maxsize = maxSize;
        }
        return 0;
    }
    if (maxSize > 0 && initialSize > maxSize) {
        initialSize = maxSize;
    }
    mprAssert(initialSize > 0);

    if (bp->data) {
        /*
         *  Buffer already exists
         */
        if (bp->buflen < initialSize) {
            if (mprGrowBuf(bp, initialSize - bp->buflen) < 0) {
                return MPR_ERR_NO_MEMORY;
            }
        }
        bp->maxsize = maxSize;
        return 0;
    }

    /*
     *  New buffer - create storage for the data
     */
    if ((bp->data = mprAlloc(bp, initialSize)) == 0) {
        mprAssert(!MPR_ERR_NO_MEMORY);
        return MPR_ERR_NO_MEMORY;
    }
    bp->growBy = initialSize;
    bp->maxsize = maxSize;
    bp->buflen = initialSize;
    bp->endbuf = &bp->data[bp->buflen];
    bp->start = bp->data;
    bp->end = bp->data;
    *bp->start = '\0';
    return 0;
}


void mprSetBufMax(MprBuf *bp, int max)
{
    bp->maxsize = max;
}


char *mprStealBuf(MprCtx ctx, MprBuf *bp)
{
    char    *str;

    str = (char*) bp->start;

    mprStealBlock(ctx, bp->start);
    bp->start = bp->end = bp->data = bp->endbuf = 0;
    bp->buflen = 0;
    return str;
}


/*
 *  This appends a silent null. It does not count as one of the actual bytes in the buffer
 */
void mprAddNullToBuf(MprBuf *bp)
{
    int     space;

    space = bp->endbuf - bp->end;
    if (space < (int) sizeof(char)) {
        if (mprGrowBuf(bp, 1) < 0) {
            return;
        }
    }
    mprAssert(bp->end < bp->endbuf);
    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
}


void mprAdjustBufEnd(MprBuf *bp, int size)
{
    mprAssert(bp->buflen == (bp->endbuf - bp->data));
    mprAssert(size <= bp->buflen);
    mprAssert((bp->end + size) <= bp->endbuf);

    bp->end += size;
    if (bp->end > bp->endbuf) {
        mprAssert(bp->end <= bp->endbuf);
        bp->end = bp->endbuf;
    }
}


/*
 *  Adjust the start pointer after a user copy
 */
void mprAdjustBufStart(MprBuf *bp, int size)
{
    mprAssert(bp->buflen == (bp->endbuf - bp->data));
    mprAssert(size <= bp->buflen);
    mprAssert((bp->start + size) <= bp->end);

    bp->start += size;
    if (bp->start > bp->end) {
        bp->start = bp->end;
    }
}


void mprFlushBuf(MprBuf *bp)
{
    bp->start = bp->data;
    bp->end = bp->data;
}


int mprGetCharFromBuf(MprBuf *bp)
{
    if (bp->start == bp->end) {
        return -1;
    }
    return (uchar) *bp->start++;
}


int mprGetBlockFromBuf(MprBuf *bp, char *buf, int size)
{
    int     thisLen, bytesRead;

    mprAssert(buf);
    mprAssert(size >= 0);
    mprAssert(bp->buflen == (bp->endbuf - bp->data));

    /*
     *  Get the max bytes in a straight copy
     */
    bytesRead = 0;
    while (size > 0) {
        thisLen = mprGetBufLength(bp);
        thisLen = min(thisLen, size);
        if (thisLen <= 0) {
            break;
        }

        memcpy(buf, bp->start, thisLen);
        buf += thisLen;
        bp->start += thisLen;
        size -= thisLen;
        bytesRead += thisLen;
    }
    return bytesRead;
}


int mprGetBufLength(MprBuf *bp)
{
    return (int) (bp->end - bp->start);
}


int mprGetBufSize(MprBuf *bp)
{
    return bp->buflen;
}


int mprGetBufSpace(MprBuf *bp)
{
    return (int) (bp->endbuf - bp->end);
}


char *mprGetBufOrigin(MprBuf *bp)
{
    return (char*) bp->data;
}


char *mprGetBufStart(MprBuf *bp)
{
    return (char*) bp->start;
}


char *mprGetBufEnd(MprBuf *bp)
{
    return (char*) bp->end;
}


int mprInsertCharToBuf(MprBuf *bp, int c)
{
    if (bp->start == bp->data) {
        return MPR_ERR_BAD_STATE;
    }
    *--bp->start = c;
    return 0;
}


int mprLookAtNextCharInBuf(MprBuf *bp)
{
    if (bp->start == bp->end) {
        return -1;
    }
    return *bp->start;
}


int mprLookAtLastCharInBuf(MprBuf *bp)
{
    if (bp->start == bp->end) {
        return -1;
    }
    return bp->end[-1];
}


int mprPutCharToBuf(MprBuf *bp, int c)
{
    char    *cp;
    int     space;

    mprAssert(bp->buflen == (bp->endbuf - bp->data));

    space = bp->buflen - mprGetBufLength(bp);
    if (space < (int) sizeof(char)) {
        if (mprGrowBuf(bp, 1) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    cp = (char*) bp->end;
    *cp++ = (char) c;
    bp->end = (char*) cp;

    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
    return 1;
}


int mprPutBlockToBuf(MprBuf *bp, cchar *str, int size)
{
    int     thisLen, bytes, space;

    mprAssert(str);
    mprAssert(size >= 0);

    /*
        Add the max we can in one copy
     */
    bytes = 0;
    while (size > 0) {
        space = mprGetBufSpace(bp);
        thisLen = min(space, size);
        if (thisLen <= 0) {
            if (mprGrowBuf(bp, size) < 0) {
                break;
            }
            space = mprGetBufSpace(bp);
            thisLen = min(space, size);
        }

        memcpy(bp->end, str, thisLen);
        str += thisLen;
        bp->end += thisLen;
        size -= thisLen;
        bytes += thisLen;
    }
    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
    return bytes;
}


int mprPutStringToBuf(MprBuf *bp, cchar *str)
{
    if (str) {
        return mprPutBlockToBuf(bp, str, (int) strlen(str));
    }
    return 0;
}


int mprPutSubStringToBuf(MprBuf *bp, cchar *str, int count)
{
    int     len;

    if (str) {
        len = strlen(str);
        len = min(len, count);
        if (len > 0) {
            return mprPutBlockToBuf(bp, str, len);
        }
    }
    return 0;
}


int mprPutPadToBuf(MprBuf *bp, int c, int count)
{
    while (count-- > 0) {
        if (mprPutCharToBuf(bp, c) < 0) {
            return -1;
        }
    }
    return count;
}


int mprPutFmtToBuf(MprBuf *bp, cchar *fmt, ...)
{
    va_list     ap;
    char        *buf;
    int         rc, space;

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    space = mprGetBufSpace(bp);

    /*
     *  Add max that the buffer can grow 
     */
    space += (bp->maxsize - bp->buflen);
    buf = mprVasprintf(bp, space, fmt, ap);
    rc = mprPutStringToBuf(bp, buf);

    mprFree(buf);
    va_end(ap);
    return rc;
}


/*
 *  Grow the buffer. Return 0 if the buffer grows. Increase by the growBy size specified when creating the buffer. 
 */
int mprGrowBuf(MprBuf *bp, int need)
{
    char    *newbuf;
    int     growBy;

    if (bp->maxsize > 0 && bp->buflen >= bp->maxsize) {
        return MPR_ERR_TOO_MANY;
    }
    if (bp->start > bp->end) {
        mprCompactBuf(bp);
    }
    if (need > 0) {
        growBy = max(bp->growBy, need);
    } else {
        growBy = bp->growBy;
    }
    if ((newbuf = mprAlloc(bp, bp->buflen + growBy)) == 0) {
        mprAssert(!MPR_ERR_NO_MEMORY);
        return MPR_ERR_NO_MEMORY;
    }
    if (bp->data) {
        memcpy(newbuf, bp->data, bp->buflen);
        mprFree(bp->data);
    }

    bp->buflen += growBy;
    bp->end = newbuf + (bp->end - bp->data);
    bp->start = newbuf + (bp->start - bp->data);
    bp->data = newbuf;
    bp->endbuf = &bp->data[bp->buflen];

    /*
     *  Increase growBy to reduce overhead
     */
    if (bp->maxsize > 0) {
        if ((bp->buflen + (bp->growBy * 2)) > bp->maxsize) {
            bp->growBy = min(bp->maxsize - bp->buflen, bp->growBy * 2);
        }
    } else {
        if ((bp->buflen + (bp->growBy * 2)) > bp->maxsize) {
            bp->growBy = min(bp->buflen, bp->growBy * 2);
        }
    }
    return 0;
}


/*
 *  Add a number to the buffer (always null terminated).
 */
int mprPutIntToBuf(MprBuf *bp, int i)
{
    char    numBuf[16];
    int     rc;

    mprItoa(numBuf, sizeof(numBuf), i, 10);
    rc = mprPutStringToBuf(bp, numBuf);

    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }

    return rc;
}


void mprCompactBuf(MprBuf *bp)
{
    if (mprGetBufLength(bp) == 0) {
        mprFlushBuf(bp);
        return;
    }
    if (bp->start > bp->data) {
        memmove(bp->data, bp->start, (bp->end - bp->start));
        bp->end -= (bp->start - bp->data);
        bp->start = bp->data;
    }
}


MprBufProc mprGetBufRefillProc(MprBuf *bp) 
{
    return bp->refillProc;
}


void mprSetBufRefillProc(MprBuf *bp, MprBufProc fn, void *arg)
{ 
    bp->refillProc = fn; 
    bp->refillArg = arg; 
}


int mprRefillBuf(MprBuf *bp) 
{ 
    return (bp->refillProc) ? (bp->refillProc)(bp, bp->refillArg) : 0; 
}


void mprResetBufIfEmpty(MprBuf *bp)
{
    if (mprGetBufLength(bp) == 0) {
        mprFlushBuf(bp);
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprBuf.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprCmd.c"
 */
/************************************************************************/

/* 
 *  mprCmd.c - Run external commands
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_CMD

static void closeFiles(MprCmd *cmd);
static int  cmdDestructor(MprCmd *cmd);
static int  makeChannel(MprCmd *cmd, int index);
static void resetCmd(MprCmd *cmd);
static int sanitizeArgs(MprCmd *cmd, int argc, char **argv, char **env);
static int  startProcess(MprCmd *cmd);
static int stdoutCallback(MprCmd *cmd, int mask);
static int stderrCallback(MprCmd *cmd, int mask);

#if BLD_UNIX_LIKE
static char **fixenv(MprCmd *cmd);
#endif
#if VXWORKS
typedef int (*MprCmdTaskFn)(int argc, char **argv, char **envp);
static void cmdTaskEntry(char *program, MprCmdTaskFn entry, int cmdArg);
#endif


MprCmdService *mprCreateCmdService(Mpr *mpr)
{
    MprCmdService   *cs;

    cs = (MprCmdService*) mprAllocObjWithDestructorZeroed(mpr, MprCmd, cmdDestructor);
    if (cs == 0) {
        return 0;
    }
    cs->cmds = mprCreateList(cs);
#if BLD_FEATURE_MULTITHREAD
    cs->mutex = mprCreateLock(cs);
#endif
    return cs;
}


MprCmd *mprCreateCmd(MprCtx ctx)
{
    MprCmdService   *cs;
    MprCmd          *cmd;
    MprCmdFile      *files;
    int             i;
    
    cmd = mprAllocObjWithDestructorZeroed(ctx, MprCmd, cmdDestructor);
    if (cmd == 0) {
        return 0;
    }
    cmd->completeCond = mprCreateCond(cmd);
    cmd->timeoutPeriod = MPR_TIMEOUT_CMD;
    cmd->timestamp = mprGetTime(cmd);
    cmd->forkCallback = (MprForkCallback) closeFiles;

#if VXWORKS
    cmd->startCond = semCCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    cmd->exitCond = semCCreate(SEM_Q_PRIORITY, SEM_EMPTY);
#endif
    files = cmd->files;
    for (i = 0; i < MPR_CMD_MAX_PIPE; i++) {
        files[i].clientFd = -1;
        files[i].fd = -1;
    }
#if BLD_FEATURE_MULTITHREAD
    cmd->mutex = mprCreateLock(cmd);
#endif
    cs = mprGetMpr(ctx)->cmdService;
    mprLock(cs->mutex);
    mprAddItem(cs->cmds, cmd);
    mprUnlock(cs->mutex);
    return cmd;
}


#if VXWORKS
static void vxCmdDestructor(MprCmd *cmd)
{
    MprCmdFile      *files;
    int             i;

    if (cmd->startCond) {
        semDelete(cmd->startCond);
    }
    if (cmd->exitCond) {
        semDelete(cmd->exitCond);
    }
    files = cmd->files;
    for (i = 0; i < MPR_CMD_MAX_PIPE; i++) {
        if (files[i].name) {
            DEV_HDR *dev;
#if _WRS_VXWORKS_MAJOR >= 6
            cchar   *tail;
#else
            char    *tail;
#endif
            if ((dev = iosDevFind(files[i].name, &tail)) != NULL) {
                iosDevDelete(dev);
            }
        }
    }
}
#endif


static int cmdDestructor(MprCmd *cmd)
{
    MprCmdService   *cs;

    resetCmd(cmd);
#if VXWORKS
    vxCmdDestructor(cmd);
#endif
    cs = mprGetMpr(cmd)->cmdService;
    mprLock(cs->mutex);
    mprRemoveItem(cs->cmds, cmd);
    mprUnlock(cs->mutex);
    return 0;
}


static void resetCmd(MprCmd *cmd)
{
    MprCmdFile      *files;
    int             i;

    files = cmd->files;
    for (i = 0; i < MPR_CMD_MAX_PIPE; i++) {
        if (cmd->handlers[i]) {
            mprFree(cmd->handlers[i]);
            cmd->handlers[i] = 0;
        }
        if (files[i].clientFd >= 0) {
            close(files[i].clientFd);
            files[i].clientFd = -1;
        }
        if (files[i].fd >= 0) {
            close(files[i].fd);
            files[i].fd = -1;
        }
    }
    cmd->eofCount = 0;
    cmd->status = -1;
    mprResetCond(cmd->completeCond);

    if (cmd->pid && !(cmd->flags & MPR_CMD_DETACH)) {
        mprStopCmd(cmd);
        mprReapCmd(cmd, 0);
        mprSignalCond(cmd->completeCond);
    }
}


void mprDisconnectCmd(MprCmd *cmd)
{
    MprCmdFile      *files;
    int             i;

    files = cmd->files;

    lock(cmd);
    for (i = 0; i < MPR_CMD_MAX_PIPE; i++) {
        if (cmd->handlers[i]) {
            mprDisconnectWaitHandler(cmd->handlers[i]);
        }
    }
    unlock(cmd);
}


/*
 *  Close a command channel. Must be able to be called redundantly.
 */
void mprCloseCmdFd(MprCmd *cmd, int channel)
{
    mprAssert(0 <= channel && channel <= MPR_CMD_MAX_PIPE);

    /*
     *  Disconnect but don't free. This prevents some races with callbacks.
     */
    if (cmd->handlers[channel]) {
        mprDisconnectWaitHandler(cmd->handlers[channel]);
    }

    if (cmd->files[channel].fd != -1) {
        close(cmd->files[channel].fd);
        cmd->files[channel].fd = -1;
#if BLD_WIN_LIKE
        cmd->files[channel].handle = 0;
#endif
        if (channel != MPR_CMD_STDIN) {
            if (++cmd->eofCount >= cmd->requiredEof) {
                mprReapCmd(cmd, MPR_TIMEOUT_STOP_TASK);
                mprSignalCond(cmd->completeCond);
            }
        }
    }
}


int mprIsCmdComplete(MprCmd *cmd)
{
    return cmd->eofCount >= cmd->requiredEof;
}


/*
 *  Default callback routine for the mprRunCmd routines. Uses may supply their own callback instead of this routine. 
 *  The callback is run whenever there is I/O to read/write to the CGI gateway.
 */
static int cmdCallback(MprCmd *cmd, int channel, void *data)
{
    MprBuf      *buf;
    int         len, space;

    /*
     *  Note: stdin, stdout and stderr are named from the client's perspective
     */
    buf = 0;
    switch (channel) {
    case MPR_CMD_STDIN:
        return 0;
    case MPR_CMD_STDOUT:
        buf = cmd->stdoutBuf;
        break;
    case MPR_CMD_STDERR:
        buf = cmd->stderrBuf;
        break;
    }

    /*
     *  Read and aggregate the result into a single string
     */
    space = mprGetBufSpace(buf);
    if (space < (MPR_BUFSIZE / 4)) {
        if (mprGrowBuf(buf, MPR_BUFSIZE) < 0) {
            mprCloseCmdFd(cmd, channel);
            return 0;
        }
        space = mprGetBufSpace(buf);
    }

    len = mprReadCmdPipe(cmd, channel, mprGetBufEnd(buf), space);
    if (len <= 0) {
        if (len == 0 || (len < 0 && !(errno == EAGAIN || EWOULDBLOCK))) {
            if (channel == MPR_CMD_STDOUT && cmd->flags & MPR_CMD_ERR) {
                /*
                 *  Now that stdout is complete, enable stderr to receive an EOF or any error output.
                 *  This is serialized to eliminate both stdin and stdout events on different threads at the same time.
                 *  Do before closing as the stderr event may come on another thread and we want to ensure avoid locking.
                 */
                mprCloseCmdFd(cmd, channel);
            } else {
                mprCloseCmdFd(cmd, channel);
            }
            return 0;
        }
    } else {
        mprAdjustBufEnd(buf, len);
    }
    return 0;
}


/*
 *  Run a simple blocking command. See arg usage below in mprRunCmdV.
 */
int mprRunCmd(MprCmd *cmd, cchar *command, char **out, char **err, int flags)
{
    char    **argv;
    int     argc;

    if (mprMakeArgv(cmd, NULL, command, &argc, &argv) < 0 || argv == 0) {
        return 0;
    }
    return mprRunCmdV(cmd, argc, argv, out, err, flags);
}


/*
 *  This routine runs a command and waits for its completion. Stdoutput and Stderr are returned in *out and *err 
 *  respectively. The command returns the exit status of the command.
 *  Valid flags are:
 *      MPR_CMD_NEW_SESSION     Create a new session on Unix
 *      MPR_CMD_SHOW            Show the commands window on Windows
 *      MPR_CMD_IN              Connect to stdin
 */
int mprRunCmdV(MprCmd *cmd, int argc, char **argv, char **out, char **err, int flags)
{
    int     rc, status;

    if (err) {
        *err = 0;
        flags |= MPR_CMD_ERR;
    } else {
        flags &= ~MPR_CMD_ERR;
    }
    if (out) {
        *out = 0;
        flags |= MPR_CMD_OUT;
    } else {
        flags &= ~MPR_CMD_OUT;
    }

    if (flags & MPR_CMD_OUT) {
        mprFree(cmd->stdoutBuf);
        cmd->stdoutBuf = mprCreateBuf(cmd, MPR_BUFSIZE, -1);
    }
    if (flags & MPR_CMD_ERR) {
        mprFree(cmd->stderrBuf);
        cmd->stderrBuf = mprCreateBuf(cmd, MPR_BUFSIZE, -1);
    }
    mprSetCmdCallback(cmd, cmdCallback, NULL);
    lock(cmd);

    rc = mprStartCmd(cmd, argc, argv, NULL, flags);

    /*
     *  Close the pipe connected to the client's stdin
     */
    if (cmd->files[MPR_CMD_STDIN].fd >= 0) {
        mprCloseCmdFd(cmd, MPR_CMD_STDIN);
    }
    if (rc < 0) {
        if (err) {
            if (rc == MPR_ERR_CANT_ACCESS) {
                *err = mprAsprintf(cmd, -1, "Can't access command %s", cmd->program);
            } else if (MPR_ERR_CANT_OPEN) {
                *err = mprAsprintf(cmd, -1, "Can't open standard I/O for command %s", cmd->program);
            } else if (rc == MPR_ERR_CANT_CREATE) {
                *err = mprAsprintf(cmd, -1, "Can't create process for %s", cmd->program);
            }
        }
        unlock(cmd);
        return rc;
    }
    if (cmd->flags & MPR_CMD_DETACH) {
        unlock(cmd);
        return 0;
    }
    unlock(cmd);
    if (mprWaitForCmd(cmd, -1) < 0) {
        return MPR_ERR_NOT_READY;
    }
    lock(cmd);

    if (mprGetCmdExitStatus(cmd, &status) < 0) {
        unlock(cmd);
        return MPR_ERR;
    }
    if (err && flags & MPR_CMD_ERR) {
        mprAddNullToBuf(cmd->stderrBuf);
        *err = mprGetBufStart(cmd->stderrBuf);
    }
    if (out && flags & MPR_CMD_OUT) {
        mprAddNullToBuf(cmd->stdoutBuf);
        *out = mprGetBufStart(cmd->stdoutBuf);
    }
    unlock(cmd);
    return status;
}


/*
 *  Start the command to run (stdIn and stdOut are named from the client's perspective). This is the lower-level way to 
 *  run a command. The caller needs to do code like mprRunCmd() themselves to wait for completion and to send/receive data.
 *  The routine does not wait. Callers must call mprWaitForCmd to wait for the command to complete.
 */
int mprStartCmd(MprCmd *cmd, int argc, char **argv, char **envp, int flags)
{
    MprPath     info;
    char        *program;
    int         rc;

    mprAssert(argv);
    mprAssert(argc > 0);

    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return MPR_ERR_BAD_STATE;
    }
    resetCmd(cmd);
    program = argv[0];
    cmd->program = program;
    cmd->flags = flags;

    if (sanitizeArgs(cmd, argc, argv, envp) < 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (access(program, X_OK) < 0) {
        program = mprJoinPathExt(cmd, program, BLD_EXE);
        if (access(program, X_OK) < 0) {
            mprLog(cmd, 1, "cmd: can't access %s, errno %d", program, mprGetOsError());
            return MPR_ERR_CANT_ACCESS;
        }
    }
    if (mprGetPathInfo(cmd, program, &info) == 0 && info.isDir) {
        mprLog(cmd, 1, "cmd: program \"%s\", is a directory", program);
        return MPR_ERR_CANT_ACCESS;
    }

#if CYGWIN
    mprGlobalLock(cmd);
#endif
    if (mprMakeCmdIO(cmd) < 0) {
#if CYGWIN
        mprGlobalUnlock(cmd);
#endif
        return MPR_ERR_CANT_OPEN;
    }

    /*
     *  Determine how many end-of-files will be seen when the child dies
     */
    cmd->requiredEof = 0;
    if (cmd->flags & MPR_CMD_OUT) {
        cmd->requiredEof++;
    }
    if (cmd->flags & MPR_CMD_ERR) {
        cmd->requiredEof++;
    }

#if UNUSED && KEEP && NON_BLOCKING
#if BLD_UNIX_LIKE || VXWORKS
    {
        int     stdinFd, stdoutFd, stderrFd, nonBlock;
      
        stdinFd = cmd->files[MPR_CMD_STDIN].fd; 
        stdoutFd = cmd->files[MPR_CMD_STDOUT].fd; 
        stderrFd = cmd->files[MPR_CMD_STDERR].fd; 
        nonBlock = 1;

        /*
         *  Put the stdout and stderr into non-blocking mode. Windows can't do this because both ends of the pipe
         *  share the same blocking mode (Ugh!).
         */
#if VXWORKS
        if (stdoutFd >= 0) {
            ioctl(stdoutFd, FIONBIO, (int) &nonBlock);
        }
        if (stderrFd >= 0) {
            ioctl(stderrFd, FIONBIO, (int) &nonBlock);
        }
#else
        if (stdoutFd >= 0) {
            fcntl(stdoutFd, F_SETFL, fcntl(stdoutFd, F_GETFL) | O_NONBLOCK);
        }
        if (stderrFd >= 0) {
            fcntl(stderrFd, F_SETFL, fcntl(stderrFd, F_GETFL) | O_NONBLOCK);
        }
#endif
        if (stdoutFd >= 0) {
            cmd->handlers[MPR_CMD_STDOUT] = mprCreateWaitHandler(cmd, stdoutFd, MPR_READABLE,
                (MprWaitProc) stdoutCallback, cmd, MPR_NORMAL_PRIORITY, MPR_WAIT_THREAD);
        }
        if (stderrFd >= 0) {
            cmd->handlers[MPR_CMD_STDERR] = mprCreateWaitHandler(cmd, stderrFd, MPR_READABLE,
                (MprWaitProc) stderrCallback, cmd, MPR_NORMAL_PRIORITY, MPR_WAIT_THREAD);
            if (stdoutFd >= 0) {
                /*
                 *  Delay enabling stderr events until stdout is complete
                 */
                mprDisableWaitEvents(cmd->handlers[MPR_CMD_STDERR]);
            }
        }
    }
#endif
#endif
    rc = startProcess(cmd);
#if CYGWIN
    mprGlobalUnlock(cmd);
#endif
    return rc;
}


int mprMakeCmdIO(MprCmd *cmd)
{
    MprCmdFile  *files;
    int         rc;

    files = cmd->files;

    rc = 0;
    if (cmd->flags & MPR_CMD_IN) {
        rc += makeChannel(cmd, MPR_CMD_STDIN);
    }
    if (cmd->flags & MPR_CMD_OUT) {
        rc += makeChannel(cmd, MPR_CMD_STDOUT);
    }
    if (cmd->flags & MPR_CMD_ERR) {
        rc += makeChannel(cmd, MPR_CMD_STDERR);
    }
    return rc;
}


/*
 *  Stop the command
 */
void mprStopCmd(MprCmd *cmd)
{
    mprLog(cmd, 7, "cmd: stop");

    if (cmd->pid) {
#if BLD_WIN_LIKE
        TerminateProcess(cmd->process, 2);
#elif VXWORKS
        taskDelete(cmd->pid);
#else
        kill(cmd->pid, SIGTERM);
#endif
    }
}


int mprReadCmdPipe(MprCmd *cmd, int channel, char *buf, int bufsize)
{
#if BLD_WIN_LIKE && !WINCE
    int     count, rc;

    rc = PeekNamedPipe(cmd->files[channel].handle, NULL, 0, NULL, &count, NULL);
    if (rc && count > 0) {
        return read(cmd->files[channel].fd, buf, bufsize);
    }
    if (cmd->process == 0) {
        return 0;
    }
    /*
     *  No waiting. Use this just to check if the process has exited and thus EOF on the pipe.
     */
    if (WaitForSingleObject(cmd->process, 0) == WAIT_OBJECT_0) {
        return 0;
    }
    errno = EAGAIN;
    return -1;

#elif VXWORKS && UNUSED && KEEP
    /*
        Only needed when using non-blocking I/O
     */
    int     rc;
    rc = read(cmd->files[channel].fd, buf, bufsize);

    /*
     *  VxWorks can't signal EOF on non-blocking pipes. Need a pattern indicator.
     */
    if (rc == MPR_CMD_VXWORKS_EOF_LEN && strncmp(buf, MPR_CMD_VXWORKS_EOF, MPR_CMD_VXWORKS_EOF_LEN) == 0) {
        /* EOF */
        return 0;

    } else if (rc == 0) {
        rc = -1;
        errno = EAGAIN;
    }
    return rc;

#else
    return read(cmd->files[channel].fd, buf, bufsize);
#endif
}


int mprWriteCmdPipe(MprCmd *cmd, int channel, char *buf, int bufsize)
{
#if BLD_WIN_LIKE
    /*
     *  No waiting. Use this just to check if the process has exited and thus EOF on the pipe.
     */
    if (WaitForSingleObject(cmd->process, 0) == WAIT_OBJECT_0) {
        return -1;
    }
#endif
    return write(cmd->files[channel].fd, buf, bufsize);
}


#if UNUSED && KEEP
void mprEnableCmdEvents(MprCmd *cmd, int channel)
{
#if BLD_UNIX_LIKE || VXWORKS
    if (cmd->handlers[channel]) {
        mprEnableWaitEvents(cmd->handlers[channel]);
    }
#endif
}


void mprDisableCmdEvents(MprCmd *cmd, int channel)
{
#if BLD_UNIX_LIKE || VXWORKS
    if (cmd->handlers[channel]) {
        mprDisableWaitEvents(cmd->handlers[channel]);
    }
#endif
}
#endif


#if BLD_WIN_LIKE && !WINCE
/*
 *  Service I/O and return a count of characters that can be read without blocking. If the proces has completed,
 *  then return 1 to indicate that EOF can be read.
 */
static int serviceCmdEvents(MprCmd *cmd, int channel, int timeout)
{
    int     rc, count, status;

    if (mprGetDebugMode(cmd)) {
        timeout = MAXINT;
    }
    if (cmd->files[channel].handle) {
        rc = PeekNamedPipe(cmd->files[channel].handle, NULL, 0, NULL, &count, NULL);
        if (rc && count > 0) {
            return count;
        }
    }
    if (cmd->process == 0) {
        return 1;
    }
    if ((status = WaitForSingleObject(cmd->process, timeout)) == WAIT_OBJECT_0) {
        if (cmd->requiredEof == 0) {
            mprReapCmd(cmd, MPR_TIMEOUT_STOP_TASK);
            mprSignalCond(cmd->completeCond);
            return 0;
        }
        return 1;
    }
}
#endif /* BLD_WIN_LIKE && !WINCE */


/*
 *  Poll for I/O events on the CGI pipes
 */
void mprPollCmdPipes(MprCmd *cmd, int timeout)
{
#if BLD_WIN_LIKE && !WINCE
    if (cmd->files[MPR_CMD_STDOUT].handle) {
        if (serviceCmdEvents(cmd, MPR_CMD_STDOUT, timeout) > 0 && (cmd->flags & MPR_CMD_OUT)) {
            stdoutCallback(cmd, MPR_READABLE);
        }
    } else if (cmd->files[MPR_CMD_STDERR].handle) {
        if (serviceCmdEvents(cmd, MPR_CMD_STDERR, timeout) > 0 && (cmd->flags & MPR_CMD_ERR)) {
            stderrCallback(cmd, MPR_READABLE);
        }
    }
#else
    if (cmd->files[MPR_CMD_STDOUT].fd >= 0) {
        if (mprWaitForSingleIO(cmd, cmd->files[MPR_CMD_STDOUT].fd, MPR_READABLE, timeout)) {
            stdoutCallback(cmd, MPR_READABLE);
        }
    } else if (cmd->files[MPR_CMD_STDERR].fd >= 0) {
        if (mprWaitForSingleIO(cmd, cmd->files[MPR_CMD_STDERR].fd, MPR_READABLE, timeout)) {
            stderrCallback(cmd, MPR_READABLE);
        }
    }
#endif
}

/*
 *  Wait for a command to complete. Return 0 if the command completed, otherwise return MPR_ERR_TIMEOUT. 
 *  This will call mprReapCmd if requried.
 */
int mprWaitForCmd(MprCmd *cmd, int timeout)
{
    MprTime     mark;
    int         complete, rc;

    if (timeout < 0) timeout = MAXINT;
    if (mprGetDebugMode(cmd)) timeout = MAXINT;

    mark = mprGetTime(cmd);
    complete = 0;

    do {
        if (cmd->requiredEof == 0) {
            if (mprReapCmd(cmd, 10) == 0) {
                mprSignalCond(cmd->completeCond);
                return 0;
            }
        }
        mprPollCmdPipes(cmd, timeout);
        rc = mprWaitForCondWithService(cmd->completeCond, 10);
        if (rc == 0) {
            complete++;
            break;
        } else if (rc != MPR_ERR_TIMEOUT) {
            mprAssert(0);
        }
    } while (mprGetElapsedTime(cmd, mark) <= timeout);

    if (!complete) {
        mprLog(cmd, 7, "cmd: mprWaitForCmd: timeout waiting for command to complete");
        return MPR_ERR_TIMEOUT;
    }
    if (cmd->pid) {
        mprReapCmd(cmd, MPR_TIMEOUT_STOP_TASK);
    }
    mprLog(cmd, 7, "cmd: waitForChild: status %d", cmd->status);
    return 0;
}


/*
 *  Collect the child's exit status. The initiating thread must do this on uClibc. 
 *  Return zero if the exit status is successfully reaped. Return -1 if an error 
 *  and return > 0 if process still running.
 */
int mprReapCmd(MprCmd *cmd, int timeout)
{
    MprTime     mark;

    mprAssert(cmd->pid);

    if (timeout < 0) {
        timeout = MAXINT;
    }
    mark = mprGetTime(cmd);

    while (cmd->pid) {
#if BLD_UNIX_LIKE
        int     status, waitrc;
        status = 0;
        if ((waitrc = waitpid(cmd->pid, &status, WNOHANG | __WALL)) < 0) {
            mprAssert(0);
            mprLog(cmd, 0, "waitpid failed for pid %d, errno %d", cmd->pid, errno);
            return MPR_ERR_CANT_READ;

        } else if (waitrc == cmd->pid) {
            if (!WIFSTOPPED(status)) {
                if (WIFEXITED(status)) {
                    cmd->status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    cmd->status = WTERMSIG(status);
                }
                cmd->pid = 0;
            }
            break;
            
        } else {
            mprAssert(waitrc == 0);
        }
#endif
#if VXWORKS
        /*
         *  The command exit status (cmd->status) is set in cmdTaskEntry
         */
        if (semTake(cmd->exitCond, MPR_TIMEOUT_STOP_TASK) != OK) {
            mprError(cmd, "cmd: child %s did not exit, errno %d", cmd->program);
            return MPR_ERR_CANT_CREATE;
        }
        semDelete(cmd->exitCond);
        cmd->exitCond = 0;
        cmd->pid = 0;
#endif
#if BLD_WIN_LIKE
        int     status, rc;
        if ((rc = WaitForSingleObject(cmd->process, 10)) != WAIT_OBJECT_0) {
            if (rc == WAIT_TIMEOUT) {
                return -MPR_ERR_TIMEOUT;
            }
            mprLog(cmd, 6, "cmd: WaitForSingleObject no child to reap rc %d, %d", rc, GetLastError());
            return MPR_ERR_CANT_READ;
        }
        if (GetExitCodeProcess(cmd->process, (ulong*) &status) == 0) {
            mprLog(cmd, 7, "cmd: GetExitProcess error");
            return MPR_ERR_CANT_READ;
        }
        if (status != STILL_ACTIVE) {
            cmd->status = status;
            CloseHandle(cmd->process);
            CloseHandle(cmd->thread);
            cmd->process = 0;
            cmd->pid = 0;
            break;
        }
#endif
        if (mprGetElapsedTime(cmd, mark) > timeout) {
            break;
        }
        /* Prevent busy waiting */
        mprSleep(cmd, 10);
    }
    return (cmd->pid == 0) ? 0 : 1;
}


static int stdoutCallback(MprCmd *cmd, int mask)
{
    return (cmd->callback)(cmd, MPR_CMD_STDOUT, cmd->callbackData);
}


static int stderrCallback(MprCmd *cmd, int mask)
{
    return (cmd->callback)(cmd, MPR_CMD_STDERR, cmd->callbackData);
}


void mprSetCmdCallback(MprCmd *cmd, MprCmdProc proc, void *data)
{
    cmd->callback = proc;
    cmd->callbackData = data;
}


int mprGetCmdExitStatus(MprCmd *cmd, int *statusp)
{
    mprAssert(statusp);

    if (cmd->pid) {
        mprReapCmd(cmd, MPR_TIMEOUT_STOP_TASK);
        if (cmd->pid) {
            return MPR_ERR_NOT_READY;
        }
    }
    *statusp = cmd->status;
    return 0;
}


bool mprIsCmdRunning(MprCmd *cmd)
{
    return cmd->pid > 0;
}


void mprSetCmdTimeout(MprCmd *cmd, int timeout)
{
    cmd->timeoutPeriod = timeout;
}


int mprGetCmdFd(MprCmd *cmd, int channel) 
{ 
    return cmd->files[channel].fd; 
}


MprBuf *mprGetCmdBuf(MprCmd *cmd, int channel)
{
    return (channel == MPR_CMD_STDOUT) ? cmd->stdoutBuf : cmd->stderrBuf;
}


void mprSetCmdDir(MprCmd *cmd, cchar *dir)
{
    mprAssert(dir && *dir);

    mprFree(cmd->dir);
    cmd->dir = mprStrdup(cmd, dir);
}


/*
 *  Sanitize args. Convert "/" to "\" and converting '\r' and '\n' to spaces, quote all args and put the program as argv[0].
 */
static int sanitizeArgs(MprCmd *cmd, int argc, char **argv, char **env)
{
#if VXWORKS
    cmd->argv = argv;
    cmd->argc = argc;
    cmd->env = 0;
#endif
#if BLD_UNIX_LIKE
    char    *cp;
    int     index, i, hasPath, hasLibPath;

    cmd->argv = argv;
    cmd->argc = argc;
    cmd->env = 0;

    if (env) {
        for (i = 0; env && env[i]; i++) {
            mprLog(cmd, 6, "cmd: env[%d]: %s", i, env[i]);
        }
        if ((cmd->env = mprAlloc(cmd, (i + 3) * sizeof(char*))) == NULL) {
            return MPR_ERR_NO_MEMORY;
        }
        hasPath = hasLibPath = 0;
        for (index = i = 0; env && env[i]; i++) {
            mprLog(cmd, 6, "cmd: env[%d]: %s", i, env[i]);
            if (strncmp(env[i], "PATH=", 5) == 0) {
                hasPath++;
            } else if  (strncmp(env[i], LD_LIBRARY_PATH "=", 16) == 0) {
                hasLibPath++;
            }
            cmd->env[index++] = env[i];
        }

        /*
         *  Add PATH and LD_LIBRARY_PATH 
         */
        if (!hasPath && (cp = getenv("PATH")) != 0) {
            cmd->env[index++] = mprAsprintf(cmd, MPR_MAX_STRING, "PATH=%s", cp);
        }
        if (!hasLibPath && (cp = getenv(LD_LIBRARY_PATH)) != 0) {
            cmd->env[index++] = mprAsprintf(cmd, MPR_MAX_STRING, "%s=%s", LD_LIBRARY_PATH, cp);
        }
        cmd->env[index++] = '\0';
        for (i = 0; i < argc; i++) {
            mprLog(cmd, 4, "cmd: arg[%d]: %s", i, argv[i]);
        }
        for (i = 0; cmd->env[i]; i++) {
            mprLog(cmd, 4, "cmd: env[%d]: %s", i, cmd->env[i]);
        }
    }
#endif

#if BLD_WIN_LIKE
    char    *program, *SYSTEMROOT, **ep, **ap, *destp, *cp, *progBuf, *localArgv[2], *saveArg0, *PATH, *endp;
    int     i, len, hasPath, hasSystemRoot;

    mprAssert(argc > 0 && argv[0] != NULL);

    cmd->argv = argv;
    cmd->argc = argc;

    program = argv[0];
    progBuf = mprAlloc(cmd, (int) strlen(program) * 2 + 1);
    strcpy(progBuf, program);
    program = progBuf;

    for (cp = program; *cp; cp++) {
        if (*cp == '/') {
            *cp = '\\';
        } else if (*cp == '\r' || *cp == '\n') {
            *cp = ' ';
        }
    }
    if (*program == '\"') {
        if ((cp = strrchr(++program, '"')) != 0) {
            *cp = '\0';
        }
    }

    if (argv == 0) {
        argv = localArgv;
        argv[1] = 0;
        saveArg0 = program;
    } else {
        saveArg0 = argv[0];
    }
    /*
     *  Set argv[0] to the program name while creating the command line. Restore later
     */
    argv[0] = program;

    /*
     *  Determine the command line length and arg count
     */
    argc = 0;
    for (len = 0, ap = argv; *ap; ap++) {
        len += (int) strlen(*ap) + 1 + 2;         /* Space and possible quotes */
        argc++;
    }
    cmd->command = (char*) mprAlloc(cmd, len + 1);
    cmd->command[len] = '\0';
    
    /*
     *  Add quotes to all args that have spaces in them including "program"
     */
    destp = cmd->command;
    for (ap = &argv[0]; *ap; ) {
        cp = *ap;
        if ((strchr(cp, ' ') != 0) && cp[0] != '\"') {
            *destp++ = '\"';
            strcpy(destp, cp);
            destp += strlen(cp);
            *destp++ = '\"';
        } else {
            strcpy(destp, cp);
            destp += strlen(cp);
        }
        if (*++ap) {
            *destp++ = ' ';
        }
    }
    *destp = '\0';
    argv[0] = saveArg0;

    for (i = 0; i < argc; i++) {
        mprLog(cmd, 4, "cmd: arg[%d]: %s", i, argv[i]);
    }

    /*
     *  Now work on the environment. Windows has a block of null separated strings with a trailing null.
     */
    cmd->env = 0;
    if (env) {
        for (hasSystemRoot =  hasPath = len = 0, ep = env; ep && *ep; ep++) {
            len += (int) strlen(*ep) + 1;
            if (strncmp(*ep, "PATH=", 5) == 0) {
                hasPath++;
            } else if (strncmp(*ep, "SYSTEMROOT=", 11) == 0) {
                hasSystemRoot++;
            }
        }
        if (!hasSystemRoot && (SYSTEMROOT = getenv("SYSTEMROOT")) != 0) {
            len += 11 + (int) strlen(SYSTEMROOT) + 1;
        }
        if (!hasPath && (PATH = getenv("PATH")) != 0) {
            len += 5 + (int) strlen(PATH) + 1;
        }
        len += 2;       /* Windows requires 2 nulls for the block end */

        destp = (char*) mprAlloc(cmd, len);
        endp = &destp[len];
        cmd->env = (char**) destp;
        for (ep = env; ep && *ep; ep++) {
            mprLog(cmd, 4, "cmd: env[%d]: %s", i, *ep);
            strcpy(destp, *ep);
            mprLog(cmd, 7, "cmd: Set env variable: %s", destp);
            destp += strlen(*ep) + 1;
        }
        if (!hasSystemRoot) {
            mprSprintf(destp, endp - destp - 1, "SYSTEMROOT=%s", SYSTEMROOT);
            destp += 12 + strlen(SYSTEMROOT);
        }
        if (!hasPath) {
            mprSprintf(destp, endp - destp - 1, "PATH=%s", PATH);
            destp += 6 + strlen(PATH);
        }
        *destp++ = '\0';
        *destp++ = '\0';                        /* Windows requires two nulls */
        mprAssert(destp <= endp);
#if TEST
        for (cp = (char*) cmd->env; *cp; cp++) {
            print("ENV %s\n", cp);
            cp += strlen(cp);
        }
#endif
    }
#endif /* BLD_WIN_LIKE */
    return 0;
}


#if BLD_WIN_LIKE
static int startProcess(MprCmd *cmd)
{
    PROCESS_INFORMATION procInfo;
    STARTUPINFO         startInfo;
    int                 err;

    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);

    startInfo.dwFlags = STARTF_USESHOWWINDOW;
    if (cmd->flags & MPR_CMD_SHOW) {
        startInfo.wShowWindow = SW_SHOW;
    } else {
        startInfo.wShowWindow = SW_HIDE;
    }
    startInfo.dwFlags |= STARTF_USESTDHANDLES;

    if (cmd->flags & MPR_CMD_IN) {
        if (cmd->files[MPR_CMD_STDIN].clientFd > 0) {
            startInfo.hStdInput = (HANDLE) _get_osfhandle(cmd->files[MPR_CMD_STDIN].clientFd);
        }
    } else {
        startInfo.hStdInput = (HANDLE) _get_osfhandle((int) fileno(stdin));
    }
    if (cmd->flags & MPR_CMD_OUT) {
        if (cmd->files[MPR_CMD_STDOUT].clientFd > 0) {
            startInfo.hStdOutput = (HANDLE)_get_osfhandle(cmd->files[MPR_CMD_STDOUT].clientFd);
        }
    } else {
        startInfo.hStdOutput = (HANDLE)_get_osfhandle((int) fileno(stdout));
    }
    if (cmd->flags & MPR_CMD_ERR) {
        if (cmd->files[MPR_CMD_STDERR].clientFd > 0) {
            startInfo.hStdError = (HANDLE) _get_osfhandle(cmd->files[MPR_CMD_STDERR].clientFd);
        }
    } else {
        startInfo.hStdError = (HANDLE) _get_osfhandle((int) fileno(stderr));
    }

    if (! CreateProcess(0, cmd->command, 0, 0, 1, 0, cmd->env, cmd->dir, &startInfo, &procInfo)) {
        err = mprGetOsError();
        if (err == ERROR_DIRECTORY) {
            mprError(cmd, "Can't create process: %s, directory %s is invalid", cmd->program, cmd->dir);
        } else {
            mprError(cmd, "Can't create process: %s, %d", cmd->program, err);
        }
        return MPR_ERR_CANT_CREATE;
    }
    cmd->thread = procInfo.hThread;
    cmd->process = procInfo.hProcess;
    cmd->pid = procInfo.dwProcessId;
    return 0;
}


#if WINCE
static int makeChannel(MprCmd *cmd, int index)
{
    SECURITY_ATTRIBUTES clientAtt, serverAtt, *att;
    HANDLE              readHandle, writeHandle;
    MprCmdFile          *file;
    char                *path;
    int                 readFd, writeFd;

    memset(&clientAtt, 0, sizeof(clientAtt));
    clientAtt.nLength = sizeof(SECURITY_ATTRIBUTES);
    clientAtt.bInheritHandle = 1;

    /*
     *  Server fds are not inherited by the child
     */
    memset(&serverAtt, 0, sizeof(serverAtt));
    serverAtt.nLength = sizeof(SECURITY_ATTRIBUTES);
    serverAtt.bInheritHandle = 0;

    file = &cmd->files[index];
    path = mprGetTempPath(cmd, NULL);

    att = (index == MPR_CMD_STDIN) ? &clientAtt : &serverAtt;
    readHandle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, att, OPEN_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL,0);
    if (readHandle == INVALID_HANDLE_VALUE) {
        mprError(cmd, "Can't create stdio pipes %s. Err %d\n", path, mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    readFd = (int) (int64) _open_osfhandle((int*) readHandle, 0);

    att = (index == MPR_CMD_STDIN) ? &serverAtt: &clientAtt;
    writeHandle = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, att, OPEN_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, 0);
    writeFd = (int) _open_osfhandle((int*) writeHandle, 0);

    if (readFd < 0 || writeFd < 0) {
        mprError(cmd, "Can't create stdio pipes %s. Err %d\n", path, mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    if (index == MPR_CMD_STDIN) {
        file->clientFd = readFd;
        file->fd = writeFd;
        file->handle = writeHandle;
    } else {
        file->clientFd = writeFd;
        file->fd = readFd;
        file->handle = readHandle;
    }
    mprFree(path);
    return 0;
}
#else /* !WINCE */

static int makeChannel(MprCmd *cmd, int index)
{
    SECURITY_ATTRIBUTES clientAtt, serverAtt, *att;
    HANDLE              readHandle, writeHandle;
    MprCmdFile          *file;
    MprTime             now;
    char                *pipeBuf;
    int                 openMode, pipeMode, readFd, writeFd;
    static int          tempSeed = 0;

    memset(&clientAtt, 0, sizeof(clientAtt));
    clientAtt.nLength = sizeof(SECURITY_ATTRIBUTES);
    clientAtt.bInheritHandle = 1;

    /*
     *  Server fds are not inherited by the child
     */
    memset(&serverAtt, 0, sizeof(serverAtt));
    serverAtt.nLength = sizeof(SECURITY_ATTRIBUTES);
    serverAtt.bInheritHandle = 0;

    file = &cmd->files[index];
    now = ((int) mprGetTime(cmd) & 0xFFFF) % 64000;

    pipeBuf = mprAsprintf(cmd, -1, "\\\\.\\pipe\\MPR_%d_%d_%d.tmp", getpid(), (int) now, ++tempSeed);

    /*
     *  Pipes are always inbound. The file below is outbound. we swap whether the client or server
     *  inherits the pipe or file. MPR_CMD_STDIN is the clients input pipe.
     *  Pipes are blocking since both ends share the same blocking mode. Client must be blocking.
     */
    openMode = PIPE_ACCESS_INBOUND;
    pipeMode = 0;

    att = (index == MPR_CMD_STDIN) ? &clientAtt : &serverAtt;
    readHandle = CreateNamedPipe(pipeBuf, openMode, pipeMode, 1, 0, 256 * 1024, 1, att);
    if (readHandle == INVALID_HANDLE_VALUE) {
        mprError(cmd, "Can't create stdio pipes %s. Err %d\n", pipeBuf, mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    readFd = (int) (int64) _open_osfhandle((long) readHandle, 0);

    att = (index == MPR_CMD_STDIN) ? &serverAtt: &clientAtt;
    writeHandle = CreateFile(pipeBuf, GENERIC_WRITE, 0, att, OPEN_EXISTING, openMode, 0);
    writeFd = (int) _open_osfhandle((long) writeHandle, 0);

    if (readFd < 0 || writeFd < 0) {
        mprError(cmd, "Can't create stdio pipes %s. Err %d\n", pipeBuf, mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    if (index == MPR_CMD_STDIN) {
        file->clientFd = readFd;
        file->fd = writeFd;
        file->handle = writeHandle;
    } else {
        file->clientFd = writeFd;
        file->fd = readFd;
        file->handle = readHandle;
    }
    mprFree(pipeBuf);
    return 0;
}
#endif /* WINCE */


#elif BLD_UNIX_LIKE
static int startProcess(MprCmd *cmd)
{
    MprCmdFile      *files;
    int             rc, i, err;

    files = cmd->files;

    /*
     *  Create the child
     */
    cmd->pid = vfork();

    if (cmd->pid < 0) {
        mprError(cmd, "start: can't fork a new process to run %s, errno %d", cmd->program, mprGetOsError());
        return MPR_ERR_CANT_INITIALIZE;

    } else if (cmd->pid == 0) {
        /*
         *  Child
         */
        umask(022);
        if (cmd->flags & MPR_CMD_NEW_SESSION) {
            setsid();
        }
        if (cmd->dir) {
            if (chdir(cmd->dir) < 0) {
                mprLog(cmd, 0, "cmd: Can't change directory to %s", cmd->dir);
                return MPR_ERR_CANT_INITIALIZE;
            }
        }
        if (cmd->flags & MPR_CMD_IN) {
            if (files[MPR_CMD_STDIN].clientFd >= 0) {
                rc = dup2(files[MPR_CMD_STDIN].clientFd, 0);
                close(files[MPR_CMD_STDIN].fd);
            } else {
                close(0);
            }
        }
        if (cmd->flags & MPR_CMD_OUT) {
            if (files[MPR_CMD_STDOUT].clientFd >= 0) {
                rc = dup2(files[MPR_CMD_STDOUT].clientFd, 1);
                close(files[MPR_CMD_STDOUT].fd);
            } else {
                close(1);
            }
        }
        if (cmd->flags & MPR_CMD_ERR) {
            if (files[MPR_CMD_STDERR].clientFd >= 0) {
                rc = dup2(files[MPR_CMD_STDERR].clientFd, 2);
                close(files[MPR_CMD_STDERR].fd);
            } else {
                close(2);
            }
        }
        cmd->forkCallback(cmd->forkData);
        if (cmd->env) {
            rc = execve(cmd->program, cmd->argv, fixenv(cmd));
        } else {
            rc = execv(cmd->program, cmd->argv);
        }
        err = errno;
        mprPrintfError(cmd, "Can't exec %s, err %d, cwd %s\n", cmd->program, err, mprGetCurrentPath(cmd));

        /*
         *  Use _exit to avoid flushing I/O any other I/O.
         */
        _exit(-(MPR_ERR_CANT_INITIALIZE));

    } else {
        /*
         *  Close the client handles
         */
        for (i = 0; i < MPR_CMD_MAX_PIPE; i++) {
            if (files[i].clientFd >= 0) {
                close(files[i].clientFd);
                files[i].clientFd = -1;
            }
        }
    }
    return 0;
}


static int makeChannel(MprCmd *cmd, int index)
{
    MprCmdFile      *file;
    int             fds[2];

    file = &cmd->files[index];

    if (pipe(fds) < 0) {
        mprError(cmd, "Can't create stdio pipes. Err %d", mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    if (index == MPR_CMD_STDIN) {
        file->clientFd = fds[0];        /* read fd */
        file->fd = fds[1];              /* write fd */
    } else {
        file->clientFd = fds[1];        /* write fd */
        file->fd = fds[0];              /* read fd */
    }
    mprLog(cmd, 7, "mprMakeCmdIO: pipe handles[%d] read %d, write %d", index, fds[0], fds[1]);
    return 0;
}
#endif /* BLD_UNIX_LIKE */


#if VXWORKS
/*
    Start the command to run (stdIn and stdOut are named from the client's perspective)
 */
int startProcess(MprCmd *cmd)
{
    MprCmdTaskFn    entryFn;
    SYM_TYPE        symType;
    char            *entryPoint, *program;
    int             i, pri;

    mprLog(cmd, 4, "cmd: start %s", cmd->program);

    entryPoint = 0;
    if (cmd->env) {
        for (i = 0; cmd->env[i]; i++) {
            if (strncmp(cmd->env[i], "entryPoint=", 11) == 0) {
                entryPoint = mprStrdup(cmd, cmd->env[i]);
            }
        }
    }
    program = mprGetPathBase(cmd, cmd->program);
    if (entryPoint == 0) {
        program = mprTrimPathExtension(cmd, program);
#if BLD_HOST_CPU_ARCH == MPR_CPU_IX86 || BLD_HOST_CPU_ARCH == MPR_CPU_IX64
        entryPoint = mprStrcat(cmd, -1, "_", program, "Main", NULL);
#else
        entryPoint = mprStrcat(cmd, -1, program, "Main", NULL);
#endif
    }

    if (symFindByName(sysSymTbl, entryPoint, (char**) &entryFn, &symType) < 0) {
        if (mprLoadModule(cmd, cmd->program, NULL) == 0) {
            mprError(cmd, "start: can't load DLL %s, errno %d", program, mprGetOsError());
            return MPR_ERR_CANT_READ;
        }
        if (symFindByName(sysSymTbl, entryPoint, (char**) &entryFn, &symType) < 0) {
            mprError(cmd, "start: can't find symbol %s, errno %d", entryPoint, mprGetOsError());
            return MPR_ERR_CANT_ACCESS;
        }
    }
    taskPriorityGet(taskIdSelf(), &pri);

    /*
     *  Pass the server output file to become the client stdin.
     */
    cmd->pid = taskSpawn(entryPoint, pri, 0, MPR_DEFAULT_STACK, (FUNCPTR) cmdTaskEntry, 
        (int) cmd->program, (int) entryFn, (int) cmd, 0, 0, 0, 0, 0, 0, 0);

    if (cmd->pid < 0) {
        mprError(cmd, "start: can't create task %s, errno %d", entryPoint, mprGetOsError());
        mprFree(entryPoint);
        return MPR_ERR_CANT_CREATE;
    }

    mprLog(cmd, 7, "cmd, child taskId %d", cmd->pid);
    mprFree(entryPoint);

    if (semTake(cmd->startCond, MPR_TIMEOUT_START_TASK) != OK) {
        mprError(cmd, "start: child %s did not initialize, errno %d", cmd->program, mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    semDelete(cmd->startCond);
    cmd->startCond = 0;

    return 0;
}


/*
 *  Executed by the child process
 */
static void cmdTaskEntry(char *program, MprCmdTaskFn entry, int cmdArg)
{
    MprCmd          *cmd;
    MprCmdFile      *files;
    WIND_TCB        *tcb;
    char            **ep, *dir;
    int             inFd, outFd, errFd, id, rc;

    cmd = (MprCmd*) cmdArg;

    /*
     *  Open standard I/O files (in/out are from the server's perspective)
     */
    files = cmd->files;
    inFd = open(files[MPR_CMD_STDIN].name, O_RDONLY, 0666);
    outFd = open(files[MPR_CMD_STDOUT].name, O_WRONLY, 0666);
    errFd = open(files[MPR_CMD_STDERR].name, O_WRONLY, 0666);

    if (inFd < 0 || outFd < 0 || errFd < 0) {
        exit(255);
    }

    id = taskIdSelf();
    ioTaskStdSet(id, 0, inFd);
    ioTaskStdSet(id, 1, outFd);
    ioTaskStdSet(id, 2, errFd);

    /*
     *  Now that we have opened the stdin and stdout, wakeup our parent.
     */
    semGive(cmd->startCond);

    /*
     *  Create the environment
     */
    if (envPrivateCreate(id, -1) < 0) {
        exit(254);
    }
    for (ep = cmd->env; ep && *ep; ep++) {
        putenv(*ep);
    }

    /*
     *  Set current directory if required
     */
    if (cmd->dir) {
        rc = chdir(cmd->dir);
    } else {
        dir = mprGetPathDir(cmd, cmd->program);
        rc = chdir(dir);
        mprFree(dir);
    }
    if (rc < 0) {
        mprLog(cmd, 0, "cmd: Can't change directory to %s", cmd->dir);
        exit(255);
    }

    /*
     *  Call the user's entry point
     */
    (entry)(cmd->argc, cmd->argv, cmd->env);

    tcb = taskTcb(id);
    cmd->status = tcb->exitCode;

    /*
     *  Cleanup
     */
    envPrivateDestroy(id);
    close(inFd);
    close(outFd);
    close(errFd);
    semGive(cmd->exitCond);
}


static int makeChannel(MprCmd *cmd, int index)
{
    MprCmdFile      *file;
    static int      tempSeed = 0;

    file = &cmd->files[index];

    file->name = mprAsprintf(cmd, -1, "/pipe/%s_%d_%d", BLD_PRODUCT, taskIdSelf(), tempSeed++);

    if (pipeDevCreate(file->name, 5, MPR_BUFSIZE) < 0) {
        mprError(cmd, "Can't create pipes to run %s", cmd->program);
        return MPR_ERR_CANT_OPEN;
    }
    
    /*
     *  Open the server end of the pipe. MPR_CMD_STDIN is from the client's perspective.
     */
    if (index == MPR_CMD_STDIN) {
        file->fd = open(file->name, O_WRONLY, 0644);
    } else {
        file->fd = open(file->name, O_RDONLY, 0644);
    }
    if (file->fd < 0) {
        mprError(cmd, "Can't create stdio pipes. Err %d", mprGetOsError());
        return MPR_ERR_CANT_CREATE;
    }
    return 0;
}
#endif /* VXWORKS */


static void closeFiles(MprCmd *cmd)
{
    int     i;
    for (i = 3; i < MPR_MAX_FILE; i++) {
        close(i);
    }
}


#if BLD_UNIX_LIKE
/*
 *  CYGWIN requires a PATH or else execve hangs in cygwin 1.7
 */
static char **fixenv(MprCmd *cmd)
{
    char    **env;

    env = cmd->env;
#if CYGWIN
    if (env) {
        int     i, envc;

        for (envc = 0; cmd->env[envc]; envc++) {
            if (strstr(cmd->env[envc], "PATH=") != 0) {
                return cmd->env;
            }
        }
        if ((env = mprAlloc(cmd, sizeof(void*) * (envc + 2))) == NULL) {
            return NULL;
        }
        i = 0;
        env[i++] = mprStrcat(cmd, -1, "PATH=", getenv("PATH"), NULL);
        for (envc = 0; cmd->env[envc]; envc++) {
            env[i++] = cmd->env[envc];
        }
        env[i++] = 0;
    }
#endif /* CYGWIN */
    return env;
}
#endif


#else /* BLD_FEATURE_CMD */
void __mprCmdDummy() {}
#endif /* BLD_FEATURE_CMD */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprCmd.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprCond.c"
 */
/************************************************************************/

/**
 *  mprCond.c - Thread Conditional variables
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



static int condDestructor(MprCond *cp);
static int waitWithService(MprCond *cp, int timeout);

/*
 *  Create a condition variable for use by single or multiple waiters
 */

MprCond *mprCreateCond(MprCtx ctx)
{
    MprCond     *cp;

    cp = mprAllocObjWithDestructor(ctx, MprCond, condDestructor);
    if (cp == 0) {
        return 0;
    }
    cp->triggered = 0;
#if BLD_FEATURE_MULTITHREAD
    cp->mutex = mprCreateLock(cp);

#if BLD_WIN_LIKE
    cp->cv = CreateEvent(NULL, FALSE, FALSE, NULL);
#elif VXWORKS
    cp->cv = semCCreate(SEM_Q_PRIORITY, SEM_EMPTY);
#else
    pthread_cond_init(&cp->cv, NULL);
#endif
#endif

    return cp;
}


/*
 *  Condition variable destructor
 */
static int condDestructor(MprCond *cp)
{
    mprAssert(cp);
    
#if BLD_FEATURE_MULTITHREAD
    mprAssert(cp->mutex);
    mprLock(cp->mutex);

#if BLD_WIN_LIKE
    CloseHandle(cp->cv);
#elif VXWORKS
    semDelete(cp->cv);
#else
    pthread_cond_destroy(&cp->cv);
#endif
    /* mprFree will call the mutex lock destructor */
#endif
    return 0;
}


#if BLD_FEATURE_MULTITHREAD
/*
 *  Wait for the event to be triggered. Should only be used when there are single waiters. If the event is already
 *  triggered, then it will return immediately. Timeout of -1 means wait forever. Timeout of 0 means no wait.
 *  Returns 0 if the event was signalled. Returns < 0 if the timeout.
 */
int mprWaitForCond(MprCond *cp, int timeout)
{
    MprTime     now, expire;
    int         rc;
#if BLD_UNIX_LIKE
    struct timespec     waitTill;
    struct timeval      current;
    int                 usec;
#endif

    rc = 0;
    if (timeout < 0) {
        timeout = MAXINT;
    }
    now = mprGetTime(cp);
    expire = now + timeout;

#if BLD_UNIX_LIKE
        gettimeofday(&current, NULL);
        usec = current.tv_usec + (timeout % 1000) * 1000;
        waitTill.tv_sec = current.tv_sec + (timeout / 1000) + (usec / 1000000);
        waitTill.tv_nsec = (usec % 1000000) * 1000;
#endif

    mprLock(cp->mutex);
    if (!cp->triggered) {
        /*
         *  WARNING: Can get spurious wakeups on some platforms (Unix + pthreads). 
         */
        do {
#if BLD_WIN_LIKE
            mprUnlock(cp->mutex);
            rc = WaitForSingleObject(cp->cv, (int) (expire - now));
            mprLock(cp->mutex);
            if (rc == WAIT_OBJECT_0) {
                rc = 0;
                ResetEvent(cp->cv);
            } else if (rc == WAIT_TIMEOUT) {
                rc = MPR_ERR_TIMEOUT;
            } else {
                rc = MPR_ERR_GENERAL;
            }
#elif VXWORKS
            mprUnlock(cp->mutex);
            rc = semTake(cp->cv, (int) (expire - now));
            mprLock(cp->mutex);
            if (rc != 0) {
                if (errno == S_objLib_OBJ_UNAVAILABLE) {
                    rc = MPR_ERR_TIMEOUT;
                } else {
                    rc = MPR_ERR_GENERAL;
                }
            }
            
#elif BLD_UNIX_LIKE
            /*
             *  NOTE: pthread_cond_timedwait can return 0 (MAC OS X and Linux). The pthread_cond_wait routines will 
             *  atomically unlock the mutex before sleeping and will relock on awakening.  
             */
            rc = pthread_cond_timedwait(&cp->cv, &cp->mutex->cs,  &waitTill);
            if (rc == ETIMEDOUT) {
                rc = MPR_ERR_TIMEOUT;
            } else if (rc != 0) {
                mprAssert(rc == 0);
                rc = MPR_ERR_GENERAL;
            }
#endif
        } while (!cp->triggered && rc == 0 && (now = mprGetTime(cp)) < expire);
    }

    if (cp->triggered) {
        cp->triggered = 0;
        rc = 0;
    } else if (rc == 0) {
        rc = MPR_ERR_TIMEOUT;
    }
    mprUnlock(cp->mutex);
    return rc;
}


/*
 *  Signal a condition and wakeup the waiter. Note: this may be called prior to the waiter waiting.
 */
void mprSignalCond(MprCond *cp)
{
    mprLock(cp->mutex);
    if (!cp->triggered) {
        cp->triggered = 1;
#if BLD_WIN_LIKE
        SetEvent(cp->cv);
#elif VXWORKS
        semGive(cp->cv);
#else
        pthread_cond_signal(&cp->cv);
#endif
    }
    mprUnlock(cp->mutex);
}


void mprResetCond(MprCond *cp)
{
    mprLock(cp->mutex);
    cp->triggered = 0;
#if BLD_WIN_LIKE
    ResetEvent(cp->cv);
#elif VXWORKS
    semDelete(cp->cv);
    cp->cv = semCCreate(SEM_Q_PRIORITY, SEM_EMPTY);
#else
    pthread_cond_destroy(&cp->cv);
    pthread_cond_init(&cp->cv, NULL);
#endif
    mprUnlock(cp->mutex);
}

#else /* BLD_FEATURE_MULTITHREAD */

/*
 *  Single threaded versions
 */
void mprSignalCond(MprCond *cp)
{
    cp->triggered = 1;
}


int mprWaitForCond(MprCond *cp, int timeout)
{
    return waitWithService(cp, timeout);
}


void mprResetCond(MprCond *cp)
{
    cp->triggered = 0;
}

#endif /* Single threaded */


/*
 *  Wait for an event to be triggered. Service events while waiting. Return 0 if triggered, < 0 on timeout.
 */
static int waitWithService(MprCond *cp, int timeout)
{
    MprTime     mark;

    if (timeout < 0) {
        timeout = MAXINT;
    }
    mark = mprGetTime(cp);
    while (!cp->triggered) {
        /*
         *  Must nap briefly incase another thread is the service thread and this thread is locked out
         */ 
        mprServiceEvents(mprGetDispatcher(cp), 10, MPR_SERVICE_IO | MPR_SERVICE_EVENTS | MPR_SERVICE_ONE_THING);
        if (mprGetElapsedTime(cp, mark) > timeout) {
            if (cp->triggered) {
                break;
            }
            return MPR_ERR_TIMEOUT;
        }
    }
    cp->triggered = 0;
    return 0;
}


/*
 *  Wait for a condition to be true and service events if required. This routine is required if single-threaded or
 *  if multi-threaded and there there is no service thread (or this thread is the service thread).
 */
int mprWaitForCondWithService(MprCond *cp, int timeout)
{
#if BLD_FEATURE_MULTITHREAD
    /*
     *  If we must wake a dispatcher, then it is safe to sleep as we are not the dispatcher.
     */
    if (mprMustWakeDispatcher(cp)) {
        return mprWaitForCond(cp, timeout);
    }
#endif
    return waitWithService(cp, timeout);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprCond.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprCrypt.c"
 */
/************************************************************************/

/*
 *  mprCrypt.c - Base-64 encoding and decoding and MD5 support.
 *
 *  Algorithms by RSA. See license at the end of the file. 
 *  This module is not thread safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Constants for transform routine.
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static uchar PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/*
 * ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/*
 *   FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 *   Rotation is separate from addition to prevent recomputation.
 */
 
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (uint)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

typedef struct {
    uint state[4];
    uint count[2];
    uchar buffer[64];
} MD5CONTEXT;


#define CRYPT_HASH_SIZE   16

/*
 *  Encoding map lookup
 */
static char encodeMap[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};


/*
 *  Decode map
 */
static signed char decodeMap[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};


static void decode(uint *output, uchar *input, uint len);
static void encode(uchar *output, uint *input, uint len);
static void finalize(uchar digest[16], MD5CONTEXT *context);
static void initMD5(MD5CONTEXT *context);
static void transform(uint state[4], uchar block[64]);
static void update(MD5CONTEXT *context, uchar *input, uint inputLen);


int mprDecode64(char *buffer, int bufsize, cchar *s)
{
    uint        bitBuf;
    char        *bp;
    int         c, i, j, shift;

    bp = buffer;
    *bp = '\0';
    while (*s && *s != '=') {
        bitBuf = 0;
        shift = 18;
        for (i = 0; i < 4 && *s && *s != '='; i++, s++) {
            c = decodeMap[*s & 0xff];
            if (c == -1) {
                return -1;
            } 
            bitBuf = bitBuf | (c << shift);
            shift -= 6;
        }
        --i;
        if ((bp + i) >= &buffer[bufsize]) {
            *buffer = '\0';
            return MPR_ERR_WONT_FIT;
        }
        for (j = 0; j < i; j++) {
            *bp++ = (char) ((bitBuf >> (8 * (2 - j))) & 0xff);
        }
        *bp = '\0';
    }
    return 0;
}


void mprEncode64(char *buffer, int bufsize, cchar *s)
{
    uint    shiftbuf;
    char    *bp;
    int     x, i, j, shift;

    bp = buffer;
    *bp = '\0';
    while (*s) {
        shiftbuf = 0;
        for (j = 2; j >= 0 && *s; j--, s++) {
            shiftbuf |= ((*s & 0xff) << (j * 8));
        }
        shift = 18;
        for (i = ++j; i < 4 && bp < &buffer[bufsize] ; i++) {
            x = (shiftbuf >> shift) & 0x3f;
            *bp++ = encodeMap[(shiftbuf >> shift) & 0x3f];
            shift -= 6;
        }
        while (j-- > 0) {
            *bp++ = '=';
        }
        *bp = '\0';
    }
}


/*
 *  Return the MD5 hash of a block
 */
char *mprGetMD5Hash(MprCtx ctx, cchar *buf, int length, cchar *prefix)
{
    MD5CONTEXT      context;
    uchar           hash[CRYPT_HASH_SIZE];
    cchar           *hex = "0123456789abcdef";
    char            *r, *str;
    char            result[(CRYPT_HASH_SIZE * 2) + 1];
    int             i, len;

    /*
     *  Take the MD5 hash of the string argument.
     */
    initMD5(&context);
    update(&context, (uchar*) buf, (uint) length);
    finalize(hash, &context);

    for (i = 0, r = result; i < 16; i++) {
        *r++ = hex[hash[i] >> 4];
        *r++ = hex[hash[i] & 0xF];
    }
    *r = '\0';

    len = (prefix) ? (int) strlen(prefix) : 0;
    str = (char*) mprAlloc(ctx, sizeof(result) + len);
    if (str) {
        if (prefix) {
            strcpy(str, prefix);
        }
        strcpy(str + len, result);
    }
    return str;
}


/*
 *  Convenience call 
 */ 
static char *md5(MprCtx ctx, cchar *string)
{
    return mprGetMD5Hash(ctx, string, (int) strlen(string), NULL);
}


/*
 *  Get a Nonce value for passing along to the client.  This function composes the string "secret:eTag:time:realm" 
 *  and calculates the MD5 digest.
 */ 
int mprCalcDigestNonce(MprCtx ctx, char **nonce, cchar *secret, cchar *etag, cchar *realm)
{
    MprTime     now;
    char        nonceBuf[256];

    mprAssert(realm && *realm);

    now = mprGetTime(ctx);

    mprSprintf(nonceBuf, sizeof(nonceBuf), "%s:%s:%x:%s", secret, etag, (uint) now, realm); 

    *nonce = md5(ctx, nonceBuf);
    return 0;
}


/*
 *  Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
 */ 
int mprCalcDigest(MprCtx ctx, char **digest, cchar *userName, cchar *password, cchar *realm, cchar *uri, 
    cchar *nonce, cchar *qop, cchar *nc, cchar *cnonce, cchar *method)
{
    char    a1Buf[256], a2Buf[256], digestBuf[256];
    char    *ha1, *ha2;

    mprAssert(qop);

    /*
     *  Compute HA1. If userName == 0, then the password is already expected to be in the HA1 format 
     *  (MD5(userName:realm:password).
     */
    if (userName == 0) {
        ha1 = mprStrdup(ctx, password);
    } else {
        mprSprintf(a1Buf, sizeof(a1Buf), "%s:%s:%s", userName, realm, password);
        ha1 = md5(ctx, a1Buf);
    }

    /*
     *  HA2
     */ 
    mprSprintf(a2Buf, sizeof(a2Buf), "%s:%s", method, uri);
    ha2 = md5(ctx, a2Buf);

    /*
     *  H(HA1:nonce:HA2)
     */
    if (strcmp(qop, "auth") == 0) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else if (strcmp(qop, "auth-int") == 0) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, nonce, ha2);
    }

    *digest = md5(ctx, digestBuf);

    mprFree(ha1);
    mprFree(ha2);

    return 0;
}


/*
 *  MD5 initialization. Begins an MD5 operation, writing a new context.
 */ 
static void initMD5(MD5CONTEXT *context)
{
    context->count[0] = context->count[1] = 0;

    /*
     *  Load constants
     */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}


/*
 *  MD5 block update operation. Continues an MD5 message-digest operation, processing another message block, 
 *  and updating the context.
 */
static void update(MD5CONTEXT *context, uchar *input, uint inputLen)
{
    uint    i, index, partLen;

    index = (uint) ((context->count[0] >> 3) & 0x3F);

    if ((context->count[0] += ((uint)inputLen << 3)) < ((uint)inputLen << 3)){
        context->count[1]++;
    }
    context->count[1] += ((uint)inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy((uchar*) &context->buffer[index], (uchar*) input, partLen);
        transform(context->state, context->buffer);

        for (i = partLen; i + 63 < inputLen; i += 64) {
            transform(context->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy((uchar*) &context->buffer[index], (uchar*) &input[i], inputLen-i);
}


/*
 *  MD5 finalization. Ends an MD5 message-digest operation, writing the message digest and zeroizing the context.
 */ 
static void finalize(uchar digest[16], MD5CONTEXT *context)
{
    uchar   bits[8];
    uint    index, padLen;

    /* Save number of bits */
    encode(bits, context->count, 8);

    /* Pad out to 56 mod 64. */
    index = (uint)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update(context, PADDING, padLen);

    /* Append length (before padding) */
    update(context, bits, 8);
    /* Store state in digest */
    encode(digest, context->state, 16);

    /* Zero sensitive information. */
    memset((uchar*)context, 0, sizeof (*context));
}


/*
 *  MD5 basic transformation. Transforms state based on block.
 */
static void transform(uint state[4], uchar block[64])
{
    uint a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zero sensitive information. */
    memset((uchar*) x, 0, sizeof(x));
}


/*
 *  Encodes input(uint) into output(uchar). Assumes len is a multiple of 4.
 */
static void encode(uchar *output, uint *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (uchar) (input[i] & 0xff);
        output[j+1] = (uchar) ((input[i] >> 8) & 0xff);
        output[j+2] = (uchar) ((input[i] >> 16) & 0xff);
        output[j+3] = (uchar) ((input[i] >> 24) & 0xff);
    }
}


/*
 *  Decodes input(uchar) into output(uint). Assumes len is a multiple of 4.
 */
static void decode(uint *output, uchar *input, uint len)
{
    uint    i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint) input[j]) | (((uint) input[j+1]) << 8) | (((uint) input[j+2]) << 16) | 
            (((uint) input[j+3]) << 24);
}



/*
 *  @copy   custom
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1994-2011. All Rights Reserved.
 *  Portions Copyright (C) 1991-2, RSA Data Security, Inc. All rights reserved. 
 *  
 *  RSA License:
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *  
 *  RSA License Details
 *  -------------------
 *  
 *  License to copy and use this software is granted provided that it is 
 *  identified as the "RSA Data Security, Inc. MD5 Message-Digest Algorithm" 
 *  in all material mentioning or referencing this software or this function.
 *  
 *  License is also granted to make and use derivative works provided that such
 *  works are identified as "derived from the RSA Data Security, Inc. MD5 
 *  Message-Digest Algorithm" in all material mentioning or referencing the 
 *  derived work.
 *  
 *  RSA Data Security, Inc. makes no representations concerning either the 
 *  merchantability of this software or the suitability of this software for 
 *  any particular purpose. It is provided "as is" without express or implied 
 *  warranty of any kind.
 *  
 *  These notices must be retained in any copies of any part of this
 *  documentation and/or software.
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprCrypt.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprDisk.c"
 */
/************************************************************************/

/**
 *  mprDisk.c - File services for systems with a (disk) based file system.
 *
 *  This module is not thread safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if !BLD_FEATURE_ROMFS

static int closeFile(MprFile *file);
static int getPathInfo(MprDiskFileSystem *fileSystem, cchar *path, MprPath *info);


static MprFile *openFile(MprCtx ctx, MprFileSystem *fileSystem, cchar *path, int omode, int perms)
{
    MprDiskFileSystem   *dfs;
    MprFile             *file;
    
    mprAssert(path);

    dfs = (MprDiskFileSystem*) fileSystem;
    file = mprAllocObjWithDestructorZeroed(ctx, MprFile, closeFile);
    
    file->fd = open(path, omode, perms);
    if (file->fd < 0) {
        mprFree(file);
        return 0;
    }
    return file;
}


static int closeFile(MprFile *file)
{
    MprBuf  *bp;

    mprAssert(file);

    if (file == 0) {
        return 0;
    }
    bp = file->buf;
    if (bp && (file->mode & (O_WRONLY | O_RDWR))) {
        mprFlush(file);
    }
    if (file->fd >= 0) {
        close(file->fd);
        file->fd = -1;
    }
    return 0;
}


static int readFile(MprFile *file, void *buf, uint size)
{
    mprAssert(file);
    mprAssert(buf);

    return read(file->fd, buf, size);
}


static int writeFile(MprFile *file, cvoid *buf, uint count)
{
    mprAssert(file);
    mprAssert(buf);

#if VXWORKS
    return write(file->fd, (void*) buf, count);
#else
    return write(file->fd, buf, count);
#endif
}


static long seekFile(MprFile *file, int seekType, long distance)
{
    mprAssert(file);

    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }
    return lseek(file->fd, distance, seekType);
}


static bool accessPath(MprDiskFileSystem *fileSystem, cchar *path, int omode)
{
    return access(path, omode) == 0;
}


static int deletePath(MprDiskFileSystem *fileSystem, cchar *path)
{
    MprPath     info;

    if (getPathInfo(fileSystem, path, &info) == 0 && info.isDir) {
        return rmdir((char*) path);
    }
#if WIN
{
    int i, rc;
    for (i = 0; i < 100; i++) {
        rc = DeleteFile((char*) path);
        if (rc != 0) {
            rc = 0;
            break;
        }
        mprSleep(fileSystem, 10);
    }
    return rc;
}
#else
    return unlink((char*) path);
#endif
}
 

static int makeDir(MprDiskFileSystem *fileSystem, cchar *path, int perms)
{
#if VXWORKS
    return mkdir((char*) path);
#else
    return mkdir(path, perms);
#endif
}


static int makeLink(MprDiskFileSystem *fileSystem, cchar *path, cchar *target, int hard)
{
#if BLD_UNIX_LIKE
    if (hard) {
        return link(target, path);
    } else {
        return symlink(target, path);
    }
#else
    return MPR_ERR_BAD_STATE;
#endif
}


static int getPathInfo(MprDiskFileSystem *fileSystem, cchar *path, MprPath *info)
{
    struct stat s;
#if BLD_WIN_LIKE
    cchar       *ext;
    char        *allocPath;

    mprAssert(path);
    mprAssert(info);

    allocPath = 0;
    info->checked = 1;
    info->valid = 0;

    if (stat(path, &s) < 0) {
        mprFree(allocPath);
        return -1;
    }

    info->valid = 1;
    info->size = s.st_size;
    info->atime = s.st_atime;
    info->ctime = s.st_ctime;
    info->mtime = s.st_mtime;
    info->inode = s.st_ino;
    info->isDir = (s.st_mode & S_IFDIR) != 0;
    info->isReg = (s.st_mode & S_IFREG) != 0;
    info->isLink = 0;
    ext = mprGetPathExtension(fileSystem, path);
    if (ext && strcmp(ext, "lnk") == 0) {
        info->isLink = 1;
    }

#if !WINCE
    /*
     *  Work hard on windows to determine if the file is a regular file.
     */
    if (info->isReg) {
        long    att;

        if ((att = GetFileAttributes(path)) == -1) {
            mprFree(allocPath);
            return -1;
        }
        if (att & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ENCRYPTED |
                FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_OFFLINE)) {
            /*
             *  Catch accesses to devices like CON, AUX, NUL, LPT etc att will be set to ENCRYPTED on Win9X and NT.
             */
            info->isReg = 0;
        }
        if (info->isReg) {
            HANDLE handle;
            handle = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
            if (handle == INVALID_HANDLE_VALUE) {
                info->isReg = 0;
            } else {
                long    fileType;
                fileType = GetFileType(handle);
                if (fileType == FILE_TYPE_CHAR || fileType == FILE_TYPE_PIPE) {
                    info->isReg = 0;
                }
                CloseHandle(handle);
            }
        }
    }
    if (strcmp(path, "nul") == 0) {
        info->isReg = 0;
    }
#endif

    mprFree(allocPath);

#else /* !BLD_WIN_LIKE */
    mprAssert(path);
    mprAssert(info);

    info->valid = 0;
    info->checked = 1;

#if VXWORKS
    if (stat((char*) path, &s) < 0) {
        return MPR_ERR_CANT_ACCESS;
    }
#else
    if (lstat((char*) path, &s) < 0) {
        return MPR_ERR_CANT_ACCESS;
    }
#endif

    info->valid = 1;
    info->size = s.st_size;
    info->atime = s.st_atime;
    info->ctime = s.st_ctime;
    info->mtime = s.st_mtime;
    info->inode = s.st_ino;
    info->isDir = S_ISDIR(s.st_mode);
    info->isReg = S_ISREG(s.st_mode);
    info->perms = s.st_mode & 07777;
#ifdef S_ISLNK
    info->isLink = S_ISLNK(s.st_mode);
    if (info->isLink) {
        struct stat realInfo;
        if (stat((char*) path, &realInfo) < 0) {
            return MPR_ERR_CANT_ACCESS;
        }
        info->size = realInfo.st_size;
    }
#endif

    if (strcmp(path, "/dev/null") == 0) {
        info->isReg = 0;
    }

#endif
    return 0;
}
 

static char *getPathLink(MprDiskFileSystem *fileSystem, cchar *path)
{
#if BLD_UNIX_LIKE
    char    pbuf[MPR_MAX_PATH];
    int     len;

    if ((len = readlink(path, pbuf, sizeof(pbuf) - 1)) < 0) {
        return NULL;
    }
    pbuf[len] = '\0';
    return mprStrdup(fileSystem, pbuf);
#else
    return NULL;
#endif
}


MprDiskFileSystem *mprCreateDiskFileSystem(MprCtx ctx, cchar *path)
{
    MprFileSystem       *fs;
    MprDiskFileSystem   *dfs;

    dfs = mprAllocObjZeroed(ctx, MprDiskFileSystem);
    if (dfs == 0) {
        return 0;
    }
    
    /*
     *  Temporary
     */
    fs = (MprFileSystem*) dfs;

    dfs->accessPath = accessPath;
    dfs->deletePath = deletePath;
    dfs->getPathInfo = getPathInfo;
    dfs->getPathLink = getPathLink;
    dfs->makeDir = makeDir;
    dfs->makeLink = makeLink;
    dfs->openFile = openFile;
    dfs->closeFile = closeFile;
    dfs->readFile = readFile;
    dfs->seekFile = seekFile;
    dfs->writeFile = writeFile;

#if !WINCE
    dfs->stdError = mprAllocObjZeroed(dfs, MprFile);
    if (dfs->stdError == 0) {
        mprFree(dfs);
    }
    dfs->stdError->fd = 2;
    dfs->stdError->fileSystem = fs;
    dfs->stdError->mode = O_WRONLY;

    dfs->stdInput = mprAllocObjZeroed(dfs, MprFile);
    if (dfs->stdInput == 0) {
        mprFree(dfs);
    }
    dfs->stdInput->fd = 0;
    dfs->stdInput->fileSystem = fs;
    dfs->stdInput->mode = O_RDONLY;

    dfs->stdOutput = mprAllocObjZeroed(dfs, MprFile);
    if (dfs->stdOutput == 0) {
        mprFree(dfs);
    }
    dfs->stdOutput->fd = 1;
    dfs->stdOutput->fileSystem = fs;
    dfs->stdOutput->mode = O_WRONLY;
#endif
    return dfs;
}
#endif /* !BLD_FEATURE_ROMFS */


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

/************************************************************************/
/*
 *  End of file "../src/mprDisk.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprEvent.c"
 */
/************************************************************************/

/*
 *  mprEvent.c - Event queue and event service
 *
 *  This module is thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static void appendEvent(MprEvent *prior, MprEvent *event);
static int  eventDestructor(MprEvent *event);
static void queueEvent(MprDispatcher *es, MprEvent *event);
static void removeEvent(MprEvent *event);

/*
 *  Initialize the event service.
 */
MprDispatcher *mprCreateDispatcher(MprCtx ctx)
{
    MprDispatcher   *dispatcher;

    dispatcher = mprAllocObjWithDestructorZeroed(ctx, MprDispatcher, eventDestructor);
    if (dispatcher == 0) {
        return 0;
    }

#if BLD_FEATURE_MULTITHREAD
    dispatcher->mutex = mprCreateLock(dispatcher);
    dispatcher->spin = mprCreateSpinLock(dispatcher);
    dispatcher->cond = mprCreateCond(dispatcher);
    if (dispatcher->mutex == 0 || dispatcher->spin == 0 || dispatcher->cond == 0) {
        mprFree(dispatcher);
        return 0;
    }
#endif
    dispatcher->eventQ.next = &dispatcher->eventQ;
    dispatcher->eventQ.prev = &dispatcher->eventQ;
    dispatcher->timerQ.next = &dispatcher->timerQ;
    dispatcher->timerQ.prev = &dispatcher->timerQ;
    dispatcher->now = mprGetTime(ctx);
    return dispatcher;
}


MprDispatcher *mprGetDispatcher(MprCtx ctx)
{
    return mprGetMpr(ctx)->dispatcher;
}


/*
 *  Queue a new event for service according to its priority and position in the event queue. Period is used as 
 *  the delay before running the event and as the period between events for continuous events.
 */
MprEvent *mprCreateEvent(MprDispatcher *dispatcher, MprEventProc proc, int period, int priority, void *data, int flags)
{
    MprEvent        *event;

#if UNUSED
    if (mprIsExiting(dispatcher)) {
        return 0;
    }
#endif
    event = mprAllocObjWithDestructor(dispatcher, MprEvent, eventDestructor);
    if (event == 0) {
        return 0;
    }
    event->proc = proc;
    event->period = period;
    event->priority = priority;
    event->data = data;
    event->flags = flags;
    event->timestamp = dispatcher->now;
    event->due = event->timestamp + period;
    event->dispatcher = dispatcher;

    /*
     *  Append in delay and priority order
     */
    queueEvent(dispatcher, event);
    mprWakeDispatcher(dispatcher);
    return event;
}


/*
 *  Called in response to mprFree on the event service
 */
static int eventDestructor(MprEvent *event)
{
    mprAssert(event);

    if (event->next) {
        mprRemoveEvent(event);
    }
    return 0;
}


/*  
 *  Remove an event from the event queues. Use mprRescheduleEvent to restart.
 */
void mprRemoveEvent(MprEvent *event)
{
    MprDispatcher   *dispatcher;
    Mpr             *mpr;

    mpr = mprGetMpr(event);
    dispatcher = mpr->dispatcher;

    mprSpinLock(dispatcher->spin);
    removeEvent(event);
    if (dispatcher->timerQ.next != &dispatcher->timerQ) {
        dispatcher->lastEventDue = dispatcher->timerQ.prev->due;
    } else {
        dispatcher->lastEventDue = dispatcher->now;
    }
    mprSpinUnlock(dispatcher->spin);
}


void mprStopContinuousEvent(MprEvent *event)
{
    event->flags &= ~MPR_EVENT_CONTINUOUS;
}


void mprRestartContinuousEvent(MprEvent *event)
{
    event->flags |= MPR_EVENT_CONTINUOUS;
    mprRescheduleEvent(event, event->period);
}


/*
 *  Internal routine to queue an event to the event queue in delay and priority order. 
 */
static void queueEvent(MprDispatcher *dispatcher, MprEvent *event)
{
    MprEvent    *prior, *q;

    mprSpinLock(dispatcher->spin);
    if (event->due > dispatcher->now) {
        /*
         *  Due in the future some time
         */
        q = &dispatcher->timerQ;

        if (event->due > dispatcher->lastEventDue) {
            prior = q->prev;
            dispatcher->lastEventDue = event->due;

        } else {
            /*
             *  Scan backwards for the event just prior to this new event
             */
            for (prior = q->prev; prior != q; prior = prior->prev) {
                if (event->due > prior->due) {
                    break;
                } else if (event->due == prior->due && event->priority >= prior->priority) {
                    break;
                }
            }
        }
    } else {
        q = &dispatcher->eventQ;
        for (prior = q->prev; prior != q; prior = prior->prev) {
            if (event->due > prior->due) {
                break;
            } else if (event->due == prior->due && event->priority >= prior->priority) {
                break;
            }
        }
        dispatcher->eventCounter++;
    }

    /*
     *  Will assert if already in the queue
     */
    mprAssert(prior != event);
    appendEvent(prior, event);
    mprSpinUnlock(dispatcher->spin);
}


/*
 *  Get the next event from the front of the event queue
 *  Return 0 if not event.
 */
MprEvent *mprGetNextEvent(MprDispatcher *dispatcher)
{
    MprEvent    *event, *next;

    mprSpinLock(dispatcher->spin);
    event = dispatcher->eventQ.next;
    if (event != &dispatcher->eventQ) {
        removeEvent(event);

    } else {
        /*
         *  Move due timer events to the event queue. Allows priorities to work.
         */
        for (event = dispatcher->timerQ.next; event != &dispatcher->timerQ; event = next) {
            if (event->due > dispatcher->now) {
                break;
            }
            next = event->next;
            removeEvent(event);
            appendEvent(&dispatcher->eventQ, event);
            dispatcher->eventCounter++;
        }
            
        event = dispatcher->eventQ.next;
        if (event != &dispatcher->eventQ) {
            removeEvent(event);
        } else {
            event = 0;
        }
    }
    mprSpinUnlock(dispatcher->spin);
    return event;
}


void mprWakeDispatcher(MprDispatcher *dispatcher)
{
#if BLD_FEATURE_MULTITHREAD
    mprSpinLock(dispatcher->spin);
    if (dispatcher->flags & MPR_DISPATCHER_WAIT_EVENTS) {
        mprSignalCond(dispatcher->cond);
    }
    if (dispatcher->flags & MPR_DISPATCHER_WAIT_IO) {
        mprWakeWaitService(dispatcher);
    }
    mprSpinUnlock(dispatcher->spin);
#endif
}


/*
 *  Service events until the timeout expires or if MPR_SERVICE_ONE_THING is set, then until one event is received.
 */
int mprServiceEvents(MprDispatcher *dispatcher, int timeout, int flags)
{
    MprTime     mark, remaining;
    MprEvent    *event;
    int         delay, total, rc;

    mprSpinLock(dispatcher->spin);
    if (flags & MPR_SERVICE_EVENTS) {
        dispatcher->flags |= MPR_DISPATCHER_WAIT_EVENTS;
    }
    if (flags & MPR_SERVICE_IO) {
        dispatcher->flags |= MPR_DISPATCHER_WAIT_IO;
    }
    mprSpinUnlock(dispatcher->spin);

    mark = dispatcher->now = mprGetTime(dispatcher);
    if (timeout < 0) {
        timeout = MAXINT;
    }
    remaining = timeout;
    total = 0;

    do {
        if (flags & MPR_SERVICE_EVENTS) {
            if ((event = mprGetNextEvent(dispatcher)) != 0) {
                mprDoEvent(event, 0);
                total++;
                if (flags & MPR_SERVICE_ONE_THING) {
                    break;
                }
                continue;
            }
        } 
        if (mprIsComplete(dispatcher)) {
            break;
        }
        if (flags & MPR_SERVICE_IO) {
            dispatcher->now = mprGetTime(dispatcher);
            delay = mprGetIdleTime(dispatcher);
            delay = (int) min(remaining, delay);
            if ((rc = mprWaitForIO(mprGetMpr(dispatcher)->waitService, delay)) > 0) {
                total += rc;
            }
#if BLD_FEATURE_MULTITHREAD
        } else if (MPR_SERVICE_EVENTS && remaining > 0) {
            mprWaitForCond(dispatcher->cond, (int) remaining);
#endif
        }
        remaining = mprGetRemainingTime(dispatcher, mark, timeout);
    } while (remaining > 0 && !mprIsComplete(dispatcher) && !(flags & MPR_SERVICE_ONE_THING));

    mprSpinLock(dispatcher->spin);
    dispatcher->flags &= ~MPR_DISPATCHER_WAIT_IO;
    dispatcher->flags &= ~MPR_DISPATCHER_WAIT_EVENTS;
    mprSpinUnlock(dispatcher->spin);
    return total;
}


void mprDoEvent(MprEvent *event, void *workerThread)
{
    MprDispatcher   *dispatcher;

#if BLD_FEATURE_MULTITHREAD
    if (event->flags & MPR_EVENT_THREAD && workerThread == 0) {
        /*
         *  Recall mprDoEvent but via a worker thread. If none available, then handle inline.
         */
        if (mprStartWorker(event->dispatcher, (MprWorkerProc) mprDoEvent, (void*) event, event->priority) == 0) {
            return;
        }
    }
#endif
    /*
     *  If it is a continuous event, we requeue here so that the event callback has the option of deleting the event.
     */
    dispatcher = mprGetMpr(event)->dispatcher;
    if (event->flags & MPR_EVENT_CONTINUOUS) {
        event->timestamp = dispatcher->now;
        event->due = event->timestamp + event->period;
        queueEvent(dispatcher, event);
    }
    /*
     *  The callback can delete the event. NOTE: callback events MUST NEVER block.
     */
    if (event->proc) {
        mprSpinLock(dispatcher->spin);
        dispatcher->flags |= MPR_DISPATCHER_DO_EVENT;
        mprSpinUnlock(dispatcher->spin);

        (*event->proc)(event->data, event);

        mprSpinLock(dispatcher->spin);
        dispatcher->flags &= ~MPR_DISPATCHER_DO_EVENT;
        mprSpinUnlock(dispatcher->spin);
    }
}


/*
 *  Return the time till the next event.
 */
int mprGetIdleTime(MprDispatcher *dispatcher)
{
    int     delay;
    
    mprSpinLock(dispatcher->spin);
    if (dispatcher->eventQ.next != &dispatcher->eventQ) {
        delay = 0;
    } else if (dispatcher->timerQ.next != &dispatcher->timerQ) {
        delay = (int) (dispatcher->timerQ.next->due - dispatcher->now);
        if (delay < 0) {
            delay = 0;
        }
        
    } else {
        delay = INT_MAX;
    }
    mprSpinUnlock(dispatcher->spin);
    return delay;
}


int mprGetEventCounter(MprDispatcher *dispatcher)
{
    return dispatcher->eventCounter;
}


void mprRescheduleEvent(MprEvent *event, int period)
{
    MprDispatcher   *dispatcher;
    Mpr             *mpr;

    mpr = mprGetMpr(event);
    dispatcher = mprGetMpr(event)->dispatcher;

    event->period = period;
    event->timestamp = dispatcher->now;
    event->due = event->timestamp + period;

    if (event->next) {
        mprRemoveEvent(event);
    }
    queueEvent(mpr->dispatcher, event);
    mprWakeDispatcher(dispatcher);
}


MprEvent *mprCreateTimerEvent(MprDispatcher *dispatcher, MprEventProc proc, int period, int priority, void *data, int flags)
{
    return mprCreateEvent(dispatcher, proc, period, priority, data, MPR_EVENT_CONTINUOUS | flags);
}


/*
 *  Append a new event. Must be locked when called.
 */
static void appendEvent(MprEvent *prior, MprEvent *event)
{
    event->prev = prior;
    event->next = prior->next;
    prior->next->prev = event;
    prior->next = event;
}


/*
 *  Remove an event. Must be locked when called.
 */
static void removeEvent(MprEvent *event)
{
    event->next->prev = event->prev;
    event->prev->next = event->next;
    event->next = 0;
    event->prev = 0;
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprEvent.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprFile.c"
 */
/************************************************************************/

/**
 *  mprFile.c - File services.
 *
 *  This modules provides a simple cross platform file I/O abstraction. It uses the MprFileSystem to provide I/O services.
 *
 *  This module is not thread safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  fillBuf(MprFile *file);


MprFile *mprAttachFd(MprCtx ctx, int fd, cchar *name, int omode)
{
    MprFileSystem   *fs;
    MprFile         *file;

    fs = mprLookupFileSystem(ctx, "/");

    file = mprAllocObjZeroed(ctx, MprFile);
    if (file) {
        file->fd = fd;
        file->fileSystem = fs;
#if BLD_DEBUG
        file->path = mprStrdup(file, name);
#endif
        file->mode = omode;
    }
    return file;
}


MprFile *mprGetStderr(MprCtx ctx)
{
    MprFileSystem   *fs;
    fs = mprLookupFileSystem(ctx, NULL);
    return fs->stdError;
}


MprFile *mprGetStdin(MprCtx ctx)
{
    MprFileSystem   *fs;
    fs = mprLookupFileSystem(ctx, NULL);
    return fs->stdInput;
}


MprFile *mprGetStdout(MprCtx ctx)
{
    MprFileSystem   *fs;
    fs = mprLookupFileSystem(ctx, NULL);
    return fs->stdOutput;
}


MprFile *mprOpen(MprCtx ctx, cchar *path, int omode, int perms)
{
    MprFileSystem   *fs;
    MprFile         *file;
    MprPath         info;

    fs = mprLookupFileSystem(ctx, path);

    file = fs->openFile(ctx, fs, path, omode, perms);
    if (file) {
        file->fileSystem = fs;
#if BLD_DEBUG
        file->path = mprStrdup(file, path);
#endif
        if (omode & (O_WRONLY | O_RDWR)) {
            /*
             *  OPT. Should compute this lazily.
             */
            fs->getPathInfo(fs, path, &info);
            file->size = (MprOffset) info.size;
        }
        file->mode = omode;
        file->perms = perms;
    }
    return file;
}


int mprRead(MprFile *file, void *buf, uint size)
{
    MprFileSystem   *fs;
    MprBuf          *bp;
    void            *bufStart;
    int             bytes, totalRead;

    mprAssert(file);
    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }

    fs = file->fileSystem;
    bp = file->buf;
    if (bp == 0) {
        totalRead = fs->readFile(file, buf, size);

    } else {
        bufStart = buf;
        while (size > 0) {
            if (mprGetBufLength(bp) == 0) {
                bytes = fillBuf(file);
                if (bytes <= 0) {
                    return -1;
                }
            }
            bytes = min((int) size, mprGetBufLength(bp));
            memcpy(buf, mprGetBufStart(bp), bytes);
            mprAdjustBufStart(bp, bytes);
            buf = (void*) (((char*) buf) + bytes);
            size -= bytes;
        }
        totalRead = (int) ((char*) buf - (char*) bufStart);
    }
    file->pos += totalRead;
    return totalRead;
}


int mprWrite(MprFile *file, cvoid *buf, uint count)
{
    MprFileSystem   *fs;
    MprBuf          *bp;
    int             bytes, written;

    mprAssert(file);
    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }

    fs = file->fileSystem;
    bp = file->buf;
    if (bp == 0) {
        if ((written = fs->writeFile(file, buf, count)) < 0) {
            return written;
        }

    } else {
        written = 0;
        while (count > 0) {
            bytes = mprPutBlockToBuf(bp, buf, count);
            if (bytes < 0) {
                return bytes;
            } 
            if (bytes != (int) count) {
                mprFlush(file);
            }
            count -= bytes;
            written += bytes;
            buf = (char*) buf + bytes;
        }
    }
    file->pos += written;
    if (file->pos > file->size) {
        file->size = file->pos;
    }
    return written;
}


int mprWriteString(MprFile *file, cchar *str)
{
    return mprWrite(file, str, strlen(str));
}


int mprWriteFormat(MprFile *file, cchar *fmt, ...)
{
    va_list     ap;
    char        *buf;
    int         rc;

    rc = -1;
    va_start(ap, fmt);
    if ((buf = mprVasprintf(file, -1, fmt, ap)) != NULL) {
        rc = mprWriteString(file, buf);
    }
    va_end(ap);
    return rc;
}


int mprFlush(MprFile *file)
{
    MprFileSystem   *fs;
    MprBuf          *bp;
    int             len, rc;

    mprAssert(file);
    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }
    if (file->buf == 0) {
        return 0;
    }

    if (file->mode & (O_WRONLY | O_RDWR)) {
        fs = file->fileSystem;
        bp = file->buf;
        while (mprGetBufLength(bp) > 0) {
            len = mprGetBufLength(bp);
            rc = fs->writeFile(file, mprGetBufStart(bp), len);
            if (rc < 0) {
                return rc;
            }
            mprAdjustBufStart(bp, rc);
        }
        mprFlushBuf(bp);
    }
    return 0;
}


long mprSeek(MprFile *file, int seekType, long pos)
{
    MprFileSystem   *fs;

    mprAssert(file);

    fs = file->fileSystem;

    if (file->buf) {
        if (! (seekType == SEEK_CUR && pos == 0)) {
            /*
             *  Discard buffering as we may be seeking outside the buffer.
             *  OPT. Could be smarter about this and preserve the buffer.
             */
            if (file->mode & (O_WRONLY | O_RDWR)) {
                if (mprFlush(file) < 0) {
                    return MPR_ERR_CANT_WRITE;
                }
            }
            if (file->buf) {
                mprFlushBuf(file->buf);
            }
        }
    }

    if (seekType == SEEK_SET) {
        file->pos = pos;

    } else if (seekType == SEEK_CUR) {
        file->pos += pos;

    } else {
        file->pos = fs->seekFile(file, SEEK_END, 0);
    }

    if (fs->seekFile(file, SEEK_SET, (long) file->pos) != (long) file->pos) {
        return MPR_ERR;
    }
    if (file->mode & (O_WRONLY | O_RDWR)) {
        if (file->pos > file->size) {
            file->size = file->pos;
        }
    }
    return (long) file->pos;
}


MprOffset mprGetFilePosition(MprFile *file)
{
    return file->pos;
}


MprOffset mprGetFileSize(MprFile *file)
{
    return file->size;
}


/*
 *  Fill the read buffer. Return the new buffer length. Only called when the buffer is empty.
 */
static int fillBuf(MprFile *file)
{
    MprFileSystem   *fs;
    MprBuf          *bp;
    int             len;

    bp = file->buf;
    fs = file->fileSystem;

    mprAssert(mprGetBufLength(bp) == 0);
    mprFlushBuf(bp);

    len = fs->readFile(file, mprGetBufStart(bp), mprGetBufSpace(bp));
    if (len <= 0) {
        return len;
    }
    mprAdjustBufEnd(bp, len);
    return len;
}


/*
 *  Get a string from the file. This will put the file into buffered mode.
 */
char *mprGets(MprFile *file, char *buf, uint size)
{
    MprBuf          *bp;
    MprFileSystem   *fs;
    int             count, c;

    mprAssert(file);
    if (file == 0) {
        return 0;
    }

    fs = file->fileSystem;
    if (file->buf == 0) {
        file->buf = mprCreateBuf(file, MPR_BUFSIZE, MPR_MAX_STRING);
    }
    bp = file->buf;

    /*
     *  Must leave room for null
     */
    count = 0;
    while (--size > 0) {
        if (mprGetBufLength(bp) == 0) {
            if (fillBuf(file) <= 0) {
                return 0;
            }
        }
        if ((c = mprGetCharFromBuf(bp)) == '\n') {
            file->pos++;
            break;
        }
        buf[count++] = c;
    }
    buf[count] = '\0';
    file->pos += count;
    return buf;
}


/*
 *  Put a string to the file. This will put the file into buffered mode.
 */
int mprPuts(MprFile *file, cchar *str)
{
    MprBuf  *bp;
    char    *buf;
    int     total, bytes, count;

    mprAssert(file);
    count = strlen(str);

    /*
     *  Buffer output and flush when full.
     */
    if (file->buf == 0) {
        file->buf = mprCreateBuf(file, MPR_BUFSIZE, 0);
        if (file->buf == 0) {
            return MPR_ERR_CANT_ALLOCATE;
        }
    }
    bp = file->buf;

    if (mprGetBufLength(bp) > 0 && mprGetBufSpace(bp) < (int) count) {
        mprFlush(file);
    }
    total = 0;
    buf = (char*) str;

    while (count > 0) {
        bytes = mprPutBlockToBuf(bp, buf, count);
        if (bytes < 0) {
            return MPR_ERR_CANT_ALLOCATE;

        } else if (bytes == 0) {
            if (mprFlush(file) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            continue;
        }
        count -= bytes;
        buf += bytes;
        total += bytes;
        file->pos += bytes;
    }
    return total;
}


/*
 *  Get a character from the file. This will put the file into buffered mode.
 */
int mprGetc(MprFile *file)
{
    MprBuf  *bp;
    int     len;

    mprAssert(file);

    if (file == 0) {
        return MPR_ERR;
    }
    if (file->buf == 0) {
        file->buf = mprCreateBuf(file, MPR_BUFSIZE, MPR_MAX_STRING);
    }
    bp = file->buf;

    if (mprGetBufLength(bp) == 0) {
        len = fillBuf(file);
        if (len <= 0) {
            return -1;
        }
    }
    if (mprGetBufLength(bp) == 0) {
        return 0;
    }
    file->pos++;
    return mprGetCharFromBuf(bp);
}


/*
 *  Peek at a character from the file without disturbing the read position. This will put the file into buffered mode.
 */
int mprPeekc(MprFile *file)
{
    MprBuf  *bp;
    int     len;

    mprAssert(file);

    if (file == 0) {
        return MPR_ERR;
    }

    if (file->buf == 0) {
        file->buf = mprCreateBuf(file, MPR_BUFSIZE, MPR_MAX_STRING);
    }
    bp = file->buf;

    if (mprGetBufLength(bp) == 0) {
        len = fillBuf(file);
        if (len <= 0) {
            return -1;
        }
    }
    if (mprGetBufLength(bp) == 0) {
        return 0;
    }
    return ((uchar*) mprGetBufStart(bp))[0];
}


/*
 *  Put a character to the file. This will put the file into buffered mode.
 */
int mprPutc(MprFile *file, int c)
{
    mprAssert(file);

    if (file == 0) {
        return -1;
    }
    if (file->buf) {
        if (mprPutCharToBuf(file->buf, c) != 1) {
            return MPR_ERR_CANT_WRITE;
        }
        file->pos++;
        return 1;

    }
    return mprWrite(file, &c, 1);
}


/*
 *  Enable and control file buffering
 */
int mprEnableFileBuffering(MprFile *file, int initialSize, int maxSize)
{
    mprAssert(file);

    if (file == 0) {
        return MPR_ERR_BAD_STATE;
    }
    if (initialSize <= 0) {
        initialSize = MPR_BUFSIZE;
    }
    if (maxSize <= 0) {
        maxSize = MPR_BUFSIZE;
    }
    if (maxSize <= initialSize) {
        maxSize = initialSize;
    }
    if (file->buf == 0) {
        file->buf = mprCreateBuf(file, initialSize, maxSize);
    }
    return 0;
}


void mprDisableFileBuffering(MprFile *file)
{
    mprFlush(file);
    mprFree(file->buf);
    file->buf = 0;
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprFile.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprFileSystem.c"
 */
/************************************************************************/

/**
 *  mprFileSystem.c - File system services.
 *
 *  This module provides a simple cross platform file system abstraction. File systems provide a file system switch and 
 *  underneath a file system provider that implements actual I/O.
 *
 *  This module is not thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




MprFileSystem *mprCreateFileSystem(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    Mpr             *mpr;
    char            *cp;

    mpr = mprGetMpr(ctx);
    mprAssert(mpr);
    
#if BLD_FEATURE_ROMFS
    fs = (MprFileSystem*) mprCreateRomFileSystem(ctx, path);
#elif BREW
    fs = (MprFileSystem*) mprCreateBrewFileSystem(ctx, path);
#else
    fs = (MprFileSystem*) mprCreateDiskFileSystem(ctx, path);
#endif

#if BLD_WIN_LIKE
    fs->separators = mprStrdup(fs, "\\/");
    fs->newline = mprStrdup(fs, "\r\n");
#else
    fs->separators = mprStrdup(fs, "/");
    fs->newline = mprStrdup(fs, "\n");
#endif

#if BLD_WIN_LIKE || MACOSX
    fs->caseSensitive = 0;
#else
    fs->caseSensitive = 1;
#endif

#if BLD_WIN_LIKE || VXWORKS
    fs->hasDriveSpecs = 1;
#endif

    if (mpr->fileSystem == NULL) {
        mpr->fileSystem = fs;
    }
    fs->root = mprGetAbsPath(ctx, path);
    if ((cp = strpbrk(fs->root, fs->separators)) != 0) {
        *++cp = '\0';
    }
    return fs;
}


void mprAddFileSystem(MprCtx ctx, MprFileSystem *fs)
{
    mprAssert(ctx);
    mprAssert(fs);
    
    mprGetMpr(ctx)->fileSystem = fs;
}


/*
 *  Note: path can be null
 */
MprFileSystem *mprLookupFileSystem(MprCtx ctx, cchar *path)
{
    mprAssert(ctx);
    
    return mprGetMpr(ctx)->fileSystem;
}


cchar *mprGetPathNewline(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    mprAssert(ctx);
    mprAssert(path);

    fs = mprLookupFileSystem(ctx, path);
    return fs->newline;
}


cchar *mprGetPathSeparators(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    mprAssert(ctx);
    mprAssert(path);

    fs = mprLookupFileSystem(ctx, path);
    return fs->separators;
}


void mprSetPathSeparators(MprCtx ctx, cchar *path, cchar *separators)
{
    MprFileSystem   *fs;

    mprAssert(ctx);
    mprAssert(path);
    mprAssert(separators);
    
    fs = mprLookupFileSystem(ctx, path);
    mprFree(fs->separators);
    fs->separators = mprStrdup(fs, separators);
}


void mprSetPathNewline(MprCtx ctx, cchar *path, cchar *newline)
{
    MprFileSystem   *fs;
    
    mprAssert(ctx);
    mprAssert(path);
    mprAssert(newline);
    
    fs = mprLookupFileSystem(ctx, path);
    mprFree(fs->newline);
    fs->newline = mprStrdup(fs, newline);
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprFileSystem.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprHash.c"
 */
/************************************************************************/

/*
 *  mprHash.cpp - Fast hashing table lookup module
 *
 *  This hash table uses a fast key lookup mechanism. Keys are strings and the value entries are arbitrary pointers.
 *  The keys are hashed into a series of buckets which then have a chain of hash entries using the standard doubly
 *  linked list classes (List/Link). The chain in in collating sequence so search time through the chain is on
 *  average (N/hashSize)/2.
 *
 *  This module is not thread-safe. It is the callers responsibility to perform all thread synchronization.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int hashIndex(cchar *key, int size);
static MprHash  *lookupInner(int *bucketIndex, MprHash **prevSp, MprHashTable *table, cchar *key);

/*
 *  Create a new hash table of a given size. Caller should provide a size that is a prime number for the greatest
 *  efficiency. Caller should use mprFree to free the hash table.
 */

MprHashTable *mprCreateHash(MprCtx ctx, int hashSize)
{
    MprHashTable    *table;

    table = mprAllocObjZeroed(ctx, MprHashTable);
    if (table == 0) {
        return 0;
    }

    if (hashSize < MPR_DEFAULT_HASH_SIZE) {
        hashSize = MPR_DEFAULT_HASH_SIZE;
    }
    table->hashSize = hashSize;

    table->count = 0;
    table->hashSize = hashSize;
    table->buckets = (MprHash**) mprAllocZeroed(table, sizeof(MprHash*) * hashSize);

    if (table->buckets == 0) {
        mprFree(table);
        return 0;
    }

    return table;
}


MprHashTable *mprCopyHash(MprCtx ctx, MprHashTable *master)
{
    MprHash         *hp;
    MprHashTable    *table;

    table = mprCreateHash(ctx, master->hashSize);
    if (table == 0) {
        return 0;
    }

    hp = mprGetFirstHash(master);
    while (hp) {
        mprAddHash(table, hp->key, hp->data);
        hp = mprGetNextHash(master, hp);
    }
    return table;
}


/*
 *  Insert an entry into the hash table. If the entry already exists, update its value. Order of insertion is not preserved.
 */
MprHash *mprAddHash(MprHashTable *table, cchar *key, cvoid *ptr)
{
    MprHash     *sp, *prevSp;
    int         index;

    sp = lookupInner(&index, &prevSp, table, key);

    if (sp != 0) {
        /*
         *  Already exists. Just update the data.
         */
        sp->data = ptr;
        return sp;
    }

    /*
     *  New entry
     */
    sp = mprAllocObjZeroed(table, MprHash);
    if (sp == 0) {
        return 0;
    }

    sp->data = ptr;
    sp->key = mprStrdup(sp, key);
    sp->bucket = index;

    sp->next = table->buckets[index];
    table->buckets[index] = sp;

    table->count++;
    return sp;
}


/*
 *  Multiple insertion. Insert an entry into the hash table allowing for multiple entries with the same key.
 *  Order of insertion is not preserved. Lookup cannot be used to retrieve all duplicate keys, some will be shadowed. 
 *  Use enumeration to retrieve the keys.
 */
MprHash *mprAddDuplicateHash(MprHashTable *table, cchar *key, cvoid *ptr)
{
    MprHash     *sp;
    int         index;

    sp = mprAllocObjZeroed(table, MprHash);
    if (sp == 0) {
        return 0;
    }

    index = hashIndex(key, table->hashSize);

    sp->data = ptr;
    sp->key = mprStrdup(sp, key);
    sp->bucket = index;

    sp->next = table->buckets[index];
    table->buckets[index] = sp;

    table->count++;
    return sp;
}


/*
 *  Remove an entry from the table
 */
int mprRemoveHash(MprHashTable *table, cchar *key)
{
    MprHash     *sp, *prevSp;
    int         index;

    if ((sp = lookupInner(&index, &prevSp, table, key)) == 0) {
        return MPR_ERR_NOT_FOUND;
    }

    if (prevSp) {
        prevSp->next = sp->next;
    } else {
        table->buckets[index] = sp->next;
    }
    table->count--;

    mprFree(sp);
    return 0;
}


/*
 *  Lookup a key and return the hash entry
 */
MprHash *mprLookupHashEntry(MprHashTable *table, cchar *key)
{
    mprAssert(key);

    return lookupInner(0, 0, table, key);
}


/*
 *  Lookup a key and return the hash entry data
 */
cvoid *mprLookupHash(MprHashTable *table, cchar *key)
{
    MprHash     *sp;

    mprAssert(key);

    sp = lookupInner(0, 0, table, key);
    if (sp == 0) {
        return 0;
    }
    return sp->data;
}


static MprHash *lookupInner(int *bucketIndex, MprHash **prevSp, MprHashTable *table, cchar *key)
{
    MprHash     *sp, *prev;
    int         index, rc;

    mprAssert(key);

    index = hashIndex(key, table->hashSize);
    if (bucketIndex) {
        *bucketIndex = index;
    }

    sp = table->buckets[index];
    prev = 0;

    while (sp) {
        rc = strcmp(sp->key, key);
        if (rc == 0) {
            if (prevSp) {
                *prevSp = prev;
            }
            return sp;
        }
        prev = sp;
        mprAssert(sp != sp->next);
        sp = sp->next;
    }
    return 0;
}


int mprGetHashCount(MprHashTable *table)
{
    return table->count;
}


/*
 *  Return the first entry in the table.
 */
MprHash *mprGetFirstHash(MprHashTable *table)
{
    MprHash     *sp;
    int         i;

    mprAssert(table);

    for (i = 0; i < table->hashSize; i++) {
        if ((sp = (MprHash*) table->buckets[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
 *  Return the next entry in the table
 */
MprHash *mprGetNextHash(MprHashTable *table, MprHash *last)
{
    MprHash     *sp;
    int         i;

    mprAssert(table);

    if (last == 0) {
        return mprGetFirstHash(table);
    }

    if (last->next) {
        return last->next;
    }

    for (i = last->bucket + 1; i < table->hashSize; i++) {
        if ((sp = (MprHash*) table->buckets[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
 *  Hash the key to produce a hash index.
 */
static int hashIndex(cchar *key, int size)
{
    uint        sum;

    sum = 0;
    while (*key) {
        sum += (sum * 33) + *key++;
    }
    return sum % size;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprHash.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprHttp.c"
 */
/************************************************************************/

/**
 *  mprHttp.c - HTTP client (and Http code support)
 *
 *  The HTTP client supports HTTP/1.1 including all methods (DELELTE, GET, OPTIONS, POST, PUT, TRACE), SSL, keep-alive and
 *  chunked transfers. This module is thread-safe. 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#if BLD_FEATURE_HTTP_CLIENT
static void cleanup(MprHttp *http);
static void completeRequest(MprHttp *http);
static MprHttpRequest *createRequest(MprHttp *http);
static void badRequest(MprHttp *http, cchar *fmt, ...);
static bool parseChunk(MprHttp *http, MprBuf *buf);
static char *getHttpToken(MprBuf *buf, cchar *delim);
static int httpReadEvent(MprHttp *http);
static int  httpDestructor(MprHttp *http);
static void httpTimer(MprHttpService *hs, MprEvent *event);
static int  parseAuthenticate(MprHttp *http, char *authDetails);
static bool parseFirstLine(MprHttp *http, MprBuf *buf);
static bool parseHeaders(MprHttp *http, MprBuf *buf);
static void processResponse(MprHttp *http, MprBuf *buf, int nbytes);
static int  writeData(MprHttp *http, cchar *buf, int len, int block);

#if BLD_DEBUG
static void traceResponseData(MprCtx ctx, cchar *buf, int size);
#define traceData(ctx, buf, size) traceResponseData(ctx, buf, size);
#else
#define traceData(ctx, buf, size)
#endif

#endif /* BLD_FEATURE_HTTP_CLIENT */
#if BLD_FEATURE_HTTP
/**
 *  Standard HTTP error code table. This gets defined regardless of whether the HTTP feature is enabled.
 *  This is because some consumers of the MPR want to use server side HTTP, but not embed the client.
 */
typedef struct MprHttpCode {
    int     code;                           /**< Http error code */
    char    *codeString;                    /**< Code as a string (for hashing) */
    char    *msg;                           /**< Error message */
} MprHttpCode;


MprHttpCode MprHttpCodes[] = {
    { 100, "100", "Continue" },
    { 200, "200", "OK" },
    { 201, "201", "Created" },
    { 202, "202", "Accepted" },
    { 204, "204", "No Content" },
    { 205, "205", "Reset Content" },
    { 206, "206", "Partial Content" },
    { 301, "301", "Moved Permanently" },
    { 302, "302", "Moved Temporarily" },
    { 304, "304", "Not Modified" },
    { 305, "305", "Use Proxy" },
    { 307, "307", "Temporary Redirect" },
    { 400, "400", "Bad Request" },
    { 401, "401", "Unauthorized" },
    { 402, "402", "Payment Required" },
    { 403, "403", "Forbidden" },
    { 404, "404", "Not Found" },
    { 405, "405", "Method Not Allowed" },
    { 406, "406", "Not Acceptable" },
    { 408, "408", "Request Time-out" },
    { 409, "409", "Conflict" },
    { 410, "410", "Length Required" },
    { 411, "411", "Length Required" },
    { 413, "413", "Request Entity Too Large" },
    { 414, "414", "Request-URI Too Large" },
    { 415, "415", "Unsupported Media Type" },
    { 416, "416", "Requested Range Not Satisfiable" },
    { 417, "417", "Expectation Failed" },
    { 500, "500", "Internal Server Error" },
    { 501, "501", "Not Implemented" },
    { 502, "502", "Bad Gateway" },
    { 503, "503", "Service Unavailable" },
    { 504, "504", "Gateway Time-out" },
    { 505, "505", "Http Version Not Supported" },
    { 507, "507", "Insufficient Storage" },

    /*
     *  Proprietary codes (used internally) when connection to client is severed
     */
    { 550, "550", "Comms Error" },
    { 551, "551", "General Client Error" },
    { 0,   0 }
};


/*
 *  Initialize the http service.
 */
MprHttpService *mprCreateHttpService(MprCtx ctx)
{
    MprHttpService      *hs;
    MprHttpCode         *ep;

    hs = mprAllocObjZeroed(ctx, MprHttpService);
    if (hs == 0) {
        return 0;
    }
    hs->connections = mprCreateList(hs);

    hs->codes = mprCreateHash(hs, 41);
    for (ep = MprHttpCodes; ep->code; ep++) {
        mprAddHash(hs->codes, ep->codeString, ep);
    }
#if BLD_FEATURE_MULTITHREAD
    hs->mutex = mprCreateLock(hs);
#endif
    return hs;
}


cchar *mprGetHttpCodeString(MprCtx ctx, int code)
{
    char            key[8];
    MprHttpCode     *ep;
    
    mprItoa(key, sizeof(key), code, 10);
    ep = (MprHttpCode*) mprLookupHash(mprGetMpr(ctx)->httpService->codes, key);
    if (ep == 0) {
        return "Custom error";
    }
    return ep->msg;
}


int mprStartHttpService(MprHttpService *hs)
{
    return 0;
}


int mprStopHttpService(MprHttpService *hs)
{
    mprFree(hs->timer);
    hs->timer = 0;
    return 0;
}
#endif /* BLD_FEATURE_HTTP */


#if BLD_FEATURE_HTTP_CLIENT
static void startHttpTimer(MprHttpService *hs)
{
    mprLock(hs->mutex);
    if (hs->timer) {
        mprUnlock(hs->mutex);
        return;
    }
    hs->timer = mprCreateTimerEvent(mprGetDispatcher(hs), (MprEventProc) httpTimer, MPR_HTTP_TIMER_PERIOD, 
        MPR_NORMAL_PRIORITY, hs, MPR_EVENT_CONTINUOUS);
    mprUnlock(hs->mutex);
}


/*
 *  Check for any expired http connections. One timer to rule them all.
 */
static void httpTimer(MprHttpService *hs, MprEvent *event)
{
    MprHttp     *http;
    MprTime     now;
    int         next, count;

    mprAssert(hs);
    mprAssert(event);
    
    /*
     *  Locking ensures http connections won't be deleted. Must always lock the list before locking the http object.
     */
    mprLock(hs->mutex);

    now = mprGetTime(hs);
    for (count = 0, next = 0; (http = mprGetNextItem(hs->connections, &next)) != 0; count++) {
        /*
         *  See if more than the timeout period has passed since the last I/O. If so, disconnect and let the event 
         *  mechanism clean up. Add grace period of 5 seconds to allow blocked requests to cleanup first. 
         *
         *  The temp "diff" is a Workaround for a GCC bug when comparing two 64 bit numerics directly.
         */
        int64 diff = now - (http->timestamp + http->timeoutPeriod + 5000);
        if (diff > 0 && !mprGetDebugMode(hs)) {
            mprLog(hs, 4, "Request has timed out, timeout %d", http->timeoutPeriod);
            http->timedout = 1;
            mprDisconnectHttp(http);
        }
    }
    if (count == 0) {
        mprFree(event);
        hs->timer = 0;
    }
    mprUnlock(hs->mutex);
}


static void addHttp(MprHttpService *hs, MprHttp *http)
{
    mprLock(hs->mutex);
    mprAddItem(hs->connections, http);
    if (hs->timer == 0) {
        startHttpTimer(hs);
    }
    mprUnlock(hs->mutex);
}


/*
 *  Create a new http instance which represents a single connection to a remote server. Only one request may be active
 *  on a http instance at a time.
 */
MprHttp *mprCreateHttp(MprCtx ctx)
{
    MprHttpService  *hs;
    MprHttp         *http;

    hs = mprGetMpr(ctx)->httpService;
    mprAssert(hs);

    http = mprAllocObjWithDestructorZeroed(ctx, MprHttp, httpDestructor);
    if (http == 0) {
        return 0;
    }
    http->timestamp = mprGetTime(http);
    http->protocolVersion = 1;
    http->protocol = mprStrdup(http, "HTTP/1.1");
    http->state = MPR_HTTP_STATE_BEGIN;
    http->currentPort = -1;
    http->proxyPort = -1;
    http->followRedirects = 1;
    http->defaultHost = mprStrdup(http, "127.0.0.1");
    http->defaultPort = 80;
    http->service = hs;
    http->timeoutPeriod = MPR_TIMEOUT_HTTP;
    http->retries = MPR_HTTP_RETRIES;
    http->useKeepAlive = 1;
    http->bufsize = MPR_HTTP_BUFSIZE;
    http->bufmax = -1;
    http->request = createRequest(http);
#if BLD_FEATURE_MULTITHREAD
    http->mutex = mprCreateLock(http);
#endif
    addHttp(hs, http);
    mprAssert(http->sock == 0);
    return http;
}


static int httpDestructor(MprHttp *http)
{
    MprHttpService      *hs;

    hs = http->service;

    mprLock(hs->mutex);
    mprRemoveItem(hs->connections, http);
    mprFree(http->sock);
    mprUnlock(hs->mutex);
    return 0;
}


void mprDisconnectHttp(MprHttp *http)
{
    mprAssert(http);

    lock(http);
    if (http->sock) {
        mprDisconnectSocket(http->sock);
    }
    unlock(http);
}


/*
 *  Open a new connection to a remote server
 */
static int openConnection(MprHttp *http, cchar *host, int port, bool secure)
{
    int     rc;

    mprAssert(http);
    mprAssert(http->sock == 0);

    mprLog(http, 3, "Http: Opening socket on: %s:%d", host, port);

    if (secure) {
#if BLD_FEATURE_SSL
        http->sock = mprCreateSocket(http, MPR_SECURE_CLIENT);
#else
        return MPR_ERR_BAD_ARGS;
#endif
    } else {
        http->sock = mprCreateSocket(http, NULL);
    }
    rc = mprOpenClientSocket(http->sock, host, port, 0);
    if (rc < 0) {
        mprFree(http->sock);
        http->sock = 0;
        return MPR_ERR_CANT_OPEN;
    }
    mprFree(http->currentHost);
    http->currentHost = mprStrdup(http, host);
    http->currentPort = port;
    http->keepAlive = http->useKeepAlive;
    return 0;
}


/*
 *  Create a new request instance
 */
static MprHttpRequest *createRequest(MprHttp *http)
{
    MprHttpRequest  *req;

    mprAssert(http->state == MPR_HTTP_STATE_BEGIN);

    req = mprAllocObjZeroed(http, MprHttpRequest);
    if (req == 0) {
        return 0;
    }
    req->http = http;
    req->headers = mprCreateHash(req, -1);
    req->outBuf = mprCreateBuf(req, http->bufsize, http->bufmax);
    req->chunked = -1;
    return req;
}


/*
 *  Called for 1XX responses which are ignored. This resets the response to its original condition.
 */
static void resetResponse(MprHttp *http)
{
    MprHttpResponse *resp;

    resp = http->response;
    resp->code = -1;
    mprFlushBuf(resp->headerBuf);
    mprFlushBuf(resp->dataBuf);
    mprFlushBuf(resp->chunkBuf);
    mprFree(resp->headers);
    resp->headers = mprCreateHash(resp, -1);
}


/*
 *  Reset the request if users stats initializing the http object and a request is underway or complete
 */
static void conditionalReset(MprHttp *http)
{
    mprAssert(http);

    if (MPR_HTTP_STATE_BEGIN < http->state && http->state < MPR_HTTP_STATE_COMPLETE) {
        cleanup(http);
        mprFree(http->sock);
        http->sock = 0;
        http->state = MPR_HTTP_STATE_BEGIN;
    }
}


/*
 *  Cleanup called at the completion of a request to prepare for follow-on requests on the same http object.
 */
static void cleanup(MprHttp *http)
{
    MprHttpRequest  *req;

    req = http->request;
    mprAssert(req);

    mprFree(req->headers);
    req->headers = mprCreateHash(req, -1);

    if (req->bodyData != req->formData) {
        mprFree(req->bodyData);
    }
    mprFree(req->formData);
    req->formData = 0;
    req->formLen = 0;
    req->bodyData = 0;
    req->bodyLen = 0;
    req->flags = 0;
    req->chunked = -1;
}


/*
 *  Create a new response object. 
 */
static MprHttpResponse *createResponse(MprHttp *http)
{
    MprHttpResponse *resp;

    resp = mprAllocObjZeroed(http, MprHttpResponse);
    if (resp == 0) {
        return 0;
    }
    resp->headers = mprCreateHash(resp, -1);
    resp->http = http;
    resp->code = -1;
    resp->headerBuf = mprCreateBuf(resp, http->bufsize, http->bufmax);
    resp->dataBuf = mprCreateBuf(resp, http->bufsize, http->bufmax);
    resp->chunkBuf = mprCreateBuf(resp, http->bufsize, http->bufmax);
    return resp;
}


/*
 *  Check the response for authentication failures and redirections. Return true if a retry is requried.
 */
bool mprNeedHttpRetry(MprHttp *http, char **url)
{
    MprHttpResponse     *resp;
    MprHttpRequest      *req;

    mprAssert(http->response);
    mprAssert(http->state > MPR_HTTP_STATE_WAIT);

    /*
     *  For sync mode requests (no callback), handle authorization and redirections inline
     */
    resp = http->response;
    req = http->request;
    *url = 0;

    if (http->state < MPR_HTTP_STATE_WAIT) {
        return 0;
    }
    if (resp->code == MPR_HTTP_CODE_UNAUTHORIZED) {
        if (http->user == 0) {
            http->error = mprStrdup(http, "Authentication required");
        } else if (http->request->sentCredentials) {
            http->error = mprStrdup(http, "Authentication failed");
        } else {
            return 1;
        }
    } else if (MPR_HTTP_CODE_MOVED_PERMANENTLY <= resp->code && resp->code <= MPR_HTTP_CODE_MOVED_TEMPORARILY && 
            http->followRedirects) {
        *url = resp->location;
        return 1;
    }
    return 0;
}


/*
 *  Start a HTTP request. Do not block.
 */
int mprStartHttpRequest(MprHttp *http, cchar *method, cchar *requestUrl)
{
    MprHttpRequest      *req;
    MprHttpResponse     *resp;
    MprUri              *url;
    MprBuf              *outBuf;
    MprHashTable        *headers;
    MprHash             *header;
    char                *host;
    int                 port, len, rc, written;

    mprAssert(http);
    mprAssert(method && *method);
    mprAssert(requestUrl && *requestUrl);

    mprLog(http, 4, "Http: request: %s %s", method, requestUrl);

    rc = 0;
    req = http->request;
    resp = http->response;
    conditionalReset(http);

    /*
     *  Prepare for a new request
     */
    http->timestamp = mprGetTime(req);
    mprFree(http->error);
    http->error = 0;

    outBuf = req->outBuf;
    req->sentCredentials = 0;

    mprFree(req->method);
    method = req->method = mprStrdup(req, method);
    mprStrUpper(req->method);

    mprFree(req->uri);
    url = req->uri = mprParseUri(req, requestUrl);

    mprFree(http->response);
    http->response = createResponse(http);

    if (req->formData) {
        req->bodyData = req->formData;
        req->bodyLen = req->formLen;
    }
    if (*requestUrl == '/') {
        host = (http->proxyHost) ? http->proxyHost : http->defaultHost;
        port = (http->proxyHost) ? http->proxyPort : http->defaultPort;

    } else {
        host = (http->proxyHost) ? http->proxyHost : url->host;
        port = (http->proxyHost) ? http->proxyPort : url->port;
    }
    if (http->sock) {
        if (port != http->currentPort || strcmp(host, http->currentHost) != 0) {
            /*
             *  This request is for a different host or port. Must close socket.
             */
            mprFree(http->sock);
            http->sock = 0;
        }
    }
    if (http->sock == 0) {
        http->secure = url->secure;
        if (openConnection(http, host, port, url->secure) < 0) {
            badRequest(http, "Can't open socket on %s:%d", host, port);
            return MPR_ERR_CANT_OPEN;
        }
    } else {
        mprLog(http, 4, "Http: reusing keep-alive socket on: %s:%d", host, port);
    }

    /*
     *  Emit the request
     */
    if (http->proxyHost && *http->proxyHost) {
        if (url->query && *url->query) {
            mprPutFmtToBuf(outBuf, "%s http://%s:%d%s?%s %s\r\n", method, http->proxyHost, http->proxyPort, 
                url->url, url->query, http->protocol);
        } else {
            mprPutFmtToBuf(outBuf, "%s http://%s:%d%s %s\r\n", method, http->proxyHost, http->proxyPort, url->url,
                http->protocol);
        }
    } else {
        if (url->query && *url->query) {
            mprPutFmtToBuf(outBuf, "%s %s?%s %s\r\n", method, url->url, url->query, http->protocol);
        } else {
            mprPutFmtToBuf(outBuf, "%s %s %s\r\n", method, url->url, http->protocol);
        }
    }

    if (http->authType && strcmp(http->authType, "basic") == 0) {
        char    abuf[MPR_MAX_STRING], encDetails[MPR_MAX_STRING];
        mprSprintf(abuf, sizeof(abuf), "%s:%s", http->user, http->password);
        mprEncode64(encDetails, sizeof(encDetails), abuf);
        mprPutFmtToBuf(outBuf, "Authorization: basic %s\r\n", encDetails);
        req->sentCredentials = 1;

    } else if (http->authType && strcmp(http->authType, "digest") == 0) {
        char    a1Buf[256], a2Buf[256], digestBuf[256];
        char    *ha1, *ha2, *digest, *qop;

        if (http->service->secret == 0 && mprCreateHttpSecret(http) < 0) {
            mprLog(req, MPR_ERROR, "Http: Can't create secret for digest authentication");
            mprFree(req);
            http->request = 0;
            return MPR_ERR_CANT_INITIALIZE;
        }
        mprFree(http->authCnonce);
        mprCalcDigestNonce(http, &http->authCnonce, http->service->secret, 0, http->authRealm);

        mprSprintf(a1Buf, sizeof(a1Buf), "%s:%s:%s", http->user, http->authRealm, http->password);
        len = strlen(a1Buf);
        ha1 = mprGetMD5Hash(req, a1Buf, len, NULL);

        mprSprintf(a2Buf, sizeof(a2Buf), "%s:%s", method, url->url);
        len = strlen(a2Buf);
        ha2 = mprGetMD5Hash(req, a2Buf, len, NULL);

        qop = (http->authQop) ? http->authQop : (char*) "";

        http->authNc++;
        if (mprStrcmpAnyCase(http->authQop, "auth") == 0) {
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, http->authNonce, http->authNc, http->authCnonce, http->authQop, ha2);
        } else if (mprStrcmpAnyCase(http->authQop, "auth-int") == 0) {
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, http->authNonce, http->authNc, http->authCnonce, http->authQop, ha2);
        } else {
            qop = "";
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, http->authNonce, ha2);
        }
        mprFree(ha1);
        mprFree(ha2);
        digest = mprGetMD5Hash(req, digestBuf, strlen(digestBuf), NULL);

        if (*qop == '\0') {
            mprPutFmtToBuf(outBuf, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
                "uri=\"%s\", response=\"%s\"\r\n",
                http->user, http->authRealm, http->authNonce, url->url, digest);

        } else if (strcmp(qop, "auth") == 0) {
            mprPutFmtToBuf(outBuf, "Authorization: Digest username=\"%s\", realm=\"%s\", domain=\"%s\", "
                "algorithm=\"MD5\", qop=\"%s\", cnonce=\"%s\", nc=\"%08x\", nonce=\"%s\", opaque=\"%s\", "
                "stale=\"FALSE\", uri=\"%s\", response=\"%s\"\r\n",
                http->user, http->authRealm, http->authDomain, http->authQop, http->authCnonce, http->authNc,
                http->authNonce, http->authOpaque, url->url, digest);

        } else if (strcmp(qop, "auth-int") == 0) {
            ;
        }
        mprFree(digest);
        req->sentCredentials = 1;
    }

    mprPutFmtToBuf(outBuf, "Host: %s\r\n", host);
    mprPutFmtToBuf(outBuf, "User-Agent: %s\r\n", MPR_HTTP_NAME);

    if (http->protocolVersion == 1) {
        if (http->keepAlive) {
            mprPutFmtToBuf(outBuf, "Connection: Keep-Alive\r\n");
        } else {
            mprPutFmtToBuf(outBuf, "Connection: close\r\n");
        }
        if (req->bodyLen > 0) {
            mprPutFmtToBuf(outBuf, "Content-Length: %d\r\n", req->bodyLen);
            req->chunked = 0;

        } else if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
            if (req->chunked != 0) {
                mprSetHttpHeader(http, 1, "Transfer-Encoding", "chunked");
                req->chunked = 1;
            }
        } else {
            req->chunked = 0;
        }

    } else {
        http->keepAlive = 0;
        mprPutFmtToBuf(outBuf, "Connection: close\r\n");
    }

    headers = http->request->headers;
    if (mprGetHashCount(headers) > 0) {
        for (header = 0; (header = mprGetNextHash(headers, header)) != 0; ) {
            mprPutFmtToBuf(outBuf, "%s: %s\r\n", header->key, header->data);
        }
    }

    mprAddNullToBuf(outBuf);
    mprLog(req, 3, "\nHttp: @@@ Request =>\n%s", mprGetBufStart(outBuf));

    if (req->chunked != 1) {
        /* As an optimization, omit the trailing CRLF if chunked. This makes adding chunks easier */
        mprPutStringToBuf(outBuf, "\r\n");
    }

    /*
     *  Write the request as a blocking write
     */
    lock(http);
    len = mprGetBufLength(outBuf);
    while (len > 0) {
        mprSetSocketBlockingMode(http->sock, 1);
        if ((written = mprWriteSocket(http->sock, mprGetBufStart(outBuf), len)) <= 0) {
            badRequest(http, "Can't write request to socket");
            mprSetSocketBlockingMode(http->sock, 0);
            unlock(http);
            return MPR_ERR_CANT_WRITE;
        }
        mprSetSocketBlockingMode(http->sock, 0);
        len -= written;
    }
    mprFlushBuf(outBuf);

    /*
     *  Write any assigned body data. Sometimes callers will invoke mprWriteHttp after this call returns.
     */
    if (req->bodyData && writeData(http, req->bodyData, req->bodyLen, 0) < 0) {
        badRequest(http, "Can't write body data");
        unlock(http);
        return MPR_ERR_CANT_WRITE;
    }
    http->state = MPR_HTTP_STATE_WAIT;
    unlock(http);
    return 0;
}


int mprHttpRequest(MprHttp *http, cchar *method, cchar *requestUrl)
{
    int     rc;

    if ((rc = mprStartHttpRequest(http, method, requestUrl)) < 0) {
        return rc;
    }
    if (mprWaitForHttp(http, MPR_HTTP_STATE_COMPLETE, -1) < 0) {
        return MPR_ERR_TIMEOUT;
    }
    mprAssert(http->response);
    return http->response->code;
}


int mprFinalizeHttpWriting(MprHttp *http)
{
    MprHttpRequest  *req;

    lock(http);
    req = http->request;
    if (req->chunked == 1) {
        /*
         *  Emit the chunk trailer to signify the end of body data
         */
        if (writeData(http, "\r\n0\r\n\r\n", 7, 1) < 0) {
            unlock(http);
            return MPR_ERR_CANT_WRITE;
        }
        req->chunked = 0;
    }
    unlock(http);
    return 0;
}


/*
 *  Wait for the Http object to achieve a given state. This will invoke mprFinalizeHttpWriting to write the chunk trailer 
 *  if required. It will also invoke the callback for any read events.
 */
int mprWaitForHttp(MprHttp *http, int state, int timeout)
{
    MprTime     mark;
    int         mask, events;

    if (timeout < 0) {
        timeout = http->timeoutPeriod;
    }
    if (timeout < 0) {
        timeout = MAXINT;
    }
    if (http->state == MPR_HTTP_STATE_BEGIN) {
        return MPR_ERR_BAD_STATE;
    } 
    lock(http);
    if (http->state < state) {
        if (state == MPR_HTTP_STATE_COMPLETE) {
            /*
             *  Incase user forgot to do a write(0), make sure the final chunk trailer has been written.
             */
            if (mprFinalizeHttpWriting(http) < 0) {
                unlock(http);
                return MPR_ERR_CANT_WRITE;
            }
        }
        mark = mprGetTime(http);
        while (http->state < state) {
            mask = MPR_READABLE;
            if (http->callback) {
                mask |= http->callbackMask;
            }
            events = MPR_READABLE;
            if (http->sock) {
                if (!mprIsSocketEof(http->sock) && !mprHasSocketPendingData(http->sock)) {
                    mprSetSocketBlockingMode(http->sock, 1);
                    if (((events = mprWaitForSingleIO(http, http->sock->fd, mask, timeout)) == 0) || 
                            mprGetElapsedTime(http, mark) >= timeout) {
                        if (!mprGetDebugMode(http)) {
                            unlock(http);
                            return MPR_ERR_TIMEOUT;
                        }
                    }
                }
            }
            httpReadEvent(http);
            if (http->callback) {
                (http->callback)(http->callbackArg, events & http->callbackMask);
            }
        }
    }
    unlock(http);
    return 0;
}


/*
 *  Wait for receipt of the response headers from the remote server.
 */
int mprWaitForHttpResponse(MprHttp *http, int timeout)
{
    return mprWaitForHttp(http, MPR_HTTP_STATE_CONTENT, timeout);
}


/*
 *  Read http data. Returns a count of bytes read (this may be less than requested). If a callback is defined, 
 *  this routine will not block. Call mprIsHttpComplete to determine if there is more data to read.
 */
int mprReadHttp(MprHttp *http, char *data, int size)
{
    MprHttpResponse *resp;
    MprBuf          *buf;
    int             nbytes;

    mprAssert(size > 0);

    if (http->state == MPR_HTTP_STATE_BEGIN) {
        return MPR_ERR_BAD_STATE;
    } 
    lock(http);
    resp = http->response;
    buf = resp->dataBuf;
    while (size > mprGetBufLength(buf) && http->state != MPR_HTTP_STATE_COMPLETE) {
        mprCompactBuf(buf);
        nbytes = httpReadEvent(http);
        if (nbytes == 0 && mprGetBufLength(buf) > 0) {
            break;
        }
        if (size <= mprGetBufLength(buf) || http->state == MPR_HTTP_STATE_COMPLETE || http->callback) {
            break;
        }
        /*
         *  Block if no data and no callback
         */
        if (mprGetBufLength(buf) == 0 && !http->callback && http->sock) {
            if (mprWaitForSingleIO(http, http->sock->fd, MPR_READABLE, http->timeoutPeriod) < 0) {
                break;
            }
        }
    }
    unlock(http);
    mprAssert(http->callback || http->state == MPR_HTTP_STATE_COMPLETE || mprGetBufLength(buf) > 0);
    return mprGetBlockFromBuf(buf, data, size);
}


/*
 *  Read a string. This will read all available data. It will block if a callback has not been defined.
 */
char *mprReadHttpString(MprHttp *http)
{
    MprBuf      *buf;
    char        data[MPR_HTTP_BUFSIZE], *result;
    int         count;

    if (http->state == MPR_HTTP_STATE_BEGIN) {
        return 0;
    } 
    buf = mprCreateBuf(http, MPR_HTTP_BUFSIZE, -1);
    do {
        if ((count = mprReadHttp(http, data, sizeof(data))) > 0) {
            mprPutBlockToBuf(buf, data, count);
        }
    } while (count > 0 && !http->callback);

    mprAddNullToBuf(buf);
    result = mprStealBuf(http, buf);
    mprFree(buf);
    return result;
}


/*
 *  Return true if the entire request is complete
 */
bool mprIsHttpComplete(MprHttp *http)
{
    return http->state & MPR_HTTP_STATE_COMPLETE;
}


/*
 *  Determine how much data to read into the input buffer and grow the buffer (for headers) if required.
 */
static int getReadSize(MprHttp *http, MprBuf *buf)
{
    MprHttpResponse     *resp;
    int                 space;

    mprAssert(buf);
    resp = http->response;
    mprAssert(resp);

    space = mprGetBufSpace(buf);
    if (space < MPR_HTTP_BUFSIZE) {
        if (mprGrowBuf(buf, MPR_HTTP_BUFSIZE) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    space = mprGetBufSpace(buf);
    if (resp && resp->contentRemaining > 0 && http->state >= MPR_HTTP_STATE_CONTENT) {
        space = min(space, resp->contentRemaining);
    }
    mprAssert(0 < space && space <= mprGetBufSize(buf));
    return space;
}


/*
 *  Process to an incoming HTTP response
 */
static int httpReadEvent(MprHttp *http)
{
    MprHttpResponse *resp;
    MprBuf          *buf;
    int             nbytes, len;

    mprAssert(http->sock);
    mprAssert(http->request);
    mprAssert(http->response);

    lock(http);
    resp = http->response;
    http->timestamp = mprGetTime(http);

    if (http->state == MPR_HTTP_STATE_WAIT) {
        buf = resp->headerBuf;
    } else if (resp->flags & MPR_HTTP_RESP_CHUNKED) {
        buf = resp->chunkBuf;
    } else {
        buf = resp->dataBuf;
    }
    mprAssert(buf);
    if ((len = getReadSize(http, buf)) < 0) {
        mprAssert(len > 0);
        unlock(http);
        return 0;
    }
    nbytes = mprReadSocket(http->sock, mprGetBufEnd(buf), len);
    if (nbytes < 0 || (nbytes == 0 && mprIsSocketEof(http->sock))) {
        /* 
         *  Server disconnection
         */
        http->keepAlive = 0;
        if (http->state != MPR_HTTP_STATE_COMPLETE && http->response->contentLength == 0) {
            mprLog(http, 5, "Socket end of file from server, rc %d, errno %d", nbytes, errno);
            if (resp->flags & MPR_HTTP_RESP_CHUNKED) {
                badRequest(http, "Communications error");
            } else {
                http->state = MPR_HTTP_STATE_COMPLETE;
                processResponse(http, buf, nbytes);
            }
        } else {
            badRequest(http, "Communications error");
        }

    } else if (nbytes > 0) {
        mprLog(http, 5, "Read %d bytes from socket, ask for %d", nbytes, len);
        traceData(http, mprGetBufStart(buf), nbytes);
        mprAdjustBufEnd(buf, nbytes);
        processResponse(http, buf, nbytes);
    }
    unlock(http);
    return nbytes;
}


/*
 *  Main HTTP state machine. Process whatever data is available so far.
 */
static void processResponse(MprHttp *http, MprBuf *buf, int nbytes)
{
    MprHttpResponse     *resp;
    MprHttpRequest      *req;
    int                 len;

    resp = http->response;
    req = http->request;

    while (1) {
        switch(http->state) {
        case MPR_HTTP_STATE_WAIT:
            if (!parseFirstLine(http, buf) || !parseHeaders(http, buf)) {
                return;
            }
            resp = http->response;
            if (100 <= resp->code && resp->code < 200) {
                /* Ignore 1XX responses and scan for a new first+headers */
                resetResponse(http);
                break;
            }
            buf = (resp->flags & MPR_HTTP_RESP_CHUNKED) ? resp->chunkBuf : resp->dataBuf;
            if ((len = mprGetBufLength(resp->headerBuf)) > 0) {
                /* Transfer remaining data to the chunk or data buffer */
                mprPutBlockToBuf(buf, mprGetBufStart(resp->headerBuf), len);
            }
            nbytes = mprGetBufLength(buf);
            if (resp->flags & MPR_HTTP_RESP_CHUNKED) {
                http->state = MPR_HTTP_STATE_CHUNK;
            } else {
                http->state = (resp->contentRemaining == 0) ? MPR_HTTP_STATE_COMPLETE: MPR_HTTP_STATE_CONTENT;
            }
            break;

        case MPR_HTTP_STATE_CONTENT:
            if (resp->flags & MPR_HTTP_RESP_CHUNKED) {
                if ((len = mprGetBufLength(buf)) == 0) {
                    return;
                }
                len = min(len, resp->chunkRemaining);
                resp->length += len;
                resp->chunkRemaining -= len;
                mprAssert(resp->chunkRemaining >= 0);
                /* Transfer data chunk to the data buffer */
                mprPutBlockToBuf(resp->dataBuf, mprGetBufStart(buf), len);
                mprAdjustBufStart(buf, len);
                if (resp->chunkRemaining > 0) {
                    return;
                } else {
                    http->state = MPR_HTTP_STATE_CHUNK;
                }
            } else {
                len = nbytes;
                resp->length += len;
                resp->contentRemaining -= len;
                mprAssert(resp->contentRemaining >= 0);
                if (resp->contentRemaining > 0) {
                    return;
                } else {
                    http->state = MPR_HTTP_STATE_COMPLETE;
                }
            }
            break;
                
        case MPR_HTTP_STATE_CHUNK:
            if (!parseChunk(http, buf)) {
                return;
            }
            http->state = (resp->chunkRemaining <= 0) ?  MPR_HTTP_STATE_COMPLETE : MPR_HTTP_STATE_CONTENT;
            break;

        case MPR_HTTP_STATE_COMPLETE:
            completeRequest(http);
            return;
                
        default:
            badRequest(http, "Bad state");
            return;
        }
    }
}


/*
 *  Process the first line of data from the HTTP response
 */
static bool parseFirstLine(MprHttp *http, MprBuf *buf)
{
    MprHttpResponse     *resp;
    char                *start, *end, *code;
    int                 len;

    mprAssert(buf);
    mprAssert(http->response);

    resp = http->response;
    start = mprGetBufStart(buf);
    len = mprGetBufLength(buf);
    if (len == 0 || (end = mprStrnstr(start, "\r\n\r\n", len)) == 0) {
        /* Request is currently incomplete, need wait for more data */
        return 0;
    }
#if BLD_DEBUG
    *end = '\0'; mprLog(http, 3, "\nHttp: @@@ Response =>\n%s\n", start); *end = '\r';
#endif
    resp->protocol = getHttpToken(buf, " ");
    if (resp->protocol == 0 || resp->protocol[0] == '\0') {
        badRequest(http, "Bad HTTP response");
        return 0;
    }
    if (strncmp(resp->protocol, "HTTP/1.", 7) != 0) {
        badRequest(http, "Unsupported protocol");
        return 0;
    }
    code = getHttpToken(buf, " ");
    if (code == 0 || *code == '\0') {
        badRequest(http, "Bad HTTP response");
        return 0;
    }
    resp->code = atoi(code);
    resp->message = getHttpToken(buf, "\r\n");
    return 1;
}


/*
 *  Parse the response headers. Only come here when all the headers are resident.
 */
static bool parseHeaders(MprHttp *http, MprBuf *buf)
{
    MprHttpResponse *resp;
    char            *key, *value, *tp;
    int             len;

    resp = http->response;

    while (mprGetBufLength(buf) > 0 && buf->start[0] != '\r') {
        if ((key = getHttpToken(buf, ":")) == 0) {
            badRequest(http, "Bad HTTP header");
            return 0;
        }
        /*
         *  Tokenize the headers insitu. This modifies the data in the input buffer
         */
        value = getHttpToken(buf, "\r\n");
        while (isspace((int) *value)) {
            value++;
        }
        /*
         *  Save each header in the headers hash. Not strduped, these are references into the buffer.
         */
        mprAddHash(resp->headers, mprStrUpper(key), value);

        switch (key[0]) {
        case 'C':
            if (strcmp("CONTENT-LENGTH", key) == 0) {
                resp->contentLength = atoi(value);
                if (resp->contentLength < 0) {
                    resp->contentLength = 0;
                }
                if (mprStrcmpAnyCase(resp->http->request->method, "HEAD") == 0 || (resp->flags & MPR_HTTP_RESP_CHUNKED)) {
                    resp->contentLength = 0;
                    resp->contentRemaining = 0;
                } else {
                    resp->contentRemaining = resp->contentLength;
                }

            } else if (strcmp("CONNECTION", key) == 0) {
                mprStrLower(value);
                if (strcmp(value, "close") == 0) {
                    http->keepAlive = 0;
                    if (resp->contentLength == 0) {
                        resp->contentRemaining = MAXINT;
                    }
                }
            }
            break;
                
        case 'K':
            if (strcmp("KEEP-ALIVE", key) == 0) {
                /*
                 *  Quick compare for "Keep-Alive: timeout=N, max=1"
                 */
                len = (int) strlen(value);
                if (len > 2 && value[len - 1] == '1' && value[len - 2] == '=' && 
                        tolower((int)(value[len - 3])) == 'x') {
                    /*
                     *  IMPORTANT: Deliberately close the connection one request early. This ensures a client-led 
                     *  termination and helps relieve server-side TIME_WAIT conditions.
                     */
                    http->keepAlive = 0;
                }
            }
            break;                
                
        case 'L':
            if (strcmp("LOCATION", key) == 0) {
                resp->location = value;
            }
            break;

        case 'T':
            if (strcmp("TRANSFER-ENCODING", key) == 0) {
                mprStrLower(value);
                if (strcmp(value, "chunked") == 0) {
                    resp->flags |= MPR_HTTP_RESP_CHUNKED;
                    resp->contentLength = 0;
                    resp->contentRemaining = 0;
                }
            }
            break;
        
        case 'W':
            if (strcmp("WWW-AUTHENTICATE", key) == 0) {
                tp = value;
                while (*value && !isspace((int) *value)) {
                    value++;
                }
                *value++ = '\0';
                mprStrLower(tp);
                
                mprFree(http->authType);
                http->authType = mprStrdup(http, tp);
                
                if (parseAuthenticate(http, value) < 0) {
                    badRequest(http, "Bad Authentication header");
                    return 0;
                }
            }
            break;
        }
    }

    /* 
     *  Step over "\r\n", except if chunked: optimization for response chunking to simplify chunk boundary parsing.
     */
    if (!(resp->flags & MPR_HTTP_RESP_CHUNKED)) {
        mprAdjustBufStart(buf, 2);
    }
    return 1;
}


/*
 *  Parse an authentication response
 */
static int parseAuthenticate(MprHttp *http, char *authDetails)
{
    MprHttpResponse *resp;
    char            *value, *tok, *key, *dp, *sp;
    int             seenComma;

    key = (char*) authDetails;
    resp = http->response;

    while (*key) {
        while (*key && isspace((int) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((int) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        *tok++ = '\0';

        while (isspace((int) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
                tok++;
            }
        } else {
            value = tok;
            while (*tok != ',' && *tok != '\0') {
                tok++;
            }
            seenComma++;
        }
        *tok++ = '\0';

        /*
         *  Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (dp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *dp++ = *sp++;
            }
            *dp = '\0';
        }

        /*
         *  algorithm, domain, nonce, oqaque, realm, qop, stale
         *  We don't strdup any of the values as the headers are persistently saved.
         */
        switch (tolower((int) *key)) {
        case 'a':
            if (mprStrcmpAnyCase(key, "algorithm") == 0) {
                mprFree(resp->authAlgorithm);
                resp->authAlgorithm = value;
                break;
            }
            break;

        case 'd':
            if (mprStrcmpAnyCase(key, "domain") == 0) {
                mprFree(http->authDomain);
                http->authDomain = mprStrdup(http, value);
                break;
            }
            break;

        case 'n':
            if (mprStrcmpAnyCase(key, "nonce") == 0) {
                mprFree(http->authNonce);
                http->authNonce = mprStrdup(http, value);
                resp->http->authNc = 0;
            }
            break;

        case 'o':
            if (mprStrcmpAnyCase(key, "opaque") == 0) {
                mprFree(http->authOpaque);
                http->authOpaque = mprStrdup(http, value);
            }
            break;

        case 'q':
            if (mprStrcmpAnyCase(key, "qop") == 0) {
                mprFree(http->authQop);
                http->authQop = mprStrdup(http, value);
            }
            break;

        case 'r':
            if (mprStrcmpAnyCase(key, "realm") == 0) {
                mprFree(http->authRealm);
                http->authRealm = mprStrdup(http, value);
            }
            break;

        case 's':
            if (mprStrcmpAnyCase(key, "stale") == 0) {
                resp->authStale = mprStrdup(resp, value);
                break;
            }

        default:
            /*  For upward compatibility --  ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    if (strcmp(resp->http->authType, "basic") == 0) {
        if (http->authRealm == 0) {
            return MPR_ERR_BAD_ARGS;
        }
        return 0;
    }
    if (http->authRealm == 0 || http->authNonce == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (http->authQop) {
        if (http->authDomain == 0 || http->authOpaque == 0 || resp->authAlgorithm == 0 || resp->authStale == 0) {
            return MPR_ERR_BAD_ARGS;
        }
    }
    return 0;
}


/*
 *  Write a block of data data. Uses blocking writes. Callers who need non-blocking writes should use mprWriteSocket
 */
static int writeData(MprHttp *http, cchar *buf, int size, int block)
{
    int     written, rc, nbytes, oldMode;

    http->timestamp = mprGetTime(http);
    block |= http->callback ? 0 : 1;
    oldMode = mprSetSocketBlockingMode(http->sock, block);
    for (written = 0; written < size; ) {
        nbytes = size - written;
        rc = mprWriteSocket(http->sock, (char*) &buf[written], nbytes);
        if (rc < 0) {
            mprSetSocketBlockingMode(http->sock, oldMode);
            return rc;
        }
        written += rc;
        if (rc != nbytes) {
            break;
        }
    }
    mprSetSocketBlockingMode(http->sock, oldMode);
    return written;
}


/*
 *  Define a body to send with the request. This body is sent at the same time as the headers. Use this if you are NOT
 *  using mprWriteHttp. The body parameter can be NULL to just define a content length.
 */
int mprSetHttpBody(MprHttp *http, cchar *body, int len)
{
    MprHttpRequest      *req;

    mprAssert(len >= 0);
    req = http->request;

    conditionalReset(http);
    if (body && len > 0) {
        req->bodyData = mprMemdup(req, body, len);
        if (req->bodyData == 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    req->bodyLen = len;
    return 0;
}


/*
 *  Add Form data
 */
int mprAddHttpFormData(MprHttp *http, cchar *body, int len)
{
    MprHttpRequest      *req;

    req = http->request;
    conditionalReset(http);

    req->formData = mprRealloc(req, req->formData, req->formLen + len + 1);
    if (req->formData == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    memcpy(&req->formData[req->formLen], body, len);
    req->formLen += len;
    req->formData[req->formLen] = '\0';
    if (req->formData) {
        mprSetHttpHeader(http, 1, "Content-Type", "application/x-www-form-urlencoded");
    }
    return 0;
}


/*
 *  Add a keyword value pair to the form data
 */
int mprAddHttpFormItem(MprHttp *http, cchar *keyArg, cchar *valueArg)
{
    MprHttpRequest  *req;
    char            *value, *key, *encodedKey, *encodedValue;

    req = http->request;
    conditionalReset(http);

    if (req == 0) {
        return MPR_ERR_BAD_STATE;
    }
    key = (char*) keyArg;
    value = (char*) valueArg;

    if (value == 0) {
        key = mprStrdup(http, key);
        if ((value = strchr(key, '=')) != 0) {
            *value++ = '\0';
        }
    }
    if (key == 0 || value == 0) {
        return MPR_ERR_BAD_ARGS;
    }

    /*
     *  Encode key and value separately
     */
    encodedKey = mprUrlEncode(http, key);
    encodedValue = mprUrlEncode(http, value);
    if (req->formData) {
        req->formData = mprReallocStrcat(req, -1, req->formData, "&", encodedKey, "=", encodedValue, NULL);
    } else {
        req->formData = mprStrcat(req, -1, encodedKey, "=", encodedValue, NULL);
    }
    mprFree(encodedValue);
    if (req->formData == 0) {
        return 0;
    }
    req->formLen = strlen(req->formData);
    return 0;
}


/*
 *  Set the request as being a multipart mime upload. This defines the content type and defines a multipart mime boundary
 */
void mprEnableHttpUpload(MprHttp *http)
{
    conditionalReset(http);
    mprFree(http->boundary);
    http->boundary = mprAsprintf(http, -1, "--BOUNDARY--%Ld", mprGetTime(http));
    mprSetFormattedHttpHeader(http, 1, "Content-Type", "multipart/form-data; boundary=%s", &http->boundary[2]);
}


/*
 *  Returns -1 if chunked transfers are undefined. Returns 0 for disabled, 1 for true
 */
int mprGetHttpChunked(MprHttp *http)
{
    MprHttpRequest  *req;

    req = http->request;
    return req->chunked;
}


/*
 *  Enable or disable chunked transfers. The req->chunked field is initially set to -1 which means undefined. Calling 
 *  this routine will define chunking as explicitly on or off.
 */
int mprSetHttpChunked(MprHttp *http, int enable)
{
    MprHttpRequest  *req;

    req = http->request;
    conditionalReset(http);

    if (enable == 1) {
        req->chunked = 1;
    } else {
        req->chunked = 0;
    }
    return 0;
}


/*
 *  This can write a complete body or just a chunk. If mprSetHttpChunked has been called, then the output will use
 *  transfer chunking.
 */
int mprWriteHttp(MprHttp *http, cchar *buf, int len)
{
    MprHttpRequest  *req;
    char            countBuf[16];
    int             rc;

    mprAssert(http);
    mprAssert(buf);
    mprAssert(len >= 0);

    req = http->request;
    if (len == 0 && buf) {
        len = (int) strlen(buf);
    }
    /*
     *  Use chunk_emitted flag to support non-blocking short writes
     */ 
    if (req->chunked == 1 && !(req->flags & MPR_HTTP_REQ_CHUNK_EMITTED)) {
        if (len == 0) {
            http->callbackMask &= ~MPR_WRITABLE;
            if (mprFinalizeHttpWriting(http) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            return 0;
        }
        mprSprintf(countBuf, sizeof(countBuf), "\r\n%x\r\n", len);
        if (writeData(http, countBuf, strlen(countBuf), 1) < 0) {
            req->flags |= MPR_HTTP_REQ_CHUNK_EMITTED;
            return MPR_ERR_CANT_WRITE;
        }
        req->flags |= MPR_HTTP_REQ_CHUNK_EMITTED;
    }
    if ((rc = writeData(http, buf, len, 0)) == len) {
        req->flags &= ~MPR_HTTP_REQ_CHUNK_EMITTED;
    }
    return rc;
}


/*
 *  Blocking file copy
 */
static int copyFile(MprHttp *http, cchar *path)
{
    MprFile     *file;
    char        buf[MPR_BUFSIZE];
    int         bytes;

    file = mprOpen(http, path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        mprError(http, "Can't open %s", path);
        return MPR_ERR_CANT_OPEN;
    }
    while ((bytes = mprRead(file, buf, sizeof(buf))) > 0) {
        if (mprWriteHttp(http, buf, bytes) != bytes) {
            mprFree(file);
            return MPR_ERR_CANT_WRITE;
        }
    }
    mprFree(file);
    return 0;
}


/*
 *  Write a formatted string. Always block.
 */
static int writeFmt(MprHttp *http, cchar *fmt, ...)
{
    va_list     ap;
    char        *data;
    int         len;

    va_start(ap, fmt);
    data = mprVasprintf(http, -1, fmt, ap);
    va_end(ap);

    len = strlen(data);
    if (mprWriteHttp(http, data, len) != len) {
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


/*
 *  Write upload data. This routine blocks. If you need non-blocking ... cut and paste.
 */
int mprWriteHttpUploadData(MprHttp *http, MprList *fileData, MprList *formData)
{
    char        *path, *pair, *key, *value, *name;
    int         next, rc, oldMode;

    oldMode = mprSetSocketBlockingMode(http->sock, 1);
    rc = 0;

    if (formData) {
        for (rc = next = 0; !rc && (pair = mprGetNextItem(formData, &next)) != 0; ) {
            key = mprStrTok(mprStrdup(http, pair), "=", &value);
            rc += writeFmt(http, "%s\r\nContent-Disposition: form-data; name=\"%s\";\r\n", http->boundary, key);
            rc += writeFmt(http, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n", value);
        }
    }
    if (fileData) {
        for (rc = next = 0; !rc && (path = mprGetNextItem(fileData, &next)) != 0; ) {
            name = mprGetPathBase(http, path);
            rc += writeFmt(http, "%s\r\nContent-Disposition: form-data; name=\"file%d\"; filename=\"%s\"\r\n", 
                http->boundary, next - 1, name);
            mprFree(name);
            rc += writeFmt(http, "Content-Type: %s\r\n\r\n", mprLookupMimeType(http, path));
            rc += copyFile(http, path);
            rc += writeFmt(http, "\r\n", value);
        }
    }
    rc += writeFmt(http, "%s--\r\n--", http->boundary);
    if (mprFinalizeHttpWriting(http) < 0) {
        mprSetSocketBlockingMode(http->sock, oldMode);
        return MPR_ERR_CANT_WRITE;
    }
    mprSetSocketBlockingMode(http->sock, oldMode);
    return rc;
}


#if BLD_DEBUG
static void traceResponseData(MprCtx ctx, cchar *src, int size)
{
    char    dest[512];
    int     index, i;

    mprRawLog(ctx, 5, "@@@ Response data => \n");

    for (index = 0; index < size; ) { 
        for (i = 0; i < (sizeof(dest) - 1) && index < size; i++) {
            dest[i] = src[index];
            index++;
        }
        dest[i] = '\0';
        mprRawLog(ctx, 5, "%s", dest);
    }
    mprRawLog(ctx, 5, "\n");
}
#endif


int mprGetHttpCode(MprHttp *http)
{
    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    return http->response->code;
}


cchar *mprGetHttpMessage(MprHttp *http)
{
    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    return http->response->message;
}


int mprGetHttpContentLength(MprHttp *http)
{
    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    return http->response->contentLength;
}


cchar *mprGetHttpHeader(MprHttp *http, cchar *key)
{
    cchar   *value;
    char    *upperKey;

    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    upperKey = mprStrdup(http, key);
    mprStrUpper(upperKey);
    value = mprLookupHash(http->response->headers, upperKey);
    mprFree(upperKey);
    return value;
}


char *mprGetHttpHeaders(MprHttp *http)
{
    MprHttpResponse     *resp;
    MprHash             *hp;
    char                *headers, *key, *cp;
    int                 len;

    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    resp = http->response;
    headers = 0;
    for (len = 0, hp = mprGetFirstHash(resp->headers); hp; ) {
        headers = mprReallocStrcat(http, -1, headers, hp->key, NULL);
        key = &headers[len];
        for (cp = &key[1]; *cp; cp++) {
            *cp = tolower((int) *cp);
            if (*cp == '-') {
                cp++;
            }
        }
        headers = mprReallocStrcat(http, -1, headers, ": ", hp->data, "\n", NULL);
        len = strlen(headers);
        hp = mprGetNextHash(resp->headers, hp);
    }
    return headers;
}


MprHashTable *mprGetHttpHeadersHash(MprHttp *http)
{
    if (mprWaitForHttpResponse(http, -1) < 0) {
        return 0;
    }
    return http->response->headers;
}


cchar *mprGetHttpError(MprHttp *http)
{
    if (http->error) {
        return http->error;
    } else if (http->state > MPR_HTTP_STATE_WAIT) {
        return mprGetHttpCodeString(http, http->response->code);
    } else {
        return "";
    }
}


void mprSetHttpProxy(MprHttp *http, cchar *host, int port)
{
    conditionalReset(http);
    mprFree(http->proxyHost);
    http->proxyHost = mprStrdup(http, host);
    http->proxyPort = port;
}


void mprSetHttpCallback(MprHttp *http, MprHttpProc fn, void *arg, int mask)
{
    conditionalReset(http);
    http->callback = fn;
    http->callbackArg = arg;
    http->callbackMask = mask;
}


int mprGetHttpState(MprHttp *http)
{
    return http->state;
}


int mprGetHttpFlags(MprHttp *http)
{
    return http->response->flags;
}


void mprSetHttpKeepAlive(MprHttp *http, bool on)
{
    conditionalReset(http);
    http->useKeepAlive = on;
    http->keepAlive = on;
}


void mprSetHttpProtocol(MprHttp *http, cchar *protocol)
{
    conditionalReset(http);
    mprFree(http->protocol);
    http->protocol = mprStrdup(http, protocol);
    if (strcmp(http->protocol, "HTTP/1.0") == 0) {
        http->useKeepAlive = 0;
        http->keepAlive = 0;
        http->protocolVersion = 0;
    }
}


void mprSetHttpRetries(MprHttp *http, int num)
{
    http->retries = num;
}


int mprGetHttpDefaultPort(MprHttp *http)
{
    return http->defaultPort;
}


cchar *mprGetHttpDefaultHost(MprHttp *http)
{
    return http->defaultHost;
}


void mprSetHttpDefaultPort(MprHttp *http, int num)
{
    http->defaultPort = num;
}


void mprSetHttpDefaultHost(MprHttp *http, cchar *host)
{
    mprFree(http->defaultHost);
    http->defaultHost = mprStrdup(http, host);
}


void mprSetHttpContentLength(MprHttp *http, int length)
{
    char    buf[16];

    conditionalReset(http);
    mprItoa(buf, sizeof(buf), (int64) length, 10);
    mprSetHttpHeader(http, 1, "Content-Length", buf);
}


void mprSetHttpCredentials(MprHttp *http, cchar *username, cchar *password)
{
    conditionalReset(http);
    mprResetHttpCredentials(http);
    http->user = mprStrdup(http, username);
    if (password == NULL && strchr(username, ':') != 0) {
        http->user = mprStrTok(http->user, ":", &http->password);
    } else {
        http->password = mprStrdup(http, password);
    }
}


void mprResetHttpCredentials(MprHttp *http)
{
    mprFree(http->user);
    mprFree(http->password);
    mprFree(http->authDomain);
    mprFree(http->authCnonce);
    mprFree(http->authNonce);
    mprFree(http->authOpaque);
    mprFree(http->authRealm);
    mprFree(http->authQop);
    mprFree(http->authType);

    http->user = 0;
    http->password = 0;
    http->authType = 0;
    http->authDomain = 0;
    http->authCnonce = 0;
    http->authNonce = 0;
    http->authOpaque = 0;
    http->authRealm = 0;
    http->authQop = 0;
    http->authType = 0;
}


void mprSetHttpFollowRedirects(MprHttp *http, bool follow)
{
    http->followRedirects = follow;
}


int mprSetHttpHeader(MprHttp *http, bool overwrite, cchar *key, cchar *value)
{
    MprHttpRequest  *req;
    char            *persistKey, *persistValue;

    req = http->request;
    conditionalReset(http);

    persistKey = mprStrdup(req->headers, key);
    persistValue = mprStrdup(req->headers, value);
    if (overwrite) {
        mprAddHash(req->headers, persistKey, persistValue);
    } else {
        mprAddDuplicateHash(req->headers, persistKey, persistValue);
    }
    return 0;
}


int mprSetFormattedHttpHeader(MprHttp *http, bool overwrite, cchar *key, cchar *fmt, ...)
{
    va_list     args;
    char        *value;

    va_start(args, fmt);
    value = mprVasprintf(http, -1, fmt, args);
    va_end(args);
    mprSetHttpHeader(http, overwrite, key, value);
    mprFree(value);
    return 0;
}


int mprSetHttpTimeout(MprHttp *http, int timeout)
{
    int     old;

    conditionalReset(http);

    old = http->timeoutPeriod;
    http->timeoutPeriod = timeout;
    return old;
}


/*
 *  Create a random secret for use in authentication. Create once for the entire http service. Created on demand.
 *  Users can recall as required to update.
 */
int mprCreateHttpSecret(MprCtx ctx)
{
    MprHttpService  *hs;
    char            *hex = "0123456789abcdef";
    char            bytes[MPR_HTTP_MAX_SECRET], ascii[MPR_HTTP_MAX_SECRET * 2 + 1], *ap;
    int             i;

    hs = mprGetMpr(ctx)->httpService;

    if (mprGetRandomBytes(hs, bytes, sizeof(bytes), 0) < 0) {
        mprAssert(0);
        return MPR_ERR_CANT_INITIALIZE;
    }

    ap = ascii;
    for (i = 0; i < (int) sizeof(bytes); i++) {
        *ap++ = hex[bytes[i] >> 4];
        *ap++ = hex[bytes[i] & 0xf];
    }
    *ap = '\0';

    mprFree(hs->secret);
    hs->secret = mprStrdup(hs, ascii);

    return 0;
}


/*
 *  Get the next input token. The conn->input content buffer is advanced to the next token. This routine
 *  always returns a non-zero token. The empty string means the delimiter was not found.
 */
static char *getHttpToken(MprBuf *buf, cchar *delim)
{
    char    *token, *nextToken;
    int     len;

    len = mprGetBufLength(buf);
    if (len == 0) {
        return "";
    }

    token = mprGetBufStart(buf);
    nextToken = mprStrnstr(mprGetBufStart(buf), delim, len);
    if (nextToken) {
        *nextToken = '\0';
        len = (int) strlen(delim);
        nextToken += len;
        buf->start = nextToken;
    } else {
        buf->start = mprGetBufEnd(buf);
    }
    return token;
}


/*
 *  Get the next chunk size. Chunked data format is:
 *      Chunk spec <CRLF>
 *      Data <CRLF>
 *      Chunk spec (size == 0) <CRLF>
 *      <CRLF>
 *
 *  Chunk spec is: "HEX_COUNT; chunk length DECIMAL_COUNT\r\n". The "; chunk length DECIMAL_COUNT is optional.
 *  As an optimization, use "\r\nSIZE ...\r\n" as the delimiter so that the CRLF after data does not special 
 *  consideration. Achive this by parseHeaders reversing the input start by 2.
 *  Return false if the chunk could not be parsed due to lack of data AND the request has not failed.
 */
static bool parseChunk(MprHttp *http, MprBuf *buf)
{
    MprHttpResponse *resp;
    char            *start, *cp;
    int             bad;

    resp = http->response;
    resp->chunkRemaining = 0;

    if (mprGetBufLength(buf) < 5) {
        return 0;
    }
    /*
     *  Validate "\r\nSIZE.*\r\n"
     */
    start = mprGetBufStart(buf);
    bad = (start[0] != '\r' || start[1] != '\n');
    for (cp = &start[2]; cp < (char*) buf->end && *cp != '\n'; cp++) {}
    if (*cp != '\n' && (cp - start) < 80) {
        /* Insufficient data */
        return 0;
    }
    bad += (cp[-1] != '\r' || cp[0] != '\n');
    if (bad) {
        badRequest(http, "Bad chunk specification");
        return 1;
    }
    resp->chunkRemaining = (int) mprAtoi(&start[2], 16);
    if (!isxdigit((int) start[2]) || resp->chunkRemaining < 0) {
        badRequest(http, "Bad chunk specification");
        return 1;
    }
    mprAdjustBufStart(buf, cp - start + 1);
    return 1;
}


/*
 *  Handle a bad request
 */
static void badRequest(MprHttp *http, cchar *fmt, ...)
{
    va_list     args;

    mprAssert(fmt);

    if (http->error == NULL) {
        va_start(args, fmt);
        http->error = mprVasprintf(http, MPR_MAX_STRING, fmt, args);
        va_end(args);
    }
    mprLog(http, 3, "Http: badRequest: %s", http->error);
    http->keepAlive = 0;
    if (http->response) {
        http->response->code = MPR_HTTP_CODE_COMMS_ERROR;
    }
    completeRequest(http);
}


/*
 *  Complete a request. And prepare the http object for a new request.
 */
static void completeRequest(MprHttp *http)
{
    if (http->sock) {
        if (http->keepAlive) {
            mprLog(http, 4, "Http: completeRequest: Attempting keep-alive");
        } else {
            mprCloseSocket(http->sock, 1);
            mprFree(http->sock);
            http->sock = 0;
        }
    }
    http->state = MPR_HTTP_STATE_COMPLETE;
    cleanup(http);
}


#endif /* BLD_FEATURE_HTTP_CLIENT */

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprHttp.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprList.c"
 */
/************************************************************************/

/**
 *  mprList.c - Simple list type.
 *
 *  The list supports two modes of operation. Compact mode where the list is compacted after removing list items, 
 *  and no-compact mode where removed items are zeroed. No-compact mode implies that all valid list entries must 
 *  be non-zero.
 *
 *  This module is not thread-safe. It is the callers responsibility to perform all thread synchronization.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int growList(MprList *lp, int incr);

/*
 *  Create a general growable list structure. Use mprFree to destroy.
 */

MprList *mprCreateList(MprCtx ctx)
{
    MprList     *lp;

    lp = mprAllocObj(ctx, MprList);
    if (lp == 0) {
        return 0;
    }
    lp->capacity = 0;
    lp->length = 0;
    lp->maxSize = MAXINT;
    lp->items = 0;
    return lp;
}


/*
 *  Initialize a list which may not be a memory context.
 */
void mprInitList(MprList *lp)
{
    lp->capacity = 0;
    lp->length = 0;
    lp->maxSize = MAXINT;
    lp->items = 0;
}


/*
 *  Define the list maximum size. If the list has not yet been written to, the initialSize will be observed.
 */
int mprSetListLimits(MprList *lp, int initialSize, int maxSize)
{
    int         size;

    if (initialSize <= 0) {
        initialSize = MPR_LIST_INCR;
    }
    if (maxSize <= 0) {
        maxSize = MAXINT;
    }
    size = initialSize * sizeof(void*);

    if (lp->items == 0) {
        lp->items = (void**) mprAllocZeroed(lp, size);
        if (lp->items == 0) {
            mprFree(lp);
            return MPR_ERR_NO_MEMORY;
        }
        lp->capacity = initialSize;
    }
    lp->maxSize = maxSize;
    return 0;
}


int mprCopyList(MprList *dest, MprList *src)
{
    void        *item;
    int         next;

    mprClearList(dest);

    if (mprSetListLimits(dest, src->capacity, src->maxSize) < 0) {
        return MPR_ERR_NO_MEMORY;
    }

    for (next = 0; (item = mprGetNextItem(src, &next)) != 0; ) {
        if (mprAddItem(dest, item) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    return 0;
}


MprList *mprDupList(MprCtx ctx, MprList *src)
{
    MprList     *list;

    list = mprCreateList(ctx);
    if (list == 0) {
        return 0;
    }

    if (mprCopyList(list, src) < 0) {
        mprFree(list);
        return 0;
    }
    return list;
}


MprList *mprAppendList(MprList *list, MprList *add)
{
    void        *item;
    int         next;

    mprAssert(list);

    for (next = 0; ((item = mprGetNextItem(add, &next)) != 0); ) {
        if (mprAddItem(list, item) < 0) {
            mprFree(list);
            return 0;
        }
    }
    return list;
}


/*
 *  Change the item in the list at index. Return the old item.
 */
void *mprSetItem(MprList *lp, int index, cvoid *item)
{
    void    *old;

    mprAssert(lp);
    mprAssert(lp->capacity >= 0);
    mprAssert(lp->length >= 0);

    if (index >= lp->length) {
        lp->length = index + 1;
    }
    if (lp->length > lp->capacity) {
        if (growList(lp, lp->length - lp->capacity) < 0) {
            return 0;
        }
    }

    old = lp->items[index];
    lp->items[index] = (void*) item;

    return old;
}



/*
 *  Add an item to the list and return the item index.
 */
int mprAddItem(MprList *lp, cvoid *item)
{
    int     index;

    mprAssert(lp);
    mprAssert(lp->capacity >= 0);
    mprAssert(lp->length >= 0);

    if (lp->length >= lp->capacity) {
        if (growList(lp, 1) < 0) {
            return MPR_ERR_TOO_MANY;
        }
    }

    index = lp->length++;
    lp->items[index] = (void*) item;

    return index;
}


/*
 *  Insert an item to the list at a specified position. We insert before the item at "index".
 *  ie. The inserted item will go into the "index" location and the other elements will be moved up.
 */
int mprInsertItemAtPos(MprList *lp, int index, cvoid *item)
{
    void    **items;
    int     i;

    mprAssert(lp);
    mprAssert(lp->capacity >= 0);
    mprAssert(lp->length >= 0);
    mprAssert(index >= 0);

    if (index < 0) {
        index = 0;
    }
    if (index >= lp->capacity) {
        if (growList(lp, index - lp->capacity + 1) < 0) {
            return MPR_ERR_TOO_MANY;
        }

    } else if (lp->length >= lp->capacity) {
        if (growList(lp, 1) < 0) {
            return MPR_ERR_TOO_MANY;
        }
    }

    if (index >= lp->length) {
        lp->length = index + 1;

    } else {
        /*
         *  Copy up items to make room to insert
         */
        items = lp->items;
        for (i = lp->length; i > index; i--) {
            items[i] = items[i - 1];
        }
        lp->length++;
    }
    lp->items[index] = (void*) item;
    return index;
}


/*
 *  Remove an item from the list. Return the index where the item resided.
 */
int mprRemoveItem(MprList *lp, void *item)
{
    int     index;

    mprAssert(lp);

    index = mprLookupItem(lp, item);
    if (index < 0) {
        return index;
    }
    return mprRemoveItemAtPos(lp, index);
}


int mprRemoveLastItem(MprList *lp)
{
    mprAssert(lp);
    mprAssert(lp->capacity > 0);
    mprAssert(lp->length > 0);

    if (lp->length <= 0) {
        return MPR_ERR_NOT_FOUND;
    }
    return mprRemoveItemAtPos(lp, lp->length - 1);
}


/*
 *  Remove an index from the list. Return the index where the item resided.
 */
int mprRemoveItemAtPos(MprList *lp, int index)
{
    void    **items;
    int     i;

    mprAssert(lp);
    mprAssert(lp->capacity > 0);
    mprAssert(index >= 0 && index < lp->capacity);
    mprAssert(lp->length > 0);

    if (index < 0 || index >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }

    items = lp->items;
    for (i = index; i < (lp->length - 1); i++) {
        items[i] = items[i + 1];
    }
    lp->length--;
    lp->items[lp->length] = 0;

    return index;
}


/*
 *  Remove a set of items. Return 0 if successful.
 */
int mprRemoveRangeOfItems(MprList *lp, int start, int end)
{
    void    **items;
    int     i, count;

    mprAssert(lp);
    mprAssert(lp->capacity > 0);
    mprAssert(lp->length > 0);
    mprAssert(start > end);

    if (start < 0 || start >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }
    if (end < 0 || end >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }
    if (start > end) {
        return MPR_ERR_BAD_ARGS;
    }

    /*
     *  Copy down to compress
     */
    items = lp->items;
    count = end - start;
    for (i = start; i < (lp->length - count); i++) {
        items[i] = items[i + count];
    }
    lp->length -= count;
    for (i = lp->length; i < lp->capacity; i++) {
        items[i] = 0;
    }

    return 0;
}


void *mprGetItem(MprList *lp, int index)
{
    mprAssert(lp);

    if (index < 0 || index >= lp->length) {
        return 0;
    }
    return lp->items[index];
}


void *mprGetFirstItem(MprList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }

    if (lp->length == 0) {
        return 0;
    }
    return lp->items[0];
}


void *mprGetLastItem(MprList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }

    if (lp->length == 0) {
        return 0;
    }
    return lp->items[lp->length - 1];
}


void *mprGetNextItem(MprList *lp, int *next)
{
    void    *item;
    int     index;

    mprAssert(next);
    mprAssert(*next >= 0);

    if (lp == 0) {
        return 0;
    }

    index = *next;

    if (index < lp->length) {
        item = lp->items[index];
        *next = ++index;
        return item;
    }
    return 0;
}


void *mprGetPrevItem(MprList *lp, int *next)
{
    int     index;

    mprAssert(next);

    if (lp == 0) {
        return 0;
    }

    if (*next < 0) {
        *next = lp->length;
    }
    index = *next;

    if (--index < lp->length && index >= 0) {
        *next = index;
        return lp->items[index];
    }
    return 0;
}


int mprPushItem(MprList *lp, cvoid *item)
{
    return mprAddItem(lp, item);
}


cvoid *mprPopItem(MprList *lp)
{
    cvoid   *item;
    int     index;

    item = NULL;
    if (lp->length > 0) {
        index = lp->length - 1;
        item = mprGetItem(lp, index);
        mprRemoveItemAtPos(lp, index);
    }
    return item;
}


int mprGetListCount(MprList *lp)
{
    if (lp == 0) {
        return 0;
    }

    return lp->length;
}


int mprGetListCapacity(MprList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }

    return lp->capacity;
}


void mprClearList(MprList *lp)
{
    int     i;

    mprAssert(lp);

    for (i = 0; i < lp->length; i++) {
        lp->items[i] = 0;
    }
    lp->length = 0;
}


int mprLookupItem(MprList *lp, cvoid *item)
{
    int     i;

    mprAssert(lp);
    
    for (i = 0; i < lp->length; i++) {
        if (lp->items[i] == item) {
            return i;
        }
    }
    return MPR_ERR_NOT_FOUND;
}


/*
 *  Grow the list by the requried increment
 */
static int growList(MprList *lp, int incr)
{
    int     len, memsize;

    if (lp->maxSize <= 0) {
        lp->maxSize = MAXINT;
    }

    /*
     *  Need to grow the list
     */
    if (lp->capacity >= lp->maxSize) {
        mprAssert(lp->capacity < lp->maxSize);
        return MPR_ERR_TOO_MANY;
    }

    /*
     *  If growing by 1, then use the default increment which exponentially grows. Otherwise, assume the caller knows exactly
     *  how much the list needs to grow.
     */
    if (incr <= 1) {
        len = MPR_LIST_INCR + (lp->capacity * 2);
    } else {
        len = lp->capacity + incr;
    }
    memsize = len * sizeof(void*);

    /*
     *  Grow the list of items. Use the existing context for lp->items if it already exists. Otherwise use the list as the
     *  memory context owner.
     */
    lp->items = (void**) mprRealloc((lp->items) ? mprGetParent(lp->items): lp, lp->items, memsize);

    /*
     *  Zero the new portion (required for no-compact lists)
     */
    memset(&lp->items[lp->capacity], 0, sizeof(void*) * (len - lp->capacity));
    lp->capacity = len;

    return 0;
}


void mprSortList(MprList *lp, MprListCompareProc compare)
{
    qsort(lp->items, lp->length, sizeof(void*), compare);
}


MprKeyValue *mprCreateKeyPair(MprCtx ctx, cchar *key, cchar *value)
{
    MprKeyValue     *pair;
    
    pair = mprAllocObj(ctx, MprKeyValue);
    if (pair == 0) {
        return 0;
    }
    pair->key = mprStrdup(pair, key);
    pair->value = mprStrdup(pair, value);
    return pair;
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprList.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprLock.c"
 */
/************************************************************************/

/**
 *  mprLock.c - Thread Locking Support
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_MULTITHREAD

static int destroyLock(MprMutex *lock);
static int destroySpinLock(MprSpin *lock);


MprMutex *mprCreateLock(MprCtx ctx)
{
    MprMutex    *lock;
#if BLD_UNIX_LIKE
    pthread_mutexattr_t attr;
#endif

    mprAssert(ctx);

    lock = mprAllocObjWithDestructor(ctx, MprMutex, destroyLock);
    if (lock == 0) {
        return 0;
    }

#if BLD_UNIX_LIKE
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&lock->cs, &attr);
    pthread_mutexattr_destroy(&attr);

#elif WINCE
    InitializeCriticalSection(&lock->cs);

#elif BLD_WIN_LIKE
    InitializeCriticalSectionAndSpinCount(&lock->cs, 5000);

#elif VXWORKS
    /* Removed SEM_INVERSION_SAFE */
    lock->cs = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
    if (lock->cs == 0) {
        mprAssert(0);
        mprFree(lock);
        return 0;
    }
#endif
    return lock;
}


MprMutex *mprInitLock(MprCtx ctx, MprMutex *lock)
{
#if BLD_UNIX_LIKE
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&lock->cs, &attr);
    pthread_mutexattr_destroy(&attr);

#elif WINCE
    InitializeCriticalSection(&lock->cs);

#elif BLD_WIN_LIKE
    InitializeCriticalSectionAndSpinCount(&lock->cs, 5000);

#elif VXWORKS
    /* Removed SEM_INVERSION_SAFE */
    lock->cs = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
    if (lock->cs == 0) {
        mprAssert(0);
        mprFree(lock);
        return 0;
    }
#endif
    return lock;
}


/*
 *  Destroy a lock. Must be locked on entrance.
 */
static int destroyLock(MprMutex *lock)
{
    mprAssert(lock);
#if BLD_UNIX_LIKE
    pthread_mutex_unlock(&lock->cs);
    pthread_mutex_destroy(&lock->cs);

#elif BLD_WIN_LIKE
    DeleteCriticalSection(&lock->cs);

#elif VXWORKS
    semDelete(lock->cs);
#endif
    return 0;
}


/*
 *  Try to attain a lock. Do not block! Returns true if the lock was attained.
 */
bool mprTryLock(MprMutex *lock)
{
    int     rc;
#if BLD_UNIX_LIKE
    rc = pthread_mutex_trylock(&lock->cs) != 0;

#elif BLD_WIN_LIKE
    rc = TryEnterCriticalSection(&lock->cs) == 0;

#elif VXWORKS
    rc = semTake(lock->cs, NO_WAIT) != OK;
#endif
    return (rc) ? 0 : 1;
}


MprSpin *mprCreateSpinLock(MprCtx ctx)
{
    MprSpin    *lock;
#if BLD_UNIX_LIKE && !MACOSX
    pthread_mutexattr_t attr;
#endif

    mprAssert(ctx);

    lock = mprAllocObjWithDestructor(ctx, MprSpin, destroySpinLock);
    if (lock == 0) {
        return 0;
    }

#if USE_MPR_LOCK
    mprInitLock(ctx, &lock->cs);

#elif MACOSX
    lock->cs = OS_SPINLOCK_INIT;

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    pthread_spin_init(&lock->cs, 0);

#elif BLD_UNIX_LIKE
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&lock->cs, &attr);
    pthread_mutexattr_destroy(&attr);

#elif WINCE
    InitializeCriticalSection(&lock->cs);

#elif BLD_WIN_LIKE
    InitializeCriticalSectionAndSpinCount(&lock->cs, 5000);

#elif VXWORKS
    /* Removed SEM_INVERSION_SAFE */
    lock->cs = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
    if (lock->cs == 0) {
        mprAssert(0);
        mprFree(lock);
        return 0;
    }
#endif

#if BLD_DEBUG
    lock->owner = 0;
#endif
    return lock;
}


/*
 *  Static version just for mprAlloc which needs locks that don't allocate memory.
 */
MprSpin *mprInitSpinLock(MprCtx ctx, MprSpin *lock)
{
#if BLD_UNIX_LIKE && !MACOSX
    pthread_mutexattr_t attr;
#endif

    mprAssert(ctx);

#if USE_MPR_LOCK
    mprInitLock(ctx, &lock->cs);

#elif MACOSX
    lock->cs = OS_SPINLOCK_INIT;

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    pthread_spin_init(&lock->cs, 0);

#elif BLD_UNIX_LIKE
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&lock->cs, &attr);
    pthread_mutexattr_destroy(&attr);

#elif WINCE
    InitializeCriticalSection(&lock->cs);

#elif BLD_WIN_LIKE
    InitializeCriticalSectionAndSpinCount(&lock->cs, 5000);

#elif VXWORKS
    /* Removed SEM_INVERSION_SAFE */
    lock->cs = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
    if (lock->cs == 0) {
        mprAssert(0);
        mprFree(lock);
        return 0;
    }
#endif

#if BLD_DEBUG
    lock->owner = 0;
#endif
    return lock;
}


/*
 *  Destroy a lock. Must be locked on entrance.
 */
static int destroySpinLock(MprSpin *lock)
{
    mprAssert(lock);
#if USE_MPR_LOCK || MACOSX
    ;

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    pthread_spin_destroy(&lock->cs);

#elif BLD_UNIX_LIKE
    pthread_mutex_destroy(&lock->cs);

#elif BLD_WIN_LIKE
    DeleteCriticalSection(&lock->cs);

#elif VXWORKS
    semDelete(lock->cs);
#endif
    return 0;
}




/*
 *  Try to attain a lock. Do not block! Returns true if the lock was attained.
 */
bool mprTrySpinLock(MprSpin *lock)
{
    int     rc;

#if USE_MPR_LOCK
    mprTryLock(&lock->cs);

#elif MACOSX
    rc = !OSSpinLockTry(&lock->cs);

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    rc = pthread_spin_trylock(&lock->cs) != 0;

#elif BLD_UNIX_LIKE
    rc = pthread_mutex_trylock(&lock->cs) != 0;

#elif BLD_WIN_LIKE
    rc = TryEnterCriticalSection(&lock->cs) == 0;

#elif VXWORKS
    rc = semTake(lock->cs, NO_WAIT) != OK;

#endif
#if BLD_DEBUG
    if (rc == 0) {
        mprAssert(lock->owner != mprGetCurrentOsThread());
        lock->owner = mprGetCurrentOsThread();
    }
#endif
    return (rc) ? 0 : 1;
}


/*
 *  Big global lock. Avoid using this.
 */
void mprGlobalLock(MprCtx ctx)
{
    Mpr *mpr;

    mpr = mprGetMpr(ctx);
    mprAssert(mpr);

    if (mpr && mpr->mutex) {
        mprLock(mpr->mutex);
    }
}


void mprGlobalUnlock(MprCtx ctx)
{
    Mpr *mpr;

    mpr = mprGetMpr(ctx);
    mprAssert(mpr);

    if (mpr && mpr->mutex) {
        mprUnlock(mpr->mutex);
    }
}


#if BLD_USE_LOCK_MACROS
/*
 *  Still define these even if using macros to make linking with *.def export files easier
 */
#undef mprLock
#undef mprUnlock
#undef mprSpinLock
#undef mprSpinUnlock
#endif

/*
 *  Lock a mutex
 */
void mprLock(MprMutex *lock)
{
#if BLD_UNIX_LIKE
    pthread_mutex_lock(&lock->cs);

#elif BLD_WIN_LIKE
    EnterCriticalSection(&lock->cs);

#elif VXWORKS
    semTake(lock->cs, WAIT_FOREVER);
#endif
}


void mprUnlock(MprMutex *lock)
{
#if BLD_UNIX_LIKE
    pthread_mutex_unlock(&lock->cs);

#elif BLD_WIN_LIKE
    LeaveCriticalSection(&lock->cs);

#elif VXWORKS
    semGive(lock->cs);
#endif
}


/*
 *  Use functions for debug mode. Production release uses macros
 */
/*
 *  Lock a mutex
 */
void mprSpinLock(MprSpin *lock)
{
#if BLD_DEBUG
    /*
     *  Spin locks don't support recursive locking on all operating systems.
     */
    mprAssert(lock->owner != mprGetCurrentOsThread());
#endif

#if USE_MPR_LOCK
    mprLock(&lock->cs);

#elif MACOSX
    OSSpinLockLock(&lock->cs);

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    pthread_spin_lock(&lock->cs);

#elif BLD_UNIX_LIKE
    pthread_mutex_lock(&lock->cs);

#elif BLD_WIN_LIKE
    EnterCriticalSection(&lock->cs);

#elif VXWORKS
    semTake(lock->cs, WAIT_FOREVER);
#endif

#if BLD_DEBUG
    mprAssert(lock->owner != mprGetCurrentOsThread());
    lock->owner = mprGetCurrentOsThread();
#endif
}


void mprSpinUnlock(MprSpin *lock)
{
#if BLD_DEBUG
    lock->owner = 0;
#endif

#if USE_MPR_LOCK
    mprUnlock(&lock->cs);

#elif MACOSX
    OSSpinLockUnlock(&lock->cs);

#elif BLD_UNIX_LIKE && BLD_HAS_SPINLOCK
    pthread_spin_unlock(&lock->cs);

#elif BLD_UNIX_LIKE
    pthread_mutex_unlock(&lock->cs);

#elif BLD_WIN_LIKE
    LeaveCriticalSection(&lock->cs);

#elif VXWORKS
    semGive(lock->cs);
#endif
}


#else /* BLD_FEATURE_MULTITHREAD */
void __dummyMprLock() {}
#endif /* BLD_FEATURE_MULTITHREAD */

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprLock.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprLog.c"
 */
/************************************************************************/

/**
 *  mprLog.c - Michael's Portable Runtime (MPR) Logging and error reporting.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static void defaultLogHandler(MprCtx ctx, int flags, int level, cchar *msg);
static void logOutput(MprCtx ctx, int flags, int level, cchar *msg);

/*
 *  Put first in file so it is easy to locate in a debugger
 */
void mprBreakpoint()
{
#if BLD_DEBUG && DEBUG_IDE
    #if BLD_HOST_CPU_ARCH == MPR_CPU_IX86 || BLD_HOST_CPU_ARCH == MPR_CPU_IX64
        #if WINCE
            /* Do nothing */
        #elif BLD_WIN_LIKE
            __asm { int 3 };
        #else
            asm("int $03");
            /*  __asm__ __volatile__ ("int $03"); */
        #endif
    #endif
#endif
}


void mprLog(MprCtx ctx, int level, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    mprAssert(ctx);

    if (level > mprGetLogLevel(ctx)) {
        return;
    }
    va_start(args, fmt);
    buf = mprVasprintf(ctx, -1, fmt, args);
    va_end(args);

    logOutput(ctx, MPR_LOG_SRC, level, buf);
    mprFree(buf);
}


/*
 *  Do raw output
 */
void mprRawLog(MprCtx ctx, int level, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    if (level > mprGetLogLevel(ctx)) {
        return;
    }
    va_start(args, fmt);
    buf = mprVasprintf(ctx, -1, fmt, args);
    va_end(args);
    
    logOutput(ctx, MPR_RAW, 0, buf);
    mprFree(buf);
}


/*
 *  Handle an error
 */
void mprError(MprCtx ctx, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    va_start(args, fmt);
    buf = mprVasprintf(ctx, -1, fmt, args);
    va_end(args);
    
    logOutput(ctx, MPR_ERROR_MSG | MPR_ERROR_SRC, 0, buf);

    mprFree(buf);
    mprBreakpoint();
}


/*
 *  Handle a memory allocation error
 */
void mprMemoryError(MprCtx ctx, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    if (fmt == 0) {
        logOutput(ctx, MPR_ERROR_MSG | MPR_ERROR_SRC, 0, "Memory allocation error");
    } else {
        va_start(args, fmt);
        buf = mprVasprintf(ctx, -1, fmt, args);
        va_end(args);
        logOutput(ctx, MPR_ERROR_MSG | MPR_ERROR_SRC, 0, buf);
        mprFree(buf);
    }
}


/*
 *  Handle an error that should be displayed to the user
 */
void mprUserError(MprCtx ctx, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    va_start(args, fmt);
    buf = mprVasprintf(ctx, -1, fmt, args);
    va_end(args);
    
    logOutput(ctx, MPR_USER_MSG | MPR_ERROR_SRC, 0, buf);
    mprFree(buf);
}


/*
 *  Handle a fatal error. Forcibly shutdown the application.
 */
void mprFatalError(MprCtx ctx, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    va_start(args, fmt);
    buf = mprVasprintf(ctx, -1, fmt, args);
    va_end(args);
    
    logOutput(ctx, MPR_USER_MSG | MPR_FATAL_SRC, 0, buf);
    mprFree(buf);

#if BREW
    mprSignalExit(ctx);
#else
    exit(2);
#endif
}


/*
 *  Handle an error without allocating memory.
 */
void mprStaticError(MprCtx ctx, cchar *fmt, ...)
{
    va_list     args;
    char        buf[MPR_MAX_STRING];

    va_start(args, fmt);
    mprVsprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    logOutput(ctx, MPR_ERROR_MSG | MPR_ERROR_SRC, 0, buf);
}


/*
 *  Direct output to the standard error. Does not hook into the logging system and does not allocate memory.
 */
void mprStaticAssert(cchar *loc, cchar *msg)
{
#if BLD_DEBUG
    char    buf[MPR_MAX_STRING];

    mprSprintf(buf, sizeof(buf), "Assertion %s, failed at %s\n", msg, loc);
    
#if BLD_UNIX_LIKE || VXWORKS
    write(2, buf, strlen(buf));
#elif BREW || BLD_WIN_LIKE
    /*
     *  Only time we use printf. We can't get an alloc context so we have to use real print
     */
#if BREW && !BREWSIM
    fprintf(stderr, " MP: %s\n", buf);
#else
    fprintf(stderr, "%s\n", buf);
#endif

#endif
    mprBreakpoint();
#endif
}


int mprGetLogLevel(MprCtx ctx)
{
    Mpr     *mpr;

    /*
     *  Leave the code like this so debuggers can patch logLevel before returning.
     */
    mpr = mprGetMpr(ctx);
    return mpr->logLevel;
}


void mprSetLogLevel(MprCtx ctx, int level)
{
    mprGetMpr(ctx)->logLevel = level;
}


/*
 *  Output a log message to the log handler
 */
static void logOutput(MprCtx ctx, int flags, int level, cchar *msg)
{
    MprLogHandler   handler;

    mprAssert(ctx != 0);
    handler = mprGetMpr(ctx)->logHandler;
    if (handler != 0) {
        (handler)(ctx, flags, level, msg);
        return;
    }
    defaultLogHandler(ctx, flags, level, msg);
}


static void defaultLogHandler(MprCtx ctx, int flags, int level, cchar *msg)
{
    Mpr     *mpr;
    char    *prefix;

    mpr = mprGetMpr(ctx);
    prefix = mpr->name;

    if (msg == 0) {
        return;
    }

    while (*msg == '\n') {
        mprPrintfError(ctx, "\n");
        msg++;
    }

    if (flags & MPR_LOG_SRC) {
#if BREW && !BREWSIM
        mprPrintfError(ctx, "%s\n", msg);
#else
        mprPrintfError(ctx, "%s: %d: %s\n", prefix, level, msg);
#endif

    } else if (flags & MPR_ERROR_SRC) {
        /*
         *  Use static printing to avoid malloc when the messages are small.
         *  This is important for memory allocation errors.
         */
        if (strlen(msg) < (MPR_MAX_STRING - 32)) {
            mprStaticPrintfError(ctx, "%s: Error: %s\n", prefix, msg);
        } else {
            mprPrintfError(ctx, "%s: Error: %s\n", prefix, msg);
        }

    } else if (flags & MPR_FATAL_SRC) {
        mprPrintfError(ctx, "%s: Fatal: %s\n", prefix, msg);

    } else if (flags & MPR_RAW) {
        mprPrintfError(ctx, "%s", msg);

    } else {
        return;
    }
}


#if BLD_WIN_LIKE
/*
 *  Return the raw O/S error code
 */
int mprGetOsError()
{
    int     rc;
    rc = GetLastError();

    /*
     *  Client has closed the pipe
     */
    if (rc == ERROR_NO_DATA) {
        return EPIPE;
    }
    return rc;
}


/*
 *  Return the mapped (portable, Posix) error code
 */
int mprGetError()
{
    int     err;

    err = mprGetOsError();

    switch (err) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
        return EPERM;
    case ERROR_INVALID_HANDLE:
        return EBADF;
    case ERROR_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case ERROR_PATH_BUSY:
    case ERROR_BUSY_DRIVE:
    case ERROR_NETWORK_BUSY:
    case ERROR_PIPE_BUSY:
    case ERROR_BUSY:
        return EBUSY;
    case ERROR_FILE_EXISTS:
        return EEXIST;
    case ERROR_BAD_PATHNAME:
    case ERROR_BAD_ARGUMENTS:
        return EINVAL;
    case WSAENOTSOCK:
        return ENOENT;
    case WSAEINTR:
        return EINTR;
    case WSAEBADF:
        return EBADF;
    case WSAEACCES:
        return EACCES;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    }
    return MPR_ERR;
}
#endif


#if MACOSX
/*
 *  Just for conditional breakpoints when debugging in Xcode
 */
int _cmp(char *s1, char *s2)
{
    return !strcmp(s1, s2);
}
#endif

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprLog.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprModule.c"
 */
/************************************************************************/

/**
 *  mprModule.c - Dynamic module loading support.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Open the module service
 */
MprModuleService *mprCreateModuleService(MprCtx ctx)
{
    MprModuleService    *ms;
    cchar               *searchPath;

    ms = mprAllocObjZeroed(ctx, MprModuleService);
    if (ms == 0) {
        return 0;
    }
    ms->modules = mprCreateList(ms);

    /*
     *  Define the default module search path
     */
    if (ms->searchPath == 0) {
#if BLD_DEBUG
        /*
         *  Put the mod prefix here incase running an installed debug build
         */
        searchPath = ".:" BLD_MOD_NAME ":../" BLD_MOD_NAME ":../../" BLD_MOD_NAME ":../../../" BLD_MOD_NAME ":" \
            BLD_MOD_PREFIX;
#else
        searchPath = BLD_MOD_PREFIX ":.";
#endif
    } else {
        searchPath = ms->searchPath;
    }
    ms->searchPath = mprStrdup(ms, (searchPath) ? searchPath : (cchar*) ".");

#if BLD_FEATURE_MULTITHREAD
    ms->mutex = mprCreateLock(ms);
#endif
    return ms;
}


/*
 *  Call the start routine for each module
 */
int mprStartModuleService(MprModuleService *ms)
{
    MprModule       *mp;
    int             next;

    mprAssert(ms);

    for (next = 0; (mp = mprGetNextItem(ms->modules, &next)) != 0; ) {
        if (mprStartModule(mp) < 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
#if VXWORKS && BLD_DEBUG && SYM_SYNC_INCLUDED
    symSyncLibInit();
#endif
    return 0;
}


/*
 *  Stop all modules
 */
void mprStopModuleService(MprModuleService *ms)
{
    MprModule       *mp;
    int             next;

    mprAssert(ms);

    mprLock(ms->mutex);
    for (next = 0; (mp = mprGetNextItem(ms->modules, &next)) != 0; ) {
        mprStopModule(mp);
    }
    mprUnlock(ms->mutex);
}


int mprStartModule(MprModule *mp)
{
    mprAssert(mp);

    if (mp->start && !(mp->flags & MPR_MODULE_STARTED)) {
        if (mp->start(mp) < 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    mp->flags |= MPR_MODULE_STARTED;
    return 0;
}


void mprStopModule(MprModule *mp)
{
    mprAssert(mp);

    if (mp->stop && (mp->flags & MPR_MODULE_STARTED) && !(mp->flags & MPR_MODULE_STOPPED)) {
        mp->stop(mp);
    }
    mp->flags |= MPR_MODULE_STOPPED;
}


/*
 *  Create a new module
 */
MprModule *mprCreateModule(MprCtx ctx, cchar *name, cchar *version, void *data, MprModuleProc start, MprModuleProc stop)
{
    MprModuleService    *ms;
    MprModule           *mp;
    Mpr                 *mpr;
    int                 index;

    mpr = mprGetMpr(ctx);
    ms = mpr->moduleService;
    mprAssert(ms);

    mp = mprAllocObj(mpr, MprModule);
    if (mp == 0) {
        return 0;
    }
    index = mprAddItem(ms->modules, mp);
    mp->name = mprStrdup(mp, name);
    mp->version = mprStrdup(mp, version);
    mp->moduleData = data;
    mp->handle = 0;
    mp->timeout = 0;
    mp->lastActivity = mprGetTime(ctx);

    if (index < 0 || mp->name == 0 || mp->version == 0) {
        mprFree(mp);
        return 0;
    }
    mp->start = start;
    mp->stop = stop;

    if (mpr->flags & MPR_STARTED) {
        if (mprStartModule(mp) < 0) {
            return 0;
        }
    }
    return mp;
}


/*
 *  See if a module is already loaded
 */
MprModule *mprLookupModule(MprCtx ctx, cchar *name)
{
    MprModuleService    *ms;
    MprModule           *mp;
    int                 next;

    mprAssert(name && name);

    ms = mprGetMpr(ctx)->moduleService;
    mprAssert(ms);

    for (next = 0; (mp = mprGetNextItem(ms->modules, &next)) != 0; ) {
        mprAssert(mp->name);
        if (mp && strcmp(mp->name, name) == 0) {
            return mp;
        }
    }
    return 0;
}


void *mprLookupModuleData(MprCtx ctx, cchar *name)
{
    MprModule   *module;

    if ((module = mprLookupModule(ctx, name)) == NULL) {
        return NULL;
    }
    return module->moduleData;
}


void mprSetModuleSearchPath(MprCtx ctx, char *searchPath)
{
    MprModuleService    *ms;
    Mpr                 *mpr;

    mprAssert(ctx);
    mprAssert(searchPath && *searchPath);

    mpr = mprGetMpr(ctx);
    mprAssert(mpr);
    ms = mpr->moduleService;

    mprFree(ms->searchPath);
    ms->searchPath = mprStrdup(ms, searchPath);

#if BLD_WIN_LIKE && !WINCE
    {
        char    *path;

        /*
         *  So dependent DLLs can be loaded by LoadLibrary
         */
        path = mprStrcat(mpr, -1, "PATH=", searchPath, ";", getenv("PATH"), NULL);
        mprMapSeparators(mpr, path, '\\');
        putenv(path);
        mprFree(path);
    }
#endif
}


cchar *mprGetModuleSearchPath(MprCtx ctx)
{
    MprModuleService    *ms;
    Mpr                 *mpr;

    mprAssert(ctx);

    mpr = mprGetMpr(ctx);
    mprAssert(mpr);
    ms = mpr->moduleService;

    return ms->searchPath;
}


#if BLD_CC_DYN_LOAD
/*
 *  Return true if the shared library in "file" can be found. Return the actual path in *path. The filename
 *  may not have a shared library extension which is typical so calling code can be cross platform.
 */
static int probe(MprCtx ctx, cchar *filename, char **pathp)
{
    char    *path;

    mprAssert(ctx);
    mprAssert(filename && *filename);
    mprAssert(pathp);

    *pathp = 0;
    mprLog(ctx, 4, "Probe for filename %s", filename);
    if (mprPathExists(ctx, filename, R_OK)) {
        *pathp = mprStrdup(ctx, filename);
        return 1;
    }

    if (strstr(filename, BLD_SHOBJ) == 0) {
        path = mprStrcat(ctx, -1, filename, BLD_SHOBJ, NULL);
        mprLog(ctx, 4, "Probe for library %s", path);
        if (mprPathExists(ctx, path, R_OK)) {
            *pathp = path;
            return 1;
        }
        mprFree(path);
    }
    return 0;
}


/*
 *  Search for a module in the modulePath.
 */
int mprSearchForModule(MprCtx ctx, cchar *name, char **path)
{
    char    *fileName, *searchPath, *dir, *tok;

    /*
     *  Search for path directly
     */
    if (probe(ctx, name, path)) {
        mprLog(ctx, 4, "Found package %s at %s", name, *path);
        return 0;
    }

    /*
     *  Search in the searchPath
     */
    searchPath = mprStrdup(ctx, mprGetModuleSearchPath(ctx));

    tok = 0;
    dir = mprStrTok(searchPath, MPR_SEARCH_SEP, &tok);
    while (dir && *dir) {
        fileName = mprJoinPath(ctx, dir, name);
        if (probe(ctx, fileName, path)) {
            mprFree(fileName);
            mprLog(ctx, 4, "Found package %s at %s", name, *path);
            return 0;
        }
        mprFree(fileName);
        dir = mprStrTok(0, MPR_SEARCH_SEP, &tok);
    }
    mprFree(searchPath);
    return MPR_ERR_NOT_FOUND;
}
#endif


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprModule.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprPath.c"
 */
/************************************************************************/

/**
    mprPath.c - Path (filename) services.
 *
    This modules provides cross platform path name services.
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
    Find the first separator in the path
 */
#if BLD_UNIX_LIKE
    #define firstSep(fs, path)      strchr(path, fs->separators[0])
#else
    #define firstSep(fs, path)      strpbrk(path, fs->separators)
#endif

#define defaultSep(fs)              (fs->separators[0])

static MPR_INLINE bool isSep(MprFileSystem *fs, int c) 
{
    char    *separators;

    mprAssert(fs);

    for (separators = fs->separators; *separators; separators++) {
        if (*separators == c)
            return 1;
    }
    return 0;
}


static MPR_INLINE bool hasDrive(MprFileSystem *fs, cchar *path) 
{
    char    *cp, *endDrive;

    mprAssert(fs);
    mprAssert(path);

    if (fs->hasDriveSpecs) {
        cp = firstSep(fs, path);
        endDrive = strchr(path, ':');
        if (endDrive && (cp == NULL || endDrive < cp)) {
            return 1;
        }
    }
    return 0;
}


static MPR_INLINE bool isAbsPath(MprFileSystem *fs, cchar *path) 
{
    char    *cp, *endDrive;

    mprAssert(fs);
    mprAssert(path);

    if (fs->hasDriveSpecs) {
        if ((cp = firstSep(fs, path)) != 0) {
            if ((endDrive = strchr(path, ':')) != 0) {
                if (&endDrive[1] == cp) {
                    return 1;
                }
            }
            if (cp == path) {
                return 1;
            }
        }
    } else {
        if (isSep(fs, path[0])) {
            return 1;
        }
    }
    return 0;
}


static MPR_INLINE bool isFullPath(MprFileSystem *fs, cchar *path) 
{
    char    *cp, *endDrive;

    mprAssert(fs);
    mprAssert(path);

    if (fs->hasDriveSpecs) {
        cp = firstSep(fs, path);
        endDrive = strchr(path, ':');
        if (endDrive && cp && &endDrive[1] == cp) {
            return 1;
        }
    } else {
        if (isSep(fs, path[0])) {
            return 1;
        }
    }
    return 0;
}


static MPR_INLINE bool isRoot(MprFileSystem *fs, cchar *path) 
{
    char    *cp;

    if (isFullPath(fs, path)) {
        cp = firstSep(fs, path);
        if (cp && cp[1] == '\0') {
            return 1;
        }
    }
    return 0;
}


static MPR_INLINE char *lastSep(MprFileSystem *fs, cchar *path) 
{
    char    *cp;

    for (cp = (char*) &path[strlen(path)] - 1; cp >= path; cp--) {
        if (isSep(fs, *cp)) {
            return cp;
        }
    }
    return 0;
}

/*
    This copies the filename at the designated path
 */
int mprCopyPath(MprCtx ctx, cchar *fromName, cchar *toName, int mode)
{
    MprFile     *from, *to;
    char        buf[MPR_BUFSIZE];
    int         count;

    if ((from = mprOpen(ctx, fromName, O_RDONLY | O_BINARY, 0)) == 0) {
        mprError(ctx, "Can't open %s", fromName);
        return MPR_ERR_CANT_OPEN;
    }
    if ((to = mprOpen(ctx, toName, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, mode)) == 0) {
        mprError(ctx, "Can't open %s", toName);
        return MPR_ERR_CANT_OPEN;
    }
    
    while ((count = mprRead(from, buf, sizeof(buf))) > 0) {
        mprWrite(to, buf, count);
    }
    mprFree(from);
    mprFree(to);
    return 0;
}


int mprDeletePath(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    if (path == NULL || *path == '\0') {
        return MPR_ERR_CANT_ACCESS;
    }
    fs = mprLookupFileSystem(ctx, path);
    if (!mprPathExists(ctx, path, F_OK)) {
        return 0;
    }
    return fs->deletePath(fs, path);
}


/*
    Return an absolute (normalized) path.
 */
char *mprGetAbsPath(MprCtx ctx, cchar *pathArg)
{
    MprFileSystem   *fs;
    char            *path;

    if (pathArg == 0 || *pathArg == '\0') {
        pathArg = ".";
    }

#if BLD_FEATURE_ROMFS
    return mprGetNormalizedPath(ctx, pathArg);
#endif

    fs = mprLookupFileSystem(ctx, pathArg);
    if (isFullPath(fs, pathArg)) {
        return mprGetNormalizedPath(ctx, pathArg);
    }

#if BLD_WIN_LIKE && !WINCE
{
    char    buf[MPR_MAX_PATH];
    GetFullPathName(pathArg, sizeof(buf) - 1, buf, NULL);
    buf[sizeof(buf) - 1] = '\0';
    path = mprGetNormalizedPath(ctx, buf);
}
#elif VXWORKS
{
    char    *dir;
    if (hasDrive(fs, pathArg)) {
        dir = mprGetCurrentPath(ctx);
        path = mprJoinPath(ctx, dir, &strchr(pathArg, ':')[1]);
        mprFree(dir);

    } else {
        if (isAbsPath(fs, pathArg)) {
            /*
             *  Path is absolute, but without a drive. Use the current drive.
             */
            dir = mprGetCurrentPath(ctx);
            path = mprJoinPath(ctx, dir, pathArg);
            mprFree(dir);
        } else {
            dir = mprGetCurrentPath(ctx);
            path = mprJoinPath(ctx, dir, pathArg);
            mprFree(dir);
        }
    }
}
#else
{
    char   *dir;
    dir = mprGetCurrentPath(ctx);
    path = mprJoinPath(ctx, dir, pathArg);
    mprFree(dir);
}
#endif
    return path;
}


/*
    This will return a fully qualified absolute path for the current working directory
 */
char *mprGetCurrentPath(MprCtx ctx)
{
    MprFileSystem   *fs;
    char            dir[MPR_MAX_PATH];

    fs = mprLookupFileSystem(ctx, dir);
    if (getcwd(dir, sizeof(dir)) == 0) {
        return mprGetAbsPath(ctx, "/");
    }

#if VXWORKS
{
    char    sep[2];

    /*
     *  Vx will sometimes just return a drive with no path.
     */
    if (firstSep(fs, dir) == NULL) {
        sep[0] = defaultSep(fs);
        sep[1] = '\0';
        return mprStrcat(ctx, -1, dir, sep, NULL);
    }
}
#elif BLD_WIN_LIKE
    mprMapSeparators(ctx, dir, fs->separators[0]);
#endif
    return mprStrdup(ctx, dir);
}


char *mprGetNativePath(MprCtx ctx, cchar *path)
{
    return mprGetTransformedPath(ctx, path, MPR_PATH_NATIVE_SEP);
}


/*
    Return the last portion of a pathname. The separators are not mapped and the path is not cleaned.
 */
char *mprGetPathBase(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    char            *cp;

    fs = mprLookupFileSystem(ctx, path);
    cp = (char*) lastSep(fs, path);
    if (cp == 0) {
        return mprStrdup(ctx, path);
    } 
    if (cp == path) {
        if (cp[1] == '\0') {
            return mprStrdup(ctx, path);
        }
    } else {
        if (cp[1] == '\0') {
            return mprStrdup(ctx, "");
        }
    }
    return mprStrdup(ctx, &cp[1]);
}


/*
    Return the directory portion of a pathname into the users buffer.
 */
char *mprGetPathDir(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    cchar           *cp;
    char            *result;
    int             len;

    mprAssert(path);
    mprAssert(ctx);

    if (*path == '\0') {
        return mprStrdup(ctx, path);
    }

    fs = mprLookupFileSystem(ctx, path);
    len = (int) strlen(path);
    cp = &path[len - 1];

    /*
     *  Step back over trailing slashes
     */
    while (cp > path && isSep(fs, *cp)) {
        cp--;
    }
    for (; cp > path && !isSep(fs, *cp); cp--) {
        ;
    }
    if (cp == path) {
        if (!isSep(fs, *cp)) {
            /* No slashes found, parent is current dir */
            return mprStrdup(ctx, ".");
        }
        return mprStrdup(ctx, fs->root);
    }

    len = (int) (cp - path);
    result = mprAlloc(ctx, len + 1);
    mprMemcpy(result, len + 1, path, len);
    result[len] = '\0';
    return result;
}


#if BLD_WIN_LIKE
MprList *mprGetPathFiles(MprCtx ctx, cchar *dir, bool enumDirs)
{
    HANDLE          h;
    MprDirEntry     *dp;
    MprPath         fileInfo;
    MprList         *list;
    char            *path, pbuf[MPR_MAX_PATH];
    int             sep;
#if WINCE
    WIN32_FIND_DATAA findData;
#else
    WIN32_FIND_DATA findData;
#endif

    list = 0;
    dp = 0;

    if ((path = mprJoinPath(ctx, dir, "*.*")) == 0) {
        return 0;
    }
    sep = mprGetPathSeparator(ctx, dir);

    h = FindFirstFile(path, &findData);
    if (h == INVALID_HANDLE_VALUE) {
        mprFree(path);
        return 0;
    }
    list = mprCreateList(ctx);

    do {
        if (findData.cFileName[0] == '.' && (findData.cFileName[1] == '\0' || findData.cFileName[1] == '.')) {
            continue;
        }
        if (enumDirs || !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            dp = mprAllocObjZeroed(list, MprDirEntry);
            if (dp == 0) {
                mprFree(path);
                return 0;
            }

            dp->name = mprStrdup(dp, findData.cFileName);
            if (dp->name == 0) {
                mprFree(path);
                return 0;
            }

            /* dp->lastModified = (uint) findData.ftLastWriteTime.dwLowDateTime; */

            if (mprSprintf(pbuf, sizeof(pbuf), "%s%c%s", dir, sep, dp->name) < 0) {
                dp->lastModified = 0;
            } else {
                mprGetPathInfo(ctx, pbuf, &fileInfo);
                dp->lastModified = fileInfo.mtime;
            }
            dp->isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;

#if FUTURE_64_BIT
            if (findData.nFileSizeLow < 0) {
                dp->size = (((uint64) findData.nFileSizeHigh) * INT64(4294967296)) + (4294967296L - 
                    (uint64) findData.nFileSizeLow);
            } else {
                dp->size = (((uint64) findData.nFileSizeHigh * INT64(4294967296))) +  (uint64) findData.nFileSizeLow;
            }
#else
            dp->size = (uint) findData.nFileSizeLow;
#endif
            mprAddItem(list, dp);
        }
    } while (FindNextFile(h, &findData) != 0);

    mprFree(path);
    FindClose(h);
    return list;
}
#endif /* WIN */


#if BLD_UNIX_LIKE || VXWORKS || CYGWIN
MprList *mprGetPathFiles(MprCtx ctx, cchar *path, bool enumDirs)
{
    DIR             *dir;
    MprPath         fileInfo;
    MprList         *list;
    struct dirent   *dirent;
    MprDirEntry     *dp;
    char            *fileName;
    int             rc;

    dp = 0;
    dir = opendir((char*) path);
    if (dir == 0) {
        return 0;
    }
    list = mprCreateList(ctx);

    while ((dirent = readdir(dir)) != 0) {
        if (dirent->d_name[0] == '.' && (dirent->d_name[1] == '\0' || dirent->d_name[1] == '.')) {
            continue;
        }
        fileName = mprJoinPath(ctx, path, dirent->d_name);
        rc = mprGetPathInfo(ctx, fileName, &fileInfo);
        mprFree(fileName);
        if (enumDirs || (rc == 0 && !fileInfo.isDir)) { 
            dp = mprAllocObjZeroed(list, MprDirEntry);
            if (dp == 0) {
                return 0;
            }
            dp->name = mprStrdup(dp, dirent->d_name);
            if (dp->name == 0) {
                return 0;
            }
            if (rc == 0) {
                dp->lastModified = fileInfo.mtime;
                dp->size = fileInfo.size;
                dp->isDir = fileInfo.isDir;
                dp->isLink = fileInfo.isLink;

            } else {
                dp->lastModified = 0;
                dp->size = 0;
                dp->isDir = 0;
                dp->isLink = 0;
            }
            mprAddItem(list, dp);
        }
    }
    closedir(dir);
    return list;
}
#endif


char *mprGetPathLink(MprCtx ctx, cchar *path)
{
    MprFileSystem  *fs;

    fs = mprLookupFileSystem(ctx, path);
    return fs->getPathLink(fs, path);
}


/*
    Return the extension portion of a pathname. Caller must not free the result.
 */
cchar *mprGetPathExtension(MprCtx ctx, cchar *path)
{
    MprFileSystem  *fs;
    char            *cp;

    if ((cp = strrchr(path, '.')) != NULL) {
        fs = mprLookupFileSystem(ctx, path);
        if (firstSep(fs, cp) == 0) {
            return ++cp;
        }
    } 
    return 0;
}


int mprGetPathInfo(MprCtx ctx, cchar *path, MprPath *info)
{
    MprFileSystem  *fs;

    fs = mprLookupFileSystem(ctx, path);
    return fs->getPathInfo(fs, path, info);
}


char *mprGetPathParent(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    char            *dir, *parent;

    fs = mprLookupFileSystem(ctx, path);

    if (path == 0 || path[0] == '\0') {
        return mprGetAbsPath(ctx, ".");
    }
    if (firstSep(fs, path) == NULL) {
        /*
         *  No parents in the path, so convert to absolute
         */
        dir = mprGetAbsPath(ctx, path);
        parent = mprGetPathDir(ctx, dir);
        mprFree(dir);
        return parent;
    }
    return mprGetPathDir(ctx, path);
}


char *mprGetPortablePath(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    char            *result, *cp;

    fs = mprLookupFileSystem(ctx, path);
    result = mprGetTransformedPath(ctx, path, 0);
    for (cp = result; *cp; cp++) {
        if (*cp == '\\') {
            *cp = '/';
        }
    }
    return result;
}


/*
    This returns a path relative to the current working directory for the given path
 */
char *mprGetRelPath(MprCtx ctx, cchar *pathArg)
{
    MprFileSystem   *fs;
    char            home[MPR_MAX_FNAME], *hp, *cp, *result, *tmp, *path, *mark;
    int             homeSegments, len, i, commonSegments, sep;

    fs = mprLookupFileSystem(ctx, pathArg);
    
    if (pathArg == 0 || *pathArg == '\0') {
        return mprStrdup(ctx, ".");
    }

    /*
     *  Must clean to ensure a minimal relative path result.
     */
    path = tmp = mprGetNormalizedPath(ctx, pathArg);

    if (!isAbsPath(fs, path)) {
        return path;
    }
    sep = (cp = firstSep(fs, path)) ? *cp : defaultSep(fs);
    
#if BLD_WIN_LIKE && !WINCE
{
    char    apath[MPR_MAX_FNAME];
    GetFullPathName(path, sizeof(apath) - 1, apath, NULL);
    apath[sizeof(apath) - 1] = '\0';
    path = apath;
    mprMapSeparators(fs, path, sep);
}
#endif
    /*
     *  Get the working directory. Ensure it is null terminated and leave room to append a trailing separators
     */
    if (getcwd(home, sizeof(home)) == 0) {
        strcpy(home, ".");
    }
    home[sizeof(home) - 2] = '\0';
    len = (int) strlen(home);

    /*
     *  Count segments in home working directory. Ignore trailing separators.
     */
    for (homeSegments = 0, cp = home; *cp; cp++) {
        if (isSep(fs, *cp) && cp[1]) {
            homeSegments++;
        }
    }

    /*
     *  Find portion of path that matches the home directory, if any. Start at -1 because matching root doesn't count.
     */
    commonSegments = -1;
    for (hp = home, mark = cp = path; *hp && *cp; hp++, cp++) {
        if (isSep(fs, *hp)) {
            if (isSep(fs, *cp)) {
                commonSegments++;
                mark = cp + 1;
            }
        } else if (fs->caseSensitive) {
            if (tolower((int) *hp) != tolower((int) *cp)) {
                break;
            }
        } else {
            if (*hp != *cp) {
                break;
            }
        }
    }
    mprAssert(commonSegments >= 0);

    /*
     *  Add one more segment if the last segment matches. Handle trailing separators
     */
    if ((isSep(fs, *hp) || *hp == '\0') && (isSep(fs, *cp) || *cp == '\0')) {
        commonSegments++;
        mark = cp;
    }
    
    if (isSep(fs, *cp)) {
        cp++;
    }
    
    hp = result = mprAlloc(ctx, homeSegments * 3 + (int) strlen(path) + 2);
    for (i = commonSegments; i < homeSegments; i++) {
        *hp++ = '.';
        *hp++ = '.';
        *hp++ = defaultSep(fs);
    }
    if (*cp) {
        strcpy(hp, cp);
    } else if (hp > result) {
        /*
         *  Cleanup trailing separators ("../" is the end of the new path)
         */
        hp[-1] = '\0';
    } else {
        strcpy(result, ".");
    }
    mprMapSeparators(fs, result, sep);
    mprFree(tmp);
    return result;
}


bool mprIsAbsPath(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return isAbsPath(fs, path);
}


bool mprIsRelPath(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return !isAbsPath(fs, path);
}


/*
    Join paths. Returns a joined (normalized) path.
    If other is absolute, then return other. If other is null, empty or "." then return path.
 */
char *mprJoinPath(MprCtx ctx, cchar *path, cchar *other)
{
    MprFileSystem   *fs;
    char            *join, *result, *drive, *cp;
    int             sep;

    fs = mprLookupFileSystem(ctx, path);
    if (other == NULL || *other == '\0' || strcmp(other, ".") == 0) {
        return mprStrdup(ctx, path);
    }
    if (isAbsPath(fs, other)) {
        if (fs->hasDriveSpecs && !isFullPath(fs, other) && isFullPath(fs, path)) {
            /*
             *  Other is absolute, but without a drive. Use the drive from path.
             */
            drive = mprStrdup(ctx, path);
            if ((cp = strchr(drive, ':')) != 0) {
                *++cp = '\0';
            }
            result = mprStrcat(ctx, -1, drive, other, NULL);
            mprFree(drive);
            return result;
        } else {
            return mprGetNormalizedPath(ctx, other);
        }
    }
    if (path == NULL || *path == '\0') {
        return mprGetNormalizedPath(ctx, other);
    }
    if ((cp = firstSep(fs, path)) != 0) {
        sep = *cp;
    } else if ((cp = firstSep(fs, other)) != 0) {
        sep = *cp;
    } else {
        sep = defaultSep(fs);
    }
    if ((join = mprAsprintf(ctx, -1, "%s%c%s", path, sep, other)) == 0) {
        return 0;
    }
    result = mprGetNormalizedPath(ctx, join);
    mprFree(join);
    return result;
}


/*
    Join an extension to a path. If path already has an extension, this call does nothing.
 */
char *mprJoinPathExt(MprCtx ctx, cchar *path, cchar *ext)
{
    MprFileSystem   *fs;
    char            *cp;

    fs = mprLookupFileSystem(ctx, path);
    if (ext == NULL || *ext == '\0') {
        return mprStrdup(ctx, path);
    }
    cp = strrchr(path, '.');
    if (cp && firstSep(fs, cp) == 0) {
        return mprStrdup(ctx, path);
    }
    return mprStrcat(ctx, -1, path, ext, NULL);
}


/*
    Make a directory with all necessary intervening directories.
 */
int mprMakeDir(MprCtx ctx, cchar *path, int perms, bool makeMissing)
{
    MprFileSystem   *fs;
    char            *parent;
    int             rc;

    fs = mprLookupFileSystem(ctx, path);

    if (mprPathExists(ctx, path, X_OK)) {
        return 0;
    }
    if (fs->makeDir(fs, path, perms) == 0) {
        return 0;
    }
    if (makeMissing && !isRoot(fs, path)) {
        parent = mprGetPathParent(ctx, path);
        rc = mprMakeDir(ctx, parent, perms, makeMissing);
        mprFree(parent);
        return fs->makeDir(fs, path, perms);
    }
    return MPR_ERR_CANT_CREATE;
}


int mprMakeLink(MprCtx ctx, cchar *path, cchar *target, bool hard)
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    if (mprPathExists(ctx, path, X_OK)) {
        return 0;
    }
    return fs->makeLink(fs, path, target, hard);
}


char *mprGetTempPath(MprCtx ctx, cchar *tempDir)
{
    MprFileSystem   *fs;
    MprFile         *file;
    char            *dir, *path;
    int             i, now;
    static int      tempSeed = 0;

    fs = mprLookupFileSystem(ctx, tempDir ? tempDir : (cchar*) "/");

    if (tempDir == 0) {
#if WINCE
        dir = mprStrdup(ctx, "/Temp");
#elif BLD_WIN_LIKE
        dir = mprStrdup(ctx, getenv("TEMP"));
        mprMapSeparators(ctx, dir, defaultSep(fs));
#elif VXWORKS
        dir = mprStrdup(ctx, ".");
#else
        dir = mprStrdup(ctx, "/tmp");
#endif
    } else {
        dir = mprStrdup(ctx, tempDir);
    }

    now = ((int) mprGetTime(ctx) & 0xFFFF) % 64000;

    file = 0;
    path = 0;

    for (i = 0; i < 128; i++) {
        mprFree(path);
        path = mprAsprintf(ctx, -1, "%s/MPR_%d_%d_%d.tmp", dir, getpid(), now, ++tempSeed);
        file = mprOpen(ctx, path, O_CREAT | O_EXCL | O_BINARY, 0664);
        if (file) {
            mprFree(file);
            break;
        }
    }
    mprFree(dir);

    if (file == 0) {
        mprFree(path);
        return 0;
    }

    return path;
}


#if BLD_WIN_LIKE && FUTURE
/*
    Normalize to a cygwin path without a drive spec
 */
static char *toCygPath(MprCtx ctx, cchar *path)
{
    Mpr     *mpr;
    char    *absPath, *result;
    int     len;

    mpr = mprGetMpr(ctx);

    absPath = NULL;
    if (!isFullPath(mpr, path)) {
        absPath = mprGetAbsPath(path);
        path = (cchar*) absPath;
    }
    mprAssert(isFullPath(mpr, path);
        
    if (fs->cygdrive) {
        len = (int) strlen(fs->cygdrive);
        if (mprStrcmpAnyCaseCount(fs->cygdrive, &path[2], len) == 0 && isSep(mpr, path[len+2])) {
            /*
             *  If path is like: "c:/cygdrive/c/..."
             *  Just strip the "c:" portion. Still validly qualified.
             */
            result = mprStrdup(ctx, &path[len + 2]);

        } else {
            /*
             *  Path is like: "c:/some/other/path". Prepend "/cygdrive/c/"
             */
            result = mprAsprintf(ctx, -1, "%s/%c%s", fs->cygdrive, path[0], &path[2]);
            len = strlen(result);
            if (isSep(mpr, result[len-1])) {
                result[len-1] = '\0';
            }
        }
    } else {
        /*
         *  Best we can do is get a relative path
         */
        result = mprGetRelPath(ctx, pathArg);
    }
    mprFree(absPath);
    return result;
}


/*
    Convert from a cygwin path
 */
static char *fromCygPath(MprCtx ctx, cchar *path)
{
    Mpr     *mpr;
    char    *buf, *result;
    int     len;

    mpr = mprGetMpr(ctx);

    if (isFullPath(mpr, path)) {
        return mprStrdup(ctx, path);
    }
    if (fs->cygdrive) {
        len = (int) strlen(fs->cygdrive);
        if (mprComparePath(mpr, fs->cygdrive, path, len) == 0 && isSep(mpr, path[len]) && 
                isalpha(path[len+1]) && isSep(mpr, path[len+2])) {
            /*
             *  Has a "/cygdrive/c/" style prefix
             */
            buf = mprAsprintf(ctx, -1, "%c:", path[len+1], &path[len + 2]);

        } else {
            /*
             *  Cygwin path. Prepend "c:/cygdrive"
             */
            buf = mprAsprintf(ctx, -1, "%s/%s", fs->cygdrive, path);
        }
        result = mprGetAbsPath(ctx, buf);
        mprFree(buf);

    } else {
        result = mprGetAbsPath(ctx, path);
    }
    mprMapSeparators(mpr, result, defaultSep(fs));
    return result;
}
#endif


/*
    Normalize a path to remove redundant "./" and cleanup "../" and make separators uniform. Does not make an abs path.
    It does not map separators or make lower case.
 */
char *mprGetNormalizedPath(MprCtx ctx, cchar *pathArg)
{
    MprFileSystem   *fs;
    char            *dupPath, *path, *sp, *dp, *mark, **segments;
    int             addSep, i, segmentCount, hasDot, len, last, sep;

    if (pathArg == 0 || *pathArg == '\0') {
        return mprStrdup(ctx, "");
    }

    fs = mprLookupFileSystem(ctx, pathArg);

    /*
     *  Allocate one spare byte incase we need to break into segments. If so, will add a trailing
     *  "/" to make parsing easier later.
     */
    len = strlen(pathArg);
    if ((path = mprAlloc(ctx, len + 2)) == 0) {
        return NULL;
    }
    dupPath = path;
    strcpy(path, pathArg);
    sep = (sp = firstSep(fs, path)) ? *sp : defaultSep(fs);

    /*
     *  Remove multiple path separators and map to the default separator.
     *  Check if we have any "." characters and count the number of path segments
     */
    hasDot = segmentCount = 0;
    for (sp = dp = mark = path; *sp; ) {
        if (isSep(fs, *sp)) {
            *sp = sep;
            segmentCount++;
            while (isSep(fs, sp[1])) {
                sp++;
            }
            mark = sp + 1;
        } 
        if (*sp == '.') {
            hasDot++;
        }
        *dp++ = *sp++;
    }
    *dp = '\0';
    if (!sep) {
        sep = defaultSep(fs);
    }

    if (!hasDot && segmentCount == 0) {
        if (fs->hasDriveSpecs) {
            last = path[strlen(path) - 1];
            if (last == ':') {
                path = mprStrcat(ctx, -1, path, ".", NULL);
                mprFree(dupPath);
            }
        }
        return path;
    }

    if (dp > path && !isSep(fs, dp[-1])) {
        *dp++ = sep;
        *dp = '\0';
        segmentCount++;
    }

    /*
     *  Have dots to process so break into path segments. Add one incase we need have an absolute path with a drive-spec.
     */
    mprAssert(segmentCount > 0);
    if ((segments = mprAlloc(ctx, sizeof(char*) * (segmentCount + 1))) == 0) {
        mprFree(dupPath);
        return NULL;
    }

    /*
     *  NOTE: The root "/" for absolute paths will be stored as empty.
     */
    i = len = 0;
    for (mark = sp = path; *sp; sp++) {
        if (isSep(fs, *sp)) {
            *sp = '\0';
            if (*mark == '.' && mark[1] == '\0' && segmentCount > 1) {
                /* Remove "."  However, preserve lone "." */
                mark = sp + 1;
                segmentCount--;
                continue;
            }
            if (*mark == '.' && mark[1] == '.' && mark[2] == '\0' && i > 0 && strcmp(segments[i-1], "..") != 0) {
                /* Erase ".." and previous segment */
                if (*segments[i - 1] == '\0' ) {
                    mprAssert(i == 1);
                    /* Previous segement is "/". Prevent escape from root */
                    segmentCount--;
                } else {
                    i--;
                    segmentCount -= 2;
                }
                mprAssert(segmentCount >= 0);
                mark = sp + 1;
                continue;
            }
            segments[i++] = mark;
            len += sp - mark;
#if KEEP
            if (i == 1 && segmentCount == 1 && fs->hasDriveSpecs && strchr(mark, ':') != 0) {
                /*
                 *  Normally we truncate a trailing "/", but this is an absolute path with a drive spec (c:/). 
                 */
                segments[i++] = "";
                segmentCount++;
            }
#endif
            mark = sp + 1;
        }
    }

    if (--sp > mark) {
        segments[i++] = mark;
        len += sp - mark;
    }
    mprAssert(i <= segmentCount);
    segmentCount = i;

    if (segmentCount <= 0) {
        mprFree(dupPath);
        mprFree(segments);
        return mprStrdup(ctx, ".");
    }

    addSep = 0;
    sp = segments[0];
    if (fs->hasDriveSpecs && *sp != '\0') {
        last = sp[strlen(sp) - 1];
        if (last == ':') {
            /* This matches an original path of: "c:/" but not "c:filename" */
            addSep++;
        }
    }
#if BLD_WIN_LIKE
    if (strcmp(segments[segmentCount - 1], " ") == 0) {
        segmentCount--;
    }
#endif

    if ((path = mprAlloc(ctx, len + segmentCount + 1)) == 0) {
        mprFree(segments);
        mprFree(dupPath);
        return NULL;
    }

    mprAssert(segmentCount > 0);

    /*
     *  First segment requires special treatment due to drive specs
     */
    dp = path;
    strcpy(dp, segments[0]);
    dp += strlen(segments[0]);

    if (segmentCount == 1 && (addSep || (*segments[0] == '\0'))) {
        *dp++ = sep;
    }
    for (i = 1; i < segmentCount; i++) {
        *dp++ = sep;
        strcpy(dp, segments[i]);
        dp += strlen(segments[i]);
    }
    *dp = '\0';
    mprFree(dupPath);
    mprFree(segments);
    return path;
}


int mprGetPathSeparator(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return fs->separators[0];
}


bool mprIsPathSeparator(MprCtx ctx, cchar *path, cchar c)
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return isSep(fs, c);
}


/*
    Return a pointer into the path at the last path separator or null if none found
 */
cchar *mprGetLastPathSeparator(MprCtx ctx, cchar *path) 
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return lastSep(fs, path);
}


cchar *mprGetFirstPathSeparator(MprCtx ctx, cchar *path) 
{
    MprFileSystem   *fs;

    fs = mprLookupFileSystem(ctx, path);
    return firstSep(fs, path);
}


void mprMapSeparators(MprCtx ctx, char *path, int separator)
{
    MprFileSystem   *fs;
    char            *cp;

    fs = mprLookupFileSystem(ctx, path);
    for (cp = path; *cp; cp++) {
        if (isSep(fs, *cp)) {
            *cp = separator;
        }
    }
}


bool mprPathExists(MprCtx ctx, cchar *path, int omode)
{
    MprFileSystem  *fs;

    fs = mprLookupFileSystem(ctx, path);
    return fs->accessPath(fs, path, omode);
}


/*
    Resolve one path against another path. Returns a joined (normalized) path.
    If other is absolute, then return other. If other is null, empty or "." then return path.
 */
char *mprResolvePath(MprCtx ctx, cchar *path, cchar *other)
{
    MprFileSystem   *fs;
    char            *join, *result, *drive, *cp, *dir;

    fs = mprLookupFileSystem(ctx, path);
    if (other == NULL || *other == '\0' || strcmp(other, ".") == 0) {
        return mprStrdup(ctx, path);
    }
    if (isAbsPath(fs, other)) {
        if (fs->hasDriveSpecs && !isFullPath(fs, other) && isFullPath(fs, path)) {
            /*
                Other is absolute, but without a drive. Use the drive from path.
             */
            drive = mprStrdup(ctx, path);
            if ((cp = strchr(drive, ':')) != 0) {
                *++cp = '\0';
            }
            result = mprStrcat(ctx, -1, drive, other, NULL);
            mprFree(drive);
            return result;
        }
        return mprGetNormalizedPath(ctx, other);
    }
    if (path == NULL || *path == '\0') {
        return mprGetNormalizedPath(ctx, other);
    }
    dir = mprGetPathDir(ctx, path);
    if ((join = mprAsprintf(ctx, -1, "%s/%s", dir, other)) == 0) {
        mprFree(dir);
        return 0;
    }
    mprFree(dir);
    result = mprGetNormalizedPath(ctx, join);
    mprFree(join);
    return result;
}


/*
    Compare two file path to determine if they point to the same file.
 */
int mprSamePath(MprCtx ctx, cchar *path1, cchar *path2)
{
    MprFileSystem   *fs;
    cchar           *p1, *p2;
    int             rc;

    fs = mprLookupFileSystem(ctx, path1);

    /*
        Convert to absolute (normalized) paths to compare. 
     */
    if (!isFullPath(fs, path1)) {
        path1 = mprGetAbsPath(ctx, path1);
    } else {
        path1 = mprGetNormalizedPath(ctx, path1);
    }
    if (!isFullPath(fs, path2)) {
        path2 = mprGetAbsPath(ctx, path2);
    } else {
        path2 = mprGetNormalizedPath(ctx, path2);
    }
    if (fs->caseSensitive) {
        for (p1 = path1, p2 = path2; *p1 && *p2; p1++, p2++) {
            if (*p1 != *p2 && !(isSep(fs, *p1) && isSep(fs, *p2))) {
                break;
            }
        }
    } else {
        for (p1 = path1, p2 = path2; *p1 && *p2; p1++, p2++) {
            if (tolower((int) *p1) != tolower((int) *p2) && !(isSep(fs, *p1) && isSep(fs, *p2))) {
                break;
            }
        }
    }
    rc = (*p1 == *p2);
    mprFree((char*) path1);
    mprFree((char*) path2);
    return rc;
}


/*
    Compare two file path to determine if they point to the same file.
 */
int mprSamePathCount(MprCtx ctx, cchar *path1, cchar *path2, int len)
{
    MprFileSystem   *fs;
    char            *tmpPath1, *tmpPath2;
    cchar           *p1, *p2;

    fs = mprLookupFileSystem(ctx, path1);
    tmpPath1 = tmpPath2 = 0;

    /*
        Convert to absolute paths to compare. 
     */
    if (!isFullPath(fs, path1)) {
        tmpPath1 = mprGetAbsPath(ctx, path1);
        path1 = tmpPath1;
    }
    if (!isFullPath(fs, path2)) {
        tmpPath2 = mprGetAbsPath(ctx, path2);
        path2 = tmpPath2;
    }
    if (fs->caseSensitive) {
        for (p1 = path1, p2 = path2; *p1 && *p2 && len > 0; p1++, p2++, len--) {
            if (*p1 != *p2 && !(isSep(fs, *p1) && isSep(fs, *p2))) {
                break;
            }
        }
    } else {
        for (p1 = path1, p2 = path2; *p1 && *p2 && len > 0; p1++, p2++, len--) {
            if (tolower((int) *p1) != tolower((int) *p2) && !(isSep(fs, *p1) && isSep(fs, *p2))) {
                break;
            }
        }
    }
    mprFree(tmpPath1);
    mprFree(tmpPath2);
    return len == 0;
}




char *mprSearchPath(MprCtx ctx, cchar *file, int flags, cchar *search, ...)
{
    va_list     args;
    char        *path, *tok, *dir, *result, *nextDir;
    int         access;

    va_start(args, search);
    access = (flags & MPR_SEARCH_EXE) ? X_OK : R_OK;

    for (nextDir = (char*) search; nextDir; nextDir = va_arg(args, char*)) {

        if (strchr(nextDir, MPR_SEARCH_SEP_CHAR)) {
            tok = NULL;
            nextDir = mprStrdup(ctx, nextDir);
            dir = mprStrTok(nextDir, MPR_SEARCH_SEP, &tok);
            while (dir && *dir) {
                mprLog(ctx, 5, "mprSearchForFile: %s in directory %s", file, nextDir);
                path = mprJoinPath(ctx, dir, file);
                if (mprPathExists(ctx, path, R_OK)) {
                    mprLog(ctx, 5, "mprSearchForFile: found %s", path);
                    result = mprGetNormalizedPath(ctx, path);
                    mprFree(path);
                    mprFree(nextDir);
                    return result;
                }
                mprFree(path);
                dir = mprStrTok(0, MPR_SEARCH_SEP, &tok);
            }
            mprFree(nextDir);

        } else {
            mprLog(ctx, 5, "mprSearchForFile: %s in directory %s", file, nextDir);
            path = mprJoinPath(ctx, nextDir, file);
            if (mprPathExists(ctx, path, R_OK)) {
                mprLog(ctx, 5, "mprSearchForFile: found %s", path);
                result = mprGetNormalizedPath(ctx, path);
                mprFree(path);
                return result;
            }
        }
    }
    va_end(args);
    return 0;
}


/*
    This normalizes a path. Returns a normalized path according to flags. Default is absolute. 
    This will map separators to the current representation unless MPR_PATH_NATIVE_SEP is specified in the flags.
 */
char *mprGetTransformedPath(MprCtx ctx, cchar *path, int flags)
{
    MprFileSystem       *fs;
    char                *result;

    fs = mprLookupFileSystem(ctx, path);

#if BLD_WIN_LIKE && FUTURE
    if (flags & MPR_PATH_CYGWIN) {
        result = toCygPath(ctx, path, flags);
    } else {
        /*
         *  Issues here. "/" is ambiguous. Is this "c:/" or is it "c:/cygdrive/c" which may map to c:/cygwin/...
         */
        result = fromCygPath(ctx, path);
    }
#endif

    if (flags & MPR_PATH_ABS) {
        result = mprGetAbsPath(ctx, path);

    } else if (flags & MPR_PATH_REL) {
        result = mprGetRelPath(ctx, path);

    } else {
        result = mprGetNormalizedPath(ctx, path);
    }

#if BLD_WIN_LIKE
    if (flags & MPR_PATH_NATIVE_SEP) {
        mprMapSeparators(ctx, result, '\\');
    }
#endif
    return result;
}


/*
    Return the extension portion of a pathname. Caller must not free the result.
 */
char *mprTrimPathExtension(MprCtx ctx, cchar *path)
{
    MprFileSystem   *fs;
    char            *cp, *ext;

    fs = mprLookupFileSystem(ctx, path);
    ext = mprStrdup(ctx, path);
    if ((cp = strrchr(ext, '.')) != NULL) {
        if (firstSep(fs, cp) == 0) {
            *cp = '\0';
        }
    } 
    return ext;
}


int mprTruncatePath(MprCtx ctx, cchar *path, int size)
{
    if (!mprPathExists(ctx, path, F_OK)) {
        return MPR_ERR_CANT_ACCESS;
    }
#if BLD_WIN_LIKE
{
    HANDLE  h;

    h = CreateFile(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    SetFilePointer(h, size, 0, FILE_BEGIN);
    if (h == INVALID_HANDLE_VALUE || SetEndOfFile(h) == 0) {
        CloseHandle(h);
        return MPR_ERR_CANT_WRITE;
    }
    CloseHandle(h);
}
#elif VXWORKS
{
#if FUTURE
    int     fd;

    fd = open(path, O_WRONLY, 0664);
    if (fd < 0 || ftruncate(fd, size) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
    close(fd);
#endif
    return MPR_ERR_CANT_WRITE;
}
#else
    if (truncate(path, size) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
#endif
    return 0;
}


char *mprGetAppPath(MprCtx ctx)
{ 
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    if (mpr->appPath) {
        return mprStrdup(ctx, mpr->appPath);
    }

#if MACOSX
{
    char    path[MPR_MAX_PATH], pbuf[MPR_MAX_PATH];
    uint    size;
    int     len;

    size = sizeof(path) - 1;
    if (_NSGetExecutablePath(path, &size) < 0) {
        return mprGetAbsPath(ctx, ".");
    }
    path[size] = '\0';
    len = readlink(path, pbuf, sizeof(pbuf) - 1);
    if (len < 0) {
        return mprGetAbsPath(ctx, path);
    }
    pbuf[len] = '\0';
    mpr->appPath = mprGetAbsPath(ctx, pbuf);
    return mprStrdup(ctx, mpr->appPath);
}

#elif FREEBSD 
{
    char    pbuf[MPR_MAX_STRING];
    int     len;

    len = readlink("/proc/curproc/file", pbuf, sizeof(pbuf) - 1);
    if (len < 0) {
        return mprGetAbsPath(ctx, ".");
     }
     pbuf[len] = '\0';
     mpr->appPath = mprGetAbsPath(ctx, pbuf);
     return mprStrdup(ctx, mpr->appPath);
}

#elif BLD_UNIX_LIKE 
{
    char    pbuf[MPR_MAX_STRING], *path;
    int     len;

#if SOLARIS
    path = mprAsprintf(ctx, -1, "/proc/%i/path/a.out", getpid()); 
#else
    path = mprAsprintf(ctx, -1, "/proc/%i/exe", getpid()); 
#endif
    len = readlink(path, pbuf, sizeof(pbuf) - 1);
    if (len < 0) {
        mprFree(path);
        return mprGetAbsPath(ctx, ".");
    }
    pbuf[len] = '\0';
    mprFree(path);
    mpr->appPath = mprGetAbsPath(ctx, pbuf); 
    return mprStrdup(ctx, mpr->appPath);
}
#elif BLD_WIN_LIKE
{
    char    path[MPR_MAX_PATH];

    if (GetModuleFileName(0, path, sizeof(path) - 1) <= 0) {
        return 0;
    }
    mpr->appPath = mprGetAbsPath(ctx, path);
    return mprStrdup(ctx, mpr->appPath);
}
#else
    mpr->appPath = mprGetCurrentPath(ctx);
    return mprStrdup(ctx, mpr->appPath);
#endif
}

 
char *mprGetAppDir(MprCtx ctx)
{ 
    Mpr     *mpr;
    char    *path;

    mpr = mprGetMpr(ctx);
    if (mpr->appDir == 0) {
        path = mprStrdup(ctx, mprGetAppPath(ctx));
        mpr->appDir = mprGetPathDir(mpr, path);
        mprFree(path);
    }
    return mprStrdup(ctx, mpr->appDir); 
} 

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprPath.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprPollWait.c"
 */
/************************************************************************/

/**
 *  mprPollWait.c - Wait for I/O by using poll on unix like systems.
 *
 *  This module augments the mprWait wait services module by providing poll() based waiting support.
 *  Also see mprAsyncSelectWait and mprSelectWait. This module is thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if LINUX || MACOSX || FREEBSD

static void getWaitFds(MprWaitService *ws);
static void growFds(MprWaitService *ws);
static void serviceIO(MprWaitService *ws, struct pollfd *stableFds, int count);


int mprInitSelectWait(MprWaitService *ws)
{
#if BLD_FEATURE_MULTITHREAD
    /*
     *  Initialize the "wakeup" pipe. This is used to wakeup the service thread if other threads need to wait for I/O.
     */
    if (pipe(ws->breakPipe) < 0) {
        mprError(ws, "Can't open breakout pipe");
        return MPR_ERR_CANT_INITIALIZE;
    }
    fcntl(ws->breakPipe[0], F_SETFL, fcntl(ws->breakPipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(ws->breakPipe[1], F_SETFL, fcntl(ws->breakPipe[1], F_GETFL) | O_NONBLOCK);
#endif
    return 0;
}


/*
 *  Wait for I/O on a single file descriptor. Return a mask of events found. Mask is the events of interest.
 *  timeout is in milliseconds.
 */
int mprWaitForSingleIO(MprCtx ctx, int fd, int mask, int timeout)
{
    struct pollfd   fds[1];

    fds[0].fd = fd;
    fds[0].events = 0;
    fds[0].revents = 0;

    if (mask & MPR_READABLE) {
        fds[0].events |= (POLLIN | POLLHUP);
    }
    if (mask & MPR_WRITABLE) {
        fds[0].events |= POLLOUT;
    }
    if (poll(fds, 1, timeout) > 0) {
        mask = 0;
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            mask |= MPR_READABLE;
        }
        if (fds[0].revents & POLLOUT) {
            mask |= MPR_WRITABLE;
        }
        return mask;
    }
    return 0;
}


static void serviceRecall(MprWaitService *ws)
{
    MprWaitHandler      *wp;
    int                 index;

    mprLock(ws->mutex);
    ws->flags &= ~MPR_NEED_RECALL;
    for (index = 0; (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &index)) != 0; ) {
        if (wp->flags & MPR_WAIT_RECALL_HANDLER) {
            if ((wp->desiredMask & wp->disableMask) && wp->inUse == 0) {
                wp->presentMask |= MPR_READABLE;
                wp->flags &= ~MPR_WAIT_RECALL_HANDLER;
#if BLD_FEATURE_MULTITHREAD
                mprAssert(wp->disableMask == -1);
                ws->maskGeneration++;
                wp->disableMask = 0;
                mprAssert(wp->inUse == 0);
                wp->inUse++;
#endif
                mprUnlock(ws->mutex);
                mprInvokeWaitCallback(wp);
                mprLock(ws->mutex);

            } else {
                ws->flags |= MPR_NEED_RECALL;
            }
        }
    }
    mprUnlock(ws->mutex);
}


/*
 *  Wait for I/O on all registered file descriptors. Timeout is in milliseconds. Return the number of events detected.
 */
int mprWaitForIO(MprWaitService *ws, int timeout)
{
    struct pollfd   *fds;
    int             rc, count;

    /*
     *  No locking. If the masks are updated after this test, the breakout pipe will wake us up soon.
     */
    mprLock(ws->mutex);
    if (ws->lastMaskGeneration != ws->maskGeneration) {
        getWaitFds(ws);
    }
    if (ws->flags & MPR_NEED_RECALL) {
        mprUnlock(ws->mutex);
        serviceRecall(ws);
        return 1;
    } else
#if BLD_DEBUG
    if (mprGetDebugMode(ws) && timeout > 30000) {
        timeout = 30000;
    }
#endif
    count = ws->fdsCount;
    if ((fds = mprMemdup(ws, ws->fds, count * sizeof(struct pollfd))) == 0) {
        mprUnlock(ws->mutex);
        return MPR_ERR_NO_MEMORY;
    }
    mprUnlock(ws->mutex);

    rc = poll(fds, count, timeout);
    if (rc < 0) {
        mprLog(ws, 8, "Poll returned %d, errno %d", rc, mprGetOsError());
    } else if (rc > 0) {
        serviceIO(ws, fds, count);
    }
    mprFree(fds);
    return rc;
}


/*
 *  Get the waiting file descriptors
 */
static void getWaitFds(MprWaitService *ws)
{
    MprWaitHandler  *wp, *nextWp;
    struct pollfd   *pollfd;
    int             mask, next;

    mprLock(ws->mutex);

    ws->lastMaskGeneration = ws->maskGeneration;
    growFds(ws);
    pollfd = ws->fds;

#if BLD_FEATURE_MULTITHREAD
    /*
     *  Add the breakout port to wakeup the service thread when other threads need selecting services.
     */
    pollfd->fd = ws->breakPipe[MPR_READ_PIPE];
    pollfd->events = POLLIN | POLLHUP;
    pollfd++;
#endif

    /*
     *  Add an entry for each descriptor desiring service.
     */
    next = 0;
    for (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &next); wp; wp = nextWp) {
        nextWp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &next);

        if (wp->fd >= 0 && wp->proc && wp->desiredMask) {
            /*
             *  The disable mask will be zero when we are already servicing an event. This prevents recursive service.
             */
            mask = wp->desiredMask & wp->disableMask;
            if (mask) {
#if BLD_FEATURE_MULTITHREAD
                if (wp->inUse) continue;
#endif
                pollfd->events = 0;
                if (mask & MPR_READABLE) {
                    pollfd->events |= POLLIN | POLLHUP;
                }
                if (mask & MPR_WRITABLE) {
                    pollfd->events |= POLLOUT;
                }
                if (pollfd->events) {
                    pollfd->fd = wp->fd;
                    pollfd++;
                }
            }
        }
    }
    ws->fdsCount = (int) (pollfd - ws->fds);
    mprAssert(ws->fdsCount <= ws->fdsSize);
    mprUnlock(ws->mutex);
}


/*
 *  Service I/O events
 */
static void serviceIO(MprWaitService *ws, struct pollfd *fds, int count)
{
    MprWaitHandler      *wp;
    struct pollfd       *fp;
    int                 i, mask, index, start;

    /*
     *  Must have the wait list stable while we service events
     */
    mprLock(ws->mutex);
    start = 0;
    
#if BLD_FEATURE_MULTITHREAD
    mprAssert(mprGetCurrentOsThread() == mprGetMpr(ws)->serviceThread);

    /*
     *  Service the breakout pipe first
     */
    if (fds[0].revents & (POLLIN | POLLHUP)) {
        char    buf[128];
        if (read(ws->breakPipe[MPR_READ_PIPE], buf, sizeof(buf)) < 0) {
            /* Ignore */
        }
        ws->flags &= ~MPR_BREAK_REQUESTED;
    }
    start++;
#endif

    /*
     *  Now service all IO wait handlers. Processing must be aborted if an active fd is removed.
     */
    for (i = start; i < count; ) {
        fp = &fds[i++];
        if (fp->revents == 0) {
            continue;
        }

        /*
         *  Go in reverse order to maximize the chance of getting the most active connection
         */
        for (index = -1; (wp = (MprWaitHandler*) mprGetPrevItem(ws->handlers, &index)) != 0; ) {
            mprAssert(wp->fd >= 0);
            if (wp->fd != fp->fd) {
                continue;
            }
            /*
             *  Present mask is only cleared after the io handler callback has completed
             */
            mask = 0;
            if ((wp->desiredMask & MPR_READABLE) && fp->revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
                mask |= MPR_READABLE;
                fp->revents &= ~(POLLIN | POLLHUP | POLLERR | POLLNVAL);
            }
            if ((wp->desiredMask & MPR_WRITABLE) && fp->revents & POLLOUT) {
                mask |= MPR_WRITABLE;
                fp->revents &= ~POLLOUT;
            }
            if (wp->flags & MPR_WAIT_RECALL_HANDLER) {
                if (wp->desiredMask & wp->disableMask) {
                    mask |= MPR_READABLE;
                    wp->flags &= ~MPR_WAIT_RECALL_HANDLER;
                } else {
                    mprAssert(wp->desiredMask & wp->disableMask);
                }
            }
            if (mask == 0) {
                break;
            }
            if (mask & wp->desiredMask) {
                wp->presentMask = mask;
#if BLD_FEATURE_MULTITHREAD
                /*
                 *  Disable events to prevent recursive I/O events. Callback must call mprEnableWaitEvents
                 */
                ws->maskGeneration++;
                if (wp->disableMask == 0) {
                    /* Should not ever get here. Just for safety. */
                    break;
                }
                wp->disableMask = 0;
                mprAssert(wp->inUse == 0);
                wp->inUse++;
#endif
                mprUnlock(ws->mutex);
                mprInvokeWaitCallback(wp);
                mprLock(ws->mutex);
            }
            break;
        }
        fp->revents = 0;
    }
    mprUnlock(ws->mutex);
}


#if BLD_FEATURE_MULTITHREAD
void mprWakeOsWaitService(MprCtx ctx)
{
    MprWaitService  *ws;
    int             c;

    ws = mprGetMpr(ctx)->waitService;
    mprLock(ws->mutex);
    if (!(ws->flags & MPR_BREAK_REQUESTED)) {
        ws->flags |= MPR_BREAK_REQUESTED;
        c = 0;
        if (write(ws->breakPipe[MPR_WRITE_PIPE], (char*) &c, 1) < 0) {
            mprError(ctx, "Can't write to break pipe");
        }
    }
    mprUnlock(ws->mutex);
}
#endif


/*
 *  Grow the fds list as required. Never shrink.
 */
static void growFds(MprWaitService *ws)
{
    int     len;

    len = max(ws->fdsSize, mprGetListCount(ws->handlers) + 1);
    if (len > ws->fdsSize) {
        ws->fds = mprRealloc(ws, ws->fds, len * sizeof(struct pollfd));
        if (ws->fds == 0) {
            /*  Global memory allocation handler will handle this */
            return;
        }
        memset(&ws->fds[ws->fdsSize], 0, (len - ws->fdsSize) * sizeof(struct pollfd));
        ws->fdsSize = len;
    }
}


#else
void __mprDummyPollWait() {}
#endif /* BLD_UNIX_LIKE */

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprPollWait.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprPrintf.c"
 */
/************************************************************************/

/**
    mprPrintf.c - Printf routines safe for embedded programming
  
    This module provides safe replacements for the standard printf formatting routines. Most routines in this file 
    are not thread-safe. It is the callers responsibility to perform all thread synchronization.
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,]          Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlL]            Length bits */
#define CLASS_TYPE      8               /* [cdefginopsSuxX] Type specifiers */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,*/
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

/*
    Format:         %[modifier][width][precision][bits][type]
  
    [-+ #,]         Modifiers
    [hlL]           Length bits
 */


/*
    Flags
 */
#define SPRINTF_LEFT        0x1         /* Left align */
#define SPRINTF_SIGN        0x2         /* Always sign the result */
#define SPRINTF_LEAD_SPACE  0x4         /* put leading space for +ve numbers */
#define SPRINTF_ALTERNATE   0x8         /* Alternate format */
#define SPRINTF_LEAD_ZERO   0x10        /* Zero pad */
#define SPRINTF_SHORT       0x20        /* 16-bit */
#define SPRINTF_LONG        0x40        /* 32-bit */
#define SPRINTF_INT64       0x80        /* 64-bit */
#define SPRINTF_COMMA       0x100       /* Thousand comma separators */
#define SPRINTF_UPPER_CASE  0x200       /* As the name says for numbers */

typedef struct Format {
    uchar   *buf;
    uchar   *endbuf;
    uchar   *start;
    uchar   *end;
    int     growBy;
    int     maxsize;

    int     precision;
    int     radix;
    int     width;
    int     flags;
    int     len;
} Format;

#define BPUT(ctx, fmt, c) \
    if (1) { \
        /* Less one to allow room for the null */ \
        if ((fmt)->end >= ((fmt)->endbuf - sizeof(char))) { \
            if (growBuf(ctx, fmt) > 0) { \
                *(fmt)->end++ = (c); \
            } \
        } else { \
            *(fmt)->end++ = (c); \
        } \
    } else

#define BPUTNULL(ctx, fmt) \
    if (1) { \
        if ((fmt)->end > (fmt)->endbuf) { \
            if (growBuf(ctx, fmt) > 0) { \
                *(fmt)->end = '\0'; \
            } \
        } else { \
            *(fmt)->end = '\0'; \
        } \
    } else 


static int  getState(char c, int state);
static int  growBuf(MprCtx ctx, Format *fmt);
static char *sprintfCore(MprCtx ctx, char *buf, int maxsize, cchar *fmt, va_list arg);
static void outNum(MprCtx ctx, Format *fmt, cchar *prefix, uint64 val);

#if BLD_FEATURE_FLOATING_POINT
static void outFloat(MprCtx ctx, Format *fmt, char specChar, double value);
#endif


int mprPrintf(MprCtx ctx, cchar *fmt, ...)
{
    MprFileSystem   *fs;
    va_list         ap;
    char            *buf;
    int             len;

    /* No asserts here as this is used as part of assert reporting */

    fs = mprLookupFileSystem(ctx, "/");

    va_start(ap, fmt);
    buf = mprVasprintf(ctx, -1, fmt, ap);
    va_end(ap);
    if (buf != 0 && fs->stdOutput) {
        len = mprWriteString(fs->stdOutput, buf);
    } else {
        len = -1;
    }
    mprFree(buf);
    return len;
}


int mprPrintfError(MprCtx ctx, cchar *fmt, ...)
{
    MprFileSystem   *fs;
    va_list         ap;
    char            *buf;
    int             len;

    /* No asserts here as this is used as part of assert reporting */

    fs = mprLookupFileSystem(ctx, "/");

    va_start(ap, fmt);
    buf = mprVasprintf(ctx, -1, fmt, ap);
    va_end(ap);
    if (buf && fs->stdError) {
        len = mprWriteString(fs->stdError, buf);
    } else {
        len = -1;
    }
    mprFree(buf);
    return len;
}


int mprFprintf(MprFile *file, cchar *fmt, ...)
{
    va_list     ap;
    char        *buf;
    int         len;

    if (file == 0) {
        return MPR_ERR_BAD_HANDLE;
    }

    va_start(ap, fmt);
    buf = mprVasprintf(file, -1, fmt, ap);
    va_end(ap);

    if (buf) {
        len = mprWriteString(file, buf);
    } else {
        len = -1;
    }
    mprFree(buf);
    return len;
}


/*
    Printf with a static buffer. Used internally only. WILL NOT MALLOC.
 */
int mprStaticPrintf(MprCtx ctx, cchar *fmt, ...)
{
    MprFileSystem   *fs;
    va_list         ap;
    char            buf[MPR_MAX_STRING];

    fs = mprLookupFileSystem(ctx, "/");

    va_start(ap, fmt);
    sprintfCore(NULL, buf, MPR_MAX_STRING, fmt, ap);
    va_end(ap);
    return mprWrite(fs->stdOutput, buf, strlen(buf));
}


/*
    Printf with a static buffer. Used internally only. WILL NOT MALLOC.
 */
int mprStaticPrintfError(MprCtx ctx, cchar *fmt, ...)
{
    MprFileSystem   *fs;
    va_list         ap;
    char            buf[MPR_MAX_STRING];

    fs = mprLookupFileSystem(ctx, "/");

    va_start(ap, fmt);
    sprintfCore(NULL, buf, MPR_MAX_STRING, fmt, ap);
    va_end(ap);
    return mprWrite(fs->stdError, buf, strlen(buf));
}


char *mprSprintf(char *buf, int bufsize, cchar *fmt, ...)
{
    va_list     ap;
    char        *result;

    mprAssert(buf);
    mprAssert(fmt);
    mprAssert(bufsize > 0);

    va_start(ap, fmt);
    result = sprintfCore(NULL, buf, bufsize, fmt, ap);
    va_end(ap);
    return result;
}


char *mprVsprintf(char *buf, int bufsize, cchar *fmt, va_list arg)
{
    mprAssert(buf);
    mprAssert(fmt);
    mprAssert(bufsize > 0);

    return sprintfCore(NULL, buf, bufsize, fmt, arg);
}


char *mprAsprintf(MprCtx ctx, int maxSize, cchar *fmt, ...)
{
    va_list     ap;
    char        *buf;

    mprAssert(fmt);

    va_start(ap, fmt);
    buf = sprintfCore(ctx, NULL, maxSize, fmt, ap);
    va_end(ap);
    return buf;
}


char *mprVasprintf(MprCtx ctx, int maxSize, cchar *fmt, va_list arg)
{
    mprAssert(fmt);
    return sprintfCore(ctx, NULL, maxSize, fmt, arg);
}


static int getState(char c, int state)
{
    /*
     *  Declared here for Brew which can't handle globals.
     */
    char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */   0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */   1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */   0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */   0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */   0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */   0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */   0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */   0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */   0,     7,      7,      7,    7,    7,   7,   0,
    };

    /*
     *  Format:         %[modifier][width][precision][bits][type]
     *
     *  The Class map will map from a specifier letter to a state.
     */
    char classMap[] = {
        /*   0  ' '    !     "     #     $     %     &     ' */
                 2,    0,    0,    2,    0,    1,    0,    0,
        /*  07   (     )     *     +     ,     -     .     / */
                 0,    0,    4,    2,    2,    2,    6,    0,
        /*  10   0     1     2     3     4     5     6     7 */
                 3,    5,    5,    5,    5,    5,    5,    5,
        /*  17   8     9     :     ;     <     =     >     ? */
                 5,    5,    0,    0,    0,    0,    0,    0,
        /*  20   @     A     B     C     D     E     F     G */
                 0,    0,    0,    0,    0,    0,    0,    0,
        /*  27   H     I     J     K     L     M     N     O */
                 0,    0,    0,    0,    7,    0,    0,    0,
        /*  30   P     Q     R     S     T     U     V     W */
                 0,    0,    0,    8,    0,    0,    0,    0,
        /*  37   X     Y     Z     [     \     ]     ^     _ */
                 8,    0,    0,    0,    0,    0,    0,    0,
        /*  40   '     a     b     c     d     e     f     g */
                 0,    0,    0,    8,    8,    8,    8,    8,
        /*  47   h     i     j     k     l     m     n     o */
                 7,    8,    0,    0,    7,    0,    8,    8,
        /*  50   p     q     r     s     t     u     v     w */
                 8,    0,    0,    8,    0,    8,    0,    0,
        /*  57   x     y     z  */
                 8,    0,    0,
    };

    int     chrClass;

    if (c < ' ' || c > 'z') {
        chrClass = CLASS_NORMAL;
    } else {
        mprAssert((c - ' ') < (int) sizeof(classMap));
        chrClass = classMap[(c - ' ')];
    }
    mprAssert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}


static char *sprintfCore(MprCtx ctx, char *buf, int maxsize, cchar *spec, va_list arg)
{
    Format      fmt;
    char        *cp, *sValue, c, *tmpBuf;
    int64       iValue;
    uint64      uValue;
    int         i, len, state;

    if (spec == 0) {
        spec = "";
    }
    if (buf != 0) {
        mprAssert(maxsize > 0);
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[maxsize];
        fmt.growBy = -1;
    } else {
        if (maxsize <= 0) {
            maxsize = MAXINT;
        }
        len = min(MPR_DEFAULT_ALLOC, maxsize);
        buf = (char*) mprAlloc(ctx, len);
        if (buf == 0) {
            return 0;
        }
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[len];
        fmt.growBy = min(MPR_DEFAULT_ALLOC * 2, maxsize - len);
    }

    fmt.maxsize = maxsize;
    fmt.start = fmt.buf;
    fmt.end = fmt.buf;
    fmt.len = 0;
    *fmt.start = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(ctx, &fmt, c);
            break;

        case STATE_PERCENT:
            fmt.precision = -1;
            fmt.width = 0;
            fmt.flags = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                fmt.flags |= SPRINTF_SIGN;
                break;
            case '-':
                fmt.flags |= SPRINTF_LEFT;
                break;
            case '#':
                fmt.flags |= SPRINTF_ALTERNATE;
                break;
            case '0':
                fmt.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                fmt.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
                fmt.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                fmt.width = va_arg(arg, int);
                if (fmt.width < 0) {
                    fmt.width = -fmt.width;
                    fmt.flags |= SPRINTF_LEFT;
                }
            } else {
                while (isdigit((int) c)) {
                    fmt.width = fmt.width * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_DOT:
            fmt.precision = 0;
#if UNUSED
            fmt.flags &= ~SPRINTF_LEAD_ZERO;
#endif
            break;

        case STATE_PRECISION:
            if (c == '*') {
                fmt.precision = va_arg(arg, int);
            } else {
                while (isdigit((int) c)) {
                    fmt.precision = fmt.precision * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                fmt.flags |= SPRINTF_INT64;
                break;

            case 'l':
                fmt.flags |= SPRINTF_LONG;
                break;

            case 'h':
                fmt.flags |= SPRINTF_SHORT;
                break;
            }
            break;

        case STATE_TYPE:
            switch (c) {
#if BLD_FEATURE_FLOATING_POINT
            case 'e':
            case 'g':
            case 'f':
                fmt.radix = 10;
                outFloat(ctx, &fmt, c, (double) va_arg(arg, double));
                break;
#endif
            case 'c':
                BPUT(ctx, &fmt, (char) va_arg(arg, int));
                break;

#if FUTURE
            case 'N':
                qualifier = va_arg(arg, char*);
                len = strlen(qualifier);
                name = va_arg(arg, char*);
                tmpBuf = mprAlloc(ctx, len + strlen(name) + 2);
                if (tmpBuf == 0) {
                    return NULL;
                }
                strcpy(tmpBuf, qualifier);
                tmpBuf[len++] = ':';
                strcpy(&tmpBuf[len], name);
                sValue = tmpBuf;
                goto emitString;
#endif

            case 's':
            case 'S':
                sValue = va_arg(arg, char*);
                tmpBuf = 0;

#if FUTURE
            emitString:
#endif
                if (sValue == 0) {
                    sValue = "null";
                    len = (int) strlen(sValue);
                } else if (fmt.flags & SPRINTF_ALTERNATE) {
                    sValue++;
                    len = (int) *sValue;
                } else if (fmt.precision >= 0) {
                    /*
                     *  Can't use strlen(), the string may not have a null
                     */
                    cp = sValue;
                    for (len = 0; len < fmt.precision; len++) {
                        if (*cp++ == '\0') {
                            break;
                        }
                    }
                } else {
                    len = (int) strlen(sValue);
                }
                if (!(fmt.flags & SPRINTF_LEFT)) {
                    for (i = len; i < fmt.width; i++) {
                        BPUT(ctx, &fmt, (char) ' ');
                    }
                }
                for (i = 0; i < len && *sValue; i++) {
                    BPUT(ctx, &fmt, *sValue++);
                }
                if (fmt.flags & SPRINTF_LEFT) {
                    for (i = len; i < fmt.width; i++) {
                        BPUT(ctx, &fmt, (char) ' ');
                    }
                }
                if (tmpBuf) {
                    mprFree(tmpBuf);
                }
                break;

            case 'i':
                ;
            case 'd':
                fmt.radix = 10;
                if (fmt.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(arg, int);
                } else if (fmt.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(arg, long);
                } else if (fmt.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(arg, int64);
                } else {
                    iValue = (int) va_arg(arg, int);
                }
                if (iValue >= 0) {
                    if (fmt.flags & SPRINTF_LEAD_SPACE) {
                        outNum(ctx, &fmt, " ", iValue);
                    } else if (fmt.flags & SPRINTF_SIGN) {
                        outNum(ctx, &fmt, "+", iValue);
                    } else {
                        outNum(ctx, &fmt, 0, iValue);
                    }
                } else {
                    outNum(ctx, &fmt, "-", -iValue);
                }
                break;

            case 'X':
                fmt.flags |= SPRINTF_UPPER_CASE;
#if MPR_64_BIT
                fmt.flags &= ~(SPRINTF_SHORT|SPRINTF_LONG);
                fmt.flags |= SPRINTF_INT64;
#else
                fmt.flags &= ~(SPRINTF_INT64);
#endif
                /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (fmt.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(arg, uint);
                } else if (fmt.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(arg, ulong);
                } else if (fmt.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(arg, uint64);
                } else {
                    uValue = va_arg(arg, uint);
                }
                if (c == 'u') {
                    fmt.radix = 10;
                    outNum(ctx, &fmt, 0, uValue);
                } else if (c == 'o') {
                    fmt.radix = 8;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        outNum(ctx, &fmt, "0", uValue);
                    } else {
                        outNum(ctx, &fmt, 0, uValue);
                    }
                } else {
                    fmt.radix = 16;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        if (c == 'X') {
                            outNum(ctx, &fmt, "0X", uValue);
                        } else {
                            outNum(ctx, &fmt, "0x", uValue);
                        }
                    } else {
                        outNum(ctx, &fmt, 0, uValue);
                    }
                }
                break;

            case 'n':       /* Count of chars seen thus far */
                if (fmt.flags & SPRINTF_SHORT) {
                    short *count = va_arg(arg, short*);
                    *count = (int) (fmt.end - fmt.start);
                } else if (fmt.flags & SPRINTF_LONG) {
                    long *count = va_arg(arg, long*);
                    *count = (int) (fmt.end - fmt.start);
                } else {
                    int *count = va_arg(arg, int *);
                    *count = (int) (fmt.end - fmt.start);
                }
                break;

            case 'p':       /* Pointer */
#if MPR_64_BIT
                uValue = (uint64) va_arg(arg, void*);
#else
                uValue = (uint) PTOI(va_arg(arg, void*));
#endif
                fmt.radix = 16;
                outNum(ctx, &fmt, "0x", uValue);
                break;

            default:
                BPUT(ctx, &fmt, c);
            }
        }
    }
    BPUTNULL(ctx, &fmt);
    return (char*) fmt.buf;
}


/*
    Output a number according to the given format. 
 */
static void outNum(MprCtx ctx, Format *fmt, cchar *prefix, uint64 value)
{
    char    numBuf[64];
    char    *cp;
    char    *endp;
    char    c;
    int     letter, len, leadingZeros, i, fill;

    endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    cp = endp;

    /*
     *  Convert to ascii
     */
    if (fmt->radix == 16) {
        do {
            letter = (int) (value % fmt->radix);
            if (letter > 9) {
                if (fmt->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = letter;
            value /= fmt->radix;
        } while (value > 0);

    } else if (fmt->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
            if ((i++ % 3) == 0 && value > 0) {
                *--cp = ',';
            }
        } while (value > 0);
    } else {
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
        } while (value > 0);
    }

    len = (int) (endp - cp);
    fill = fmt->width - len;

    if (prefix != 0) {
        fill -= (int) strlen(prefix);
    }
    leadingZeros = (fmt->precision > len) ? fmt->precision - len : 0;
    fill -= leadingZeros;

    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(ctx, fmt, c);
        }
    }
    if (prefix != 0) {
        while (*prefix) {
            BPUT(ctx, fmt, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(ctx, fmt, '0');
    }
    while (*cp) {
        BPUT(ctx, fmt, *cp);
        cp++;
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(ctx, fmt, ' ');
        }
    }
}


#if BLD_FEATURE_FLOATING_POINT
/*
    Output a floating point number
 */
static void outFloat(MprCtx ctx, Format *fmt, char specChar, double value)
{
    char    result[MPR_MAX_STRING], *cp;
    int     c, fill, i, len, index;

    result[0] = '\0';
    if (specChar == 'f') {
        // sprintf(result, "%*.*f", fmt->width, fmt->precision, value);
        sprintf(result, "%.*f", fmt->precision, value);

    } else if (specChar == 'g') {
        // sprintf(result, "%*.*g", fmt->width, fmt->precision, value);
        sprintf(result, "%*.*g", fmt->width, fmt->precision, value);

    } else if (specChar == 'e') {
        // sprintf(result, "%*.*e", fmt->width, fmt->precision, value);
        sprintf(result, "%*.*e", fmt->width, fmt->precision, value);
    }

    len = strlen(result);
    fill = fmt->width - len;
    if (fmt->flags & SPRINTF_COMMA) {
        if (((len - 1) / 3) > 0) {
            fill -= (len - 1) / 3;
        }
    }

    if (fmt->flags & SPRINTF_SIGN && value > 0) {
        BPUT(ctx, fmt, '+');
        fill--;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(ctx, fmt, c);
        }
    }
    index = len;
    for (cp = result; *cp; cp++) {
        BPUT(ctx, fmt, *cp);
        if (fmt->flags & SPRINTF_COMMA) {
            if ((--index % 3) == 0 && index > 0) {
                BPUT(ctx, fmt, ',');
            }
        }
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(ctx, fmt, ' ');
        }
    }
    BPUTNULL(ctx, fmt);
}


int mprIsNan(double value) {
#if WIN
    return _fpclass(value) & (_FPCLASS_SNAN | _FPCLASS_QNAN);
#elif VXWORKS
    return value == (0.0 / 0.0);
#else
    return fpclassify(value) == FP_NAN;
#endif
}


int mprIsInfinite(double value) {
#if WIN
    return _fpclass(value) & (_FPCLASS_PINF | _FPCLASS_NINF);
#elif VXWORKS
    return value == (1.0 / 0.0) || value == (-1.0 / 0.0);
#else
    return fpclassify(value) == FP_INFINITE;
#endif
}

int mprIsZero(double value) {
#if WIN
    return _fpclass(value) & (_FPCLASS_NZ | _FPCLASS_PZ);
#elif VXWORKS
    return value == 0.0 || value == -0.0;
#else
    return fpclassify(value) == FP_ZERO;
#endif
}

/*
    Convert a double to ascii. Caller must free the result. This uses the JavaScript ECMA-262 spec for formatting rules.

    function dtoa(double value, int mode, int ndigits, int *periodOffset, int *sign, char **end)
 */
char *mprDtoa(MprCtx ctx, double value, int ndigits, int mode, int flags)
{
    MprBuf  *buf;
    char    *intermediate, *ip;
    int     period, sign, len, exponentForm, fixedForm, exponent, count, totalDigits;

    buf = mprCreateBuf(ctx, MPR_MAX_STRING, -1);
    intermediate = 0;
    exponentForm = 0;
    fixedForm = 0;

    if (mprIsNan(value)) {
        mprPutStringToBuf(buf, "NaN");

    } else if (mprIsInfinite(value)) {
        if (value < 0) {
            mprPutStringToBuf(buf, "-Infinity");
        } else {
            mprPutStringToBuf(buf, "Infinity");
        }
    } else if (value == 0) {
        mprPutCharToBuf(buf, '0');

    } else {
        if (ndigits <= 0) {
            if (!(flags & MPR_DTOA_FIXED_FORM)) {
                mode = MPR_DTOA_ALL_DIGITS;
            }
            ndigits = 0;

        } else if (mode == MPR_DTOA_ALL_DIGITS) {
            mode = MPR_DTOA_N_DIGITS;
        }
        if (flags & MPR_DTOA_EXPONENT_FORM) {
            exponentForm = 1;
            if (ndigits > 0) {
                ndigits++;
            } else {
                ndigits = 0;
                mode = MPR_DTOA_ALL_DIGITS;
            }
        } else if (flags & MPR_DTOA_FIXED_FORM) {
            fixedForm = 1;
        }

        /*
            Convert to an intermediate string representation. Period is the offset of the decimal point. NOTE: the
            intermediate representation may have less digits than period.
            Note: ndigits < 0 seems to trim N digits from the end with rounding.
         */
        ip = intermediate = dtoa(value, mode, ndigits, &period, &sign, NULL);
        len = strlen(intermediate);
        exponent = period - 1;

        if (mode == MPR_DTOA_ALL_DIGITS && ndigits == 0) {
            ndigits = len;
        }
        if (!fixedForm) {
            if (period <= -6 || period > 21) {
                exponentForm = 1;
            }
        }
        if (sign) {
            mprPutCharToBuf(buf, '-');
        }
        if (exponentForm) {
            mprPutCharToBuf(buf, ip[0] ? ip[0] : '0');
            if (len > 1) {
                mprPutCharToBuf(buf, '.');
                mprPutSubStringToBuf(buf, &ip[1], (ndigits == 0) ? len - 1: ndigits);
            }
            mprPutCharToBuf(buf, 'e');
            mprPutCharToBuf(buf, (period < 0) ? '-' : '+');
            mprPutFmtToBuf(buf, "%d", (exponent < 0) ? -exponent: exponent);

        } else {
            if (mode == MPR_DTOA_N_FRACTION_DIGITS) {
                /* Count of digits */
                if (period <= 0) {
                    /* Leading fractional zeros required */
                    mprPutStringToBuf(buf, "0.");
                    mprPutPadToBuf(buf, '0', -period);
                    mprPutStringToBuf(buf, ip);
                    mprPutPadToBuf(buf, '0', ndigits - len + period);

                } else {
                    count = min(len, period);
                    /* Leading integral digits */
                    mprPutSubStringToBuf(buf, ip, count);
                    /* Trailing zero pad */
                    mprPutPadToBuf(buf, '0', period - len);
                    totalDigits = count + ndigits;
                    if (period < totalDigits) {
                        count = totalDigits + sign - mprGetBufLength(buf);
                        mprPutCharToBuf(buf, '.');
                        mprPutSubStringToBuf(buf, &ip[period], count);
                        mprPutPadToBuf(buf, '0', count - strlen(&ip[period]));
                    }
                }

            } else if (len <= period && period <= 21) {
                /* data shorter than period */
                mprPutStringToBuf(buf, ip);
                mprPutPadToBuf(buf, '0', period - len);

            } else if (0 < period && period <= 21) {
                /* Period shorter than data */
                mprPutSubStringToBuf(buf, ip, period);
                mprPutCharToBuf(buf, '.');
                mprPutStringToBuf(buf, &ip[period]);

            } else if (-6 < period && period <= 0) {
                /* Small negative exponent */
                mprPutStringToBuf(buf, "0.");
                mprPutPadToBuf(buf, '0', -period);
                mprPutStringToBuf(buf, ip);

            } else {
                mprAssert(0);
            }
        }
    }
    mprAddNullToBuf(buf);
    if (intermediate) {
        freedtoa(intermediate);
    }
    return mprStealBuf(ctx, buf);
}
#endif /* BLD_FEATURE_FLOATING_POINT */


/*
    Grow the buffer to fit new data. Return 1 if the buffer can grow. 
    Grow using the growBy size specified when creating the buffer. 
 */
static int growBuf(MprCtx ctx, Format *fmt)
{
    uchar   *newbuf;
    int     buflen;

    buflen = (int) (fmt->endbuf - fmt->buf);
    if (fmt->maxsize >= 0 && buflen >= fmt->maxsize) {
        return 0;
    }
    if (fmt->growBy <= 0) {
        /*
            User supplied buffer
         */
        return 0;
    }
    mprAssert(ctx);
    newbuf = (uchar*) mprAlloc(ctx, buflen + fmt->growBy);
    if (newbuf == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (fmt->buf) {
        memcpy(newbuf, fmt->buf, buflen);
        mprFree(fmt->buf);
    }

    buflen += fmt->growBy;
    fmt->end = newbuf + (fmt->end - fmt->buf);
    fmt->start = newbuf + (fmt->start - fmt->buf);
    fmt->buf = newbuf;
    fmt->endbuf = &fmt->buf[buflen];

    /*
     *  Increase growBy to reduce overhead
     */
    if ((buflen + (fmt->growBy * 2)) < fmt->maxsize) {
        fmt->growBy *= 2;
    }
    return 1;
}


/*
    For easy debug trace
 */
int print(cchar *fmt, ...)
{
    MprFileSystem   *fs;
    MprCtx          ctx;
    va_list         ap;
    char            *buf;
    int             len;

    ctx = mprGetMpr(NULL);

    fs = mprLookupFileSystem(ctx, "/");
    va_start(ap, fmt);
    buf = mprVasprintf(ctx, -1, fmt, ap);
    va_end(ap);
    if (buf != 0 && fs->stdOutput) {
        len = mprWriteString(fs->stdOutput, buf);
        len += mprWriteString(fs->stdOutput, "\n");
    } else {
        len = -1;
    }
    mprFree(buf);
    return len;
}

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */


/************************************************************************/
/*
 *  End of file "../src/mprPrintf.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprRomFile.c"
 */
/************************************************************************/

/*
 *  mprRomFile.c - ROM File system
 *
 *  ROM support for systems without disk or flash based file systems. This module provides read-only file retrieval 
 *  from compiled file images. Use the mprRomComp program to compile files into C code and then link them into your 
 *  application. This module uses a hashed symbol table for fast file lookup.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_ROMFS 

static int closeFile(MprFile *file);
static int getPathInfo(MprRomFileSystem *rfs, cchar *path, MprPath *info);
static MprRomInode *lookup(MprRomFileSystem *rfs, cchar *path);


static MprFile *openFile(MprCtx ctx, MprFileSystem *fileSystem, cchar *path, int flags, int omode)
{
    MprRomFileSystem    *rfs;
    MprFile             *file;
    
    mprAssert(path && *path);

    rfs = (MprRomFileSystem*) fileSystem;
    file = mprAllocObjWithDestructorZeroed(ctx, MprFile, closeFile);
    file->fileSystem = fileSystem;
    file->mode = omode;
    file->fd = -1;

    if ((file->inode = lookup(rfs, path)) == 0) {
        return 0;
    }
    return file;
}


static int closeFile(MprFile *file)
{
    return 0;
}


static int readFile(MprFile *file, void *buf, uint size)
{
    MprRomInode     *inode;
    int             len;

    mprAssert(buf);

    if (file->fd == 0) {
        return read(file->fd, buf, size);
    }
    inode = file->inode;
    len = min(inode->size - file->iopos, size);
    mprAssert(len >= 0);
    memcpy(buf, &inode->data[file->iopos], len);
    file->iopos += len;
    return len;
}


static int writeFile(MprFile *file, const void *buf, uint size)
{
    if (file->fd == 1 || file->fd == 2) {
        return write(file->fd, buf, size);
    }
    return MPR_ERR_CANT_WRITE;
}


static long seekFile(MprFile *file, int seekType, long distance)
{
    MprRomInode     *inode;

    mprAssert(seekType == SEEK_SET || seekType == SEEK_CUR || seekType == SEEK_END);

    inode = file->inode;

    switch (seekType) {
    case SEEK_CUR:
        file->iopos += distance;
        break;

    case SEEK_END:
        file->iopos = inode->size + distance;
        break;

    default:
        file->iopos = distance;
        break;
    }
    if (file->iopos < 0) {
        errno = EBADF;
        return MPR_ERR_BAD_STATE;
    }
    return file->iopos;
}


static bool accessPath(MprRomFileSystem *fileSystem, cchar *path, int omode)
{
    MprPath     info;

    return getPathInfo(fileSystem, path, &info) == 0 ? 1 : 0;
}


static int deletePath(MprRomFileSystem *fileSystem, cchar *path)
{
    return MPR_ERR_CANT_WRITE;
}
 

static int makeDir(MprRomFileSystem *fileSystem, cchar *path, int perms)
{
    return MPR_ERR_CANT_WRITE;
}


static int makeLink(MprRomFileSystem *fileSystem, cchar *path, cchar *target, int hard)
{
    return MPR_ERR_CANT_WRITE;
}


static int getPathInfo(MprRomFileSystem *rfs, cchar *path, MprPath *info)
{
    MprRomInode *ri;

    mprAssert(path && *path);

    info->checked = 1;

    if ((ri = (MprRomInode*) lookup(rfs, path)) == 0) {
        return MPR_ERR_NOT_FOUND;
    }
    memset(info, 0, sizeof(MprPath));

    info->valid = 1;
    info->size = ri->size;
    info->mtime = 0;
    info->inode = ri->num;

    if (ri->data == 0) {
        info->isDir = 1;
        info->isReg = 0;
    } else {
        info->isReg = 1;
        info->isDir = 0;
    }
    return 0;
}


static int getPathLink(MprRomFileSystem *rfs, cchar *path)
{
    /* Links not supported on ROMfs */
    return NULL;
}


static MprRomInode *lookup(MprRomFileSystem *rfs, cchar *path)
{
    if (path == 0) {
        return 0;
    }

    /*
     *  Remove "./" segments
     */
    while (*path == '.') {
        if (path[1] == '\0') {
            path++;
        } else if (path[1] == '/') {
            path += 2;
        } else {
            break;
        }
    }

    /*
     *  Skip over the leading "/"
     */
    if (*path == '/') {
        path++;
    }
    return (MprRomInode*) mprLookupHash(rfs->fileIndex, path);
}


int mprSetRomFileSystem(MprCtx ctx, MprRomInode *inodeList)
{
    MprRomFileSystem   * rfs;
    MprRomInode         *ri;

    rfs = (MprRomFileSystem*) mprGetMpr(ctx)->fileSystem;

    rfs->romInodes = inodeList;
    rfs->fileIndex = mprCreateHash(rfs, MPR_FILES_HASH_SIZE);

    for (ri = inodeList; ri->path; ri++) {
        if (mprAddHash(rfs->fileIndex, ri->path, ri) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    return 0;
}


MprRomFileSystem *mprCreateRomFileSystem(MprCtx ctx, cchar *path)
{
    MprFileSystem      *fs;
    MprRomFileSystem   *rfs;

    rfs = mprAllocObjZeroed(ctx, MprRomFileSystem);
    if (rfs == 0) {
        return rfs;
    }

    fs = &rfs->fileSystem;
    fs->accessPath = (MprAccessFileProc) accessPath;
    fs->deletePath = (MprDeleteFileProc) deletePath;
    fs->getPathInfo = (MprGetPathInfoProc) getPathInfo;
    fs->getPathLink = (MprGetPathLinkProc) getPathLink;
    fs->makeDir = (MprMakeDirProc) makeDir;
    fs->makeLink = (MprMakeDirProc) makeLink;
    fs->openFile = (MprOpenFileProc) openFile;
    fs->closeFile = closeFile;
    fs->readFile = readFile;
    fs->seekFile = seekFile;
    fs->writeFile = writeFile;

#if !WINCE
    fs->stdError = mprAllocObjZeroed(fs, MprFile);
    if (fs->stdError == 0) {
        mprFree(fs);
    }
    fs->stdError->fd = 2;
    fs->stdError->fileSystem = fs;
    fs->stdError->mode = O_WRONLY;

    fs->stdInput = mprAllocObjZeroed(fs, MprFile);
    if (fs->stdInput == 0) {
        mprFree(fs);
    }
    fs->stdInput->fd = 0;
    fs->stdInput->fileSystem = fs;
    fs->stdInput->mode = O_RDONLY;

    fs->stdOutput = mprAllocObjZeroed(fs, MprFile);
    if (fs->stdOutput == 0) {
        mprFree(fs);
    }
    fs->stdOutput->fd = 1;
    fs->stdOutput->fileSystem = fs;
    fs->stdOutput->mode = O_WRONLY;
#endif
    return rfs;
}


#else /* BLD_FEATURE_ROMFS */
void __dummy_romfs() {}
#endif /* BLD_FEATURE_ROMFS */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprRomFile.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprSelectWait.c"
 */
/************************************************************************/

/**
 *  mprSelectWait.c - Wait for I/O by using select.
 *
 *  This module provides I/O wait management for sockets on VxWorks and systems that use select(). Windows and Unix
 *  uses different mechanisms. See mprAsyncSelectWait and mprPollWait. This module is thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if WINCE || VXWORKS || CYGWIN

static void getWaitFds(MprWaitService *ws);
static void serviceIO(MprWaitService *ws);


int mprInitSelectWait(MprWaitService *ws)
{
#if BLD_FEATURE_MULTITHREAD
    int     rc, retries, breakPort, breakSock, maxTries;

    maxTries = 100;

    /*
     *  Initialize the "wakeup" socket. This is used to wakeup the service thread if other threads need to wait for I/O.
     */
    mprLock(ws->mutex);

    /*
     *  Try to find a good port to use to break out of the select wait
     */ 
    breakPort = MPR_DEFAULT_BREAK_PORT;
    for (rc = retries = 0; retries < maxTries; retries++) {
        breakSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (breakSock < 0) {
            mprLog(ws, MPR_WARN, "Can't open port %d to use for select. Retrying.\n");
        }
#if BLD_UNIX_LIKE
        fcntl(breakSock, F_SETFD, FD_CLOEXEC);
#endif
        ws->breakAddress.sin_family = AF_INET;
#if CYGWIN
        /*
            Cygwin doesn't work with INADDR_ANY
         */
        ws->breakAddress.sin_addr.s_addr = inet_addr("127.0.0.1");;
#else
        ws->breakAddress.sin_addr.s_addr = INADDR_ANY;
#endif
        ws->breakAddress.sin_port = htons((short) breakPort);
        rc = bind(breakSock, (struct sockaddr *) &ws->breakAddress, sizeof(ws->breakAddress));
        if (breakSock >= 0 && rc == 0) {
#if VXWORKS
            /*
             *  VxWorks 6.0 bug workaround
             */
            ws->breakAddress.sin_port = htons((short) breakPort);
#endif
            break;
        }
        if (breakSock >= 0) {
            closesocket(breakSock);
        }
        breakPort++;
    }

    if (breakSock < 0 || rc < 0) {
        mprLog(ws, MPR_WARN, "Can't bind any port to use for select. Tried %d-%d\n", breakPort, breakPort - maxTries);
        mprUnlock(ws->mutex);
        return MPR_ERR_CANT_OPEN;
    }
    ws->breakSock = breakSock;
    mprUnlock(ws->mutex);
#endif
    return 0;
}


/*
 *  Wait for I/O on a single file descriptor. Return a mask of events found. Mask is the events of interest.
 *  timeout is in milliseconds.
 */
int mprWaitForSingleIO(MprCtx ctx, int fd, int mask, int timeout)
{
    MprWaitService  *ws;
    struct timeval  tval;
    fd_set          readMask, writeMask;

    ws = mprGetMpr(ctx)->waitService;
    tval.tv_sec = timeout / 1000;
    tval.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&readMask);
    if (mask & MPR_READABLE) {
        FD_SET(fd, &readMask);
    }
    FD_ZERO(&writeMask);
    if (mask & MPR_WRITABLE) {
        FD_SET(fd, &writeMask);
    }
    if (select(fd + 1, &readMask, &writeMask, NULL, &tval) > 0) {
        mask = 0;
        if (FD_ISSET(fd, &readMask))
            mask |= MPR_READABLE;
        if (FD_ISSET(fd, &writeMask))
            mask |= MPR_WRITABLE;
        return mask;
    }
    return 0;
}


static void serviceRecall(MprWaitService *ws) 
{
    MprWaitHandler      *wp;
    int                 index;

    mprLock(ws->mutex);
    ws->flags &= ~MPR_NEED_RECALL;
    for (index = 0; (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &index)) != 0; ) {
        if (wp->flags & MPR_WAIT_RECALL_HANDLER) {
            if ((wp->desiredMask & wp->disableMask) && wp->inUse == 0) {
                wp->presentMask |= MPR_READABLE;
                wp->flags &= ~MPR_WAIT_RECALL_HANDLER;
#if BLD_FEATURE_MULTITHREAD
                mprAssert(wp->disableMask == -1);
                ws->maskGeneration++;
                wp->disableMask = 0;
                mprAssert(wp->inUse == 0);
                wp->inUse++;
#endif
                mprUnlock(ws->mutex);
                mprInvokeWaitCallback(wp);
                mprLock(ws->mutex);

            } else {
                ws->flags |= MPR_NEED_RECALL;
            }
        }
    }
    mprUnlock(ws->mutex);
}


/*
 *  Wait for I/O on all registered file descriptors. Timeout is in milliseconds. Return the number of events detected.
 */
int mprWaitForIO(MprWaitService *ws, int timeout)
{
    struct timeval  tval;
    int             rc;

    /*
     *  No locking. If the masks are updated after this test, the breakout port will wake us up soon.
     */
    if (ws->lastMaskGeneration != ws->maskGeneration) {
        getWaitFds(ws);
    }

    if (ws->flags & MPR_NEED_RECALL) {
        serviceRecall(ws);
        return 1;
    }
#if BLD_DEBUG
    if (mprGetDebugMode(ws) && timeout > 30000) {
        timeout = 30000;
    }
#endif
#if VXWORKS
    /*
     *  To minimize VxWorks task starvation
     */
    timeout = max(timeout, 50);
#endif
    tval.tv_sec = timeout / 1000;
    tval.tv_usec = (timeout % 1000) * 1000;

    /*
        Copy into a stable list while selecting
     */
    ws->selectReadMask = ws->readMask;
    ws->selectWriteMask = ws->writeMask;

    rc = select(ws->maxfd + 1, &ws->selectReadMask, &ws->selectWriteMask, NULL, &tval);
    if (rc > 0) {
        serviceIO(ws);
    }
    return rc;
}


/*
 *  Build the select wait masks
 */
static void getWaitFds(MprWaitService *ws)
{
    MprWaitHandler  *wp, *nextWp;
    int             mask, next;

    mprLock(ws->mutex);

    ws->lastMaskGeneration = ws->maskGeneration;

    FD_ZERO(&ws->readMask);
    FD_ZERO(&ws->writeMask);
    ws->maxfd = 0;

#if BLD_FEATURE_MULTITHREAD
    /*
     *  Add the breakout port to wakeup the service thread when other threads need selecting services.
     */
    FD_SET(ws->breakSock, &ws->readMask);
    ws->maxfd = ws->breakSock + 1;
#endif

    /*
     *  Add an entry for each descriptor desiring service.
     */
    next = 0;
    for (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &next); wp; wp = nextWp) {
        nextWp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &next);
        mprAssert(wp->fd >= 0);

        if (wp->proc && wp->desiredMask) {
            /*
             *  The disable mask will be zero when we are already servicing an event. This prevents recursive service.
             */
            mask = wp->desiredMask & wp->disableMask;
            if (mask) {
#if BLD_FEATURE_MULTITHREAD
                if (wp->inUse) continue;
#endif
                if (mask & MPR_READABLE) {
                    FD_SET(wp->fd, &ws->readMask);
                }
                if (mask & MPR_WRITABLE) {
                    FD_SET(wp->fd, &ws->writeMask);
                }
                ws->maxfd = max(ws->maxfd, wp->fd);
            }
        }
    }
    mprUnlock(ws->mutex);
}


/*
 *  Service I/O events
 */
static void serviceIO(MprWaitService *ws)
{
    MprWaitHandler      *wp;
    int                 mask, index;

    /*
     *  Must have the wait list stable while we service events
     */
    mprLock(ws->mutex);

#if BLD_FEATURE_MULTITHREAD
    /*
     *  Service the breakout pipe first
     */
    if (FD_ISSET(ws->breakSock, &ws->selectReadMask)) {
        char        buf[128];
        int         rc;

#if VXWORKS
        int len = sizeof(ws->breakAddress);
        rc = recvfrom(ws->breakSock, buf, sizeof(buf), 0, (struct sockaddr*) &ws->breakAddress, (int*) &len);
#else
        socklen_t   len = sizeof(ws->breakAddress);
        rc = recvfrom(ws->breakSock, buf, sizeof(buf), 0, (struct sockaddr*) &ws->breakAddress, (socklen_t*) &len);
#endif
        if (rc < 0) {
            closesocket(ws->breakSock);
            if (mprInitSelectWait(ws) < 0) {
                mprError(ws, "Can't re-open select breakout port");
            }
        }
        ws->flags &= ~MPR_BREAK_REQUESTED;
    }
#endif

    /*
     *  Now service all IO wait handlers
     */
    for (index = 0; (wp = (MprWaitHandler*) mprGetNextItem(ws->handlers, &index)) != 0; ) {
        mprAssert(wp->fd >= 0);
        /*
         *  Present mask is only cleared after the io handler callback has completed
         */
        mask = 0;
        if ((wp->desiredMask & MPR_READABLE) && FD_ISSET(wp->fd, &ws->selectReadMask)) {
            mask |= MPR_READABLE;
            FD_CLR((uint) wp->fd, &ws->selectReadMask);
        }
        if ((wp->desiredMask & MPR_WRITABLE) && FD_ISSET(wp->fd, &ws->selectWriteMask)) {
            mask |= MPR_WRITABLE;
            FD_CLR((uint) wp->fd, &ws->selectWriteMask);
        }
        if (mask == 0) {
            continue;
        }
        if (mask & wp->desiredMask) {
#if BLD_FEATURE_MULTITHREAD
            /*
             *  Disable events to prevent recursive I/O events. Callback must call mprEnableWaitEvents
             */
            ws->maskGeneration++;
            if (wp->disableMask == 0) {
                /* Should never get here. Just for safety. */
                continue;
            }
            wp->disableMask = 0;
            mprAssert(wp->inUse == 0);
            wp->inUse++;
#endif
            wp->presentMask = mask;

            mprUnlock(ws->mutex);
            mprInvokeWaitCallback(wp);
            mprLock(ws->mutex);
        }
    }

    mprUnlock(ws->mutex);
}


#if BLD_FEATURE_MULTITHREAD
/*
 *  Wake the wait service (i.e. select/poll call)
 */
void mprWakeOsWaitService(MprCtx ctx)
{
    MprWaitService  *ws;
    int             c, rc;

    ws = mprGetMpr(ctx)->waitService;
    mprLock(ws->mutex);
    if (!(ws->flags & MPR_BREAK_REQUESTED)) {
        ws->flags |= MPR_BREAK_REQUESTED;
        c = 0;
        rc = sendto(ws->breakSock, (char*) &c, 1, 0, (struct sockaddr*) &ws->breakAddress, sizeof(ws->breakAddress));
        if (rc < 0) {
            static int warnOnce = 0;
            if (warnOnce++ == 0) {
                mprLog(ws, 0, "Can't send wakeup to breakout socket: errno %d", errno);
            }
            ws->lastMaskGeneration = 0;
        }
    }
    mprUnlock(ws->mutex);
}
#endif

#else
void __dummyMprSelectWait() {}
#endif /* VXWORKS */

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprSelectWait.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprSocket.c"
 */
/************************************************************************/

/**
 *  mprSocket.c - Convenience class for the management of sockets
 *
 *  This module provides a higher level interface to interact with the standard sockets API. It does not perform
 *  buffering.
 *
 *  This module is thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if !VXWORKS && !WINCE
/*
 *  Set this to 0 to disable the IPv6 address service. You can do this if you only need IPv4.
 *  On MAC OS X, getaddrinfo is not thread-safe and crashes when called by a 2nd thread at any time. ie. locking wont help.
 */
#define BLD_HAS_GETADDRINFO 1
#endif


static MprSocket *acceptSocket(MprSocket *sp, bool invokeCallback);
static void closeSocket(MprSocket *sp, bool gracefully);
static int  connectSocket(MprSocket *sp, cchar *host, int port, int initialFlags);
static MprSocket *createSocket(MprCtx ctx, struct MprSsl *ssl);
static MprSocketProvider *createStandardProvider(MprSocketService *ss);
static void disconnectSocket(MprSocket *sp);
static int  flushSocket(MprSocket *sp);
static int  getSocketIpAddr(MprCtx ctx, struct sockaddr *addr, int addrlen, char *ipAddr, int size, int *port);
static int  ioProc(MprSocket *sp, int mask);
static int  ipv6(cchar *ip);
static int  listenSocket(MprSocket *sp, cchar *host, int port, MprSocketAcceptProc acceptFn, void *data, int initialFlags);
static int  readSocket(MprSocket *sp, void *buf, int bufsize);
static int  socketDestructor(MprSocket *sp);
static int  writeSocket(MprSocket *sp, void *buf, int bufsize);

/*
 *  Open the socket service
 */

MprSocketService *mprCreateSocketService(MprCtx ctx)
{
    MprSocketService    *ss;

    mprAssert(ctx);

    ss = mprAllocObjZeroed(ctx, MprSocketService);
    if (ss == 0) {
        return 0;
    }
    ss->next = 0;
    ss->maxClients = INT_MAX;
    ss->numClients = 0;

    ss->standardProvider = createStandardProvider(ss);
    if (ss->standardProvider == NULL) {
        mprFree(ss);
        return 0;
    }
    ss->secureProvider = NULL;

#if BLD_FEATURE_MULTITHREAD
    ss->mutex = mprCreateLock(ss);
    if (ss->mutex == 0) {
        mprFree(ss);
        return 0;
    }
#endif
    return ss;
}


/*
 *  Start the socket service
 */
int mprStartSocketService(MprSocketService *ss)
{
    char            hostName[MPR_MAX_IP_NAME], serverName[MPR_MAX_IP_NAME], domainName[MPR_MAX_IP_NAME], *dp;

    mprAssert(ss);

    serverName[0] = '\0';
    domainName[0] = '\0';
    hostName[0] = '\0';

    if (gethostname(serverName, sizeof(serverName)) < 0) {
        mprStrcpy(serverName, sizeof(serverName), "localhost");
        mprUserError(ss, "Can't get host name. Using \"localhost\".");
        /* Keep going */
    }

    if ((dp = strchr(serverName, '.')) != 0) {
        mprStrcpy(hostName, sizeof(hostName), serverName);
        *dp++ = '\0';
        mprStrcpy(domainName, sizeof(domainName), dp);

    } else {
        mprStrcpy(hostName, sizeof(hostName), serverName);
    }
    mprSetServerName(ss, serverName);
    mprSetDomainName(ss, domainName);
    mprSetHostName(ss, hostName);
    return 0;
}


void mprStopSocketService(MprSocketService *ss)
{
}


static MprSocketProvider *createStandardProvider(MprSocketService *ss)
{
    MprSocketProvider   *provider;

    provider = mprAllocObj(ss, MprSocketProvider);
    if (provider == 0) {
        return 0;
    }
    provider->name = "standard";
    provider->acceptSocket = acceptSocket;
    provider->closeSocket = closeSocket;
    provider->connectSocket = connectSocket;
    provider->createSocket = createSocket;
    provider->disconnectSocket = disconnectSocket;
    provider->flushSocket = flushSocket;
    provider->listenSocket = listenSocket;
    provider->readSocket = readSocket;
    provider->writeSocket = writeSocket;
    return provider;
}


void mprSetSecureProvider(MprCtx ctx, MprSocketProvider *provider)
{
    mprGetMpr(ctx)->socketService->secureProvider = provider;
}


bool mprHasSecureSockets(MprCtx ctx)
{
    return (mprGetMpr(ctx)->socketService->secureProvider != 0);
}


int mprSetMaxSocketClients(MprCtx ctx, int max)
{
    MprSocketService    *ss;

    mprAssert(ctx);
    mprAssert(max >= 0);

    ss = mprGetMpr(ctx)->socketService;
    ss->maxClients = max;
    return 0;
}


/*
 *  Create a new socket
 */
static MprSocket *createSocket(MprCtx ctx, struct MprSsl *ssl)
{
    MprSocket       *sp;

    sp = mprAllocObjWithDestructorZeroed(ctx, MprSocket, socketDestructor);
    if (sp == 0) {
        return 0;
    }

    sp->handlerPriority = MPR_NORMAL_PRIORITY;
    sp->port = -1;
    sp->fd = -1;
    sp->flags = 0;

    sp->provider = mprGetMpr(ctx)->socketService->standardProvider;
    sp->service = mprGetMpr(ctx)->socketService;

#if BLD_FEATURE_MULTITHREAD
    sp->mutex = mprCreateLock(sp);
#endif
    return sp;
}


/*
 *  Create a new socket
 */
MprSocket *mprCreateSocket(MprCtx ctx, struct MprSsl *ssl)
{
    MprSocketService    *ss;
    MprSocket           *sp;

    ss = mprGetMpr(ctx)->socketService;

    if (ssl) {
#if !BLD_FEATURE_SSL
        return 0;
#endif
        if (ss->secureProvider == NULL || ss->secureProvider->createSocket == NULL) {
            return 0;
        }
        sp = ss->secureProvider->createSocket(ctx, ssl);

    } else {
        mprAssert(ss->standardProvider->createSocket);
        sp = ss->standardProvider->createSocket(ctx, NULL);
    }
    sp->service = ss;
    return sp;
}


static int socketDestructor(MprSocket *sp)
{
    MprSocketService    *ss;

    ss = sp->service;

    mprLock(ss->mutex);
    if (sp->fd >= 0) {
        mprCloseSocket(sp, 1);
    }
    mprUnlock(ss->mutex);
    return 0;
}


/*
 *  Re-initialize all socket variables so the Socket can be reused.
 */
static void resetSocket(MprSocket *sp)
{
    if (sp->fd >= 0) {
        mprCloseSocket(sp, 0);
    }
    if (sp->flags & MPR_SOCKET_CLOSED) {
        sp->acceptCallback = 0;
        sp->acceptData = 0;
        sp->waitForEvents = 0;
        sp->currentEvents = 0;
        sp->error = 0;
        sp->flags = 0;
        sp->ioCallback = 0;
        sp->ioData = 0;
        sp->handlerMask = 0;
        sp->handlerPriority = MPR_NORMAL_PRIORITY;
        sp->interestEvents = 0;
        sp->port = -1;
        sp->fd = -1;
        mprFree(sp->ipAddr);
        sp->ipAddr = 0;
    }
    mprAssert(sp->provider);
}


/*
 *  Open a server socket connection
 */
int mprOpenServerSocket(MprSocket *sp, cchar *host, int port, MprSocketAcceptProc acceptFn, void *data, int flags)
{
    if (sp->provider == 0) {
        return MPR_ERR_NOT_INITIALIZED;
    }
    return sp->provider->listenSocket(sp, host, port, acceptFn, data, flags);
}


static int listenSocket(MprSocket *sp, cchar *host, int port, MprSocketAcceptProc acceptFn, void *data, int initialFlags)
{
    struct sockaddr     *addr;
    socklen_t           addrlen;
    int                 datagram, family, protocol, rc;

    lock(sp);

    if (host == 0 || *host == '\0') {
        mprLog(sp, 6, "listenSocket: *:%d, flags %x", port, initialFlags);
    } else {
        mprLog(sp, 6, "listenSocket: %s:%d, flags %x", host, port, initialFlags);
    }
    resetSocket(sp);

    sp->ipAddr = mprStrdup(sp, host);
    sp->port = port;
    sp->acceptCallback = acceptFn;
    sp->acceptData = data;

    sp->flags = (initialFlags &
        (MPR_SOCKET_BROADCAST | MPR_SOCKET_DATAGRAM | MPR_SOCKET_BLOCK |
         MPR_SOCKET_LISTENER | MPR_SOCKET_NOREUSE | MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD));
    datagram = sp->flags & MPR_SOCKET_DATAGRAM;

    if (mprGetSocketInfo(sp, host, port, &family, &protocol, &addr, &addrlen) < 0) {
        return MPR_ERR_NOT_FOUND;
    }
    sp->fd = (int) socket(family, datagram ? SOCK_DGRAM: SOCK_STREAM, protocol);
    if (sp->fd < 0) {
        unlock(sp);
        return MPR_ERR_CANT_OPEN;
    }

#if !BLD_WIN_LIKE && !VXWORKS
    /*
     *  Children won't inherit this fd
     */
    fcntl(sp->fd, F_SETFD, FD_CLOEXEC);
#endif

#if BLD_UNIX_LIKE
    if (!(sp->flags & MPR_SOCKET_NOREUSE)) {
        rc = 1;
        setsockopt(sp->fd, SOL_SOCKET, SO_REUSEADDR, (char*) &rc, sizeof(rc));
    }
#endif
    if (sp->service->prebind) {
        if ((sp->service->prebind)(sp) < 0) {
            mprFree(addr);
            closesocket(sp->fd);
            sp->fd = -1;
            unlock(sp);
            return MPR_ERR_CANT_OPEN;
        }
    }
    rc = bind(sp->fd, addr, addrlen);
    if (rc < 0) {
        rc = errno;
        mprFree(addr);
        closesocket(sp->fd);
        sp->fd = -1;
        unlock(sp);
        return MPR_ERR_CANT_OPEN;
    }
    mprFree(addr);

    if (! datagram) {
        sp->flags |= MPR_SOCKET_LISTENER;
        if (listen(sp->fd, SOMAXCONN) < 0) {
            mprLog(sp, 3, "Listen error %d", mprGetOsError());
            closesocket(sp->fd);
            sp->fd = -1;
            unlock(sp);
            return MPR_ERR_CANT_OPEN;
        }
        sp->handlerMask |= MPR_SOCKET_READABLE;
        sp->handler = mprCreateWaitHandler(sp, sp->fd, MPR_SOCKET_READABLE, (MprWaitProc) mprAcceptProc, sp, 
            sp->handlerPriority, (sp->flags & MPR_SOCKET_THREAD) ? MPR_WAIT_THREAD : 0);
    }

#if BLD_WIN_LIKE
    /*
     *  Delay setting reuse until now so that we can be assured that we have exclusive use of the port.
     */
    if (!(sp->flags & MPR_SOCKET_NOREUSE)) {
        rc = 1;
        setsockopt(sp->fd, SOL_SOCKET, SO_REUSEADDR, (char*) &rc, sizeof(rc));
    }
#endif

    mprSetSocketBlockingMode(sp, (bool) (sp->flags & MPR_SOCKET_BLOCK));

    /*
     *  TCP/IP stacks have the No delay option (nagle algorithm) on by default.
     */
    if (sp->flags & MPR_SOCKET_NODELAY) {
        mprSetSocketNoDelay(sp, 1);
    }
    unlock(sp);
    return sp->fd;
}


/*
 *  Open a client socket connection
 */
int mprOpenClientSocket(MprSocket *sp, cchar *host, int port, int flags)
{
    if (sp->provider == 0) {
        return MPR_ERR_NOT_INITIALIZED;
    }
    return sp->provider->connectSocket(sp, host, port, flags);
}


static int connectSocket(MprSocket *sp, cchar *host, int port, int initialFlags)
{
    struct sockaddr     *addr;
    socklen_t           addrlen;
    int                 broadcast, datagram, family, protocol, rc, err;

    lock(sp);

    resetSocket(sp);

    mprLog(sp, 6, "openClient: %s:%d, flags %x", host, port, initialFlags);

    sp->port = port;
    sp->flags = (initialFlags &
        (MPR_SOCKET_BROADCAST | MPR_SOCKET_DATAGRAM | MPR_SOCKET_BLOCK |
         MPR_SOCKET_LISTENER | MPR_SOCKET_NOREUSE | MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD));
    sp->flags |= MPR_SOCKET_CLIENT;

    mprFree(sp->ipAddr);
    sp->ipAddr = mprStrdup(sp, host);

    broadcast = sp->flags & MPR_SOCKET_BROADCAST;
    if (broadcast) {
        sp->flags |= MPR_SOCKET_DATAGRAM;
    }
    datagram = sp->flags & MPR_SOCKET_DATAGRAM;

    if (mprGetSocketInfo(sp, host, port, &family, &protocol, &addr, &addrlen) < 0) {
        err = mprGetSocketError(sp);
        closesocket(sp->fd);
        sp->fd = -1;
        unlock(sp);
        return MPR_ERR_CANT_ACCESS;
    }
    /*
     *  Create the O/S socket
     */
    sp->fd = (int) socket(family, datagram ? SOCK_DGRAM: SOCK_STREAM, protocol);
    if (sp->fd < 0) {
        err = mprGetSocketError(sp);
        unlock(sp);
        return MPR_ERR_CANT_OPEN;
    }

#if !BLD_WIN_LIKE && !VXWORKS
    /*
     *  Children should not inherit this fd
     */
    fcntl(sp->fd, F_SETFD, FD_CLOEXEC);
#endif

    if (broadcast) {
        int flag = 1;
        if (setsockopt(sp->fd, SOL_SOCKET, SO_BROADCAST, (char *) &flag, sizeof(flag)) < 0) {
            err = mprGetSocketError(sp);
            closesocket(sp->fd);
            sp->fd = -1;
            unlock(sp);
            return MPR_ERR_CANT_INITIALIZE;
        }
    }

    if (!datagram) {
        sp->flags |= MPR_SOCKET_CONNECTING;
        do {
            rc = connect(sp->fd, addr, addrlen);
        } while (rc == -1 && errno == EINTR);
        err = errno;
        if (rc < 0) {
            /* MAC/BSD returns EADDRINUSE */
            if (errno == EINPROGRESS || errno == EALREADY || errno == EADDRINUSE) {
#if MACOSX || LINUX || FREEBSD
                do {
                    struct pollfd pfd;
                    pfd.fd = sp->fd;
                    pfd.events = POLLOUT;
                    rc = poll(&pfd, 1, MPR_TIMEOUT_SOCKETS);
                } while (rc < 0 && errno == EINTR);
#endif
                if (rc > 0) {
                    errno = EISCONN;
                }
            } 
            if (errno != EISCONN) {
                err = mprGetSocketError(sp);
                closesocket(sp->fd);
                sp->fd = -1;
                unlock(sp);
                mprFree(addr);
                return MPR_ERR_CANT_COMPLETE;
            }
        }
    }
    mprFree(addr);

    mprSetSocketBlockingMode(sp, (bool) (sp->flags & MPR_SOCKET_BLOCK));

    /*
     *  TCP/IP stacks have the no delay option (nagle algorithm) on by default.
     */
    if (sp->flags & MPR_SOCKET_NODELAY) {
        mprSetSocketNoDelay(sp, 1);
    }

    unlock(sp);
    return sp->fd;
}


/*
 *  Abortive disconnect. Thread-safe. (e.g. from a timeout or callback thread). This closes the underlying socket file
 *  descriptor but keeps the handler and socket object intact.
 */
void mprDisconnectSocket(MprSocket *sp)
{
    mprAssert(sp);
    mprAssert(sp->provider);

    if (sp->provider) {
        sp->provider->disconnectSocket(sp);
    }
}


static void disconnectSocket(MprSocket *sp)
{
    char    buf[16];

    /*
     *  Defensive lock buster. Use try lock incase an operation is blocked somewhere with a lock asserted. 
     *  Should never happen.
     */
    if (!mprTryLock(sp->mutex)) {
        return;
    }
    if (sp->fd >= 0 && !(sp->flags & MPR_SOCKET_EOF)) {
        /*
         *  Read any outstanding read data to minimize resets. Then do a shutdown to send a FIN and read 
         *  outstanding data.  All non-blocking.
         */
        mprSetSocketBlockingMode(sp, 0);
        while (recv(sp->fd, buf, sizeof(buf), 0) > 0) {
            ;
        }
        shutdown(sp->fd, SHUT_RDWR);
        sp->flags |= MPR_SOCKET_EOF;
        if (sp->handler) {
            mprRecallWaitHandler(sp->handler);
        }
    }
    unlock(sp);
}


/*
 *  Close a socket
 */
void mprCloseSocket(MprSocket *sp, bool gracefully)
{
    mprAssert(sp);
    mprAssert(sp->provider);

    if (sp->provider == 0) {
        return;
    }
    sp->provider->closeSocket(sp, gracefully);
}


/*
 *  Standard (non-SSL) close. Permit multiple calls.
 */
static void closeSocket(MprSocket *sp, bool gracefully)
{
    MprSocketService    *ss;
    MprWaitService      *waitService;
    MprTime             timesUp;
    char                buf[16];

    waitService = mprGetMpr(sp)->waitService;
    ss = mprGetMpr(sp)->socketService;

    lock(sp);

    if (sp->flags & MPR_SOCKET_CLOSED) {
        unlock(sp);
        return;
    }
    sp->flags |= MPR_SOCKET_CLOSED | MPR_SOCKET_EOF;

    if (sp->handler) {
        mprFree(sp->handler);
        sp->handler = 0;
    }

    if (sp->fd >= 0) {
        /*
         *  Read any outstanding read data to minimize resets. Then do a shutdown to send a FIN and read outstanding 
         *  data. All non-blocking.
         */
        if (gracefully) {
            mprSetSocketBlockingMode(sp, 0);
            while (recv(sp->fd, buf, sizeof(buf), 0) > 0) {
                ;
            }
        }
        if (shutdown(sp->fd, SHUT_RDWR) == 0) {
            if (gracefully) {
                timesUp = mprGetTime(0) + MPR_TIMEOUT_LINGER;
                do {
                    if (recv(sp->fd, buf, sizeof(buf), 0) <= 0) {
                        break;
                    }
                } while (mprGetTime(0) < timesUp);
            }
        }
        closesocket(sp->fd);
        sp->fd = -1;
    }

    if (! (sp->flags & (MPR_SOCKET_LISTENER | MPR_SOCKET_CLIENT))) {
        mprLock(ss->mutex);
        if (--ss->numClients < 0) {
            ss->numClients = 0;
        }
        mprUnlock(ss->mutex);
    }
    unlock(sp);
}


/*
 *  Accept wait handler. May be called directly if single-threaded or on a worker thread.
 */
int mprAcceptProc(MprSocket *listen, int mask)
{
    if (listen->provider) {
        listen->provider->acceptSocket(listen, 1);
        return 0;
    }
    return 0;
}


/*
 *  Standard accept
 */
static MprSocket *acceptSocket(MprSocket *listen, bool invokeCallback)
{
    MprSocketService            *ss;
    MprSocket                   *nsp;
    struct sockaddr_storage     addrStorage;
    struct sockaddr             *addr;
    char                        clientIpAddr[MPR_MAX_IP_ADDR];
    socklen_t                   addrlen;
    int                         fd, port;

    if (listen->acceptCallback == 0) {
        return 0;
    }

    ss = mprGetMpr(listen)->socketService;
    addr = (struct sockaddr*) &addrStorage;
    addrlen = sizeof(addrStorage);

    fd = (int) accept(listen->fd, addr, &addrlen);
    if (fd < 0) {
        if (mprGetError() != EAGAIN) {
            mprLog(listen, 1, "socket: accept failed, errno %d", mprGetOsError());
        }
        mprEnableSocketEvents(listen);
        return 0;
    }
    mprAssert(addrlen <= sizeof(addrStorage));

    nsp = mprCreateSocket(ss, listen->ssl);
    if (nsp == 0) {
        closesocket(fd);
        mprEnableSocketEvents(listen);
        return 0;
    }
    nsp->fd = fd;

    /*
     *  Limit the number of simultaneous clients
     */
    mprLock(ss->mutex);
    if (++ss->numClients >= ss->maxClients) {
        mprUnlock(ss->mutex);
        mprLog(listen, 1, "Rejecting connection, too many client connections (%d)", ss->numClients);
        mprFree(nsp);
        mprEnableSocketEvents(listen);
        return 0;
    }
    mprUnlock(ss->mutex);

#if !BLD_WIN_LIKE && !VXWORKS
    fcntl(fd, F_SETFD, FD_CLOEXEC);     /* Prevent children inheriting this socket */
#endif

    nsp->ipAddr = listen->ipAddr;
    nsp->acceptData = listen->acceptData;
    nsp->ioData = listen->ioData;
    nsp->port = listen->port;
    nsp->acceptCallback = listen->acceptCallback;
    nsp->flags = listen->flags;
    nsp->flags &= ~MPR_SOCKET_LISTENER;
    nsp->listenSock = listen;

    mprSetSocketBlockingMode(nsp, (nsp->flags & MPR_SOCKET_BLOCK) ? 1: 0);

    if (nsp->flags & MPR_SOCKET_NODELAY) {
        mprSetSocketNoDelay(nsp, 1);
    }

    if (getSocketIpAddr(nsp, addr, addrlen, clientIpAddr, sizeof(clientIpAddr), &port) != 0) {
        mprAssert(0);
        mprFree(nsp);
        mprEnableSocketEvents(listen);
        return 0;
    }
    nsp->clientIpAddr = mprStrdup(nsp, clientIpAddr);

    if (invokeCallback) {
        /*
         *  Call the user accept callback. We do not remember the socket handle, it is up to the callback to manage it
         *  from here on. The callback can delete the socket.
         */
        if (nsp->acceptCallback) {
            if ((nsp->acceptCallback)(nsp, nsp->acceptData, clientIpAddr, port) != 0) {
                return 0;
            }
        } else {
            mprFree(nsp);
            mprEnableSocketEvents(listen);
            return 0;
        }
    }
    return nsp;
}


/*
 *  Read data. Return zero for EOF or no data if in non-blocking mode. Return -1 for errors. On success,
 *  return the number of bytes read. Use getEof to tell if we are EOF or just no data (in non-blocking mode).
 */
int mprReadSocket(MprSocket *sp, void *buf, int bufsize)
{
    mprAssert(sp);
    mprAssert(buf);
    mprAssert(bufsize > 0);
    mprAssert(sp->provider);

    if (sp->provider == 0) {
        return MPR_ERR_NOT_INITIALIZED;
    }
    return sp->provider->readSocket(sp, buf, bufsize);
}


/*
 *  Standard read from a socket (Non SSL)
 */
static int readSocket(MprSocket *sp, void *buf, int bufsize)
{
    struct sockaddr_storage server;
    socklen_t               len;
    int                     bytes, errCode;

    mprAssert(buf);
    mprAssert(bufsize > 0);
    mprAssert(~(sp->flags & MPR_SOCKET_CLOSED));

    lock(sp);

    if (sp->flags & MPR_SOCKET_EOF) {
        unlock(sp);
        return 0;
    }
again:
    if (sp->flags & MPR_SOCKET_DATAGRAM) {
        len = sizeof(server);
        bytes = recvfrom(sp->fd, buf, bufsize, MSG_NOSIGNAL, (struct sockaddr*) &server, (socklen_t*) &len);
    } else {
        bytes = recv(sp->fd, buf, bufsize, MSG_NOSIGNAL);
    }

    if (bytes < 0) {
        errCode = mprGetSocketError(sp);
        if (errCode == EINTR) {
            goto again;

        } else if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
            bytes = 0;                          /* No data available */

        } else if (errCode == ECONNRESET) {
            sp->flags |= MPR_SOCKET_EOF;        /* Disorderly disconnect */
            bytes = 0;

        } else {
            sp->flags |= MPR_SOCKET_EOF;        /* Some other error */
            bytes = -errCode;
        }

    } else if (bytes == 0) {                    /* EOF */
        sp->flags |= MPR_SOCKET_EOF;
    }

#if KEEP && FOR_SSL
    /*
     *  If there is more buffered data to read, then ensure the handler recalls us again even if there is no more IO events.
     */
    if (isBufferedData()) {
        if (sp->handler) {
            mprRecallWaitHandler(sp->handler);
        }
    }
#endif
    unlock(sp);
    return bytes;
}


/*
 *  Write data. Return the number of bytes written or -1 on errors. NOTE: this routine will return with a
 *  short write if the underlying socket can't accept any more data.
 */
int mprWriteSocket(MprSocket *sp, void *buf, int bufsize)
{
    mprAssert(sp);
    mprAssert(buf);
    mprAssert(bufsize > 0);
    mprAssert(sp->provider);

    if (sp->provider == 0) {
        return MPR_ERR_NOT_INITIALIZED;
    }
    return sp->provider->writeSocket(sp, buf, bufsize);
}


/*
 *  Standard write to a socket (Non SSL)
 */
static int writeSocket(MprSocket *sp, void *buf, int bufsize)
{
    struct sockaddr     *addr;
    socklen_t           addrlen;
    int                 family, protocol, sofar, errCode, len, written;

    mprAssert(buf);
    mprAssert(bufsize >= 0);
    mprAssert((sp->flags & MPR_SOCKET_CLOSED) == 0);

    lock(sp);

    if (sp->flags & (MPR_SOCKET_BROADCAST | MPR_SOCKET_DATAGRAM)) {
        if (mprGetSocketInfo(sp, sp->ipAddr, sp->port, &family, &protocol, &addr, &addrlen) < 0) {
            unlock(sp);
            return MPR_ERR_NOT_FOUND;
        }
    }
    if (sp->flags & MPR_SOCKET_EOF) {
        sofar = MPR_ERR_CANT_WRITE;
    } else {
        errCode = 0;
        len = bufsize;
        sofar = 0;
        while (len > 0) {
            unlock(sp);
            if ((sp->flags & MPR_SOCKET_BROADCAST) || (sp->flags & MPR_SOCKET_DATAGRAM)) {
                written = sendto(sp->fd, &((char*) buf)[sofar], len, MSG_NOSIGNAL, addr, addrlen);
            } else {
                written = send(sp->fd, &((char*) buf)[sofar], len, MSG_NOSIGNAL);
            }
            lock(sp);

            if (written < 0) {
                errCode = mprGetSocketError(sp);
                if (errCode == EINTR) {
                    continue;
                } else if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
#if BLD_WIN_LIKE
                    /*
                     *  Windows sockets don't support blocking I/O. So we simulate here
                     */
                    if (sp->flags & MPR_SOCKET_BLOCK) {
                        mprSleep(sp, 0);
                        continue;
                    }
#endif
                    unlock(sp);
                    return sofar;
                }
                unlock(sp);
                return -errCode;
            }
            len -= written;
            sofar += written;
        }
    }
    unlock(sp);
    return sofar;
}


/*
 *  Write a string to the socket
 */
int mprWriteSocketString(MprSocket *sp, cchar *str)
{
    return mprWriteSocket(sp, (void*) str, (int) strlen(str));
}


int mprWriteSocketVector(MprSocket *sp, MprIOVec *iovec, int count)
{
    char    *start;
    int     total, len, i, written;

#if BLD_UNIX_LIKE
    if (sp->ssl == 0) {
        return writev(sp->fd, (const struct iovec*) iovec, count);
    } else
#endif
    {
        if (count <= 0) {
            return 0;
        }

        start = iovec[0].start;
        len = (int) iovec[0].len;
        mprAssert(len > 0);

        for (total = i = 0; i < count; ) {
            written = mprWriteSocket(sp, start, len);
            if (written < 0) {
                return written;

            } else if (written == 0) {
                break;

            } else {
                len -= written;
                start += written;
                total += written;
                if (len <= 0) {
                    i++;
                    start = iovec[i].start;
                    len = (int) iovec[i].len;
                }
            }
        }
        return total;
    }
}


#if !BLD_FEATURE_ROMFS
#if !LINUX || __UCLIBC__
static int localSendfile(MprSocket *sp, MprFile *file, MprOffset offset, int len)
{
    char    buf[MPR_BUFSIZE];

    mprSeek(file, SEEK_SET, (int) offset);
    len = min(len, sizeof(buf));
    if ((len = mprRead(file, buf, len)) < 0) {
        mprAssert(0);
        return MPR_ERR_CANT_READ;
    }
    return mprWriteSocket(sp, buf, len);
}
#endif


/*
 *  Write data from a file to a socket. Includes the ability to write header before and after the file data.
 *  Works even with a null "file" to just output the headers.
 */
MprOffset mprSendFileToSocket(MprSocket *sock, MprFile *file, MprOffset offset, int bytes, MprIOVec *beforeVec, 
    int beforeCount, MprIOVec *afterVec, int afterCount)
{
#if MACOSX && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
    struct sf_hdtr  def;
#endif
    off_t           written, off;
    int             rc, i, done, toWriteBefore, toWriteAfter, toWriteFile;

    rc = 0;

#if MACOSX && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
    written = bytes;
    def.hdr_cnt = beforeCount;
    def.headers = (beforeCount > 0) ? (struct iovec*) beforeVec: 0;
    def.trl_cnt = afterCount;
    def.trailers = (afterCount > 0) ? (struct iovec*) afterVec: 0;

    if (file && file->fd >= 0) {
        rc = sendfile(file->fd, sock->fd, offset, &written, &def, 0);
    } else
#else
    if (1) 
#endif
    {
        /*
         *  Either !MACOSX or no file is opened
         */
        done = written = 0;
        for (i = toWriteBefore = 0; i < beforeCount; i++) {
            toWriteBefore += (int) beforeVec[i].len;
        }
        for (i = toWriteAfter = 0; i < afterCount; i++) {
            toWriteAfter += (int) afterVec[i].len;
        }
        toWriteFile = bytes - toWriteBefore - toWriteAfter;
        mprAssert(toWriteFile >= 0);

        /*
         *  Linux sendfile does not have the integrated ability to send headers. Must do it separately here.
         *  I/O requests may return short (write fewer than requested bytes).
         */
        if (beforeCount > 0) {
            rc = mprWriteSocketVector(sock, beforeVec, beforeCount);
            if (rc > 0) {
                written += rc;
            }
            if (rc != toWriteBefore) {
                done++;
            }
        }

        if (!done && toWriteFile > 0) {
            off = (off_t) offset;
#if LINUX && !__UCLIBC__
            rc = sendfile(sock->fd, file->fd, &off, toWriteFile);
#else
            rc = localSendfile(sock, file, offset, toWriteFile);
#endif
            if (rc > 0) {
                written += rc;
                if (rc != toWriteFile) {
                    done++;
                }
            }
        }
        if (!done && afterCount > 0) {
            rc = mprWriteSocketVector(sock, afterVec, afterCount);
            if (rc > 0) {
                written += rc;
            }
        }
    }

    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return written;
        }
        return -1;
    }
    return written;
}
#endif /* !BLD_FEATURE_ROMFS */


static int flushSocket(MprSocket *sp)
{
    return 0;
}


int mprFlushSocket(MprSocket *sp)
{
    if (sp->provider == 0) {
        return MPR_ERR_NOT_INITIALIZED;
    }
    return sp->provider->flushSocket(sp);
}


bool mprHasSocketPendingData(MprSocket *sp)
{
    return (sp->flags & MPR_SOCKET_PENDING) ? 1 : 0;
}

/*
 *  Return true if end of file
 */
bool mprIsSocketEof(MprSocket *sp)
{
    return ((sp->flags & MPR_SOCKET_EOF) != 0);
}


/*
 *  Set the EOF condition
 */
void mprSetSocketEof(MprSocket *sp, bool eof)
{
    if (eof) {
        sp->flags |= MPR_SOCKET_EOF;
    } else {
        sp->flags &= ~MPR_SOCKET_EOF;
    }
}


/*
 *  Define an IO callback for this socket. The callback called whenever there is an event of interest as
 *  defined by handlerMask (MPR_SOCKET_READABLE, ...)
 *  If a handler is already defined, just enable events.
 */
void mprSetSocketCallback(MprSocket *sp, MprSocketProc fn, void *data, int handlerMask, int pri)
{
    lock(sp);
    sp->handlerMask = handlerMask;
    if (sp->handler == 0) {
        sp->ioCallback = fn;
        sp->ioData = data;
        sp->handlerPriority = pri;
        sp->handler = mprCreateWaitHandler(sp, sp->fd, sp->handlerMask, (MprWaitProc) ioProc, sp, sp->handlerPriority, 
            (sp->flags & MPR_SOCKET_THREAD) ? MPR_WAIT_THREAD : 0);
    } else {
        mprSetWaitEvents(sp->handler, handlerMask, -1);
    }
    unlock(sp);
}


/*
 *  Define the events of interest. Must only be called with a locked socket.
 */
void mprSetSocketEventMask(MprSocket *sp, int handlerMask)
{
    lock(sp);
    sp->handlerMask = handlerMask;
    if (handlerMask) {
        if (sp->handler) {
            mprSetWaitEvents(sp->handler, handlerMask, -1);

        } else {
            sp->handler = mprCreateWaitHandler(sp, sp->fd, handlerMask, (MprWaitProc) ioProc, sp, sp->handlerPriority, 
                (sp->flags & MPR_SOCKET_THREAD) ? MPR_WAIT_THREAD : 0);
        }

    } else if (sp->handler) {
        mprSetWaitEvents(sp->handler, handlerMask, -1);
    }
    unlock(sp);
}


void mprEnableSocketEvents(MprSocket *sp)
{
    mprAssert(sp);

    lock(sp);
    if (sp->handler) {
        mprEnableWaitEvents(sp->handler);
    }
    unlock(sp);
}


void mprDisableSocketEvents(MprSocket *sp)
{
    mprAssert(sp);

    lock(sp);
    if (sp->handler) {
        mprDisableWaitEvents(sp->handler);
    }
    unlock(sp);
}

/*
 *  Return the O/S socket file handle
 */
int mprGetSocketFd(MprSocket *sp)
{
    return sp->fd;
}


/*
 *  Return the blocking mode of the socket
 */
bool mprGetSocketBlockingMode(MprSocket *sp)
{
    return sp->flags & MPR_SOCKET_BLOCK;
}


/*
 *  Get the socket flags
 */
int mprGetSocketFlags(MprSocket *sp)
{
    return sp->flags;
}


/*
 *  Set whether the socket blocks or not on read/write
 */
int mprSetSocketBlockingMode(MprSocket *sp, bool on)
{
    int     flag, oldMode;

    lock(sp);
    oldMode = sp->flags & MPR_SOCKET_BLOCK;

    sp->flags &= ~(MPR_SOCKET_BLOCK);
    if (on) {
        sp->flags |= MPR_SOCKET_BLOCK;
    }

    flag = (sp->flags & MPR_SOCKET_BLOCK) ? 0 : 1;

#if BLD_WIN_LIKE
    ioctlsocket(sp->fd, FIONBIO, (ulong*) &flag);
#elif VXWORKS
    ioctl(sp->fd, FIONBIO, (int) &flag);
#else
    flag = 0;
    if (on) {
        fcntl(sp->fd, F_SETFL, fcntl(sp->fd, F_GETFL) & ~O_NONBLOCK);
    } else {
        fcntl(sp->fd, F_SETFL, fcntl(sp->fd, F_GETFL) | O_NONBLOCK);
    }
#endif
    unlock(sp);
    return oldMode;
}


/*
 *  Set the TCP delay behavior (nagle algorithm)
 */
int mprSetSocketNoDelay(MprSocket *sp, bool on)
{
    int     oldDelay;

    lock(sp);

    oldDelay = sp->flags & MPR_SOCKET_NODELAY;
    if (on) {
        sp->flags |= MPR_SOCKET_NODELAY;
    } else {
        sp->flags &= ~(MPR_SOCKET_NODELAY);
    }
#if BLD_WIN_LIKE
    {
        BOOL    noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->fd, IPPROTO_TCP, TCP_NODELAY, (FAR char*) &noDelay, sizeof(BOOL));
    }
#else
    {
        int     noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->fd, IPPROTO_TCP, TCP_NODELAY, (char*) &noDelay, sizeof(int));
    }
#endif /* BLD_WIN_LIKE */

    unlock(sp);
    return oldDelay;
}


/*
 *  Get the port number
 */
int mprGetSocketPort(MprSocket *sp)
{
    return sp->port;
}


/*
 *  IO ready handler. May be called directly if single-threaded or on a worker thread.
 */
static int ioProc(MprSocket *sp, int mask)
{
    int     rc;

    lock(sp);
    if (sp->ioCallback == 0 || (sp->handlerMask & mask) == 0) {
        mprAssert(sp->handlerMask & mask);
        if ((sp->handlerMask & mask) == 0) {
            mprLog(sp, 0, "ioProc: Spurious event. callback %x, handlerMask %x mask %x", sp->ioCallback, 
                sp->handlerMask, mask);
        }
        unlock(sp);
        return 0;
    }
    mask &= sp->handlerMask;
    if (sp->flags & MPR_SOCKET_RUNNING) {
        return 0;
    }
    sp->flags |= MPR_SOCKET_RUNNING;
    unlock(sp);

    rc = (sp->ioCallback)(sp->ioData, mask);

    if (rc == 0) {
        lock(sp);
        sp->flags &= ~MPR_SOCKET_RUNNING;
        unlock(sp);
    }
    return rc;
}


/*
 *  Map the O/S error code to portable error codes.
 */
int mprGetSocketError(MprSocket *sp)
{
#if BLD_WIN_LIKE
    int     rc;
    switch (rc = WSAGetLastError()) {
    case WSAEINTR:
        return EINTR;

    case WSAENETDOWN:
        return ENETDOWN;

    case WSAEWOULDBLOCK:
        return EWOULDBLOCK;

    case WSAEPROCLIM:
        return EAGAIN;

    case WSAECONNRESET:
    case WSAECONNABORTED:
        return ECONNRESET;

    case WSAECONNREFUSED:
        return ECONNREFUSED;

    case WSAEADDRINUSE:
        return EADDRINUSE;
    default:
        return EINVAL;
    }
#else
    return errno;
#endif
}


#if BLD_HAS_GETADDRINFO
/*
 *  Get a socket address from a host/port combination. If a host provides both IPv4 and IPv6 addresses, 
 *  prefer the IPv4 address.
 */
int mprGetSocketInfo(MprCtx ctx, cchar *host, int port, int *family, int *protocol, struct sockaddr **addr, 
    socklen_t *addrlen)
{
    MprSocketService    *ss;
    struct addrinfo     hints, *res, *r;
    char                portBuf[MPR_MAX_IP_PORT];
    int                 v6;

    mprAssert(ctx);
    mprAssert(host);
    mprAssert(addr);

    ss = mprGetMpr(ctx)->socketService;

    mprLock(ss->mutex);
    memset((char*) &hints, '\0', sizeof(hints));

    /*
     *  Note that IPv6 does not support broadcast, there is no 255.255.255.255 equivalent.
     *  Multicast can be used over a specific link, but the user must provide that address plus %scope_id.
     */
    if (host == 0 || strcmp(host, "") == 0) {
        host = 0;
        hints.ai_flags |= AI_PASSIVE;           /* Bind to 0.0.0.0 and :: */
    }
    v6 = ipv6(host);
    hints.ai_socktype = SOCK_STREAM;
    if (host) {
        hints.ai_family = v6 ? AF_INET6 : AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    mprItoa(portBuf, sizeof(portBuf), port, 10);

#if UNUSED
    if (host == NULL || strchr(host, ':') == 0) {
        /* 
            Looks like IPv4. Map localhost to 127.0.0.1 to avoid crash bug in MAC OS X.
         */
        if (host && strcmp(host, "localhost") == 0) {
            host = "127.0.0.1";
        }
    }
#endif
    res = 0;
    if (getaddrinfo(host, portBuf, &hints, &res) != 0) {
        mprUnlock(ss->mutex);
        return MPR_ERR_CANT_OPEN;
    }
    /*
        Prefer IPv4 if IPv6 not requested
     */
    for (r = res; r; r = r->ai_next) {
        if (v6) {
            if (r->ai_family == AF_INET6) {
                break;
            }
        } else {
            if (r->ai_family == AF_INET) {
                break;
            }
        }
    }
    if (r == NULL) {
        r = res;
    }
    *addr = (struct sockaddr*) mprAllocObjZeroed(ctx, struct sockaddr_storage);
    mprMemcpy((char*) *addr, sizeof(struct sockaddr_storage), (char*) r->ai_addr, (int) r->ai_addrlen);

    *addrlen = (int) r->ai_addrlen;
    *family = r->ai_family;
    *protocol = r->ai_protocol;

    freeaddrinfo(res);
    mprUnlock(ss->mutex);
    return 0;
}


#elif MACOSX
int mprGetSocketInfo(MprCtx ctx, cchar *host, int port, int *family, int *protocol, struct sockaddr **addr, 
    socklen_t *addrlen)
{
    MprSocketService    *ss;
    struct hostent      *hostent;
    struct sockaddr_in  *sa;
    struct sockaddr_in6 *sa6;
    int                 len, err;

    mprAssert(addr);
    ss = mprGetMpr(ctx)->socketService;

    mprLock(ss->mutex);
    len = sizeof(struct sockaddr_in);
    if ((hostent = getipnodebyname(host, AF_INET, 0, &err)) == NULL) {
        len = sizeof(struct sockaddr_in6);
        if ((hostent = getipnodebyname(host, AF_INET6, 0, &err)) == NULL) {
            mprUnlock(ss->mutex);
            return MPR_ERR_CANT_OPEN;
        }
        sa6 = (struct sockaddr_in6*) mprAllocZeroed(ctx, len);
        if (sa6 == 0) {
            mprUnlock(ss->mutex);
            return MPR_ERR_NO_MEMORY;
        }
        memcpy((char*) &sa6->sin6_addr, (char*) hostent->h_addr_list[0], (size_t) hostent->h_length);
        sa6->sin6_family = hostent->h_addrtype;
        sa6->sin6_port = htons((short) (port & 0xFFFF));
        *addr = (struct sockaddr*) sa6;

    } else {
        sa = (struct sockaddr_in*) mprAllocZeroed(ctx, len);
        if (sa == 0) {
            mprUnlock(ss->mutex);
            return MPR_ERR_NO_MEMORY;
        }
        memcpy((char*) &sa->sin_addr, (char*) hostent->h_addr_list[0], (size_t) hostent->h_length);
        sa->sin_family = hostent->h_addrtype;
        sa->sin_port = htons((short) (port & 0xFFFF));
        *addr = (struct sockaddr*) sa;
    }

    mprAssert(hostent);
    *addrlen = len;
    *family = hostent->h_addrtype;
    *protocol = 0;
    freehostent(hostent);

    mprUnlock(ss->mutex);
    return 0;
}


#else

int mprGetSocketInfo(MprCtx ctx, cchar *host, int port, int *family, int *protocol, struct sockaddr **addr, 
    socklen_t *addrlen)
{
    MprSocketService    *ss;
    struct sockaddr_in  *sa;

    ss = mprGetMpr(ctx)->socketService;

    sa = mprAllocObjZeroed(ctx, struct sockaddr_in);
    if (sa == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    memset((char*) sa, '\0', sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port = htons((short) (port & 0xFFFF));

    if (strcmp(host, "") != 0) {
        sa->sin_addr.s_addr = inet_addr((char*) host);
    } else {
        sa->sin_addr.s_addr = INADDR_ANY;
    }

    /*
     *  gethostbyname is not thread safe on some systems
     */
    mprLock(ss->mutex);
    if (sa->sin_addr.s_addr == INADDR_NONE) {
#if VXWORKS
        /*
         *  VxWorks only supports one interface and this code only supports IPv4
         */
        sa->sin_addr.s_addr = (ulong) hostGetByName((char*) host);
        if (sa->sin_addr.s_addr < 0) {
            mprUnlock(ss->mutex);
            mprAssert(0);
            return 0;
        }
#else
        struct hostent *hostent;
        hostent = gethostbyname2(host, AF_INET);
        if (hostent == 0) {
            hostent = gethostbyname2(host, AF_INET6);
            if (hostent == 0) {
                mprUnlock(ss->mutex);
                return MPR_ERR_NOT_FOUND;
            }
            protocol = AF_INET6;
        }
        memcpy((char*) &sa->sin_addr, (char*) hostent->h_addr_list[0], (size_t) hostent->h_length);
#endif
    }
    *addr = (struct sockaddr*) sa;
    *addrlen = sizeof(struct sockaddr_in);
    *family = sa->sin_family;
    *protocol = 0;
    mprUnlock(ss->mutex);
    return 0;
}
#endif


/*
 *  Return a numerical IP address and port for the given socket info
 */
static int getSocketIpAddr(MprCtx ctx, struct sockaddr *addr, int addrlen, char *host, int hostLen, int *port)
{
#if (BLD_UNIX_LIKE || WIN)
    char    service[NI_MAXSERV];

    if (getnameinfo(addr, addrlen, host, hostLen, service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN)) {
        return MPR_ERR_BAD_VALUE;
    }
    *port = atoi(service);

#else
    struct sockaddr_in  *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, host, hostLen);
#else
    uchar   *cp;
    sa = (struct sockaddr_in*) addr;
    cp = (uchar*) &sa->sin_addr;
    mprSprintf(host, hostLen, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
#endif
    *port = ntohs(sa->sin_port);
#endif
    return 0;
}


static int ipv6(cchar *ip)
{
    cchar   *cp;
    int     colons;

    if (ip == 0 || *ip == 0) {
        return 0;
    }
    colons = 0;
    for (cp = (char*) ip; ((*cp != '\0') && (colons < 2)) ; cp++) {
        if (*cp == ':') {
            colons++;
        }
    }
    return colons >= 2;
}


/*
 *  Parse ipAddrPort and return the IP address and port components. Handles ipv4 and ipv6 addresses. When an ipAddrPort
 *  contains an ipv6 port it should be written as
 *
 *      aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii
 *  or
 *      [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii]:port
 */
int mprParseIp(MprCtx ctx, cchar *ipAddrPort, char **ipAddrRef, int *port, int defaultPort)
{
    char    *ipAddr;
    char    *cp;

    ipAddr = NULL;
    if (defaultPort < 0) {
        defaultPort = 80;
    }
    if ((cp = strstr(ipAddrPort, "://")) != 0) {
        ipAddrPort = &cp[3];
    }
    if (ipv6(ipAddrPort)) {
        /*
         *  IPv6. If port is present, it will follow a closing bracket ']'
         */
        if ((cp = strchr(ipAddrPort, ']')) != 0) {
            cp++;
            if ((*cp) && (*cp == ':')) {
                *port = (*++cp == '*') ? -1 : atoi(cp);

                /* set ipAddr to ipv6 address without brackets */
                ipAddr = mprStrdup(ctx, ipAddrPort+1);
                cp = strchr(ipAddr, ']');
                *cp = '\0';

            } else {
                /* Handles [a:b:c:d:e:f:g:h:i] case (no port)- should not occur */
                ipAddr = mprStrdup(ctx, ipAddrPort+1);
                cp = strchr(ipAddr, ']');
                *cp = '\0';

                /* No port present, use callers default */
                *port = defaultPort;
            }
        } else {
            /* Handles a:b:c:d:e:f:g:h:i case (no port) */
            ipAddr = mprStrdup(ctx, ipAddrPort);

            /* No port present, use callers default */
            *port = defaultPort;
        }

    } else {
        /* 
         *  ipv4 
         */
        ipAddr = mprStrdup(ctx, ipAddrPort);

        if ((cp = strchr(ipAddr, ':')) != 0) {
            *cp++ = '\0';
            if (*cp == '*') {
                *port = -1;
            } else {
                *port = atoi(cp);
            }
            if (*ipAddr == '*') {
                mprFree(ipAddr);
                ipAddr = mprStrdup(ctx, "127.0.0.1");
            }

        } else {
            if (isdigit((int) *ipAddr)) {
                *port = atoi(ipAddr);
                mprFree(ipAddr);
                ipAddr = mprStrdup(ctx, "127.0.0.1");

            } else {
                /* No port present, use callers default */
                *port = defaultPort;
            }
        }
    }

    if (ipAddrRef) {
        *ipAddrRef = ipAddr;
    }

    return 0;
}


bool mprIsSocketSecure(MprSocket *sp)
{
    return sp->sslSocket != 0;
}


void mprSetSocketPrebindCallback(MprCtx ctx, MprSocketPrebind callback)
{
    mprGetMpr(ctx)->socketService->prebind = callback;
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprSocket.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprString.c"
 */
/************************************************************************/

/**
 *  mprString.c - String routines safe for embedded programming
 *
 *  This module provides safe replacements for the standard string library. 
 *
 *  Most routines in this file are not thread-safe. It is the callers responsibility to perform all thread 
 *  synchronization.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




int mprStrcpy(char *dest, int destMax, cchar *src)
{
    int     len;

    mprAssert(dest);
    mprAssert(destMax >= 0);
    mprAssert(src);
    mprAssert(src != dest);

    len = (int) strlen(src);
    if (destMax > 0 && len >= destMax && len > 0) {
        return MPR_ERR_WONT_FIT;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    } 
    return len;
}


int mprStrcpyCount(char *dest, int destMax, cchar *src, int count)
{
    int     len;

    mprAssert(dest);
    mprAssert(destMax >= 0);
    mprAssert(src);
    mprAssert(src != dest);

    len = (int) strlen(src);
    len = min(len, count);

    if (destMax > 0 && len >= destMax && len > 0) {
        return MPR_ERR_WONT_FIT;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    } 
    return len;
}


#if !BREW

int mprMemcmp(const void *s1, int s1Len, const void *s2, int s2Len)
{
    int     len, rc;

    mprAssert(s1);
    mprAssert(s2);
    mprAssert(s1Len >= 0);
    mprAssert(s2Len >= 0);

    len = min(s1Len, s2Len);

    rc = memcmp(s1, s2, len);
    if (rc == 0) {
        if (s1Len < s2Len) {
            return -1;
        } else if (s1Len > s2Len) {
            return 1;
        }
    }
    return rc;
}

#endif


/*
 *  Supports insitu copy where src and destination overlap
 */
int mprMemcpy(void *dest, int destMax, const void *src, int nbytes)
{
    mprAssert(dest);
    mprAssert(destMax <= 0 || destMax >= nbytes);
    mprAssert(src);
    mprAssert(nbytes >= 0);

    if (destMax > 0 && nbytes > destMax) {
        mprAssert(0);
        return MPR_ERR_WONT_FIT;
    }
    if (nbytes > 0) {
        memmove(dest, src, nbytes);
        return nbytes;
    } else {
        return 0;
    }
}


char *mprStrcatV(MprCtx ctx, int destMax, cchar *src, va_list args)
{
    va_list     ap;
    char        *dest, *str, *dp;
    int         required;

    mprAssert(ctx);
    mprAssert(src);

    if (destMax <= 0) {
        destMax = INT_MAX;
    }

#ifdef __va_copy
    __va_copy(ap, args);
#else
    ap = args;
#endif

    required = 1;
    str = (char*) src;

    while (str) {
        required += (int) strlen(str);
        str = va_arg(ap, char*);
    }
    if (required >= destMax) {
        return 0;
    }

    if ((dest = (char*) mprAlloc(ctx, required)) == 0) {
        return 0;
    }

    dp = dest;
#ifdef __va_copy
    __va_copy(ap, args);
#else
    ap = args;
#endif
    str = (char*) src;
    while (str) {
        strcpy(dp, str);
        dp += (int) strlen(str);
        str = va_arg(ap, char*);
    }
    *dp = '\0';
    return dest;
}


char *mprStrcat(MprCtx ctx, int destMax, cchar *src, ...)
{
    va_list     ap;
    char        *result;

    mprAssert(ctx);
    mprAssert(src);

    va_start(ap, src);
    result = mprStrcatV(ctx, destMax, src, ap);
    va_end(ap);
    return result;
}


char *mprReallocStrcat(MprCtx ctx, int destMax, char *buf, cchar *src, ...)
{
    va_list     ap;
    char        *str, *dp;
    int         required, existingLen;

    mprAssert(ctx);
    mprAssert(src);

    va_start(ap, src);
    if (destMax <= 0) {
        destMax = INT_MAX;
    }

    existingLen = (buf) ? strlen(buf) : 0;
    required = existingLen + 1;

    str = (char*) src;
    while (str) {
        required += (int) strlen(str);
        str = va_arg(ap, char*);
    }
    if (required >= destMax) {
        return 0;
    }
    if ((buf = mprRealloc(ctx, (char*) buf, required)) == 0) {
        return 0;
    }
    dp = &buf[existingLen];

    va_end(ap);
    va_start(ap, src);

    str = (char*) src;
    while (str) {
        strcpy(dp, str);
        dp += (int) strlen(str);
        str = va_arg(ap, char*);
    }
    *dp = '\0';
    va_end(ap);
    return buf;
}


int mprStrlen(cchar *src, int max)
{
    int     len;

    len = (int) strlen(src);
    if (len >= max) {
        mprAssert(0);
        return MPR_ERR_WONT_FIT;
    }
    return len;
}


char *mprStrTrim(char *str, cchar *set)
{
    int     len, i;

    if (str == 0 || set == 0) {
        return str;
    }

    i = (int) strspn(str, set);
    str += i;

    len = (int) strlen(str);
    while (len > 0 && strspn(&str[len - 1], set) > 0) {
        str[len - 1] = '\0';
        len--;
    }
    return str;
}


/*  
 *  Map a string to lower case (overwrites original string)
 */
char *mprStrLower(char *str)
{
    char    *cp;

    mprAssert(str);

    if (str == 0) {
        return 0;
    }

    for (cp = str; *cp; cp++) {
        if (isupper((int) *cp)) {
            *cp = (char) tolower((int) *cp);
        }
    }
    return str;
}


/*  
 *  Map a string to upper case (overwrites buffer)
 */
char *mprStrUpper(char *str)
{
    char    *cp;

    mprAssert(str);
    if (str == 0) {
        return 0;
    }

    for (cp = str; *cp; cp++) {
        if (islower((int) *cp)) {
            *cp = (char) toupper((int) *cp);
        }
    }
    return str;
}


/*
 *  Case sensitive string comparison.
 */
int mprStrcmp(cchar *str1, cchar *str2)
{
    int     rc;

    if (str1 == 0) {
        return -1;
    }
    if (str2 == 0) {
        return 1;
    }
    if (str1 == str2) {
        return 0;
    }

    for (rc = 0; *str1 && *str2 && rc == 0; str1++, str2++) {
        rc = *str1 - *str2;
    }
    if (rc) {
        return rc < 0 ? -1 : 1;
    }
    if (*str1 == '\0' && *str2) {
        return -1;
    }
    if (*str2 == '\0' && *str1) {
        return 1;
    }
    return rc;
}


/*
 *  Case sensitive string comparison. Limited by length
 */
int mprStrcmpCount(cchar *str1, cchar *str2, int len)
{
    int     rc;

    if (str1 == 0 || str2 == 0) {
        return -1;
    }
    if (str1 == str2) {
        return 0;
    }

    for (rc = 0; len-- > 0 && *str1 && rc == 0; str1++, str2++) {
        rc = *str1 - *str2;
    }
    if (rc || len < 0) {
        return rc;
    } else if (*str1 == '\0' && *str2 == '\0') {
        return 0;
    } else if (*str1 == '\0') {
        return -1;
    } else if (*str2 == '\0') {
        return 1;
    }
    return 0;
}


int mprStrStartsWith(cchar *dest, cchar *pat)
{
    return mprStrcmpCount(dest, pat, strlen(pat)) == 0;
}


/*
 *  Case insensitive string comparison. Stop at the end of str1.
 */
int mprStrcmpAnyCase(cchar *str1, cchar *str2)
{
    int     rc;

    if (str1 == 0) {
        return -1;
    }
    if (str2 == 0) {
        return 1;
    }
    if (str1 == str2) {
        return 0;
    }
    for (rc = 0; *str1 && *str2 && rc == 0; str1++, str2++) {
        rc = tolower((int) *str1) - tolower((int) *str2);
    }
    if (rc) {
        return rc < 0 ? -1 : 1;
    } else if (*str1 == '\0' && *str2 == '\0') {
        return 0;
    } else if (*str1 == '\0') {
        return -1;
    } else if (*str2 == '\0') {
        return 1;
    }

    return 0;
}


/*
 *  Case insensitive string comparison. Limited by length
 */
int mprStrcmpAnyCaseCount(cchar *str1, cchar *str2, int len)
{
    int     rc;

    if (str1 == 0 || str2 == 0) {
        return -1;
    }
    if (str1 == str2) {
        return 0;
    }

    for (rc = 0; len-- > 0 && *str1 && rc == 0; str1++, str2++) {
        rc = tolower((int) *str1) - tolower((int) *str2);
    }
    if (rc || len < 0) {
        return rc;
    } else if (*str1 == '\0' && *str2 == '\0') {
        return 0;
    } else if (*str1 == '\0') {
        return -1;
    } else if (*str2 == '\0') {
        return 1;
    }
    return 0;
}


/*
 *  Thread-safe wrapping of strtok. Note "str" is modifed as per strtok()
 */
char *mprStrTok(char *str, cchar *delim, char **last)
{
    char    *start, *end;
    int     i;

    start = str ? str : *last;

    if (start == 0) {
        *last = 0;
        return 0;
    }
    
    i = (int) strspn(start, delim);
    start += i;
    if (*start == '\0') {
        *last = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = (int) strspn(end, delim);
        end += i;
    }
    *last = end;
    return start;
}


/*
 *  Split the buffer into word tokens
 */
char *mprGetWordTok(char *buf, int bufsize, cchar *str, cchar *delim, cchar **tok)
{
    cchar       *start, *end;
    int         i, len;

    start = str ? str : *tok;

    if (start == 0) {
        return 0;
    }
    
    i = (int) strspn(start, delim);
    start += i;
    if (*start =='\0') {
        *tok = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        len = min((int) (end - start), bufsize - 1);
        mprMemcpy(buf, bufsize, start, len);
        buf[len] = '\0';
    } else {
        if (mprStrcpy(buf, bufsize, start) < 0) {
            buf[bufsize - 1] = '\0';
            return 0;
        }
        buf[bufsize - 1] = '\0';
    }
    *tok = end;
    return buf;
}


/*
 *  Format a number as a string. Support radix 10 and 16.
 */
char *mprItoa(char *buf, int size, int64 value, int radix)
{
    char    numBuf[32];
    char    *cp, *dp, *endp;
    char    digits[] = "0123456789ABCDEF";
    int     negative;

    if (radix != 10 && radix != 16) {
        return 0;
    }

    cp = &numBuf[sizeof(numBuf)];
    *--cp = '\0';

    if (value < 0) {
        negative = 1;
        value = -value;
        size--;
    } else {
        negative = 0;
    }

    do {
        *--cp = digits[value % radix];
        value /= radix;
    } while (value > 0);

    if (negative) {
        *--cp = '-';
    }

    dp = buf;
    endp = &buf[size];
    while (dp < endp && *cp) {
        *dp++ = *cp++;
    }
    *dp++ = '\0';
    return buf;
}


/*
    Parse an ascii number. Supports radix 8, 10 or 16. If radix is <= 0, then the radix is sleuthed from the input.
    Supports formats:
        [(+|-)][0][OCTAL_DIGITS]
        [(+|-)][0][(x|X)][HEX_DIGITS]
        [(+|-)][DIGITS]

 */
int64 mprAtoi(cchar *str, int radix)
{
    int64   val;
    int     c, negative;

    mprAssert(radix <= 0 || radix == 8 || radix == 10 || radix == 16);

    if (str == 0) {
        return 0;
    }
    while (isspace((int) *str)) {
        str++;
    }
    val = 0;
    if (*str == '-') {
        negative = 1;
        str++;
    } else {
        negative = 0;
    }
    if (radix <= 0) {
        radix = 10;
        if (*str == '0') {
            if (tolower((int) str[1]) == 'x') {
                radix = 16;
                str += 2;
            } else {
                radix = 8;
                str++;
            }
        }
    } else if (radix == 16) {
        if (*str == '0' && tolower((int) str[1]) == 'x') {
            str += 2;
        }
    }

    if (radix == 16) {
        while (*str) {
            c = tolower((int) *str);
            if (isdigit(c)) {
                val = (val * radix) + c - '0';
            } else if (c >= 'a' && c <= 'f') {
                val = (val * radix) + c - 'a' + 10;
            } else {
                break;
            }
            str++;
        }
    } else {
        while (*str && isdigit((int) *str)) {
            val = (val * radix) + *str - '0';
            str++;
        }
    }
    return (negative) ? -val: val;
}


/*
 *  Make an argv array. Caller must free by calling mprFree(argv) to free everything.
 */
int mprMakeArgv(MprCtx ctx, cchar *program, cchar *cmd, int *argcp, char ***argvp)
{
    char        *cp, **argv, *buf, *args;
    int         size, argc;

    /*
     *  Allocate one buffer for argv and the actual args themselves
     */
    size = (int) strlen(cmd) + 1;

    buf = (char*) mprAlloc(ctx, (MPR_MAX_ARGC * sizeof(char*)) + size);
    if (buf == 0) {
        return MPR_ERR_NO_MEMORY;
    }

    args = &buf[MPR_MAX_ARGC * sizeof(char*)];
    strcpy(args, cmd);
    argv = (char**) buf;

    argc = 0;
    if (program) {
        argv[argc++] = (char*) mprStrdup(ctx, program);
    }

    for (cp = args; cp && *cp != '\0'; argc++) {
        if (argc >= MPR_MAX_ARGC) {
            mprAssert(argc < MPR_MAX_ARGC);
            mprFree(buf);
            *argvp = 0;
            if (argcp) {
                *argcp = 0;
            }
            return MPR_ERR_TOO_MANY;
        }
        while (isspace((int) *cp)) {
            cp++;
        }
        if (*cp == '\0')  {
            break;
        }
        if (*cp == '"') {
            cp++;
            argv[argc] = cp;
            while ((*cp != '\0') && (*cp != '"')) {
                cp++;
            }
        } else if (*cp == '\'') {
            cp++;
            argv[argc] = cp;
            while ((*cp != '\0') && (*cp != '\'')) {
                cp++;
            }
        } else {
            argv[argc] = cp;
            while (*cp != '\0' && !isspace((int) *cp)) {
                cp++;
            }
        }
        if (*cp != '\0') {
            *cp++ = '\0';
        }
    }
    argv[argc] = 0;

    if (argcp) {
        *argcp = argc;
    }
    *argvp = argv;

    return argc;
}


char *mprStrnstr(cchar *str, cchar *pattern, int len)
{
    cchar   *start, *p;
    int     i;

    if (str == 0 || pattern == 0 || len == 0) {
        return 0;
    }

    while (*str && len-- > 0) {
        if (*str++ == *pattern) {
            start = str - 1;
            for (p = pattern + 1, i = len; *p && *str && i >= 0; i--, p++) {
                if (*p != *str++) {
                    break;
                }
            }
            if (*p == '\0') {
                return (char*) start;
            }
        }
    }
    return 0;
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprString.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprTest.c"
 */
/************************************************************************/

/*
 *  mprTestLib.c - Embedthis Unit Test Framework Library
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_TEST

static void     adjustFailedCount(MprTestService *sp, int adj);
static void     adjustThreadCount(MprTestService *sp, int adj);
static void     buildFullNames(MprTestGroup *gp, cchar *runName);
static MprTestFailure *createFailure(MprTestGroup *gp, cchar *loc, cchar *message);
static MprTestGroup *createTestGroup(MprTestService *sp, MprTestDef *def, MprTestGroup *parent);
static bool     filterTestGroup(MprTestGroup *gp);
static bool     filterTestCast(MprTestGroup *gp, MprTestCase *tc);
static char     *getErrorMessage(MprTestGroup *gp);
static int      parseFilter(MprTestService *sp, cchar *str);
static void     runTestGroup(MprTestGroup *gp);
static void     runTestProc(MprTestGroup *gp, MprTestCase *test);
static void     runTestThread(MprList *groups, void *threadp);
static int      setLogging(Mpr *mpr, char *logSpec);

#if BLD_FEATURE_MULTITHREAD
static MprList  *copyGroups(MprTestService *sp, MprList *groups);
#endif


MprTestService *mprCreateTestService(MprCtx ctx)
{
    MprTestService      *sp;

    sp = mprAllocObjZeroed(ctx, MprTestService);
    if (sp == 0) {
        return 0;
    }

    sp->iterations = 1;
    sp->numThreads = 1;
    sp->workers = 0;
    sp->testFilter = mprCreateList(sp);
    sp->groups = mprCreateList(sp);
    sp->start = mprGetTime(sp);

#if BLD_FEATURE_MULTITHREAD
    sp->mutex = mprCreateLock(sp);
#endif
    return sp;
}


int mprParseTestArgs(MprTestService *sp, int argc, char *argv[])
{
    Mpr         *mpr;
    cchar       *programName;
    char        *argp, *logSpec;
    int         err, i, depth, nextArg, outputVersion;

    i = 0;
    err = 0;
    outputVersion = 0;
    logSpec = "stderr:1";

    mpr = mprGetMpr(sp);
    programName = mprGetPathBase(mpr, argv[0]);

    sp->name = BLD_PRODUCT;

    /*
     *  Save the command line
     */
    sp->commandLine = mprStrcat(sp, -1, mprGetPathBase(mpr, argv[i++]), NULL);
    for (; i < argc; i++) {
        sp->commandLine = mprReallocStrcat(sp, -1, sp->commandLine, " ", argv[i], NULL);
    }

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];

        if (strcmp(argp, "--continue") == 0) {
            sp->continueOnFailures = 1; 

        } else if (strcmp(argp, "--depth") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                depth = atoi(argv[++nextArg]);
                if (depth < 0 || depth > 10) {
                    mprError(sp, "Bad test depth %d, (range 0-9)", depth);
                    err++;
                } else {
                    sp->testDepth = depth;
                }
            }

        } else if (strcmp(argp, "--debug") == 0 || strcmp(argp, "-d") == 0) {
            mprSetDebugMode(mpr, 1);
            sp->debugOnFailures = 1;

        } else if (strcmp(argp, "--echo") == 0) {
            sp->echoCmdLine = 1;

        } else if (strcmp(argp, "--filter") == 0 || strcmp(argp, "-f") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                if (parseFilter(sp, argv[++nextArg]) < 0) {
                    err++;
                }
            }

        } else if (strcmp(argp, "--iterations") == 0 || (strcmp(argp, "-i") == 0)) {
            if (nextArg >= argc) {
                err++;
            } else {
                sp->iterations = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--log") == 0 || strcmp(argp, "-l") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                setLogging(mpr, argv[++nextArg]);
            }

        } else if (strcmp(argp, "--name") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                sp->name = argv[++nextArg];
            }

        } else if (strcmp(argp, "--step") == 0 || strcmp(argp, "-s") == 0) {
            sp->singleStep = 1; 

        } else if (strcmp(argp, "--threads") == 0 || strcmp(argp, "-t") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                i = atoi(argv[++nextArg]);
                if (i <= 0 || i > 100) {
                    mprError(sp, "%s: Bad number of threads (1-100)", programName);
                    return MPR_ERR_BAD_ARGS;
                }
#if BLD_FEATURE_MULTITHREAD
                sp->numThreads = i;
#else
                if (i > 1) {
                    mprLog(sp, 0, "%s: Program built single-threaded. Ignoring threads directive", programName);
                }
#endif
            }

        } else if (strcmp(argp, "--verbose") == 0 || strcmp(argp, "-v") == 0) {
            sp->verbose++;

        } else if (strcmp(argp, "--version") == 0 || strcmp(argp, "-V") == 0) {
            outputVersion++;

        } else if (strcmp(argp, "--workers") == 0 || strcmp(argp, "-w") == 0) {
            if (nextArg >= argc) {
                err++;
            } else {
                i = atoi(argv[++nextArg]);
                if (i < 0 || i > 100) {
                    mprError(sp, "%s: Bad number of worker threads (0-100)", programName);
                    return MPR_ERR_BAD_ARGS;
                }
#if BLD_FEATURE_MULTITHREAD
                sp->workers = i;
#else
                if (i > 1) {
                    mprLog(sp, 0, "%s: Program built single-threaded. Ignoring threads directive", programName);
                }
#endif
            }

        } else if (strcmp(argp, "-?") == 0 || (strcmp(argp, "--help") == 0 || strcmp(argp, "--?") == 0)) {
            err++;

        } else {
            /* Ignore unknown args */
        }
    }

    if (sp->workers == 0) {
        sp->workers = 2 + sp->numThreads * 2;
    }
#if LOAD_TEST_PACKAGES
    /* Must be at least one test module to load */
    if (nextArg >= argc) {
        err++;
    }
#endif

    if (err) {
        mprPrintfError(mpr, 
        "usage: %s [options]\n"
        "    --continue            # Continue on errors\n"
        "    --depth number        # Zero == basic, 1 == throrough, 2 extensive\n"
        "    --debug               # Run in debug mode\n"
        "    --echo                # Echo the command line\n"
        "    --filter pattern      # Filter tests by pattern x.y.z...\n"
        "    --iterations count    # Number of iterations to run the test\n"
        "    --log logFile:level   # Log to file file at verbosity level\n"
        "    --name testName       # Set test name\n"
        "    --step                # Single step tests\n"
        "    --threads count       # Number of test threads\n"
        "    --verbose             # Verbose mode\n"
        "    --version             # Output version information\n"
        "    --workers count       # Set maximum worker threads\n\n",
        programName);
        return MPR_ERR_BAD_ARGS;
    }

    if (outputVersion) {
        mprPrintfError(mpr, "%s: Version: %s\n", BLD_NAME, BLD_VERSION);
        mprFree(mpr);
        return MPR_ERR_BAD_ARGS;
    }

    sp->argc = argc;
    sp->argv = argv;
    sp->firstArg = nextArg;

#if LOAD_TEST_PACKAGES
    for (i = nextArg; i < argc; i++) {
        if (loadModule(sp, argv[i]) < 0) {
            return MPR_ERR_CANT_OPEN;
        }
    }
#endif
    
#if BLD_FEATURE_MULTITHREAD
    mprSetMaxWorkers(sp, sp->workers);
#endif

    return 0;
}


static int parseFilter(MprTestService *sp, cchar *filter)
{
    char    *str, *word, *tok;

    mprAssert(filter);
    if (filter == 0 || *filter == '\0') {
        return 0;
    }

    tok = 0;
    str = mprStrdup(sp, filter);
    word = mprStrTok(str, " \t\r\n", &tok);
    while (word) {
        if (mprAddItem(sp->testFilter, mprStrdup(sp, word)) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
        word = mprStrTok(0, " \t\r\n", &tok);
    }
    mprFree(str);
    return 0;
}


#if LOAD_TEST_PACKAGES
static int loadModule(MprTestService *sp, cchar *fileName)
{
    char    *cp, *base, entry[MPR_MAX_FNAME], path[MPR_MAX_FNAME];

    mprAssert(fileName && *fileName);

    base = mprGetPathBase(sp, fileName);
    mprAssert(base);
    if ((cp = strrchr(base, '.')) != 0) {
        *cp = '\0';
    }
    if (mprLookupModule(sp, base)) {
        return 0;
    }
                
    mprSprintf(entry, sizeof(entry), "%sInit", base);

    if (fileName[0] == '/' || (*fileName && fileName[1] == ':')) {
        mprSprintf(path, sizeof(path), "%s%s", fileName, BLD_BUILD_SHOBJ);
    } else {
        mprSprintf(path, sizeof(path), "./%s%s", fileName, BLD_BUILD_SHOBJ);
    }
    if (mprLoadModule(sp, path, entry, (void*) sp) == 0) {
        mprError(sp, "Can't load module %s", path);
        return -1;
    }
    return 0;
}
#endif


int mprRunTests(MprTestService *sp)
{
    MprTestGroup    *gp;
    int             next;
#if BLD_FEATURE_MULTITHREAD
    MprThread       *tp;
#endif

    /*
     *  Build the full names for all groups
     */
    next = 0; 
    while ((gp = mprGetNextItem(sp->groups, &next)) != 0) {
        buildFullNames(gp, gp->name);
    }

    sp->activeThreadCount = sp->numThreads;

    if (sp->echoCmdLine) {
        mprPrintf(sp, "%12s %s ... ", "[Test]", sp->commandLine);
        if (sp->verbose) {
            mprPrintf(sp, "\n");
        }
    }
    sp->start = mprGetTime(sp);

#if BLD_FEATURE_MULTITHREAD
{
    int i;
    /*
     *  Create worker threads for each test thread. 
     */
    for (i = 0; i < sp->numThreads; i++) {
        MprList     *lp;
        char        tName[64];

        mprSprintf(tName, sizeof(tName), "test.%d", i);

        lp = copyGroups(sp, sp->groups);
        if (lp == 0) {
            return MPR_ERR_NO_MEMORY;
        }
        
        /*
         *  Build the full names for all groups
         */
        next = 0; 
        while ((gp = mprGetNextItem(lp, &next)) != 0) {
            buildFullNames(gp, gp->name);
        }
        tp = mprCreateThread(sp, tName, (MprThreadProc) runTestThread, (void*) lp, MPR_NORMAL_PRIORITY, 0);
        if (tp == 0) {
            return MPR_ERR_NO_MEMORY;
        }
        if (mprStartThread(tp) < 0) {
            mprError(sp, "Can't start thread %d", i);
            return MPR_ERR_CANT_INITIALIZE;
        }
    }

    /*
     *  Wait for all the threads to complete (simple but effective)
     */
    while (sp->activeThreadCount > 0) {
        mprServiceEvents(mprGetDispatcher(sp), 250, MPR_SERVICE_EVENTS | MPR_SERVICE_IO);
    }
}
#else
    runTestThread(sp->groups, 0);
#endif

    return (sp->totalFailedCount == 0) ? 0 : 1;
}


#if BLD_FEATURE_MULTITHREAD
static MprList *copyGroups(MprTestService *sp, MprList *groups)
{
    MprTestGroup    *gp, *newGp;
    MprList         *lp;
    int             next;

    lp = mprCreateList(sp);
    if (lp == 0) {
        return 0;
    }

    next = 0; 
    while ((gp = mprGetNextItem(groups, &next)) != 0) {
        newGp = createTestGroup(sp, gp->def, NULL);
        if (newGp == 0) {
            mprFree(lp);
            return 0;
        }
        if (mprAddItem(lp, newGp) < 0) {
            mprFree(lp);
            return 0;
        }
    }
    return lp;
}
#endif


/*
 *  Run the test groups. One invocation per thread. Used even if not multithreaded.
 */
void runTestThread(MprList *groups, void *threadp)
{
    MprTestService  *sp;
    MprTestGroup    *gp;
    int             next, i;

    /*
     *  Get the service pointer
     */
    gp = mprGetFirstItem(groups);
    if (gp == 0) {
        return;
    }
    sp = gp->service;
    mprAssert(sp);

    for (i = (sp->iterations + sp->numThreads - 1) / sp->numThreads; i > 0; i--) {
        if (sp->totalFailedCount > 0 && !sp->continueOnFailures) {
            break;
        }
        next = 0; 
        while ((gp = mprGetNextItem(groups, &next)) != 0) {
            runTestGroup(gp);
        }
    }

    if (threadp) {
        adjustThreadCount(sp, -1);
    }
}


void mprReportTestResults(MprTestService *sp)
{
    if (sp->verbose <= 1 && sp->totalFailedCount == 0) {
        if (sp->verbose > 1) {
            mprPrintf(sp, "\n");
            mprPrintf(sp, "PASSED all tests for \"%s\"\n", sp->name);
        }
    }
    if (sp->verbose >= 2) {
        double  elapsed;
        elapsed = ((mprGetTime(sp) - sp->start) * 1.0 / 1000.0);
        mprPrintf(sp, "%s: %d tests completed, %d test(s) failed. ", 
            mprGetAppName(sp), sp->totalTestCount, sp->totalFailedCount);
        mprPrintf(sp, "Elapsed time: %5.2f seconds.\n", elapsed);
    }
}


static void buildFullNames(MprTestGroup *gp, cchar *name)
{
    MprTestGroup    *np;
    char            *nameBuf;
    cchar           *nameStack[MPR_TEST_MAX_STACK];
    int             tos, nextItem;

    tos = 0;

    /*
     *  Build the full name for this case
     */
    nameStack[tos++] = name;
    for (np = gp->parent; np && np != np->parent && tos < MPR_TEST_MAX_STACK;  np = np->parent) {
        nameStack[tos++] = np->name;
    }
    nameBuf = mprStrdup(gp, gp->service->name);
    while (--tos >= 0) {
        nameBuf = mprReallocStrcat(gp, -1, nameBuf, ".", nameStack[tos], NULL);
    }
    mprAssert(gp->fullName == 0);
    gp->fullName = mprStrdup(gp, nameBuf);

    /*
     *  Recurse for all test case groups
     */
    nextItem = 0;
    np = mprGetNextItem(gp->groups, &nextItem);
    while (np) {
        buildFullNames(np, np->name);
        np = mprGetNextItem(gp->groups, &nextItem);
    }
}


MprTestGroup *mprAddTestGroup(MprTestService *sp, MprTestDef *def)
{
    MprTestGroup    *gp;

    gp = createTestGroup(sp, def, NULL);
    if (gp == 0) {
        return 0;
    }

    if (mprAddItem(sp->groups, gp) < 0) {
        mprFree(gp);
        return 0;
    }
    return gp;
}


static MprTestGroup *createTestGroup(MprTestService *sp, MprTestDef *def, MprTestGroup *parent)
{
    MprTestGroup    *gp, *child;
    MprTestDef      **dp;
    MprTestCase     *tc;

    gp = mprAllocObjZeroed(sp, MprTestGroup);
    if (gp == 0) {
        return 0;
    }
    gp->service = sp;
    gp->cond = mprCreateCond(gp);

    gp->failures = mprCreateList(sp);
    if (gp->failures == 0) {
        mprFree(gp);
        return 0;
    }

    gp->cases = mprCreateList(sp);
    if (gp->cases == 0) {
        mprFree(gp);
        return 0;
    }

    gp->groups = mprCreateList(sp);
    if (gp->groups == 0) {
        mprFree(gp);
        return 0;
    }

    gp->def = def;
    gp->name = mprStrdup(sp, def->name);
    gp->success = 1;

#if BLD_FEATURE_HTTP_CLIENT
    if (parent == 0) {
        gp->http = mprCreateHttp(gp);
        mprSetHttpFollowRedirects(gp->http, 0);
    } else {
        gp->http = parent->http;
    }
#endif

    for (tc = def->caseDefs; tc->proc; tc++) {
        if (mprAddItem(gp->cases, tc) < 0) {
            mprFree(gp);
            return 0;
        }
    }

    if (def->groupDefs) {
        for (dp = &def->groupDefs[0]; *dp && (*dp)->name; dp++) {
            child = createTestGroup(sp, *dp, gp);
            if (child == 0) {
                mprFree(gp);
                return 0;
            }
            if (mprAddItem(gp->groups, child) < 0) {
                mprFree(gp);
                return 0;
            }
            child->parent = gp;
            child->root = gp->parent;
        }
    }
    return gp;
}


void mprResetTestGroup(MprTestGroup *gp)
{
    gp->success = 1;

#if BLD_FEATURE_MULTITHREAD
    if (gp->mutex) {
        mprFree(gp->mutex);
    }
    gp->mutex = mprCreateLock(gp);
#endif
}


static void runTestGroup(MprTestGroup *parent)
{
    MprTestService  *sp;
    MprTestGroup    *gp, *nextGroup;
    MprTestCase     *tc;
    int             count, nextItem;

    sp = parent->service;

    if (parent->def->init && (*parent->def->init)(parent) < 0) {
        parent->failedCount++;
        return;
    }

    /*
     *  Recurse over sub groups
     */
    nextItem = 0;
    gp = mprGetNextItem(parent->groups, &nextItem);
    while (gp && (parent->success || sp->continueOnFailures)) {
        nextGroup = mprGetNextItem(parent->groups, &nextItem);
        if (gp->testDepth > sp->testDepth) {
            gp = nextGroup;
            continue;
        }

        /*
         *  See if this group has been filtered for execution
         */
        if (! filterTestGroup(gp)) {
            gp = nextGroup;
            continue;
        }
        count = sp->totalFailedCount;
        if (count > 0 && !sp->continueOnFailures) {
            if (parent->def->term) {
                (*parent->def->term)(parent);
            }
            return;
        }

        /*
         *  Recurse over all tests in this group
         */
        runTestGroup(gp);

        gp->testCount++;

        if (! gp->success) {
            /*  Propagate the failures up the parent chain */
            parent->failedCount++;
            parent->success = 0;
        }
        gp = nextGroup;
    }

    /*
     *  Run test cases for this group
     */
    nextItem = 0;
    tc = mprGetNextItem(parent->cases, &nextItem);
    while (tc && (parent->success || sp->continueOnFailures)) {
        if (parent->testDepth <= sp->testDepth) {
            if (filterTestCast(parent, tc)) {
                runTestProc(parent, tc);
            }
        }
        tc = mprGetNextItem(parent->cases, &nextItem);
    }

    if (parent->def->term && (*parent->def->term)(parent) < 0) {
        parent->failedCount++;
    }
}


/*
 *  Return true if we are to run the test group
 */
static bool filterTestGroup(MprTestGroup *gp)
{
    MprTestService  *sp;
    MprList         *testFilter;
    char            *pattern;
    int             len, next;

    sp = gp->service;
    testFilter = sp->testFilter;

    if (testFilter == 0) {
        return 1;
    }

    /*
     *  See if this test has been filtered
     */
    if (mprGetListCount(testFilter) > 0) {
        next = 0;
        pattern = mprGetNextItem(testFilter, &next);
        while (pattern) {
            len = min((int) strlen(pattern), (int) strlen(gp->fullName));
            if (mprStrcmpAnyCaseCount(gp->fullName, pattern, len) == 0) {
                break;
            }
            pattern = mprGetNextItem(testFilter, &next);
        }
        if (pattern == 0) {
            return 0;
        }
    }
    return 1;
}


/*
 *  Return true if we are to run the test case
 */
static bool filterTestCast(MprTestGroup *gp, MprTestCase *tc)
{
    MprTestService  *sp;
    MprList         *testFilter;
    char            *pattern, *fullName;
    int             len, next;

    sp = gp->service;
    testFilter = sp->testFilter;

    if (testFilter == 0) {
        return 1;
    }

    /*
     *  See if this test has been filtered
     */
    if (mprGetListCount(testFilter) > 0) {
        fullName = mprAsprintf(gp, -1, "%s.%s", gp->fullName, tc->name);
        next = 0;
        pattern = mprGetNextItem(testFilter, &next);
        while (pattern) {
            len = min((int) strlen(pattern), (int) strlen(fullName));
            if (mprStrcmpAnyCaseCount(fullName, pattern, len) == 0) {
                break;
            }
            pattern = mprGetNextItem(testFilter, &next);
        }
        mprFree(fullName);
        if (pattern == 0) {
            return 0;
        }
    }
    return 1;
}


static void runTestProc(MprTestGroup *gp, MprTestCase *test)
{
    MprTestService      *sp;

    if (test->proc == 0) {
        return;
    }
    sp = gp->service;

    mprResetTestGroup(gp);

    if (sp->singleStep) {
        mprPrintf(gp, "%12s Run test \"%s.%s\", press <ENTER>: ", "[Test]", gp->fullName, test->name);
        getchar();

    } else if (sp->verbose) {
        mprPrintf(gp, "%12s Run test \"%s.%s\": ", "[Test]", gp->fullName, test->name);
    }

    if (gp->skip) {
        if (sp->verbose) {
            if (gp->skipWarned++ == 0) {
                mprPrintf(gp, "%12s Skipping test: \"%s.%s\": \n", "[Skip]", gp->fullName, test->name);
            }
        }
        
    } else {
        /*
         *  The function is part of the enclosing MprTest group
         */
        mprResetCond(gp->cond);
        (test->proc)(gp);
    
        mprLock(sp->mutex);
        if (gp->success) {
            ++sp->totalTestCount;
            if (sp->verbose) {
                mprPrintf(sp, "PASSED\n");
            }
        } else {
            mprPrintfError(gp, "FAILED test \"%s.%s\"\nDetails: %s\n", gp->fullName, test->name, getErrorMessage(gp));
        }
    }
    mprUnlock(sp->mutex);
}


static char *getErrorMessage(MprTestGroup *gp)
{
    MprTestFailure  *fp;
    char            msg[MPR_MAX_STRING], *errorMsg;
    int             nextItem;

    nextItem = 0;
    errorMsg = mprStrdup(gp, "");
    fp = mprGetNextItem(gp->failures, &nextItem);
    while (fp) {
        mprSprintf(msg, sizeof(msg), "Failure in %s\nAssertion: \"%s\"\n", fp->loc, fp->message);
        if ((errorMsg = mprStrcat(gp, -1, msg, NULL)) == NULL) {
            break;
        }
        fp = mprGetNextItem(gp->failures, &nextItem);
    }
    return errorMsg;
}


static int addFailure(MprTestGroup *gp, cchar *loc, cchar *message)
{
    MprTestFailure  *fp;

    fp = createFailure(gp, loc, message);
    if (fp == 0) {
        mprAssert(fp);
        return MPR_ERR_NO_MEMORY;
    }
    mprAddItem(gp->failures, fp);
    return 0;
}


static MprTestFailure *createFailure(MprTestGroup *gp, cchar *loc, cchar *message)
{
    MprTestFailure  *fp;

    fp = mprAllocObj(gp, MprTestFailure);
    if (fp == 0) {
        return 0;
    }
    fp->loc = mprStrdup(fp, loc);
    fp->message = mprStrdup(fp, message);
    return fp;
}


bool assertTrue(MprTestGroup *gp, cchar *loc, bool isTrue, cchar *msg)
{
    if (! isTrue) {
        gp->success = isTrue;
    }
    if (! isTrue) {
        if (gp->service->debugOnFailures) {
            mprBreakpoint();
        }
        addFailure(gp, loc, msg);
        gp->failedCount++;
        adjustFailedCount(gp->service, 1);
    }
    return isTrue;
}


bool mprWaitForTestToComplete(MprTestGroup *gp, int timeout)
{
    int     rc;
    
    mprAssert(gp->cond);

    rc = (mprWaitForCondWithService(gp->cond, timeout) == 0);
    mprResetCond(gp->cond);
    return rc;
}


void mprSignalTestComplete(MprTestGroup *gp)
{
    mprSignalCond(gp->cond);
}


static void adjustThreadCount(MprTestService *sp, int adj)
{
#if BLD_FEATURE_MULTITHREAD
    mprLock(sp->mutex);
    sp->activeThreadCount += adj;
    mprUnlock(sp->mutex);
#endif
}


static void adjustFailedCount(MprTestService *sp, int adj)
{
    mprLock(sp->mutex);
    sp->totalFailedCount += adj;
    mprUnlock(sp->mutex);
}


static void logHandler(MprCtx ctx, int flags, int level, cchar *msg)
{
    Mpr         *mpr;
    MprFile     *file;
    char        *prefix;

    mpr = mprGetMpr(ctx);
    file = (MprFile*) mpr->logHandlerData;
    prefix = mpr->name;

    while (*msg == '\n') {
        mprFprintf(file, "\n");
        msg++;
    }

    if (flags & MPR_LOG_SRC) {
        mprFprintf(file, "%s: %d: %s\n", prefix, level, msg);

    } else if (flags & MPR_ERROR_SRC) {
        /*
         *  Use static printing to avoid malloc when the messages are small.
         *  This is important for memory allocation errors.
         */
        if (strlen(msg) < (MPR_MAX_STRING - 32)) {
            mprStaticPrintf(file, "%s: Error: %s\n", prefix, msg);
        } else {
            mprFprintf(file, "%s: Error: %s\n", prefix, msg);
        }

    } else if (flags & MPR_FATAL_SRC) {
        mprFprintf(file, "%s: Fatal: %s\n", prefix, msg);
        
    } else if (flags & MPR_RAW) {
        mprFprintf(file, "%s", msg);
    }
    
    if (flags & (MPR_ERROR_SRC | MPR_FATAL_SRC)) {
        mprBreakpoint();
    }
}


static int setLogging(Mpr *mpr, char *logSpec)
{
    MprFile     *file;
    char        *levelSpec;
    int         level;

    level = 0;

    if ((levelSpec = strchr(logSpec, ':')) != 0) {
        *levelSpec++ = '\0';
        level = atoi(levelSpec);
    }

    if (strcmp(logSpec, "stdout") == 0) {
        file = mpr->fileSystem->stdOutput;

    } else if (strcmp(logSpec, "stderr") == 0) {
        file = mpr->fileSystem->stdError;

    } else {
        if ((file = mprOpen(mpr, logSpec, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprPrintfError(mpr, "Can't open log file %s\n", logSpec);
            return MPR_ERR_CANT_OPEN;
        }
    }

    mprSetLogLevel(mpr, level);
    mprSetLogHandler(mpr, logHandler, (void*) file);

    return 0;
}

#else
void __mprUnitTest() {}
#endif /* BLD_FEATURE_TEST */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprTest.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprThread.c"
 */
/************************************************************************/

/**
 *  mprThread.c - Primitive multi-threading support for Windows
 *
 *  This module provides threading, mutex and condition variable APIs.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_MULTITHREAD


static int  changeState(MprWorker *worker, int state);
static MprWorker *createWorker(MprWorkerService *ws, int stackSize);
static int  getNextThreadNum(MprWorkerService *ws);
static int  workerDestructor(MprWorker *worker);
static void pruneWorkers(MprWorkerService *ws, MprEvent *timer);
static void threadProc(MprThread *tp);
static int threadDestructor(MprThread *tp);
static void workerMain(MprWorker *worker, MprThread *tp);


MprThreadService *mprCreateThreadService(Mpr *mpr)
{
    MprThreadService    *ts;

    mprAssert(mpr);

    ts = mprAllocObjZeroed(mpr, MprThreadService);
    if (ts == 0) {
        return 0;
    }
    ts->mutex = mprCreateLock(mpr);
    if (ts->mutex == 0) {
        mprFree(ts);
        return 0;
    }
    ts->threads = mprCreateList(ts);
    if (ts->threads == 0) {
        mprFree(ts);
        return 0;
    }
    mpr->serviceThread = mpr->mainOsThread = mprGetCurrentOsThread();
    mpr->threadService = ts;
    ts->stackSize = MPR_DEFAULT_STACK;

    /*
     *  Don't actually create the thread. Just create a thread object for this main thread.
     */
    ts->mainThread = mprCreateThread(ts, "main", 0, 0, MPR_NORMAL_PRIORITY, 0);
    if (ts->mainThread == 0) {
        mprFree(ts);
        return 0;
    }
    ts->mainThread->isMain = 1;
    return ts;
}


bool mprStopThreadService(MprThreadService *ts, int timeout)
{
    while (timeout > 0 && ts->threads->length > 1) {
        mprSleep(ts, 50);
        timeout -= 50;
    }
    return ts->threads->length == 0;
}


void mprSetThreadStackSize(MprCtx ctx, int size)
{
    mprGetMpr(ctx)->threadService->stackSize = size;
}


/*
 *  Return the current thread object
 */
MprThread *mprGetCurrentThread(MprCtx ctx)
{
    MprThreadService    *ts;
    MprThread           *tp;
    MprOsThread         id;
    int                 i;

    ts = mprGetMpr(ctx)->threadService;
    mprLock(ts->mutex);
    id = mprGetCurrentOsThread();
    for (i = 0; i < ts->threads->length; i++) {
        tp = (MprThread*) mprGetItem(ts->threads, i);
        if (tp->osThread == id) {
            mprUnlock(ts->mutex);
            return tp;
        }
    }
    mprUnlock(ts->mutex);
    return 0;
}


/*
 *  Return the current thread object
 */
cchar *mprGetCurrentThreadName(MprCtx ctx)
{
    MprThread       *tp;

    tp = mprGetCurrentThread(ctx);
    if (tp == 0) {
        return 0;
    }
    return tp->name;
}


/*
 *  Return the current thread object
 */
void mprSetCurrentThreadPriority(MprCtx ctx, int pri)
{
    MprThread       *tp;

    tp = mprGetCurrentThread(ctx);
    if (tp == 0) {
        return;
    }
    mprSetThreadPriority(tp, pri);
}


/*
 *  Create a main thread
 */
MprThread *mprCreateThread(MprCtx ctx, cchar *name, MprThreadProc entry, void *data, int priority, int stackSize)
{
    MprThreadService    *ts;
    MprThread           *tp;

    ts = mprGetMpr(ctx)->threadService;
    if (ts) {
        ctx = ts;
    }
    tp = mprAllocObjWithDestructorZeroed(ctx, MprThread, threadDestructor);
    if (tp == 0) {
        return 0;
    }
    tp->data = data;
    tp->entry = entry;
    tp->name = mprStrdup(tp, name);
    tp->mutex = mprCreateLock(tp);
    tp->pid = getpid();
    tp->priority = priority;

    if (stackSize == 0) {
        tp->stackSize = ts->stackSize;
    } else {
        tp->stackSize = stackSize;
    }
#if BLD_WIN_LIKE
    tp->threadHandle = 0;
#endif
    if (ts && ts->threads) {
        mprLock(ts->mutex);
        if (mprAddItem(ts->threads, tp) < 0) {
            mprFree(tp);
            mprUnlock(ts->mutex);
            return 0;
        }
        mprUnlock(ts->mutex);
    }
    return tp;
}


/*
 *  Destroy a thread
 */
static int threadDestructor(MprThread *tp)
{
    MprThreadService    *ts;

    mprLock(tp->mutex);

    ts = mprGetMpr(tp)->threadService;
    mprRemoveItem(ts->threads, tp);

#if BLD_WIN_LIKE
    if (tp->threadHandle) {
        CloseHandle(tp->threadHandle);
    }
#endif
    return 0;
}


/*
 *  Entry thread function
 */ 
#if BLD_WIN_LIKE
static uint __stdcall threadProcWrapper(void *data) 
{
    threadProc((MprThread*) data);
    return 0;
}
#elif VXWORKS

static int threadProcWrapper(void *data) 
{
    threadProc((MprThread*) data);
    return 0;
}

#else
void *threadProcWrapper(void *data) 
{
    threadProc((MprThread*) data);
    return 0;
}

#endif


/*
 *  Thread entry
 */
static void threadProc(MprThread *tp)
{
    mprAssert(tp);

    tp->osThread = mprGetCurrentOsThread();

#if VXWORKS
    tp->pid = tp->osThread;
#else
    tp->pid = getpid();
#endif
    (tp->entry)(tp->data, tp);
    mprFree(tp);
}


/*
 *  Start a thread
 */
int mprStartThread(MprThread *tp)
{
    mprLock(tp->mutex);

#if BLD_WIN_LIKE
{
    HANDLE          h;
    uint            threadId;

#if WINCE
    h = (HANDLE) CreateThread(NULL, 0, threadProcWrapper, (void*) tp, 0, &threadId);
#else
    h = (HANDLE) _beginthreadex(NULL, 0, threadProcWrapper, (void*) tp, 0, &threadId);
#endif
    if (h == NULL) {
        return MPR_ERR_CANT_INITIALIZE;
    }
    tp->osThread = (int) threadId;
    tp->threadHandle = (HANDLE) h;
}
#elif VXWORKS
{
    int     taskHandle, pri;

    taskPriorityGet(taskIdSelf(), &pri);
    taskHandle = taskSpawn(tp->name, pri, 0, tp->stackSize, (FUNCPTR) threadProcWrapper, (int) tp, 
        0, 0, 0, 0, 0, 0, 0, 0, 0);

    if (taskHandle < 0) {
        mprError(tp, "Can't create thread %s\n", tp->name);
        return MPR_ERR_CANT_INITIALIZE;
    }
}
#else /* UNIX */
{
    pthread_attr_t  attr;
    pthread_t       h;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, tp->stackSize);

    if (pthread_create(&h, &attr, threadProcWrapper, (void*) tp) != 0) { 
        mprAssert(0);
        pthread_attr_destroy(&attr);
        return MPR_ERR_CANT_CREATE;
    }
    pthread_attr_destroy(&attr);
}
#endif
    mprSetThreadPriority(tp, tp->priority);
    mprUnlock(tp->mutex);
    return 0;
}


MprOsThread mprGetCurrentOsThread()
{
#if BLD_UNIX_LIKE
    return (MprOsThread) pthread_self();
#elif BLD_WIN_LIKE
    return (MprOsThread) GetCurrentThreadId();
#elif VXWORKS
    return (MprOsThread) taskIdSelf();
#endif
}


void mprSetThreadPriority(MprThread *tp, int newPriority)
{
    int     osPri;

    mprLock(tp->mutex);

    osPri = mprMapMprPriorityToOs(newPriority);

#if BLD_WIN_LIKE
    SetThreadPriority(tp->threadHandle, osPri);
#elif VXWORKS
    taskPrioritySet(tp->osThread, osPri);
#else
    setpriority(PRIO_PROCESS, tp->pid, osPri);
#endif
    tp->priority = newPriority;
    mprUnlock(tp->mutex);
}


static int threadLocalDestructor(MprThreadLocal *tls)
{
#if BLD_UNIX_LIKE
    if (tls->key) {
        pthread_key_delete(tls->key);
    }
#elif BLD_WIN_LIKE
    if (tls->key >= 0) {
        TlsFree(tls->key);
    }
#endif
    return 0;
}


MprThreadLocal *mprCreateThreadLocal(MprCtx ctx)
{
    MprThreadLocal      *tls;

    tls = mprAllocObjWithDestructorZeroed(ctx, MprThreadLocal, threadLocalDestructor);
    if (tls == 0) {
        return 0;
    }
#if BLD_UNIX_LIKE
    if (pthread_key_create(&tls->key, NULL) != 0) {
        tls->key = 0;
        mprFree(tls);
        return 0;
    }
#elif BLD_WIN_LIKE
    if ((tls->key = TlsAlloc()) < 0) {
        return 0;
    }
#endif
    return tls;
}


int mprSetThreadData(MprThreadLocal *tls, void *value)
{
    bool    err;

    err = 1;
#if BLD_UNIX_LIKE
    err = pthread_setspecific(tls->key, value) != 0;
#elif BLD_WIN_LIKE
    err = TlsSetValue(tls->key, value) != 0;
#endif
    return (err) ? MPR_ERR_CANT_WRITE: 0;
}


void *mprGetThreadData(MprThreadLocal *tls)
{
#if BLD_UNIX_LIKE
    return pthread_getspecific(tls->key);
#elif BLD_WIN_LIKE
    return TlsGetValue(tls->key);
#elif VXWORKS
    /* Not supported */
    return 0;
#endif
}


#if BLD_WIN_LIKE
/*
 *  Map Mpr priority to Windows native priority. Windows priorities range from -15 to +15 (zero is normal). 
 *  Warning: +15 will not yield the CPU, -15 may get starved. We should be very wary going above +11.
 */

int mprMapMprPriorityToOs(int mprPriority)
{
    mprAssert(mprPriority >= 0 && mprPriority <= 100);
 
    if (mprPriority <= MPR_BACKGROUND_PRIORITY) {
        return THREAD_PRIORITY_LOWEST;
    } else if (mprPriority <= MPR_LOW_PRIORITY) {
        return THREAD_PRIORITY_BELOW_NORMAL;
    } else if (mprPriority <= MPR_NORMAL_PRIORITY) {
        return THREAD_PRIORITY_NORMAL;
    } else if (mprPriority <= MPR_HIGH_PRIORITY) {
        return THREAD_PRIORITY_ABOVE_NORMAL;
    } else {
        return THREAD_PRIORITY_HIGHEST;
    }
}


/*
 *  Map Windows priority to Mpr priority
 */ 
int mprMapOsPriorityToMpr(int nativePriority)
{
    int     priority;

    priority = (45 * nativePriority) + 50;
    if (priority < 0) {
        priority = 0;
    }
    if (priority >= 100) {
        priority = 99;
    }
    return priority;
}


#elif VXWORKS
/*
 *  Map MPR priority to VxWorks native priority.
 */

int mprMapMprPriorityToOs(int mprPriority)
{
    int     nativePriority;

    mprAssert(mprPriority >= 0 && mprPriority < 100);

    nativePriority = (100 - mprPriority) * 5 / 2;

    if (nativePriority < 10) {
        nativePriority = 10;
    } else if (nativePriority > 255) {
        nativePriority = 255;
    }
    return nativePriority;
}


/*
 *  Map O/S priority to Mpr priority.
 */ 
int mprMapOsPriorityToMpr(int nativePriority)
{
    int     priority;

    priority = (255 - nativePriority) * 2 / 5;
    if (priority < 0) {
        priority = 0;
    }
    if (priority >= 100) {
        priority = 99;
    }
    return priority;
}


#else /* UNIX */
/*
 *  Map MR priority to linux native priority. Unix priorities range from -19 to +19. Linux does -20 to +19. 
 */
int mprMapMprPriorityToOs(int mprPriority)
{
    mprAssert(mprPriority >= 0 && mprPriority < 100);

    if (mprPriority <= MPR_BACKGROUND_PRIORITY) {
        return 19;
    } else if (mprPriority <= MPR_LOW_PRIORITY) {
        return 10;
    } else if (mprPriority <= MPR_NORMAL_PRIORITY) {
        return 0;
    } else if (mprPriority <= MPR_HIGH_PRIORITY) {
        return -8;
    } else {
        return -19;
    }
    mprAssert(0);
    return 0;
}


/*
 *  Map O/S priority to Mpr priority.
 */ 
int mprMapOsPriorityToMpr(int nativePriority)
{
    int     priority;

    priority = (nativePriority + 19) * (100 / 40); 
    if (priority < 0) {
        priority = 0;
    }
    if (priority >= 100) {
        priority = 99;
    }
    return priority;
}

#endif /* UNIX */


MprWorkerService *mprCreateWorkerService(MprCtx ctx)
{
    MprWorkerService      *ws;

    ws = mprAllocObjZeroed(ctx, MprWorkerService);
    if (ws == 0) {
        return 0;
    }
    ws->mutex = mprCreateLock(ws);
    ws->minThreads = MPR_DEFAULT_MIN_THREADS;
    ws->maxThreads = MPR_DEFAULT_MAX_THREADS;

    /*
     *  Presize the lists so they cannot get memory allocation failures later on.
     */
    ws->idleThreads = mprCreateList(ws);
    mprSetListLimits(ws->idleThreads, ws->maxThreads, -1);

    ws->busyThreads = mprCreateList(ws);
    mprSetListLimits(ws->busyThreads, ws->maxThreads, -1);
    return ws;
}


int mprStartWorkerService(MprWorkerService *ws)
{
    /*
     *  Create a timer to trim excess threads in the worker
     */
    mprSetMinWorkers(ws, ws->minThreads);
    ws->pruneTimer = mprCreateTimerEvent(mprGetDispatcher(ws), (MprEventProc) pruneWorkers, MPR_TIMEOUT_PRUNER, 
        MPR_NORMAL_PRIORITY, (void*) ws, 0);
    return 0;
}


bool mprStopWorkerService(MprWorkerService *ws, int timeout)
{
    MprWorker     *worker;
    int           rc, next;

    rc = 0;
    mprLock(ws->mutex);

    if (ws->pruneTimer) {
        mprFree(ws->pruneTimer);
        ws->pruneTimer = 0;
    }

    /*
     *  Wake up all idle threads. Busy threads take care of themselves. An idle thread will wakeup, exit and be 
     *  removed from the busy list and then delete the thread. We progressively remove the last thread in the idle
     *  list. ChangeState will move the threads to the busy queue.
     */
    for (next = -1; (worker = (MprWorker*) mprGetPrevItem(ws->idleThreads, &next)) != 0; ) {
        changeState(worker, MPR_WORKER_BUSY);
    }

    /*
     *  Wait until all tasks and threads have exited
     */
    while (timeout > 0 && ws->numThreads > 0) {
        mprUnlock(ws->mutex);
        mprSleep(ws, 50);
        timeout -= 50;
        mprLock(ws->mutex);
    }

    mprAssert(ws->idleThreads->length == 0);
    mprAssert(ws->busyThreads->length == 0);
    mprUnlock(ws->mutex);
    return ws->numThreads == 0;
}


/*
 *  Define the new minimum number of threads. Pre-allocate the minimum.
 */
void mprSetMinWorkers(MprCtx ctx, int n)
{ 
    MprWorker           *worker;
    MprWorkerService    *ws;

    ws = mprGetMpr(ctx)->workerService;

    mprLock(ws->mutex);

    ws->minThreads = n; 
    while (ws->numThreads < ws->minThreads) {
        worker = createWorker(ws, ws->stackSize);
        ws->numThreads++;
        ws->maxUseThreads = max(ws->numThreads, ws->maxUseThreads);
        ws->pruneHighWater = max(ws->numThreads, ws->pruneHighWater);
        changeState(worker, MPR_WORKER_BUSY);
        mprStartThread(worker->thread);
    }
    mprUnlock(ws->mutex);
}


/*
 *  Define a new maximum number of theads. Prune if currently over the max.
 */
void mprSetMaxWorkers(MprCtx ctx, int n)
{
    MprWorkerService  *ws;

    ws = mprGetMpr(ctx)->workerService;

    mprLock(ws->mutex);
    ws->maxThreads = n; 
    if (ws->numThreads > ws->maxThreads) {
        pruneWorkers(ws, 0);
    }
    if (ws->minThreads > ws->maxThreads) {
        ws->minThreads = ws->maxThreads;
    }
    mprUnlock(ws->mutex);
}


int mprGetMaxWorkers(MprCtx ctx)
{
    return mprGetMpr(ctx)->workerService->maxThreads;
}


/*
 *  Return the current worker thread object
 */
MprWorker *mprGetCurrentWorker(MprCtx ctx)
{
    MprWorkerService    *ws;
    MprWorker           *worker;
    MprThread           *thread;
    int                 next;

    ws = mprGetMpr(ctx)->workerService;

    mprLock(ws->mutex);
    thread = mprGetCurrentThread(ws);
    for (next = -1; (worker = (MprWorker*) mprGetPrevItem(ws->busyThreads, &next)) != 0; ) {
        if (worker->thread == thread) {
            mprUnlock(ws->mutex);
            return worker;
        }
    }
    mprUnlock(ws->mutex);
    return 0;
}


/*
 *  Set the worker as dedicated to the current task
 */
void mprDedicateWorker(MprWorker *worker)
{
    mprLock(worker->workerService->mutex);
    worker->flags |= MPR_WORKER_DEDICATED;
    mprUnlock(worker->workerService->mutex);
}


void mprReleaseWorker(MprWorker *worker)
{
    mprLock(worker->workerService->mutex);
    worker->flags &= ~MPR_WORKER_DEDICATED;
    mprUnlock(worker->workerService->mutex);
}


void mprActivateWorker(MprWorker *worker, MprWorkerProc proc, void *data, int priority)
{
    MprWorkerService    *ws;

    ws = worker->workerService;

    mprLock(ws->mutex);
    worker->proc = proc;
    worker->data = data;
    worker->priority = priority;
    mprAssert(worker->flags & MPR_WORKER_DEDICATED);
    changeState(worker, MPR_WORKER_BUSY);
    mprUnlock(ws->mutex);
}


int mprStartWorker(MprCtx ctx, MprWorkerProc proc, void *data, int priority)
{
    MprWorkerService    *ws;
    MprWorker           *worker;
    int                 next;

    ws = mprGetMpr(ctx)->workerService;

    mprLock(ws->mutex);

    /*
     *  Try to find an idle thread and wake it up. It will wakeup in workerMain(). If not any available, then add 
     *  another thread to the worker. Must account for threads we've already created but have not yet gone to work 
     *  and inserted themselves in the idle/busy queues.
     */
    for (next = 0; (worker = (MprWorker*) mprGetNextItem(ws->idleThreads, &next)) != 0; ) {
        if (!(worker->flags & MPR_WORKER_DEDICATED)) {
            break;
        }
    }

    if (worker) {
        worker->proc = proc;
        worker->data = data;
        worker->priority = priority;
        changeState(worker, MPR_WORKER_BUSY);

    } else if (ws->numThreads < ws->maxThreads) {

        /*
         *  Can't find an idle thread. Try to create more threads in the worker. Otherwise, we will have to wait. 
         *  No need to wakeup the thread -- it will immediately go to work.
         */
        worker = createWorker(ws, ws->stackSize);

        ws->numThreads++;
        ws->maxUseThreads = max(ws->numThreads, ws->maxUseThreads);
        ws->pruneHighWater = max(ws->numThreads, ws->pruneHighWater);

        worker->proc = proc;
        worker->data = data;
        worker->priority = priority;

        changeState(worker, MPR_WORKER_BUSY);
        mprStartThread(worker->thread);

    } else {
        static int warned = 0;
        /*
         *  No free threads and can't create anymore
         */
        if (warned++ == 0) {
            mprError(ctx, "No free worker threads, using service thread. (currently allocated %d)", ws->numThreads);
        }
        mprUnlock(ws->mutex);
        return MPR_ERR_BUSY;
    }
    mprUnlock(ws->mutex);
    return 0;
}


/*
 *  Trim idle threads from a task
 */
static void pruneWorkers(MprWorkerService *ws, MprEvent *timer)
{
    MprWorker     *worker;
    int           index, toTrim;

    if (mprGetDebugMode(ws)) {
        return;
    }
    /*
     *  Prune half of what we could prune. This gives exponentional decay. We use the high water mark seen in 
     *  the last period.
     */
    mprLock(ws->mutex);
    toTrim = (ws->pruneHighWater - ws->minThreads) / 2;

    for (index = 0; toTrim-- > 0 && index < ws->idleThreads->length; index++) {
        worker = (MprWorker*) mprGetItem(ws->idleThreads, index);
        /*
         *  Leave floating -- in no queue. The thread will kill itself.
         */
        changeState(worker, MPR_WORKER_PRUNED);
    }
    ws->pruneHighWater = ws->minThreads;
    mprUnlock(ws->mutex);
}


int mprGetAvailableWorkers(MprCtx ctx)
{
    MprWorkerService  *ws;

    ws = mprGetMpr(ctx)->workerService;
    return ws->idleThreads->length + (ws->maxThreads - ws->numThreads); 
}


static int getNextThreadNum(MprWorkerService *ws)
{
    int     rc;

    mprLock(ws->mutex);
    rc = ws->nextThreadNum++;
    mprUnlock(ws->mutex);
    return rc;
}


/*
 *  Define a new stack size for new threads. Existing threads unaffected.
 */
void mprSetWorkerStackSize(MprCtx ctx, int n)
{
    MprWorkerService  *ws;

    ws = mprGetMpr(ctx)->workerService;
    ws->stackSize = n; 
}


void mprGetWorkerServiceStats(MprWorkerService *ws, MprWorkerStats *stats)
{
    mprAssert(ws);

    stats->maxThreads = ws->maxThreads;
    stats->minThreads = ws->minThreads;
    stats->numThreads = ws->numThreads;
    stats->maxUse = ws->maxUseThreads;
    stats->pruneHighWater = ws->pruneHighWater;
    stats->idleThreads = ws->idleThreads->length;
    stats->busyThreads = ws->busyThreads->length;
}


/*
 *  Create a new thread for the task
 */
static MprWorker *createWorker(MprWorkerService *ws, int stackSize)
{
    MprWorker   *worker;

    char    name[16];

    worker = mprAllocObjWithDestructorZeroed(ws, MprWorker, workerDestructor);
    if (worker == 0) {
        return 0;
    }

    worker->flags = 0;
    worker->proc = 0;
    worker->cleanup = 0;
    worker->data = 0;
    worker->priority = 0;
    worker->state = 0;
    worker->workerService = ws;
    worker->idleCond = mprCreateCond(worker);

    mprSprintf(name, sizeof(name), "worker.%u", getNextThreadNum(ws));
    worker->thread = mprCreateThread(ws, name, (MprThreadProc) workerMain, (void*) worker, MPR_WORKER_PRIORITY, 0);
    return worker;
}


static int workerDestructor(MprWorker *worker)
{
    if (worker->thread != 0) {
        mprAssert(worker->thread);
        return 1;
    }
    return 0;
}


/*
 *  Worker thread main service routine
 */
static void workerMain(MprWorker *worker, MprThread *tp)
{
    MprWorkerService    *ws;
    int                 rc;

    ws = mprGetMpr(worker)->workerService;
    mprAssert(worker->state == MPR_WORKER_BUSY);
    mprAssert(!worker->idleCond->triggered);

    mprLock(ws->mutex);

    while (!(worker->state & MPR_WORKER_PRUNED)) {
        if (worker->proc) {
            mprUnlock(ws->mutex);
            mprSetThreadPriority(worker->thread, worker->priority);

            (*worker->proc)(worker->data, worker);

            mprLock(ws->mutex);
            worker->proc = 0;
            mprSetThreadPriority(worker->thread, MPR_WORKER_PRIORITY);
        }
        changeState(worker, MPR_WORKER_SLEEPING);

        if (worker->cleanup) {
            (*worker->cleanup)(worker->data, worker);
            worker->cleanup = NULL;
        }
        mprUnlock(ws->mutex);

        /*
         *  Sleep till there is more work to do
         */
        rc = mprWaitForCond(worker->idleCond, -1);

        mprLock(ws->mutex);
        mprAssert(worker->state == MPR_WORKER_BUSY || worker->state == MPR_WORKER_PRUNED);
    }

    changeState(worker, 0);

    ws->numThreads--;
    worker->thread = 0;
    mprUnlock(ws->mutex);
}


static int changeState(MprWorker *worker, int state)
{
    MprWorkerService    *ws;
    MprList             *lp;

    mprAssert(worker->state != state);

    ws = worker->workerService;

    lp = 0;
    mprLock(ws->mutex);
    switch (worker->state) {
    case MPR_WORKER_BUSY:
        lp = ws->busyThreads;
        break;

    case MPR_WORKER_IDLE:
        lp = ws->idleThreads;
        break;

    case MPR_WORKER_SLEEPING:
        if (!(worker->flags & MPR_WORKER_DEDICATED)) {
            lp = ws->idleThreads;
        }
        mprSignalCond(worker->idleCond); 
        break;
        
    case MPR_WORKER_PRUNED:
        break;
    }

    /*
     *  Reassign the worker to the appropriate queue
     */
    if (lp) {
        mprRemoveItem(lp, worker);
    }
    lp = 0;
    switch (state) {
    case MPR_WORKER_BUSY:
        lp = ws->busyThreads;
        break;

    case MPR_WORKER_IDLE:
    case MPR_WORKER_SLEEPING:
        if (!(worker->flags & MPR_WORKER_DEDICATED)) {
            lp = ws->idleThreads;
        }
        break;

    case MPR_WORKER_PRUNED:
        /* Don't put on a queue and the thread will exit */
        break;
    }
    
    worker->state = state;

    if (lp) {
        if (mprAddItem(lp, worker) < 0) {
            mprUnlock(ws->mutex);
            return MPR_ERR_NO_MEMORY;
        }
    }
    mprUnlock(ws->mutex);
    return 0;
}


#else
cchar *mprGetCurrentThreadName(MprCtx ctx) { return "main"; }
#endif /* BLD_FEATURE_MULTITHREAD */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprThread.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprTime.c"
 */
/************************************************************************/

/**
    mprTime.c - Date and Time handling
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#define MS_PER_SEC  (MPR_TICKS_PER_SEC)
#define MS_PER_HOUR (60 * 60 * MPR_TICKS_PER_SEC)
#define MS_PER_MIN  (60 * MPR_TICKS_PER_SEC)
#define MS_PER_DAY  (86400 * MPR_TICKS_PER_SEC)
#define MS_PER_YEAR (INT64(31556952000))

/*
    On some platforms, time_t is only 32 bits (linux-32) and on some 64 bit systems, time calculations
    outside the range of 32 bits are unreliable. This means there is a minimum and maximum year that 
    can be analysed using the O/S localtime routines. However, we really want to use the O/S 
    calculations for daylight savings time, so when a date is outside a 32 bit time_t range, we use
    some trickery to remap the year to a valid year when using localtime.
    FYI: 32 bit time_t expires at: 03:14:07 UTC on Tuesday, 19 January 2038
 */
#if UNUSED
#define MAX_TIME    (((time_t) -1) & ~(((time_t) 1) << ((sizeof(time_t) * 8) - 1)))
#define MIN_TIME    (((time_t) 1) << ((sizeof(time_t) * 8) - 1))
#else
#define MAX_TIME    (((time_t) -1) & ~(((time_t) 1) << 31))
#define MIN_TIME    (((time_t) 1) << 31)
#endif

#if UNUSED
/*
    Approximate, conservative min and max year. The 31556952 constant is approx sec/year (365.2425 * 86400)
    Reduce by one to ensure no overflow. Note: this does not reduce actual date range representations and is
    only used in DST calculations. Use 1900 for the minimum as 
 */
#define MIN_YEAR    ((MIN_TIME / 31556952) + 1)
#define MAX_YEAR    ((MAX_TIME / 31556952) - 1)
#else
#define MIN_YEAR    1901
#define MAX_YEAR    2037
#endif

/*
    Token types or'd into the TimeToken value
 */
#define TOKEN_DAY       0x01000000
#define TOKEN_MONTH     0x02000000
#define TOKEN_ZONE      0x04000000
#define TOKEN_OFFSET    0x08000000
#define TOKEN_MASK      0xFF000000

typedef struct TimeToken {
    char    *name;
    int     value;
} TimeToken;

static TimeToken days[] = {
    { "sun",  0 | TOKEN_DAY },
    { "mon",  1 | TOKEN_DAY },
    { "tue",  2 | TOKEN_DAY },
    { "wed",  3 | TOKEN_DAY },
    { "thu",  4 | TOKEN_DAY },
    { "fri",  5 | TOKEN_DAY },
    { "sat",  6 | TOKEN_DAY },
    { 0, 0 },
};

static TimeToken fullDays[] = {
    { "sunday",     0 | TOKEN_DAY },
    { "monday",     1 | TOKEN_DAY },
    { "tuesday",    2 | TOKEN_DAY },
    { "wednesday",  3 | TOKEN_DAY },
    { "thursday",   4 | TOKEN_DAY },
    { "friday",     5 | TOKEN_DAY },
    { "saturday",   6 | TOKEN_DAY },
    { 0, 0 },
};

/*
    Make origin 1 to correspond to user date entries 10/28/2011
 */
static TimeToken months[] = {
    { "jan",  1 | TOKEN_MONTH },
    { "feb",  2 | TOKEN_MONTH },
    { "mar",  3 | TOKEN_MONTH },
    { "apr",  4 | TOKEN_MONTH },
    { "may",  5 | TOKEN_MONTH },
    { "jun",  6 | TOKEN_MONTH },
    { "jul",  7 | TOKEN_MONTH },
    { "aug",  8 | TOKEN_MONTH },
    { "sep",  9 | TOKEN_MONTH },
    { "oct", 10 | TOKEN_MONTH },
    { "nov", 11 | TOKEN_MONTH },
    { "dec", 12 | TOKEN_MONTH },
    { 0, 0 },
};

static TimeToken fullMonths[] = {
    { "january",    1 | TOKEN_MONTH },
    { "february",   2 | TOKEN_MONTH },
    { "march",      3 | TOKEN_MONTH },
    { "april",      4 | TOKEN_MONTH },
    { "may",        5 | TOKEN_MONTH },
    { "june",       6 | TOKEN_MONTH },
    { "july",       7 | TOKEN_MONTH },
    { "august",     8 | TOKEN_MONTH },
    { "september",  9 | TOKEN_MONTH },
    { "october",   10 | TOKEN_MONTH },
    { "november",  11 | TOKEN_MONTH },
    { "december",  12 | TOKEN_MONTH },
    { 0, 0 }
};

static TimeToken ampm[] = {
    { "am", 0 | TOKEN_OFFSET },
    { "pm", (12 * 3600) | TOKEN_OFFSET },
    { 0, 0 },
};


static TimeToken zones[] = {
    { "ut",      0 | TOKEN_ZONE},
    { "utc",     0 | TOKEN_ZONE},
    { "gmt",     0 | TOKEN_ZONE},
    { "edt",  -240 | TOKEN_ZONE},
    { "est",  -300 | TOKEN_ZONE},
    { "cdt",  -300 | TOKEN_ZONE},
    { "cst",  -360 | TOKEN_ZONE},
    { "mdt",  -360 | TOKEN_ZONE},
    { "mst",  -420 | TOKEN_ZONE},
    { "pdt",  -420 | TOKEN_ZONE},
    { "pst",  -480 | TOKEN_ZONE},
    { 0, 0 },
};


static TimeToken offsets[] = {
    { "tomorrow",    86400 | TOKEN_OFFSET},
    { "yesterday",  -86400 | TOKEN_OFFSET},
    { "next week",   (86400 * 7) | TOKEN_OFFSET},
    { "last week",  -(86400 * 7) | TOKEN_OFFSET},
    { 0, 0 },
};

static int timeSep = ':';

/*
    Formats for mprFormatTime
 */
#if WIN
    #define VALID_FMT "AaBbCcDdEeFHhIjklMmnOPpRrSsTtUuvWwXxYyZz+%"
#elif MACOSX
    #define VALID_FMT "AaBbCcDdEeFGgHhIjklMmnOPpRrSsTtUuVvWwXxYyZz+%"
#else
    #define VALID_FMT "AaBbCcDdEeFGgHhIjklMmnOPpRrSsTtUuVvWwXxYyZz+%"
#endif

#if WINCE
    #define HAS_STRFTIME 0
#else
    #define HAS_STRFTIME 1
#endif

#if !HAS_STRFTIME
static char *abbrevDay[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char *day[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

static char *abbrevMonth[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *month[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
#endif /* !HAS_STRFTIME */


static int normalMonthStart[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0,
};
static int leapMonthStart[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 0
};

static MprTime daysSinceEpoch(int year);
static void decodeTime(MprCtx ctx, struct tm *tp, MprTime when, bool local);
static int getTimeZoneOffsetFromTm(MprCtx ctx, struct tm *tp);
static int leapYear(int year);
static int localTime(MprCtx ctx, struct tm *timep, MprTime time);
static MprTime makeTime(MprCtx ctx, struct tm *tp);
static void validateTime(MprCtx ctx, struct tm *tm, struct tm *defaults);

/*
    Initialize the time service
 */
int mprCreateTimeService(MprCtx ctx)
{
    Mpr                 *mpr;
    TimeToken           *tt;
#if UNUSED
    struct timezone     tz;
    struct timeval      tv;
#endif

    mpr = mprGetMpr(ctx);
    mpr->timeTokens = mprCreateHash(mpr, -1);
    ctx = mpr->timeTokens;

    for (tt = days; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = fullDays; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = months; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = fullMonths; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = ampm; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = zones; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
    for (tt = offsets; tt->name; tt++) {
        mprAddHash(mpr->timeTokens, tt->name, (void*) tt);
    }
#if UNUSED
    /*
        Get timezone without DST
     */
    gettimeofday(&tv, &tz);
    mpr->timezone = -tz.tz_minuteswest * MS_PER_MIN;
#endif
    return 0;
}


int mprCompareTime(MprTime t1, MprTime t2)
{
    if (t1 < t2) {
        return -1;
    } else if (t1 == t2) {
        return 0;
    }
    return 1;
}


void mprDecodeLocalTime(MprCtx ctx, struct tm *tp, MprTime when)
{
    decodeTime(ctx, tp, when, 1);
}


void mprDecodeUniversalTime(MprCtx ctx, struct tm *tp, MprTime when)
{
    decodeTime(ctx, tp, when, 0);
}


char *mprFormatLocalTime(MprCtx ctx, MprTime time)
{
    struct tm   tm;
    mprDecodeLocalTime(ctx, &tm, time);
    return mprFormatTime(ctx, MPR_DEFAULT_DATE, &tm);
}


/*
    Returns time in milliseconds since the epoch: 0:0:0 UTC Jan 1 1970.
 */
MprTime mprGetTime(MprCtx ctx)
{
#if VXWORKS
    struct timespec  tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return (MprTime) (((MprTime) tv.tv_sec) * 1000) + (tv.tv_nsec / (1000 * 1000));
#else
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    return (MprTime) (((MprTime) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
#endif
}


/*
    Return the number of milliseconds until the given timeout has expired.
 */
MprTime mprGetRemainingTime(MprCtx ctx, MprTime mark, uint timeout)
{
    MprTime     now, diff;

    now = mprGetTime(ctx);
    diff = (now - mark);

    if (diff < 0) {
        /*
            Detect time going backwards
         */
        diff = 0;
    }
    return (timeout - diff);
}


/*
    Get the elapsed time since a time marker
 */
MprTime mprGetElapsedTime(MprCtx ctx, MprTime mark)
{
    return mprGetTime(ctx) - mark;
}


/*
    Get the timezone offset including DST
 */
int mprGetTimeZoneOffset(MprCtx ctx, MprTime when)
{
    MprTime     alternate, secs;
    struct tm   t;
    int         offset;

    alternate = when;
    secs = when / MS_PER_SEC;
    if (secs < MIN_TIME || secs > MAX_TIME) {
        /* secs overflows time_t on this platform. Need to map to an alternate valid year */
        decodeTime(ctx, &t, when, 0);
        t.tm_year = 110;
        alternate = makeTime(ctx, &t);
    }
    t.tm_isdst = -1;
    if (localTime(ctx, &t, alternate) < 0) {
        localTime(ctx, &t, time(0) * MS_PER_SEC);
    }
    offset = getTimeZoneOffsetFromTm(ctx, &t);
    return offset;
}


/*
    Make a time value interpreting "tm" as a local time
 */
MprTime mprMakeTime(MprCtx ctx, struct tm *tp)
{
    MprTime     when, alternate;
    struct tm   t;
    int         offset, year, rc;

    when = makeTime(ctx, tp);
    year = tp->tm_year;
    if (MIN_YEAR <= year && year <= MAX_YEAR) {
        rc = localTime(ctx, &t, when);
        offset = getTimeZoneOffsetFromTm(ctx, &t);
    } else {
        t = *tp;
        t.tm_year = 110;
        alternate = makeTime(ctx, &t);
        rc = localTime(ctx, &t, alternate);
        offset = getTimeZoneOffsetFromTm(ctx, &t);
    }
    return when - offset;
}


MprTime mprMakeUniversalTime(MprCtx ctx, struct tm *tp)
{
    return makeTime(ctx, tp);
}



static int localTime(MprCtx ctx, struct tm *timep, MprTime time)
{
#if BLD_UNIX_LIKE || WINCE
    time_t when = (time_t) (time / MS_PER_SEC);
    if (localtime_r(&when, timep) == 0) {
        return MPR_ERR;
    }
#else
    struct tm   *tp;
    time_t when = (time_t) (time / MS_PER_SEC);
    if ((tp = localtime(&when)) == 0) {
        return MPR_ERR;
    }
    *timep = *tp;
#endif
    return 0;
}

struct tm *universalTime(MprCtx ctx, struct tm *timep, MprTime time)
{
#if BLD_UNIX_LIKE || WINCE
    time_t when = (time_t) (time / MS_PER_SEC);
    return gmtime_r(&when, timep);
#else
    struct tm   *tp;
    time_t      when;
    when = (time_t) (time / MS_PER_SEC);
    if ((tp = gmtime(&when)) == 0) {
        return 0;
    }
    *timep = *tp;
    return timep;
#endif
}


/*
    Return the timezone offset (including DST) in msec. local == (UTC + offset)
    Assumes a valid "tm" with isdst correctly set.
 */
static int getTimeZoneOffsetFromTm(MprCtx ctx, struct tm *tp)
{
#if BLD_WIN_LIKE
    int                     offset;
    TIME_ZONE_INFORMATION   tinfo;
    GetTimeZoneInformation(&tinfo);
    offset = tinfo.Bias;
    if (tp->tm_isdst) {
        offset += tinfo.DaylightBias;
    } else {
        offset += tinfo.StandardBias;
    }
    return -offset * 60 * MS_PER_SEC;
#elif VXWORKS
    char  *tze, *p;
    int   offset = 0;
    if ((tze = getenv("TIMEZONE")) != 0) {
        if ((p = strchr(tze, ':')) != 0) {
            if ((p = strchr(tze, ':')) != 0) {
                offset = -mprAtoi(++p, 10) * MS_PER_MIN;
            }
        }
        if (tp->tm_isdst) {
            offset += MS_PER_HOUR;
        }
    }
    return offset;
#elif BLD_UNIX_LIKE && !CYGWIN
    return tp->tm_gmtoff * MS_PER_SEC;
#else
    struct timezone     tz;
    struct timeval      tv;
    int offset;
    gettimeofday(&tv, &tz);
    offset = -tz.tz_minuteswest * MS_PER_MIN;
    if (tp->tm_isdst) {
        offset += MS_PER_HOUR;
    }
    return offset;
#endif
}

/*
    Convert "struct tm" to MprTime
 */
static MprTime makeTime(MprCtx ctx, struct tm *tp)
{
    MprTime     days;
    int         year, month;

    year = tp->tm_year + 1900 + tp->tm_mon / 12; 
    month = tp->tm_mon % 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    days = daysSinceEpoch(year);
    days += leapYear(year) ? leapMonthStart[month] : normalMonthStart[month];
    days += tp->tm_mday - 1;
    return (days * MS_PER_DAY) + ((((((tp->tm_hour * 60)) + tp->tm_min) * 60) + tp->tm_sec) * MS_PER_SEC);
}


static MprTime daysSinceEpoch(int year)
{
    int     days;

    days = ((MprTime) 365) * (year - 1970);
    days += ((year-1) / 4) - (1970 / 4);
    days -= ((year-1) / 100) - (1970 / 100);
    days += ((year-1) / 400) - (1970 / 400);
    return days;
}


static int leapYear(int year)
{
    if (year % 4) {
        return 0;
    } else if (year % 400 == 0) {
        return 1;
    } else if (year % 100 == 0) {
        return 0;
    }
    return 1;
}


static int getMonth(int year, int day)
{
    int     *days, i;

    days = leapYear(year) ? leapMonthStart : normalMonthStart;
    for (i = 1; days[i]; i++) {
        if (day < days[i]) {
            return i - 1;
        }
    }
    return 11;
}


static int getYear(MprTime when)
{
    MprTime     ms;
    int         year;

    year = 1970 + (int) (when / MS_PER_YEAR);
    ms = daysSinceEpoch(year) * MS_PER_DAY;
    if (ms > when) {
        return year - 1;
    } else if (ms + (((MprTime) MS_PER_DAY) * (365 + leapYear(year))) <= when) {
        return year + 1;
    }
    return year;
}


MprTime floorDiv(MprTime x, MprTime divisor)
{
    if (x < 0) {
        return (x - divisor + 1) / divisor;
    } else {
        return x / divisor;
    }
}


/*
    Decode an MprTime into components in a "struct tm" 
 */
static void decodeTime(MprCtx ctx, struct tm *tp, MprTime when, bool local)
{
    MprTime     timeForZoneCalc;
    struct tm   t;
    char        *zoneName;
    int         year, offset, dst;

    zoneName = 0;
    offset = dst = 0;

    if (local) {
        timeForZoneCalc = when;
        if (when < MIN_TIME || when > MAX_TIME) {
            /*
                Can't use localTime on this date. Map to an alternate date with a valid year.
             */
            decodeTime(ctx, &t, when, 0);
            t.tm_year = 110;
            timeForZoneCalc = makeTime(ctx, &t);
        }
        t.tm_isdst = -1;
        if (localTime(ctx, &t, timeForZoneCalc) == 0) {
            offset = getTimeZoneOffsetFromTm(ctx, &t);
            dst = t.tm_isdst;
        }
#if BLD_UNIX_LIKE && !CYGWIN
        zoneName = (char*) t.tm_zone;
#endif
        when += offset;
    }
    year = getYear(when);

    tp->tm_year     = year - 1900;
#if UNUSED
    tp->tm_hour     = (int) ((when / MS_PER_HOUR) % 24);
    tp->tm_min      = (int) ((when / MS_PER_MIN) % 60);
    tp->tm_sec      = (int) ((when / MS_PER_SEC) % 60);
    tp->tm_wday     = (int) (((when / MS_PER_DAY) + 4) % 7);
    tp->tm_yday     = (int) ((when / MS_PER_DAY) - daysSinceEpoch(year));
#else
    tp->tm_hour     = (int) (floorDiv(when, MS_PER_HOUR) % 24);
    tp->tm_min      = (int) (floorDiv(when, MS_PER_MIN) % 60);
    tp->tm_sec      = (int) (floorDiv(when, MS_PER_SEC) % 60);
    tp->tm_wday     = (int) ((floorDiv(when, MS_PER_DAY) + 4) % 7);
    tp->tm_yday     = (int) (floorDiv(when, MS_PER_DAY) - daysSinceEpoch(year));
#endif
    tp->tm_mon      = getMonth(year, tp->tm_yday);
    tp->tm_isdst    = dst != 0;
#if BLD_UNIX_LIKE && !CYGWIN
    tp->tm_gmtoff   = offset / MS_PER_SEC;
    tp->tm_zone     = zoneName;
#endif
    if (tp->tm_hour < 0) {
        tp->tm_hour += 24;
    }
    if (tp->tm_min < 0) {
        tp->tm_min += 60;
    }
    if (tp->tm_sec < 0) {
        tp->tm_sec += 60;
    }
    if (tp->tm_wday < 0) {
        tp->tm_wday += 7;
    }
    if (tp->tm_yday < 0) {
        tp->tm_yday += 365;
    }
    if (leapYear(year)) {
        tp->tm_mday = tp->tm_yday - leapMonthStart[tp->tm_mon] + 1;
    } else {
        tp->tm_mday = tp->tm_yday - normalMonthStart[tp->tm_mon] + 1;
    }
    mprAssert(tp->tm_hour >= 0);
    mprAssert(tp->tm_min >= 0);
    mprAssert(tp->tm_sec >= 0);
    mprAssert(tp->tm_wday >= 0);
    mprAssert(tp->tm_mon >= 0);
    mprAssert(tp->tm_yday >= 0);
    mprAssert(tp->tm_yday < 365 || (tp->tm_yday < 366 && leapYear(year)));
    mprAssert(tp->tm_mday >= 1);
}


/*
    Format a time string. This uses strftime if available and so the supported formats vary from platform to platform.
    Strftime should supports some of these these formats:

     %A      full weekday name (Monday)
     %a      abbreviated weekday name (Mon)
     %B      full month name (January)
     %b      abbreviated month name (Jan)
     %C      century. Year / 100. (0-N)
     %c      standard date and time representation
     %D      date (%m/%d/%y)
     %d      day-of-month (01-31)
     %e      day-of-month with a leading space if only one digit ( 1-31)
     %F      same as %Y-%m-%d
     %H      hour (24 hour clock) (00-23)
     %h      same as %b
     %I      hour (12 hour clock) (01-12)
     %j      day-of-year (001-366)
     %k      hour (24 hour clock) (0-23)
     %l      the hour (12-hour clock) as a decimal number (1-12); single digits are preceded by a blank.
     %M      minute (00-59)
     %m      month (01-12)
     %n      a newline
     %P      lower case am / pm
     %p      AM / PM
     %R      same as %H:%M
     %r      same as %H:%M:%S %p
     %S      second (00-59)
     %s      seconds since epoch
     %T      time (%H:%M:%S)
     %t      a tab.
     %U      week-of-year, first day sunday (00-53)
     %u      the weekday (Monday as the first day of the week) as a decimal number (1-7).
     %v      is equivalent to ``%e-%b-%Y''.
     %W      week-of-year, first day monday (00-53)
     %w      weekday (0-6, sunday is 0)
     %X      standard time representation
     %x      standard date representation
     %Y      year with century
     %y      year without century (00-99)
     %Z      timezone name
     %z      offset from UTC (-hhmm or +hhmm)
     %+      national representation of the date and time (the format is similar to that produced by date(1)).
     %%      percent sign

     Some platforms may also support the following format extensions:
     %E*     POSIX locale extensions. Where "*" is one of the characters: c, C, x, X, y, Y.
             representations. 
     %G      a year as a decimal number with century. This year is the one that contains the greater part of
             the week (Monday as the first day of the week).
     %g      the same year as in ``%G'', but as a decimal number without century (00-99).
     %O*     POSIX locale extensions. Where "*" is one of the characters: d, e, H, I, m, M, S, u, U, V, w, W, y.
             Additionly %OB implemented to represent alternative months names (used standalone, without day mentioned). 
     %V      the week number of the year (Monday as the first day of the week) as a decimal number (01-53). If the week 
             containing January 1 has four or more days in the new year, then it is week 1; otherwise it is the last 
             week of the previous year, and the next week is week 1.

    Useful formats:
        RFC822: "%a, %d %b %Y %H:%M:%S %Z           "Fri, 07 Jan 2003 12:12:21 PDT"
                "%T %F                              "12:12:21 2007-01-03"
                "%v                                 "07-Jul-2003"
 */

#if HAS_STRFTIME
/*
    Preferred implementation as strftime() will be localized
 */
char *mprFormatTime(MprCtx ctx, cchar *fmt, struct tm *tp)
{
    struct tm       tm;
    char            localFmt[MPR_MAX_STRING];
    cchar           *cp;
    char            *dp, *endp, *sign;
    char            buf[MPR_MAX_STRING];
    int             value, size;

    dp = localFmt;
    if (fmt == 0) {
        fmt = MPR_DEFAULT_DATE;
    }
    if (tp == 0) {
        mprDecodeLocalTime(ctx, &tm, mprGetTime(ctx));
        tp = &tm;
    }
    endp = &localFmt[sizeof(localFmt) - 1];
    for (cp = fmt, size = sizeof(localFmt) - 1; *cp && dp < &localFmt[sizeof(localFmt) - 32]; size = endp - dp - 1) {
        if (*cp == '%') {
            *dp++ = *cp++;
        again:
            switch (*cp) {
            case '+':
                if (tp->tm_mday < 10) {
                    /* Some platforms don't support 'e' so avoid it here. Put a double space before %d */
                    mprSprintf(dp, size, "%s  %d %s", "a %b", tp->tm_mday, "%H:%M:%S %Z %Y");
                } else {
                    strcpy(dp, "a %b %d %H:%M:%S %Z %Y");
                }
                dp += strlen(dp);
                cp++;
                break;

            case 'C':
                dp--;
                mprItoa(dp, size, (int64) (1900 + tp->tm_year) / 100, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'D':
                strcpy(dp, "m/%d/%y");
                dp += 7;
                cp++;
                break;

            case 'e':                       /* day of month (1-31). Single digits preceeded by blanks */
                dp--;
                if (tp->tm_mday < 10) {
                    *dp++ = ' ';
                }
                mprItoa(dp, size - 1, (int64) tp->tm_mday, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'E':
                /* Skip the 'E' */
                cp++;
                goto again;
            
            case 'F':
                strcpy(dp, "Y-%m-%d");
                dp += 7;
                cp++;
                break;

            case 'h':
                *dp++ = 'b';
                cp++;
                break;

            case 'k':
                dp--;
                if (tp->tm_hour < 10) {
                    *dp++ = ' ';
                }
                mprItoa(dp, size - 1, (int64) tp->tm_hour, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'l':
                dp--;
                value = tp->tm_hour;
                if (value < 10) {
                    *dp++ = ' ';
                }
                if (value > 12) {
                    value -= 12;
                }
                mprItoa(dp, size - 1, (int64) value, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'n':
                dp[-1] = '\n';
                cp++;
                break;

            case 'O':
                /* Skip the 'O' */
                cp++;
                goto again;
            
            case 'P':
                dp--;
                strcpy(dp, (tp->tm_hour > 11) ? "pm" : "am");
                dp += 2;
                cp++;
                break;

            case 'R':
                strcpy(dp, "H:%M");
                dp += 4;
                cp++;
                break;

            case 'r':
                strcpy(dp, "I:%M:%S %p");
                dp += 10;
                cp++;
                break;

            case 's':
                dp--;
                mprItoa(dp, size, (int64) mprMakeTime(ctx, tp) / MS_PER_SEC, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'T':
                strcpy(dp, "H:%M:%S");
                dp += 7;
                cp++;
                break;

            case 't':
                dp[-1] = '\t';
                cp++;
                break;

            case 'u':
                dp--;
                value = tp->tm_wday;
                if (value == 0) {
                    value = 7;
                }
                mprItoa(dp, size, (int64) value, 10);
                dp += strlen(dp);
                cp++;
                break;

            case 'v':
                /* Inline '%e' */
                dp--;
                if (tp->tm_mday < 10) {
                    *dp++ = ' ';
                }
                mprItoa(dp, size - 1, (int64) tp->tm_mday, 10);
                dp += strlen(dp);
                cp++;
                strcpy(dp, "-%b-%Y");
                dp += 6;
                break;

            case 'z':
                dp--;
                value = mprGetTimeZoneOffset(ctx, makeTime(ctx, tp)) / (MS_PER_SEC * 60);
                sign = (value < 0) ? "-" : "";
                if (value < 0) {
                    value = -value;
                }
                mprSprintf(dp, size, "%s%02d%02d", sign, value / 60, value % 60);
                dp += strlen(dp);
                cp++;
                break;

            default: 
                if (strchr(VALID_FMT, (int) *cp) != 0) {
                    *dp++ = *cp++;
                } else {
                    dp--;
                    cp++;
                }
                break;
            }
        } else {
            *dp++ = *cp++;
        }
    }
    *dp = '\0';
    fmt = localFmt;
    if (*fmt == '\0') {
        fmt = "%a %b %d %H:%M:%S %Z %Y";
    }
    if (strftime(buf, sizeof(buf) - 1, fmt, tp) > 0) {
        buf[sizeof(buf) - 1] = '\0';
        return mprStrdup(ctx, buf);
    }
    return 0;
}


#else /* !HAS_STRFTIME */
/*
    This implementation is used only on platforms that don't support strftime. This version is not localized.
 */
static void digits(MprBuf *buf, int count, int fill, int value)
{
    char    tmp[32]; 
    int     i, j; 

    if (value < 0) {
        mprPutCharToBuf(buf, '-');
        value = -value;
    }
    for (i = 0; value && i < count; i++) { 
        tmp[i] = '0' + value % 10; 
        value /= 10; 
    } 
    if (fill) {
        for (j = i; j < count; j++) {
            mprPutCharToBuf(buf, fill);
        }
    }
    while (i-- > 0) {
        mprPutCharToBuf(buf, tmp[i]); 
    } 
}


static char *getTimeZoneName(MprCtx ctx, struct tm *tp)
{
#if BLD_WIN_LIKE
    TIME_ZONE_INFORMATION   tz;
    WCHAR                   *wzone;
    GetTimeZoneInformation(&tz);
    wzone = tp->tm_isdst ? tz.DaylightName : tz.StandardName;
    return mprToAsc(ctx, wzone);
#else
    tzset();
    return mprStrdup(ctx, tp->tm_zone);
#endif
}


char *mprFormatTime(MprCtx ctx, cchar *fmt, struct tm *tp)
{
    struct tm       tm;
    MprBuf          *buf;
    char            *result, *zone;
    int             w, value;

    if (fmt == 0) {
        fmt = MPR_DEFAULT_DATE;
    }
    if (tp == 0) {
        mprDecodeLocalTime(ctx, &tm, mprGetTime(ctx));
        tp = &tm;
    }
    if ((buf = mprCreateBuf(ctx, 64, -1)) == 0) {
        return 0;
    }
    while ((*fmt != '\0')) {
        if (*fmt++ != '%') {
            mprPutCharToBuf(buf, fmt[-1]);
            continue;
        }
    again:
        switch (*fmt++) {
        case '%' :                                      /* percent */
            mprPutCharToBuf(buf, '%');
            break;

        case '+' :                                      /* date (Mon May 18 23:29:50 PDT 2011) */
            mprPutStringToBuf(buf, abbrevDay[tp->tm_wday]);
            mprPutCharToBuf(buf, ' ');
            mprPutStringToBuf(buf, abbrevMonth[tp->tm_mon]);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 2, ' ', tp->tm_mday);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 2, '0', tp->tm_hour);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_sec);
            mprPutCharToBuf(buf, ' ');
            zone = getTimeZoneName(ctx, tp);
            mprPutStringToBuf(buf, zone);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 4, 0, tp->tm_year + 1900);
            mprFree(zone);
            break;

        case 'A' :                                      /* full weekday (Sunday) */
            mprPutStringToBuf(buf, day[tp->tm_wday]);
            break;

        case 'a' :                                      /* abbreviated weekday (Sun) */
            mprPutStringToBuf(buf, abbrevDay[tp->tm_wday]);
            break;

        case 'B' :                                      /* full month (January) */
            mprPutStringToBuf(buf, month[tp->tm_mon]);
            break;

        case 'b' :                                      /* abbreviated month (Jan) */
            mprPutStringToBuf(buf, abbrevMonth[tp->tm_mon]);
            break;

        case 'C' :                                      /* century number (19, 20) */
            digits(buf, 2, '0', (1900 + tp->tm_year) / 100);
            break;

        case 'c' :                                      /* preferred date+time in current locale */
            mprPutStringToBuf(buf, abbrevDay[tp->tm_wday]);
            mprPutCharToBuf(buf, ' ');
            mprPutStringToBuf(buf, abbrevMonth[tp->tm_mon]);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 2, ' ', tp->tm_mday);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 2, '0', tp->tm_hour);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_sec);
            mprPutCharToBuf(buf, ' ');
            digits(buf, 4, 0, tp->tm_year + 1900);
            break;

        case 'D' :                                      /* mm/dd/yy */
            digits(buf, 2, '0', tp->tm_mon + 1);
            mprPutCharToBuf(buf, '/');
            digits(buf, 2, '0', tp->tm_mday);
            mprPutCharToBuf(buf, '/');
            digits(buf, 2, '0', tp->tm_year - 100);
            break;

        case 'd' :                                      /* day of month (01-31) */
            digits(buf, 2, '0', tp->tm_mday);
            break;

        case 'E':
            /* Skip the 'E' */
            goto again;

        case 'e':                                       /* day of month (1-31). Single digits preceeded by a blank */
            digits(buf, 2, ' ', tp->tm_mday);
            break;

        case 'F':                                       /* %m/%d/%y */
            digits(buf, 4, 0, tp->tm_year + 1900);
            mprPutCharToBuf(buf, '-');
            digits(buf, 2, '0', tp->tm_mon + 1);
            mprPutCharToBuf(buf, '-');
            digits(buf, 2, '0', tp->tm_mday);
            break;

        case 'H':                                       /* hour using 24 hour clock (00-23) */
            digits(buf, 2, '0', tp->tm_hour);
            break;

        case 'h':                                       /* Same as %b */
            mprPutStringToBuf(buf, abbrevMonth[tp->tm_mon]);
            break;

        case 'I':                                       /* hour using 12 hour clock (00-01) */
            digits(buf, 2, '0', (tp->tm_hour % 12) ? tp->tm_hour % 12 : 12);
            break;

        case 'j':                                       /* julian day (001-366) */
            digits(buf, 3, '0', tp->tm_yday+1);
            break;

        case 'k':                                       /* hour (0-23). Single digits preceeded by a blank */
            digits(buf, 2, ' ', tp->tm_hour);
            break;

        case 'l':                                       /* hour (1-12). Single digits preceeded by a blank */
            digits(buf, 2, ' ', tp->tm_hour < 12 ? tp->tm_hour : (tp->tm_hour - 12));
            break;

        case 'M':                                       /* minute as a number (00-59) */
            digits(buf, 2, '0', tp->tm_min);
            break;

        case 'm':                                       /* month as a number (01-12) */
            digits(buf, 2, '0', tp->tm_mon + 1);
            break;

        case 'n':                                       /* newline */
            mprPutCharToBuf(buf, '\n');
            break;

        case 'O':
            /* Skip the 'O' */
            goto again;

        case 'p':                                       /* AM/PM */
            mprPutStringToBuf(buf, (tp->tm_hour > 11) ? "PM" : "AM");
            break;

        case 'P':                                       /* am/pm */
            mprPutStringToBuf(buf, (tp->tm_hour > 11) ? "pm" : "am");
            break;

        case 'R':
            digits(buf, 2, '0', tp->tm_hour);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            break;

        case 'r':
            digits(buf, 2, '0', (tp->tm_hour % 12) ? tp->tm_hour % 12 : 12);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_sec);
            mprPutCharToBuf(buf, ' ');
            mprPutStringToBuf(buf, (tp->tm_hour > 11) ? "PM" : "AM");
            break;

        case 'S':                                       /* seconds as a number (00-60) */
            digits(buf, 2, '0', tp->tm_sec);
            break;

        case 's':                                       /* seconds since epoch */
            mprPutFmtToBuf(buf, "%d", mprMakeTime(ctx, tp) / MS_PER_SEC);
            break;

        case 'T':
            digits(buf, 2, '0', tp->tm_hour);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_sec);
            break;

        case 't':                                       /* Tab */
            mprPutCharToBuf(buf, '\t');
            break;

        case 'U':                                       /* week number (00-53. Staring with first Sunday */
            w = tp->tm_yday / 7;
            if (tp->tm_yday % 7 > tp->tm_wday) {
                w++;
            }
            digits(buf, 2, '0', w);
            break;

        case 'u':                                       /* Week day (1-7) */
            value = tp->tm_wday;
            if (value == 0) {
                value = 7;
            }
            digits(buf, 1, 0, tp->tm_wday == 0 ? 7 : tp->tm_wday);
            break;

        case 'v':                                       /* %e-%b-%Y */
            digits(buf, 2, ' ', tp->tm_mday);
            mprPutCharToBuf(buf, '-');
            mprPutStringToBuf(buf, abbrevMonth[tp->tm_mon]);
            mprPutCharToBuf(buf, '-');
            digits(buf, 4, '0', tp->tm_year + 1900);
            break;

        case 'W':                                       /* week number (00-53). Staring with first Monday */
            w = (tp->tm_yday + 7 - (tp->tm_wday ?  (tp->tm_wday - 1) : (7 - 1))) / 7;
            digits(buf, 2, '0', w);
            break;

        case 'w':                                       /* day of week (0-6) */
            digits(buf, 1, '0', tp->tm_wday);
            break;

        case 'X':                                       /* preferred time without date */
            digits(buf, 2, '0', tp->tm_hour);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_min);
            mprPutCharToBuf(buf, ':');
            digits(buf, 2, '0', tp->tm_sec);
            break;

        case 'x':                                      /* preferred date without time */
            digits(buf, 2, '0', tp->tm_mon + 1);
            mprPutCharToBuf(buf, '/');
            digits(buf, 2, '0', tp->tm_mday);
            mprPutCharToBuf(buf, '/');
            digits(buf, 2, '0', tp->tm_year + 1900);
            break;

        case 'Y':                                       /* year as a decimal including century (1900) */
            digits(buf, 4, '0', tp->tm_year + 1900);
            break;

        case 'y':                                       /* year without century (00-99) */
            digits(buf, 2, '0', tp->tm_year % 100);
            break;

        case 'Z':                                       /* Timezone */
            zone = getTimeZoneName(ctx, tp);
            mprPutStringToBuf(buf, zone);
            mprFree(zone);
            break;

        case 'z':
            value = mprGetTimeZoneOffset(ctx, makeTime(ctx, tp)) / (MS_PER_SEC * 60);
            if (value < 0) {
                mprPutCharToBuf(buf, '-');
                value = -value;
            }
            digits(buf, 2, '0', value / 60);
            digits(buf, 2, '0', value % 60);
            break;

        case 'g':
        case 'G':
        case 'V':
            break;

        default:
            mprPutCharToBuf(buf, '%');
            mprPutCharToBuf(buf, fmt[-1]);
            break;
        }
    }
    mprAddNullToBuf(buf);
    result = mprStealBuf(ctx, buf);
    mprFree(buf);
    return result;
}
#endif /* HAS_STRFTIME */



static int lookupSym(Mpr *mpr, cchar *token, int kind)
{
    TimeToken   *tt;

    if ((tt = (TimeToken*) mprLookupHash(mpr->timeTokens, token)) == 0) {
        return -1;
    }
    if (kind != (tt->value & TOKEN_MASK)) {
        return -1;
    }
    return tt->value & ~TOKEN_MASK;
}


static int getNum(Mpr *mpr, char **token, int sep)
{
    int     num;

    if (*token == 0) {
        return 0;
    }

    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    return num;
}


static int getNumOrSym(Mpr *mpr, char **token, int sep, int kind, int *isAlpah)
{
    char    *cp;
    int     num;

    mprAssert(token && *token);

    if (*token == 0) {
        return 0;
    }
    if (isalpha((int) **token)) {
        *isAlpah = 1;
        cp = strchr(*token, sep);
        if (cp) {
            *cp++ = '\0';
        }
        num = lookupSym(mpr, *token, kind);
        *token = cp;
        return num;
    }
    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    *isAlpah = 0;
    return num;
}


static bool allDigits(cchar *token)
{
    cchar   *cp;

    for (cp = token; *cp; cp++) {
        if (!isdigit((int) *cp)) {
            return 0;
        }
    }
    return 1;
} 


static void swapDayMonth(struct tm *tp)
{
    int     tmp;

    tmp = tp->tm_mday;
    tp->tm_mday = tp->tm_mon;
    tp->tm_mon = tmp;
}


/*
    Parse the a date/time string according to the given zoneFlags and return the result in *time. Missing date items 
    may be provided via the defaults argument.
 */ 
int mprParseTime(MprCtx ctx, MprTime *time, cchar *dateString, int zoneFlags, struct tm *defaults)
{
    Mpr             *mpr;
    TimeToken       *tt;
    struct tm       tm;
    char            *str, *next, *token, *cp, *sep;
    int64           value;
    int             kind, hour, min, negate, value1, value2, value3, alpha, alpha2, alpha3;
    int             dateSep, offset, zoneOffset, explicitZone, fullYear;

    mpr = mprGetMpr(ctx);

    offset = 0;
    zoneOffset = 0;
    explicitZone = 0;
    sep = ", \t";
    cp = 0;
    next = 0;
    fullYear = 0;

    /*
        Set these mandatory values to -1 so we can tell if they are set to valid values
        WARNING: all the calculations use tm_year with origin 0, not 1900. It is fixed up below.
     */
    tm.tm_year = -MAXINT;
    tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_sec = tm.tm_min = tm.tm_wday = -1;
    tm.tm_min = tm.tm_sec = tm.tm_yday = -1;
#if BLD_UNIX_LIKE && !CYGWIN
    tm.tm_gmtoff = 0;
    tm.tm_zone = 0;
#endif

    /*
        Set to -1 to cause mktime will try to determine if DST is in effect
     */
    tm.tm_isdst = -1;

    str = mprStrdup(ctx, dateString);
    mprStrLower(str);

    /*
        Handle ISO dates: "2009-05-21t16:06:05.000z
     */
    if (strchr(str, ' ') == 0 && strchr(str, '-') && str[strlen(str) - 1] == 'z') {
        for (cp = str; *cp; cp++) {
            if (*cp == '-') {
                *cp = '/';
            } else if (*cp == 't' && cp > str && isdigit((uchar) cp[-1]) && isdigit((uchar) cp[1]) ) {
                *cp = ' ';
            }
        }
    }
    token = mprStrTok(str, sep, &next);

    while (token && *token) {
        if (allDigits(token)) {
            /*
                Parse either day of month or year. Priority to day of month. Format: <29> Jan <15> <2011>
             */ 
            value = mprAtoi(token, 10);
            if (value > 3000) {
                *time = value;
                mprFree(str);
                return 0;
            } else if (value > 32 || (tm.tm_mday >= 0 && tm.tm_year == -MAXINT)) {
                if (value >= 1000) {
                    fullYear = 1;
                }
                tm.tm_year = (int) value - 1900;
            } else if (tm.tm_mday < 0) {
                tm.tm_mday = (int) value;
            }

        } else if ((*token == '+') || (*token == '-') ||
                ((strncmp(token, "gmt", 3) == 0 || strncmp(token, "utc", 3) == 0) &&
                ((cp = strchr(&token[3], '+')) != 0 || (cp = strchr(&token[3], '-')) != 0))) {
            /*
                Timezone. Format: [GMT|UTC][+-]NN[:]NN
             */
            if (!isalpha((int) *token)) {
                cp = token;
            }
            negate = *cp == '-' ? -1 : 1;
            cp++;
            hour = getNum(mpr, &cp, timeSep);
            if (hour >= 100) {
                hour /= 100;
            }
            min = getNum(mpr, &cp, timeSep);
            zoneOffset = negate * (hour * 60 + min);
            explicitZone = 1;

        } else if (isalpha((int) *token)) {
            if ((tt = (TimeToken*) mprLookupHash(mpr->timeTokens, token)) != 0) {
                kind = tt->value & TOKEN_MASK;
                value = tt->value & ~TOKEN_MASK; 
                switch (kind) {

                case TOKEN_DAY:
                    tm.tm_wday = (int) value;
                    break;

                case TOKEN_MONTH:
                    tm.tm_mon = (int) value;
                    break;

                case TOKEN_OFFSET:
                    /* Named timezones or symbolic names like: tomorrow, yesterday, next week ... */ 
                    /* Units are seconds */
                    offset += (int) value;
                    break;

                case TOKEN_ZONE:
                    zoneOffset = (int) value;
                    explicitZone = 1;
                    break;

                default:
                    /* Just ignore unknown values */
                    break;
                }
            }

        } else if ((cp = strchr(token, timeSep)) != 0 && isdigit((int) token[0])) {
            /*
                Time:  10:52[:23]
                Must not parse GMT-07:30
             */
            tm.tm_hour = getNum(mpr, &token, timeSep);
            tm.tm_min = getNum(mpr, &token, timeSep);
            tm.tm_sec = getNum(mpr, &token, timeSep);

        } else {
            dateSep = '/';
            if (strchr(token, dateSep) == 0) {
                dateSep = '-';
                if (strchr(token, dateSep) == 0) {
                    dateSep = '.';
                    if (strchr(token, dateSep) == 0) {
                        dateSep = 0;
                    }
                }
            }
            if (dateSep) {
                /*
                    Date:  07/28/2011, 07/28/08, Jan/28/2011, Jaunuary-28-2011, 28-jan-2011
                    Support order: dd/mm/yy, mm/dd/yy and yyyy/mm/dd
                    Support separators "/", ".", "-"
                 */
                value1 = getNumOrSym(mpr, &token, dateSep, TOKEN_MONTH, &alpha);
                value2 = getNumOrSym(mpr, &token, dateSep, TOKEN_MONTH, &alpha2);
                value3 = getNumOrSym(mpr, &token, dateSep, TOKEN_MONTH, &alpha3);

                if (value1 > 31) {
                    /* yy/mm/dd */
                    tm.tm_year = value1;
                    tm.tm_mon = value2;
                    tm.tm_mday = value3;

                } else if (value1 > 12 || alpha2) {
                    /* 
                        dd/mm/yy 
                        Can't detect 01/02/03  This will be evaluated as Jan 2 2003 below.
                     */  
                    tm.tm_mday = value1;
                    tm.tm_mon = value2;
                    tm.tm_year = value3;

                } else {
                    /*
                        The default to parse is mm/dd/yy unless the mm value is out of range
                     */
                    tm.tm_mon = value1;
                    tm.tm_mday = value2;
                    tm.tm_year = value3;
                }
            }
        }
        token = mprStrTok(NULL, sep, &next);
    }
    mprFree(str);

    /*
        Y2K fix and rebias
     */
    if (0 <= tm.tm_year && tm.tm_year < 100 && !fullYear) {
        if (tm.tm_year < 50) {
            tm.tm_year += 2000;
        } else {
            tm.tm_year += 1900;
        }
    }    
    if (tm.tm_year >= 1900) {
        tm.tm_year -= 1900;
    }

    /*
        Convert back to origin 0 for months
     */
    if (tm.tm_mon > 0) {
        tm.tm_mon--;
    }

    /*
        Validate and fill in missing items with defaults
     */
    validateTime(mpr, &tm, defaults);

    if (zoneFlags == MPR_LOCAL_TIMEZONE && !explicitZone) {
        *time = mprMakeTime(ctx, &tm);
    } else {
        *time = mprMakeUniversalTime(ctx, &tm);
        *time += -(zoneOffset * MS_PER_MIN);
    }
    *time += (offset * MS_PER_SEC);
    return 0;
}


static void validateTime(MprCtx ctx, struct tm *tp, struct tm *defaults)
{
    struct tm   empty;

    /*
        Fix apparent day-mon-year ordering issues. Can't fix everything!
     */
    if ((12 <= tp->tm_mon && tp->tm_mon <= 31) && 0 <= tp->tm_mday && tp->tm_mday <= 11) {
        /*
            Looks like day month are swapped
         */
        swapDayMonth(tp);
    }

    if (tp->tm_year != -MAXINT && tp->tm_mon >= 0 && tp->tm_mday >= 0 && tp->tm_hour >= 0) {
        /*  Everything defined */
        return;
    }

    /*
        Use empty time if missing
     */
    if (defaults == NULL) {
        memset(&empty, 0, sizeof(empty));
        defaults = &empty;
        empty.tm_mday = 1;
        empty.tm_year = 70;
    }
    if (tp->tm_hour < 0 && tp->tm_min < 0 && tp->tm_sec < 0) {
        tp->tm_hour = defaults->tm_hour;
        tp->tm_min = defaults->tm_min;
        tp->tm_sec = defaults->tm_sec;
    }

    /*
        Get weekday, if before today then make next week
     */
    if (tp->tm_wday >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + (tp->tm_wday - defaults->tm_wday + 7) % 7;
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }

    /*
        Get month, if before this month then make next year
     */
    if (tp->tm_mon >= 0 && tp->tm_mon <= 11 && tp->tm_mday < 0) {
        if (tp->tm_year == -MAXINT) {
            tp->tm_year = defaults->tm_year + (((tp->tm_mon - defaults->tm_mon) < 0) ? 1 : 0);
        }
        tp->tm_mday = defaults->tm_mday;
    }

    /*
        Get date, if before current time then make tomorrow
     */
    if (tp->tm_hour >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + ((tp->tm_hour - defaults->tm_hour) < 0 ? 1 : 0);
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_year == -MAXINT) {
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_mon < 0) {
        tp->tm_mon = defaults->tm_mon;
    }
    if (tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday;
    }
    if (tp->tm_yday < 0) {
        tp->tm_yday = (leapYear(tp->tm_year + 1900) ? 
            leapMonthStart[tp->tm_mon] : normalMonthStart[tp->tm_mon]) + tp->tm_mday - 1;
    }
    if (tp->tm_hour < 0) {
        tp->tm_hour = defaults->tm_hour;
    }
    if (tp->tm_min < 0) {
        tp->tm_min = defaults->tm_min;
    }
    if (tp->tm_sec < 0) {
        tp->tm_sec = defaults->tm_sec;
    }
}


/*
    Compatibility for windows and VxWorks
 */
#if BLD_WIN_LIKE || VXWORKS
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    #if BLD_WIN_LIKE
        FILETIME        fileTime;
        MprTime         now;
        static int      tzOnce;

        if (NULL != tv) {
            /* Convert from 100-nanosec units to microsectonds */
            GetSystemTimeAsFileTime(&fileTime);
            now = ((((MprTime) fileTime.dwHighDateTime) << BITS(uint)) + ((MprTime) fileTime.dwLowDateTime));
            now /= 10;
            now -= TIME_GENESIS;
            tv->tv_sec = (long) (now / 1000000);
            tv->tv_usec = (long) (now % 1000000);
        }
        if (NULL != tz) {
            TIME_ZONE_INFORMATION   zone;
            int                     rc, bias;
            rc = GetTimeZoneInformation(&zone);
            bias = (int) zone.Bias;
            if (rc == TIME_ZONE_ID_DAYLIGHT) {
                tz->tz_dsttime = 1;
            } else {
                tz->tz_dsttime = 0;
            }
            tz->tz_minuteswest = bias;
        }
        return 0;

    #elif VXWORKS
        struct tm       tm;
        struct timespec now;
        time_t          t;
        char            *tze, *p;
        int             rc;

        if ((rc = clock_gettime(CLOCK_REALTIME, &now)) == 0) {
            tv->tv_sec  = now.tv_sec;
            tv->tv_usec = (now.tv_nsec + 500) / MS_PER_SEC;
            if ((tze = getenv("TIMEZONE")) != 0) {
                if ((p = strchr(tze, ':')) != 0) {
                    if ((p = strchr(tze, ':')) != 0) {
                        tz->tz_minuteswest = mprAtoi(++p, 10);
                    }
                }
                t = tickGet();
                tz->tz_dsttime = (localtime_r(&t, &tm) == 0) ? tm.tm_isdst : 0;
            }
        }
        return rc;
    #endif
}
#endif /* BLD_WIN_LIKE || VXWORKS */

/*
    High resolution timer
 */
#if MPR_HIGH_RES_TIMER
    #if BLD_UNIX_LIKE
        uint64 mprGetTicks() {
            uint64  now;
            __asm__ __volatile__ ("rdtsc" : "=A" (now));
            return now;
        }
    #elif BLD_WIN_LIKE
        uint64 mprGetTicks() {
            LARGE_INTEGER  now;
            QueryPerformanceCounter(&now);
            return (((uint64) now.HighPart) << 32) + now.HighPart;
        }
    #else
        uint64 mprGetTicks() {
            return (uint64) mprGetTime();
        }
    #endif
#else 
    uint64 mprGetTicks() {
        return 0;
    }
#endif


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprTime.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprUnicode.c"
 */
/************************************************************************/

/**
 *  mprUnicode.c - Unicode 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#if KEEP


/*
 *  Allocate a new (empty) unicode string
 */
uni *mprAllocUs(MprCtx ctx)
{
    mprAssert(ctx);

    return mprAllocObjZeroed(ctx, uni);
}



/*
 *  Grow the string buffer for a unicode string
 */
static int growUs(uni *us, int len)
{
    mprAssert(us);
    mprAssert(len >= 0);

    if (len < us->length) {
        return 0;
    }

    us->str = mprRealloc(us, us->str, len);
    if (us->str == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    us->length = len;
    return 0;
}



/*
 *  Convert a ASCII string to UTF-8/16
 */
static int memToUs(uni *us, const uchar *str, int len)
{
    uniData   *up;
    cchar       *cp;

    mprAssert(us);
    mprAssert(str);

    if (len > us->length) {
        if (growUs(us, len) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    us->length = len;

#if BLD_FEATURE_UTF16
    cp = (cchar*) str;
    up = us->str;
    while (*cp) {
        *up++ = *cp++;
    }
#else
    memcpy((void*) us->str, str, len);
#endif

    return 0;
}



/*
 *  Convert a C string to a newly allocated unicode string
 */
uni *mprStrToUs(MprCtx ctx, cchar *str)
{
    uni   *us;
    int     len;

    mprAssert(ctx);
    mprAssert(str);

    us = mprAllocUs(ctx);
    if (us == 0) {
        return 0;
    }

    if (str == 0) {
        str = "";
    }

    len = strlen(str);

    if (memToUs(us, (const uchar*) str, len) < 0) {
        return 0;
    }
    
    return us;
}



/*
 *  Convert a memory buffer to a newly allocated unicode string
 */
uni *mprMemToUs(MprCtx ctx, const uchar *buf, int len)
{
    uni   *us;

    mprAssert(ctx);
    mprAssert(buf);

    us = mprAllocUs(ctx);
    if (us == 0) {
        return 0;
    }

    if (memToUs(us, buf, len) < 0) {
        return 0;
    }
    
    return us;
}



/*
 *  Convert a unicode string newly allocated C string
 */
char *mprUsToStr(uni *us)
{
    char    *str, *cp;

    mprAssert(us);

    str = cp = mprAlloc(us, us->length + 1);
    if (cp == 0) {
        return 0;
    }

#if BLD_FEATURE_UTF16
{
    uniData   *up;
    int         i;

    up = us->str;
    for (i = 0; i < us->length; i++) {
        cp[i] = up[i];
    }
}
#else
    mprStrcpy(cp, us->length, us->str);
#endif
    return str;
}



/*
 *  Copy one unicode string to another. No allocation
 */
static void copyUs(uni *dest, uni *src)
{
    mprAssert(dest);
    mprAssert(src);
    mprAssert(dest->length <= src->length);
    mprAssert(dest->str);
    mprAssert(src->str);

    memcpy(dest->str, src->str, src->length * sizeof(uniData));
    dest->length = src->length;
}



/*
 *  Copy one unicode string to another. Grow the destination unicode string buffer as required.
 */
int mprCopyUs(uni *dest, uni *src)
{
    mprAssert(dest);
    mprAssert(src);

    dest->length = src->length;

    if (src->length > dest->length) {
        if (growUs(dest, src->length) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }

    copyUs(dest, src);

    return 0;
}



/*
 *  Catenate a unicode string onto another.
 */
int mprCatUs(uni *dest, uni *src)
{
    int     len;

    len = dest->length + src->length;
    if (growUs(dest, len) < 0) {
        return MPR_ERR_NO_MEMORY;
    }

    memcpy(&dest->str[dest->length], src->str, src->length * sizeof(uniData));
    dest->length += src->length;

    return 0;
}



/*
 *  Catenate a set of unicode string arguments onto another.
 */
int mprCatUsArgs(uni *dest, uni *src, ...)
{
    va_list     args;
    uni       *us;
    int         len;

    va_start(args, src);

    len = 0;
    us = src;
    for (us = src; us; ) {
        us = va_arg(args, uni*);
        len += us->length;
    }

    if (growUs(dest, len) < 0) {
        return MPR_ERR_NO_MEMORY;
    }

    va_start(args, src);
    for (us = src; us; ) {
        us = va_arg(args, uni*);
        
        memcpy(&dest->str[dest->length], src->str, src->length * sizeof(uniData));
        dest->length += src->length;
    }

    va_end(args);
    
    return 0;
}



/*
 *  Duplicate a unicode string by allocating a new unicode string and copying the source data.
 */
uni *mprDupUs(uni *src)
{
    uni   *dest;

    dest = mprAllocUs(src);
    if (dest == 0) {
        return 0;
    }

    copyUs(dest, src);

    return dest;
}




/*
 *  Copy a C string into an existing unicode string.
 */
int mprCopyStrToUs(uni *dest, cchar *str)
{
    mprAssert(dest);
    mprAssert(str);

    return memToUs(dest, (const uchar*) str, strlen(str));
}



/*
 *  Return the lenght of a unicoded string.
 */
int mprGetUsLength(uni *us)
{
    mprAssert(us);

    return us->length;
}




/*
 *  Return the index in a unicode string of a given unicode character code. Return -1 if not found.
 */
int mprContainsChar(uni *us, int charPat)
{
    int     i;

    mprAssert(us);

    for (i = 0; i < us->length; i++) {
        if (us->str[i] == charPat) {
            return i;
        }
    }
    return -1;
}



/*
 *  Return TRUE if a unicode string contains a given unicode string.
 */
int mprContainsUs(uni *us, uni *pat)
{
    int     i, j;

    mprAssert(us);
    mprAssert(pat);
    mprAssert(pat->str);

    if (pat == 0 || pat->str == 0) {
        return 0;
    }
    
    for (i = 0; i < us->length; i++) {
        for (j = 0; j < pat->length; j++) {
            if (us->str[i] != pat->str[j]) {
                break;
            }
        }
        if (j == pat->length) {
            return i;
        }
    }
    return -1;
}



/*
 *  Return TRUE if a unicode string contains a given unicode string after doing a case insensitive comparison.
 */
int mprContainsCaselessUs(uni *us, uni *pat)
{
    int     i, j;

    mprAssert(us);
    mprAssert(pat);
    mprAssert(pat->str);

    for (i = 0; i < us->length; i++) {
        for (j = 0; j < pat->length; j++) {
            if (tolower(us->str[i]) != tolower(pat->str[j])) {
                break;
            }
        }
        if (j == pat->length) {
            return i;
        }
    }
    return -1;
}



/*
 *  Return TRUE if a unicode string contains a given C string.
 */
int mprContainsStr(uni *us, cchar *pat)
{
    int     i, j, len;

    mprAssert(us);
    mprAssert(pat);

    if (pat == 0 || *pat == '\0') {
        return 0;
    }
    
    len = strlen(pat);
    
    for (i = 0; i < us->length; i++) {
        for (j = 0; j < len; j++) {
            if (us->str[i] != pat[j]) {
                break;
            }
        }
        if (j == len) {
            return i;
        }
    }
    return -1;
}



#if FUTURE
int mprContainsPattern(uni *us, MprRegex *pat)
{
    return 0;
}
#endif




uni *mprTrimUs(uni *us, uni *pat)
{
    return 0;
}



int mprTruncateUs(uni *us, int len)
{
    mprAssert(us);

    mprAssert(us->length >= len);

    if (us->length < len) {
        return MPR_ERR_WONT_FIT;
    }

    us->length = len;
    return 0;
}



uni *mprSubUs(uni *src, int start, int len)
{
    uni   *dest;

    mprAssert(src);
    mprAssert(start >= 0);
    mprAssert(len > 0);
    mprAssert((start + len) <= src->length);

    if ((start + len) > src->length) {
        return 0;
    }

    dest = mprAllocUs(src);
    if (dest == 0) {
        return 0;
    }

    if (growUs(dest, len) < 0) {
        mprFree(dest);
        return 0;
    }
    memcpy(dest->str, &src->str[start], len * sizeof(uniData));
    dest->length = len;

    return dest;
}







void mprUsToLower(uni *us)
{
    int     i;

    mprAssert(us);
    mprAssert(us->str);

    for (i = 0; i < us->length; i++) {
        us->str[i] = tolower(us->str[i]);
    }
}



void mprUsToUpper(uni *us)
{
    int     i;

    mprAssert(us);
    mprAssert(us->str);

    for (i = 0; i < us->length; i++) {
        us->str[i] = toupper(us->str[i]);
    }
}




uni *mprTokenizeUs(uni *us, uni *delim, int *last)
{
    return 0;
}




int mprFormatUs(uni *us, int maxSize, cchar *fmt, ...)
{
    return 0;
}



int mprScanUs(uni *us, cchar *fmt, ...)
{
    return 0;
}



#else
void __mprDummyUnicode() {}
#endif

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprUnicode.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprUnix.c"
 */
/************************************************************************/

/**
 *  mprUnix.c - Unix specific adaptions
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_UNIX_LIKE

MprOsService *mprCreateOsService(MprCtx ctx)
{
    MprOsService    *os;

    os = mprAllocObj(ctx, MprOsService);
    if (os == 0) {
        return 0;
    }

    umask(022);

    /*
     *  Cleanup the environment. IFS is often a security hole
     */
     putenv("IFS=\t ");
    return os;
}


int mprStartOsService(MprOsService *os)
{
    /* 
     *  Open a syslog connection
     */
#if SOLARIS
    openlog(mprGetAppName(os), LOG_CONS, LOG_LOCAL0);
#else
    openlog(mprGetAppName(os), LOG_CONS || LOG_PERROR, LOG_LOCAL0);
#endif
    return 0;
}


void mprStopOsService(MprOsService *os)
{
}


int mprGetRandomBytes(MprCtx ctx, char *buf, int length, int block)
{
    int     fd, sofar, rc;

    fd = open((block) ? "/dev/random" : "/dev/urandom", O_RDONLY, 0666);
    if (fd < 0) {
        return MPR_ERR_CANT_OPEN;
    }

    sofar = 0;
    do {
        rc = read(fd, &buf[sofar], length);
        if (rc < 0) {
            mprAssert(0);
            return MPR_ERR_CANT_READ;
        }
        length -= rc;
        sofar += rc;
    } while (length > 0);
    close(fd);
    return 0;
}


MprModule *mprLoadModule(MprCtx ctx, cchar *moduleName, cchar *initFunction)
{
#if BLD_CC_DYN_LOAD
    MprModuleEntry  fn;
    MprModule       *mp;
    char            *path, *module;
    void            *handle;

    mprAssert(moduleName && *moduleName);

    mp = 0;
    path = 0;
    module = mprGetNormalizedPath(ctx, moduleName);

    if (mprSearchForModule(ctx, module, &path) < 0) {
        mprError(ctx, "Can't find module \"%s\" in search path \"%s\"", moduleName, mprGetModuleSearchPath(ctx));
    } else {
        mprLog(ctx, MPR_INFO, "Loading module %s from %s", module, path);
        if ((handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL)) == 0) {
            mprError(ctx, "Can't load module %s\nReason: \"%s\"",  path, dlerror());
        } else if (initFunction) {
            if ((fn = (MprModuleEntry) dlsym(handle, initFunction)) != 0) {
                if ((mp = (fn)(ctx, path)) == 0) {
                    mprError(ctx, "Initialization for module %s failed", module);
                    dlclose(handle);
                } else {
                    mp->handle = handle;
                }
            } else {
                mprError(ctx, "Can't load module %s\nReason: can't find function \"%s\"",  path, initFunction);
                dlclose(handle);
            }
        }
    }
    mprFree(path);
    mprFree(module);
    return mp;
#else
    mprError(ctx, "Product built without the ability to load modules dynamically");
    return 0;
#endif
}


void mprSleep(MprCtx ctx, int milliseconds)
{
    struct timespec timeout;
    int             rc;

    mprAssert(milliseconds >= 0);
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_nsec = (milliseconds % 1000) * 1000000;
    do {
        rc = nanosleep(&timeout, &timeout);
    } while (rc < 0 && errno == EINTR);
}


void mprUnloadModule(MprModule *mp)
{
    mprStopModule(mp);
    if (mp->handle) {
        dlclose(mp->handle);
    }
    mprRemoveItem(mprGetMpr(mp)->moduleService->modules, mp);
}


/*  
 *  Write a message in the O/S native log (syslog in the case of linux)
 */
void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
    char    *msg;
    int     sflag;

    if (flags & MPR_FATAL_SRC) {
        msg = "fatal error: ";
        sflag = LOG_ERR;

    } else if (flags & MPR_ASSERT_SRC) {
        msg = "program assertion error: ";
        sflag = LOG_WARNING;

    } else {
        msg = "error: ";
        sflag = LOG_WARNING;
    }
    syslog(sflag, "%s %s: %s\n", mprGetAppName(ctx), msg, message);
}

#else
void __dummyMprUnix() {}
#endif /* BLD_UNIX_LIKE */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprUnix.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprUrl.c"
 */
/************************************************************************/

/**
 *  mprUrl.c - Url manipulation routines
 *
 *  Miscellaneous routines to parse and enscape URLs.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Character escape/descape matching codes. Generated by charGen.
 */
static uchar charMatch[256] = {
     0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 7, 5, 6, 4, 7, 6, 7, 7, 2, 0, 4, 0, 0, 4,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 6, 7, 4, 7, 6,
     4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 0,
     2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 
};

/*
 *  Basic mime type support
 */
static char *mimeTypes[] = {
    "ai", "application/postscript",
    "asc", "text/plain",
    "au", "audio/basic",
    "avi", "video/x-msvideo",
    "bin", "application/octet-stream",
    "bmp", "image/bmp",
    "class", "application/octet-stream",
    "css", "text/css",
    "dll", "application/octet-stream",
    "doc", "application/msword",
    "ejs", "text/html",
    "eps", "application/postscript",
    "es", "application/x-javascript",
    "exe", "application/octet-stream",
    "gif", "image/gif",
    "gz", "application/x-gzip",
    "htm", "text/html",
    "html", "text/html",
    "ico", "image/x-icon",
    "jar", "application/octet-stream",
    "jpeg", "image/jpeg",
    "jpg", "image/jpeg",
    "js", "application/javascript",
    "mp3", "audio/mpeg",
    "pdf", "application/pdf",
    "png", "image/png",
    "ppt", "application/vnd.ms-powerpoint",
    "ps", "application/postscript",
    "ra", "audio/x-realaudio",
    "ram", "audio/x-pn-realaudio",
    "rmm", "audio/x-pn-realaudio",
    "rtf", "text/rtf",
    "rv", "video/vnd.rn-realvideo",
    "so", "application/octet-stream",
    "swf", "application/x-shockwave-flash",
    "tar", "application/x-tar",
    "tgz", "application/x-gzip",
    "tiff", "image/tiff",
    "txt", "text/plain",
    "wav", "audio/x-wav",
    "xls", "application/vnd.ms-excel",
    "zip", "application/zip",
    "php", "application/x-appweb-php",
    "pl", "application/x-appweb-perl",
    "py", "application/x-appweb-python",
    NULL, NULL,
};

/*
 *  Max size of the port specification in a URL
 */
#define MAX_PORT_LEN 8

/*
 *  Parse a complete URI. This accepts full URIs with schemes (http:) and partial URLs
 */
MprUri *mprParseUri(MprCtx ctx, cchar *uri)
{
    MprUri  *up;
    char    *tok, *cp, *last_delim, *hostbuf, *urlTok;
    int     c, len, ulen, http;

    mprAssert(uri);

    up = mprAllocObj(ctx, MprUri);
    if (up == 0) {
        return 0;
    }

    /*
     *  Allocate a single buffer to hold all the cracked fields.
     */
    ulen = (int) strlen(uri);
    len = ulen * 2 + 3;

    up->originalUri = mprStrdup(up, uri);
    up->parsedUriBuf = (char*) mprAlloc(up, len * sizeof(char));

    hostbuf = &up->parsedUriBuf[ulen+1];
    strcpy(up->parsedUriBuf, uri);
    urlTok = up->parsedUriBuf;

    /*
     *  Defaults for missing URL fields
     */
    up->url = "/";
    up->scheme = "http";
    up->host = "localhost";
    up->port = 80;
    up->query = 0;
    up->ext = 0;
    up->secure = 0;

    http = 0;
    tok = 0;

    if (strncmp(urlTok, "https://", 8) == 0) {
        up->secure = 1;
        up->port = 443;
        tok = &urlTok[8];
        http++;

    } else if (strncmp(urlTok, "http://", 7) == 0) {
        tok = &urlTok[7];
        http++;
    }

    if (http) {
        up->scheme = urlTok;
        up->host = tok;
        tok[-3] = '\0';
        for (cp = tok; *cp; cp++) {
            if (*cp == '/') {
                break;
            }
            if (*cp == ':') {
                *cp++ = '\0';
                up->port = atoi(cp);
                tok = cp;
            }
        }
        if ((cp = strchr(tok, '/')) != NULL) {
            c = *cp;
            *cp = '\0';
            mprStrcpy(hostbuf, ulen + 1, up->host);
            *cp = c;
            up->host = hostbuf;
            up->url = cp;
            tok = cp;
        }

    } else {
        up->url = urlTok;
        tok = urlTok;
    }

    if ((cp = strchr(tok, '#')) != NULL) {
        *cp++ = '\0';
        up->reference = cp;
        tok = cp;
    }
    if ((cp = strchr(tok, '?')) != NULL) {
        *cp++ = '\0';
        up->query = cp;
        tok = up->query;
    }

    if ((cp = strrchr(up->url, '.')) != NULL) {
        if ((last_delim = strrchr(up->url, '/')) != NULL) {
            if (last_delim <= cp) {
                up->ext = cp + 1;
#if BLD_WIN_LIKE
                mprStrLower(up->ext);
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            mprStrLower(up->ext);
#endif
        }
    } else {
        len = (int) strlen(up->url);
    }

    return up;
}


/*
 *  Format a fully qualified URI
 */
char *mprFormatUri(MprCtx ctx, cchar *scheme, cchar *host, int port, cchar *path, cchar *query)
{
    char    portBuf[16], *uri;
    cchar   *portDelim, *pathDelim, *queryDelim;
    int     defaultPort, len;

    len = 0;

    if (scheme == 0 || *scheme == '\0') {
        scheme = "http";
    }
    len += (int) strlen(scheme) + 3;                            /* Add 3 for "://" */

    defaultPort = (strcmp(scheme, "http") == 0) ? 80 : 443;

    if (host == 0 || *host == '\0') {
        host = "localhost";
    }

    /*
     *  Hosts with integral port specifiers override
     */
    if (strchr(host, ':')) {
        portDelim = 0;
    } else {
        if (port != defaultPort) {
            mprItoa(portBuf, sizeof(portBuf), port, 10);
            portDelim = ":";
        } else {
            portBuf[0] = '\0';
            portDelim = "";
        }
        len += (int) strlen(portBuf) + (int) strlen(portDelim);
    }
    len += (int) strlen(host);

    if (path) {
        pathDelim = (*path == '/') ? "" :  "/";
    } else {
        pathDelim = "/";
        path = "";
    }
    len += (int) strlen(path) + (int) strlen(pathDelim);

    if (query && *query) {
        queryDelim = "?";
    } else {
        queryDelim = query = "";
    }
    len += (int) strlen(query) + (int) strlen(queryDelim);
    len += 1;                                               /* Add one for the null */

    uri = mprAlloc(ctx, len);
    if (uri == 0) {
        return 0;
    }

    if (portDelim) {
        uri = mprAsprintf(ctx, len, "%s://%s%s%s%s%s%s%s", scheme, host, portDelim, portBuf, pathDelim, 
            path, queryDelim, query);
    } else {
        uri = mprAsprintf(ctx, len, "%s://%s%s%s%s%s", scheme, host, pathDelim, path, queryDelim, query);
    }
    if (uri == 0) {
        return 0;
    }
    return uri;
}


/*
 *  Url encode by encoding special characters with hex equivalents.
 */
char *mprUrlEncode(MprCtx ctx, cchar *inbuf)
{
    static cchar    hexTable[] = "0123456789abcdef";
    uchar           c;
    cchar           *ip;
    char            *result, *op;
    int             len;

    mprAssert(inbuf);
    mprAssert(inbuf);

    for (len = 1, ip = inbuf; *ip; ip++, len++) {
        if (charMatch[(int) *ip] & MPR_HTTP_ESCAPE_URL) {
            len += 2;
        }
    }

    if ((result = mprAlloc(ctx, len)) == 0) {
        return 0;
    }

    ip = inbuf;
    op = result;

    while ((c = (uchar) (*inbuf++)) != 0) {
        if (c == ' ') {
            *op++ = '+';
        } else if (charMatch[c] & MPR_HTTP_ESCAPE_URL) {
            *op++ = '%';
            *op++ = hexTable[c >> 4];
            *op++ = hexTable[c & 0xf];
        } else {
            *op++ = c;
        }
    }
    mprAssert(op < &result[len]);
    *op = '\0';
    return result;
}


/*
 *  Decode a string using URL encoding.
 */
char *mprUrlDecode(MprCtx ctx, cchar *inbuf)
{
    cchar   *ip;
    char    *result, *op;
    int     num, i, c;

    mprAssert(inbuf);

    if ((result = mprStrdup(ctx, inbuf)) == 0) {
        return 0;
    }

    for (op = result, ip = inbuf; *ip; ip++, op++) {
        if (*ip == '+') {
            *op = ' ';

        } else if (*ip == '%' && isxdigit((int) ip[1]) && isxdigit((int) ip[2])) {
            ip++;
            num = 0;
            for (i = 0; i < 2; i++, ip++) {
                c = tolower((int) *ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else if (c >= '0' && c <= '9') {
                    num = (num * 16) + c - '0';
                } else {
                    /* Bad chars in URL */
                    return 0;
                }
            }
            *op = (char) num;
            ip--;

        } else {
            *op = *ip;
        }
    }
    *op = '\0';
    return result;
}


/*
 *  Escape a shell command
 */
char *mprEscapeCmd(MprCtx ctx, cchar *cmd, int escChar)
{
    uchar   c;
    cchar   *ip;
    char    *result, *op;
    int     len;

    mprAssert(cmd);

    for (len = 1, ip = cmd; *ip; ip++, len++) {
        if (charMatch[(int) *ip] & MPR_HTTP_ESCAPE_SHELL) {
            len++;
        }
    }
    if ((result = mprAlloc(ctx, len)) == 0) {
        return 0;
    }

    if (escChar == 0) {
        escChar = '\\';
    }
    op = result;
    while ((c = (uchar) *cmd++) != 0) {
#if BLD_WIN_LIKE
        if ((c == '\r' || c == '\n') && *cmd != '\0') {
            c = ' ';
            continue;
        }
#endif
        if (charMatch[c] & MPR_HTTP_ESCAPE_SHELL) {
            *op++ = escChar;
        }
        *op++ = c;
    }
    mprAssert(op < &result[len]);
    *op = '\0';
    return result;
}


/*
 *  Escape HTML to escape defined characters (prevent cross-site scripting)
 */
char *mprEscapeHtml(MprCtx ctx, cchar *html)
{
    cchar   *ip;
    char    *result, *op;
    int     len;

    for (len = 1, ip = html; *ip; ip++, len++) {
        if (charMatch[(int) *ip] & MPR_HTTP_ESCAPE_HTML) {
            len += 5;
        }
    }
    if ((result = mprAlloc(ctx, len)) == 0) {
        return 0;
    }

    /*
     *  Leave room for the biggest expansion
     */
    op = result;
    while (*html != '\0') {
        if (charMatch[(uchar) *html] & MPR_HTTP_ESCAPE_HTML) {
            if (*html == '&') {
                strcpy(op, "&amp;");
                op += 5;
            } else if (*html == '<') {
                strcpy(op, "&lt;");
                op += 4;
            } else if (*html == '>') {
                strcpy(op, "&gt;");
                op += 4;
            } else if (*html == '#') {
                strcpy(op, "&#35;");
                op += 5;
            } else if (*html == '(') {
                strcpy(op, "&#40;");
                op += 5;
            } else if (*html == ')') {
                strcpy(op, "&#41;");
                op += 5;
            } else if (*html == '"') {
                strcpy(op, "&quot;");
                op += 5;
            } else {
                mprAssert(0);
            }
            html++;
        } else {
            *op++ = *html++;
        }
    }
    mprAssert(op < &result[len]);
    *op = '\0';
    return result;
}


/*
 *  Validate a Url
 *
 *  WARNING: this code will not fully validate against certain Windows 95/98/Me bugs. Don't use this code in these
 *  operating systems without modifying this code to remove "con", "nul", "aux", "clock$" and "config$" in either
 *  case from the URI. The MprFileSystem::stat() will perform these checks to determine if a file is a device file.
 */
char *mprValidateUrl(MprCtx ctx, char *url)
{
    char    *sp, *dp, *xp, *dot;

    if ((url = mprStrdup(ctx, url)) == 0) {
        return 0;
    }

    /*
     *  Remove multiple path separators and map '\\' to '/' for windows
     */
    sp = dp = url;
    while (*sp) {
#if BLD_WIN_LIKE
        if (*sp == '\\') {
            *sp = '/';
        }
#endif
        if (sp[0] == '/' && sp[1] == '/') {
            sp++;
        } else {
            *dp++ = *sp++;
        }
    }
    *dp = '\0';

    dot = strchr(url, '.');
    if (dot == 0) {
        return url;
    }

    /*
     *  Per RFC 1808, remove "./" segments
     */
    dp = dot;
    for (sp = dot; *sp; ) {
        if (*sp == '.' && sp[1] == '/' && (sp == url || sp[-1] == '/')) {
            sp += 2;
        } else {
            *dp++ = *sp++;
        }
    }
    *dp = '\0';

    /*
     *  Remove trailing "."
     */
    if ((dp == &url[1] && url[0] == '.') ||
        (dp > &url[1] && dp[-1] == '.' && dp[-2] == '/')) {
        *--dp = '\0';
    }

    /*
     *  Remove "../"
     */
    for (sp = dot; *sp; ) {
        if (*sp == '.' && sp[1] == '.' && sp[2] == '/' && (sp == url || sp[-1] == '/')) {
            xp = sp + 3;
            sp -= 2;
            if (sp < url) {
                sp = url;
            } else {
                while (sp >= url && *sp != '/') {
                    sp--;
                }
                sp++;
            }
            dp = sp;
            while ((*dp++ = *xp) != 0) {
                xp++;
            }
        } else {
            sp++;
        }
    }
    *dp = '\0';

    /*
     *  Remove trailing "/.."
     */
    if (sp == &url[2] && *url == '.' && url[1] == '.') {
        *url = '\0';
    } else {
        if (sp > &url[2] && sp[-1] == '.' && sp[-2] == '.' && sp[-3] == '/') {
            sp -= 4;
            if (sp < url) {
                sp = url;
            } else {
                while (sp >= url && *sp != '/') {
                    sp--;
                }
                sp++;
            }
            *sp = '\0';
        }
    }
#if BLD_WIN_LIKE
    if (*url != '\0') {
        char    *cp;
        /*
         *  There was some extra URI past the matching alias prefix portion.  Windows will ignore trailing "."
         *  and " ". We must reject here as the URL probably won't match due to the trailing character and the
         *  copyHandler will return the unprocessed content to the user. Bad.
         */
        cp = &url[strlen(url) - 1];
        while (cp >= url) {
            if (*cp == '.' || *cp == ' ') {
                *cp-- = '\0';
            } else {
                break;
            }
        }
    }
#endif
    return url;
}


cchar *mprLookupMimeType(MprCtx ctx, cchar *ext)
{
    Mpr     *mpr;
    char    **cp;
    cchar   *ep, *mtype;

    mprAssert(ext);

    mpr = mprGetMpr(ctx);
    if (mpr->mimeTypes == 0) {
        mpr->mimeTypes = mprCreateHash(mpr, 67);
        for (cp = mimeTypes; cp[0]; cp += 2) {
            mprAddHash(mpr->mimeTypes, cp[0], cp[1]);
        }
    }
    if ((ep = strrchr(ext, '.')) != 0) {
        ext = &ep[1];
    }
    mtype = (cchar*) mprLookupHash(mpr->mimeTypes, ext);
    if (mtype == 0) {
        return "application/octet-stream";
    }
    return mtype;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprUrl.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprVxworks.c"
 */
/************************************************************************/

/**
 *  mprVxworks.c - Vxworks specific adaptions
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#if VXWORKS



MprOsService *mprCreateOsService(MprCtx ctx)
{
    return mprAllocObj(ctx, MprOsService);
}


int mprStartOsService(MprOsService *os)
{
    return 0;
}


void mprStopOsService(MprOsService *os)
{
}


int access(const char *path, int mode)
{
    struct stat sbuf;

    return stat((char*) path, &sbuf);
}


int mprGetRandomBytes(MprCtx ctx, char *buf, int length, int block)
{
    int     i;

    for (i = 0; i < length; i++) {
        buf[i] = (char) (mprGetTime(ctx) >> i);
    }
    return 0;
}


MprModule *mprLoadModule(MprCtx ctx, cchar *moduleName, cchar *initFunction)
{
    MprModule       *mp;
    MprModuleEntry  fn;
    SYM_TYPE        symType;
    void            *handle;
    char            entryPoint[MPR_MAX_FNAME], *module, *path;
    int             fd;

    mprAssert(moduleName && *moduleName);

    mp = 0;
    path = 0;
    module = mprGetNormalizedPath(ctx, moduleName);

    if (mprSearchForModule(ctx, module, &path) < 0) {
        mprError(ctx, "Can't find module \"%s\" in search path \"%s\"", moduleName, mprGetModuleSearchPath(ctx));

    } else if (moduleFindByName((char*) path) == 0) {
        if ((fd = open(path, O_RDONLY, 0664)) < 0) {
            mprError(ctx, "Can't open module \"%s\"", path);

        } else {
            mprLog(ctx, MPR_INFO, "Loading module %s", path);
            errno = 0;
            handle = loadModule(fd, LOAD_GLOBAL_SYMBOLS);
            if (handle == 0 || errno != 0) {
                close(fd);
                if (handle) {
                    unldByModuleId(handle, 0);
                }
                mprError(ctx, "Can't load module %s", path);

            } else {
                close(fd);
                if (initFunction) {
#if BLD_HOST_CPU_ARCH == MPR_CPU_IX86 || BLD_HOST_CPU_ARCH == MPR_CPU_IX64
                    mprSprintf(entryPoint, sizeof(entryPoint), "_%s", initFunction);
#else
                    mprStrcpy(entryPoint, sizeof(entryPoint), initFunction);
#endif
                    fn = 0;
                    if (symFindByName(sysSymTbl, entryPoint, (char**) &fn, &symType) == -1) {
                        mprError(ctx, "Can't find symbol %s when loading %s", initFunction, path);

                    } else {
                        if ((mp = (fn)(ctx, path)) == 0) {
                            mprError(ctx, "Initialization for %s failed.", path);
                        } else {
                            mp->handle = handle;
                        }
                    }
                }
            }
        }
    }
    mprFree(path);
    mprFree(module);
    return mp;
}


void mprSleep(MprCtx ctx, int milliseconds)
{
    struct timespec timeout;
    int             rc;

    mprAssert(milliseconds >= 0);
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_nsec = (milliseconds % 1000) * 1000000;
    do {
        rc = nanosleep(&timeout, &timeout);
    } while (rc < 0 && errno == EINTR);
}


void mprUnloadModule(MprModule *mp)
{
    mprRemoveItem(mprGetMpr(mp)->moduleService->modules, mp);
    unldByModuleId((MODULE_ID) mp->handle, 0);
}


void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
}


uint mprGetpid(void) {
    return taskIdSelf();
}


int fsync(int fd) { 
    return 0; 
}


int ftruncate(int fd, off_t offset) { 
    return 0; 
}

int usleep(uint msec)
{
    struct timespec     timeout;
    int                 rc;

    timeout.tv_sec = msec / (1000 * 1000);
    timeout.tv_nsec = msec % (1000 * 1000) * 1000;
    do {
        rc = nanosleep(&timeout, &timeout);
    } while (rc < 0 && errno == EINTR);
    return 0;
}


#else
void __dummyMprVxWorks() {}
#endif /* VXWORKS */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprVxworks.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprWait.c"
 */
/************************************************************************/

/**
 *  mprWait.c - Wait for I/O service.
 *
 *  This module provides wait management for sockets and other file descriptors and allows users to create wait
 *  handlers which will be called when I/O events are detected. Multiple backends (one at a time) are supported.
 *
 *  This module is thread-safe.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  handlerDestructor(MprWaitHandler *wp);

/*
 *  Initialize the service
 */
MprWaitService *mprCreateWaitService(Mpr *mpr)
{
    MprWaitService  *ws;

    ws = mprAllocObjZeroed(mpr, MprWaitService);
    if (ws == 0) {
        return 0;
    }
    ws->flags = 0;
    ws->maskGeneration = 0;
    ws->lastMaskGeneration = -1;
    ws->handlers = mprCreateList(ws);

#if BLD_WIN_LIKE && !WINCE
    ws->socketMessage = MPR_SOCKET_MESSAGE;
#endif
#if BLD_FEATURE_MULTITHREAD
    ws->mutex = mprCreateLock(ws);
#endif
    mprInitSelectWait(ws);
    return ws;
}


/*
 *  Create a handler. Priority is only observed when multi-threaded.
 */
MprWaitHandler *mprCreateWaitHandler(MprCtx ctx, int fd, int mask, MprWaitProc proc, void *data, int pri, int flags)
{
    MprWaitService  *ws;
    MprWaitHandler  *wp;

    mprAssert(fd >= 0);

    ws = mprGetMpr(ctx)->waitService;

    if (mprGetListCount(ws->handlers) == FD_SETSIZE) {
        mprError(ws, "io: Too many io handlers: %d\n", FD_SETSIZE);
        return 0;
    }

    wp = mprAllocObjWithDestructorZeroed(ws, MprWaitHandler, handlerDestructor);
    if (wp == 0) {
        return 0;
    }
#if BLD_UNIX_LIKE || VXWORKS
    if (fd >= FD_SETSIZE) {
        mprError(ws, "File descriptor %d exceeds max io of %d", fd, FD_SETSIZE);
    }
#endif
    if (pri == 0) {
        pri = MPR_NORMAL_PRIORITY;
    }

    wp->fd              = fd;
    wp->proc            = proc;
    wp->flags           = flags | MPR_WAIT_MASK_CHANGED;
    wp->handlerData     = data;
    wp->disableMask     = -1;
    wp->waitService     = ws;
    wp->desiredMask     = mask;
#if BLD_FEATURE_MULTITHREAD
    wp->priority        = pri;
#endif

    mprLock(ws->mutex);
    if (mprAddItem(ws->handlers, wp) < 0) {
        mprUnlock(ws->mutex);
        mprFree(wp);
        return 0;
    }
    mprUnlock(ws->mutex);
    mprUpdateWaitHandler(wp, 1);
    return wp;
}


/*
 *  Wait handler Destructor. Called from mprFree.
 */
static int handlerDestructor(MprWaitHandler *wp)
{
    mprDisconnectWaitHandler(wp);
    return 0;
}


/*
 *  Disconnect a wait handler so it cannot be invoked. The memory is still intact.
 */
void mprDisconnectWaitHandler(MprWaitHandler *wp)
{
    MprWaitService      *ws;

    ws = wp->waitService;

    /*
     *  Lock the service to stabilize the list, then lock the handler to prevent callbacks. 
     */
    mprLock(ws->mutex);
    mprRemoveItem(ws->handlers, wp);

#if BLD_FEATURE_MULTITHREAD
    /*
     *  Extra measures if multi-threaded to catch worker threads that have already been dispatched.
     *  If there is an active callback on another thread, wait for it to complete.
     */
    if (wp->inUse == 0 || wp->thread == mprGetCurrentThread(ws)) {
        /*
         *  Either the callback is not active or this thread is the callback. Either case, removal is okay.
         */
        mprUnlock(ws->mutex);

    } else { 
        MprTime     mark;

        /* wp->inUse could be cleared any time - even while locked as the callback runs unlocked */
        wp->callbackComplete = mprCreateCond(wp);
        wp->flags |= MPR_WAIT_DESTROYING;
        mprUnlock(ws->mutex);

        mark = mprGetTime(ws);
        while (wp->inUse > 0) {
            if (mprWaitForCond(wp->callbackComplete, 10) == 0) {
                break;
            }
            if (mprGetElapsedTime(ws, mark) > MPR_TIMEOUT_HANDLER) {
                break;
            }
        }
    }
#endif
    ws->maskGeneration++;
    mprWakeWaitService(ws);
}


#if BLD_FEATURE_MULTITHREAD
/*
 *  Designate the required worker thread to run the callback
 */
void mprDedicateWorkerToHandler(MprWaitHandler *wp, MprWorker *worker)
{
    wp->requiredWorker = worker;
    mprDedicateWorker(worker);
}


void mprReleaseWorkerFromHandler(MprWaitHandler *wp, MprWorker *worker)
{
    wp->requiredWorker = 0;
    mprReleaseWorker(worker);
}


/*
 *  Cleanup after the callback has run. This is called once the worker is back on the idle queue, with the service locked.
 */
static void waitCleanup(MprWaitHandler *wp, MprWorker *worker)
{
    wp->inUse = 0;
    if (wp->flags & MPR_WAIT_DESTROYING) {
        mprSignalCond(wp->callbackComplete);
    } else {
        mprUpdateWaitHandler(wp, 1);
    }
}


/*
 *  Called by the mprInvokeWaitCallback either directory or indirectly via a worker thread.
 *  WARNING: Called unlocked with inUse set.
 */
static void waitCallback(MprWaitHandler *wp, MprWorker *worker)
{
    MprWaitService      *ws;

    mprAssert(wp->disableMask == 0);
    mprAssert(wp->inUse == 1);

    ws = wp->waitService;
    if (wp->flags & MPR_WAIT_DESTROYING) {
        wp->inUse = 0;
        return;
    }
    wp->thread = mprGetCurrentThread(wp);

    /* 
     *  Configure a cleanup for the callback if it has not been deleted (returns non-zero) and if there is work to do.
     */
    if ((wp->proc)(wp->handlerData, wp->presentMask) == 0) {
        if (wp->flags & (MPR_WAIT_RECALL_HANDLER | MPR_WAIT_MASK_CHANGED | MPR_WAIT_DESTROYING)) {
            if (worker == 0) {
                waitCleanup(wp, NULL);
            } else {
                worker->cleanup = (MprWorkerProc) waitCleanup;
            }
        } else {
            wp->inUse = 0;
        }
    }
}


void mprWakeWaitService(MprCtx ctx)
{
    if (mprMustWakeDispatcher(ctx)) {
        mprWakeOsWaitService(ctx);
    }
}
#endif


/*
 *  Invoke the wait handler callback. Invoked by the wait backend and indirectly via a worker thread. 
 */
void mprInvokeWaitCallback(MprWaitHandler *wp)
{
    MprWaitService      *ws;

    /* Entry with the the service locked */

    ws = wp->waitService;
    if (wp->flags & MPR_WAIT_DESTROYING) {
        return;
    }
#if BLD_FEATURE_MULTITHREAD
    mprAssert(wp->inUse > 0);

    if (wp->requiredWorker) {
        mprActivateWorker(wp->requiredWorker, (MprWorkerProc) waitCallback, (void*) wp, MPR_REQUEST_PRIORITY);
        return;
    } else {
        if (mprStartWorker(wp, (MprWorkerProc) waitCallback, (void*) wp, MPR_REQUEST_PRIORITY) == 0) {
            return;
        }
    }
    /* Can't create a new worker, so fall through and use the service events thread */
    waitCallback(wp, NULL);
#else
    /*
     *  Single-threaded - invoke the callback directly
     */
    (wp->proc)(wp->handlerData, wp->presentMask);
#endif
}


void mprSetWaitCallback(MprWaitHandler *wp, MprWaitProc newProc, int mask)
{
    mprLock(wp->waitService->mutex);
    wp->proc = newProc;
    mprSetWaitEvents(wp, mask, wp->disableMask);
    mprUnlock(wp->waitService->mutex);
}


void mprSetWaitEvents(MprWaitHandler *wp, int desiredMask, int disableMask)
{
    MprWaitService  *ws;

    ws = wp->waitService;
    mprLock(ws->mutex);
    if (wp->desiredMask != desiredMask || wp->disableMask != disableMask) {
        wp->desiredMask = desiredMask;
        wp->disableMask = disableMask;
        wp->flags |= MPR_WAIT_MASK_CHANGED;
        mprUpdateWaitHandler(wp, 1);
    }
    mprUnlock(ws->mutex);
}


void mprDisableWaitEvents(MprWaitHandler *wp)
{
    if (wp->disableMask != 0) {
        mprLock(wp->waitService->mutex);
        wp->disableMask = 0;
        wp->flags |= MPR_WAIT_MASK_CHANGED;
        mprUpdateWaitHandler(wp, 1);
        mprUnlock(wp->waitService->mutex);
    }
}


void mprEnableWaitEvents(MprWaitHandler *wp)
{
    if (wp->disableMask != -1) {
        mprLock(wp->waitService->mutex);
        wp->disableMask = -1;
        wp->flags |= MPR_WAIT_MASK_CHANGED;
        mprUpdateWaitHandler(wp, 1);
        mprUnlock(wp->waitService->mutex);
    }
}


/*
 *  Set a handler to be recalled without further I/O. May be called with a null wp.
 */
void mprRecallWaitHandler(MprWaitHandler *wp)
{
    if (wp) {
        wp->flags |= MPR_WAIT_RECALL_HANDLER;
        mprUpdateWaitHandler(wp, 1);
    }
}


#if BLD_UNIX_LIKE || VXWORKS || WINCE
void mprUpdateWaitHandler(MprWaitHandler *wp, bool wakeup)
{
    MprWaitService  *ws;

    /*
     *  If the handler callback is in-use, don't bother to awaken the wait service yet. 
     *  This routine will be recalled when inUse is zero on callback exit.
     */
    if (!wp->inUse && wp->flags & (MPR_WAIT_RECALL_HANDLER | MPR_WAIT_MASK_CHANGED)) {
        ws = wp->waitService;
        if (wp->flags & MPR_WAIT_RECALL_HANDLER) {
            ws->flags |= MPR_NEED_RECALL;
        }
        if (wp->flags & MPR_WAIT_MASK_CHANGED) {
            wp->flags &= ~MPR_WAIT_MASK_CHANGED;
            ws->maskGeneration++;
        }
        if (wakeup) {
            mprWakeWaitService(wp->waitService);
        }
    }
}
#endif

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprWait.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprWin.c"
 */
/************************************************************************/

/**
 *  mprWin.c - Windows specific adaptions
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_WIN_LIKE && !WINCE

static cchar    *getHive(cchar *key, HKEY *root);

/*
 *  Initialize the O/S platform layer
 */ 

MprOsService *mprCreateOsService(MprCtx ctx)
{
    return mprAllocObj(ctx, MprOsService);
}


int mprStartOsService(MprOsService *os)
{
    WSADATA     wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
    return 0;
}


void mprStopOsService(MprOsService *os)
{
    WSACleanup();
}


long mprGetInst(Mpr *mpr)
{
    return (long) mpr->appInstance;
}


HWND mprGetHwnd(MprCtx ctx)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    return mpr->waitService->hwnd;
}


int mprGetRandomBytes(MprCtx ctx, char *buf, int length, int block)
{
    HCRYPTPROV      prov;
    int             rc;

    rc = 0;

    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | 0x40)) {
        return mprGetError();
    }
    if (!CryptGenRandom(prov, length, buf)) {
        rc = mprGetError();
    }
    CryptReleaseContext(prov, 0);
    return rc;
}


MprModule *mprLoadModule(MprCtx ctx, cchar *moduleName, cchar *initFunction)
{
    MprModule       *mp;
    MprModuleEntry  fn;
    char            *module;
    char            *path, *name;
    void            *handle;

    mprAssert(moduleName && *moduleName);

    mp = 0;
    name = path = 0;
    module = mprGetNormalizedPath(ctx, moduleName);

    if (mprSearchForModule(ctx, module, &path) < 0) {
        mprError(ctx, "Can't find module \"%s\" in search path \"%s\"", moduleName, mprGetModuleSearchPath(ctx));

    } else {
        name = mprGetPathBase(ctx, module);
        mprLog(ctx, MPR_INFO, "Loading module %s from %s", name, path);

        if ((handle = GetModuleHandle(name)) == 0 && (handle = LoadLibrary(path)) == 0) {
            mprError(ctx, "Can't load module %s\nReason: \"%d\"\n",  path, mprGetOsError());

        } else if (initFunction) {
            if ((fn = (MprModuleEntry) GetProcAddress((HINSTANCE) handle, initFunction)) != 0) {
                if ((mp = (fn)(ctx, path)) == 0) {
                    mprError(ctx, "Initialization for module %s failed", name);
                    FreeLibrary((HINSTANCE) handle);

                } else {
                    mp->handle = handle;
                }

            } else {
                mprError(ctx, "Can't load module %s\nReason: can't find function \"%s\"\n",  name, initFunction);
                FreeLibrary((HINSTANCE) handle);

            }
        }
    }
    mprFree(name);
    mprFree(path);
    mprFree(module);
    return mp;
}


int mprReadRegistry(MprCtx ctx, char **buf, int max, cchar *key, cchar *name)
{
    HKEY        top, h;
    char        *value;
    ulong       type, size;

    mprAssert(key && *key);
    mprAssert(buf);

    /*
     *  Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }

    if (RegOpenKeyEx(top, key, 0, KEY_READ, &h) != ERROR_SUCCESS) {
        return MPR_ERR_CANT_ACCESS;
    }

    /*
     *  Get the type
     */
    if (RegQueryValueEx(h, name, 0, &type, 0, &size) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return MPR_ERR_CANT_READ;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        RegCloseKey(h);
        return MPR_ERR_BAD_TYPE;
    }

    value = (char*) mprAlloc(ctx, size);
    if ((int) size > max) {
        RegCloseKey(h);
        return MPR_ERR_WONT_FIT;
    }
    if (RegQueryValueEx(h, name, 0, &type, (uchar*) value, &size) != ERROR_SUCCESS) {
        mprFree(value);
        RegCloseKey(h);
        return MPR_ERR_CANT_READ;
    }

    RegCloseKey(h);
    *buf = value;
    return 0;
}


void mprSetInst(Mpr *mpr, long inst)
{
    mpr->appInstance = inst;
}


void mprSetHwnd(MprCtx ctx, HWND h)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mpr->waitService->hwnd = h;
}


void mprSetSocketMessage(MprCtx ctx, int socketMessage)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mpr->waitService->socketMessage = socketMessage;
}


void mprSleep(MprCtx ctx, int milliseconds)
{
    Sleep(milliseconds);
}


uni *mprToUni(MprCtx ctx, cchar* a)
{
    uni     *wstr;
    int     len;

    len = MultiByteToWideChar(CP_ACP, 0, a, -1, NULL, 0);
    wstr = (uni*) mprAlloc(ctx, (len+1) * sizeof(uni));
    if (wstr) {
        MultiByteToWideChar(CP_ACP, 0, a, -1, wstr, len);
    }
    return wstr;
}


char *mprToAsc(MprCtx ctx, cuni *w)
{
    char    *str;
    int     len;

    len = WideCharToMultiByte(CP_ACP, 0, w, -1, NULL, 0, NULL, NULL);
    if ((str = mprAlloc(ctx, len + 1)) != 0) {
        WideCharToMultiByte(CP_ACP, 0, w, -1, str, (DWORD) len, NULL, NULL);
    }
    return str;
}


void mprUnloadModule(MprModule *mp)
{
    mprAssert(mp->handle);

    mprStopModule(mp);
    mprRemoveItem(mprGetMpr(mp)->moduleService->modules, mp);
    FreeLibrary((HINSTANCE) mp->handle);
}


void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
    HKEY        hkey;
    void        *event;
    long        errorType;
    ulong       exists;
    char        buf[MPR_MAX_STRING], logName[MPR_MAX_STRING], *lines[9], *cp, *value;
    int         type;
    static int  once = 0;

    mprStrcpy(buf, sizeof(buf), message);
    cp = &buf[strlen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
    }

    type = EVENTLOG_ERROR_TYPE;

    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        mprSprintf(logName, sizeof(logName), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
            mprGetAppName(ctx));
        hkey = 0;

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) == ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                    (uchar*) value, (int) strlen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }

            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }
            RegCloseKey(hkey);
        }
    }

    event = RegisterEventSource(0, mprGetAppName(ctx));
    if (event) {
        /*
         *  3299 is the event number for the generic message in netmsg.dll.
         *  "%1 %2 %3 %4 %5 %6 %7 %8 %9" -- thanks Apache for the tip
         */
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
}


int mprWriteRegistry(MprCtx ctx, cchar *key, cchar *name, cchar *value)
{
    HKEY    top, h, subHandle;
    ulong   disposition;

    mprAssert(key && *key);
    mprAssert(name && *name);
    mprAssert(value && *value);

    /*
     *  Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }

    if (name) {
        /*
         *  Write a registry string value
         */
        if (RegOpenKeyEx(top, key, 0, KEY_ALL_ACCESS, &h) != ERROR_SUCCESS) {
            return MPR_ERR_CANT_ACCESS;
        }
        if (RegSetValueEx(h, name, 0, REG_SZ, value, (int) strlen(value) + 1) != ERROR_SUCCESS) {
            RegCloseKey(h);
            return MPR_ERR_CANT_READ;
        }

    } else {
        /*
         *  Create a new sub key
         */
        if (RegOpenKeyEx(top, key, 0, KEY_CREATE_SUB_KEY, &h) != ERROR_SUCCESS){
            return MPR_ERR_CANT_ACCESS;
        }
        if (RegCreateKeyEx(h, name, 0, NULL, REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS, NULL, &subHandle, &disposition) != ERROR_SUCCESS) {
            return MPR_ERR_CANT_ACCESS;
        }
        RegCloseKey(subHandle);
    }
    RegCloseKey(h);
    return 0;
}


/*
 *  Determine the registry hive by the first portion of the path. Return 
 *  a pointer to the rest of key path after the hive portion.
 */ 
static cchar *getHive(cchar *keyPath, HKEY *hive)
{
    char    key[MPR_MAX_STRING], *cp;
    int     len;

    mprAssert(keyPath && *keyPath);

    *hive = 0;

    mprStrcpy(key, sizeof(key), keyPath);
    key[sizeof(key) - 1] = '\0';

    if (cp = strchr(key, '\\')) {
        *cp++ = '\0';
    }
    if (cp == 0 || *cp == '\0') {
        return 0;
    }

    if (!mprStrcmpAnyCase(key, "HKEY_LOCAL_MACHINE")) {
        *hive = HKEY_LOCAL_MACHINE;
    } else if (!mprStrcmpAnyCase(key, "HKEY_CURRENT_USER")) {
        *hive = HKEY_CURRENT_USER;
    } else if (!mprStrcmpAnyCase(key, "HKEY_USERS")) {
        *hive = HKEY_USERS;
    } else if (!mprStrcmpAnyCase(key, "HKEY_CLASSES_ROOT")) {
        *hive = HKEY_CLASSES_ROOT;
    } else {
        return 0;
    }

    if (*hive == 0) {
        return 0;
    }
    len = (int) strlen(key) + 1;
    return keyPath + len;
}

#else
void __dummyMprWin() {}
#endif /* BLD_WIN_LIKE */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprWin.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprWince.c"
 */
/************************************************************************/

/**
 *  mprWince.c - Windows CE platform specific code.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */



#if WINCE
/*
 *  Windows file time is in 100 ns units starting 1601
 *  Unix (time_t) time is in sec units starting 1970
 *  MprTime time is in msec units starting 1970
 */
#define WIN_TICKS         10000000      /* Number of windows units in a second */
#define ORIGIN_GAP        11644473600   /* Gap in seconds between 1601 and 1970 */
#define fileTimeToTime(f) ((((((uint64) ((f).dwHighDateTime)) << 32) | (f).dwLowDateTime) / WIN_TICKS) - ORIGIN_GAP)

static char     *currentDir;            /* Current working directory */
static MprList  *files;                 /* List of open files */
int             errno;                  /* Last error */
static char     timzeone[2][32];        /* Standard and daylight savings zones */

/*
 *  Adjust by seconds between 1601 and 1970
 */
#define WIN_TICKS_TO_MPR  (WIN_TICKS / MPR_TICKS_PER_SEC)
#define MPR               mprGetMpr(NULL)


static HANDLE getHandle(int fd);
static long getJulianDays(SYSTEMTIME *when);
static void timeToFileTime(uint64 t, FILETIME *ft);


MprOsService *mprCreateOsService(MprCtx ctx)
{
    files = mprCreateList(ctx);
    currentDir = mprStrdup(ctx, "/");
    return mprAllocObj(ctx, MprOsService);
}


int mprStartOsService(MprOsService *os)
{
    WSADATA     wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
    return 0;
}


void mprStopOsService(MprOsService *os)
{
    WSACleanup();
}


int mprGetRandomBytes(MprCtx ctx, char *buf, int length, int block)
{
    HCRYPTPROV      prov;
    int             rc;

    rc = 0;

    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | 0x40)) {
        return mprGetError();
    }
    if (!CryptGenRandom(prov, length, buf)) {
        rc = mprGetError();
    }
    CryptReleaseContext(prov, 0);
    return rc;
}


MprModule *mprLoadModule(MprCtx ctx, cchar *moduleName, cchar *initFunction)
{
    MprModule       *mp;
    MprModuleEntry  fn;
    char            *module;
    char            *path, *name;
    void            *handle;

    mprAssert(moduleName && *moduleName);

    mp = 0;
    name = path = 0;
    module = mprGetAbsPath(ctx, moduleName);

    if (mprSearchForModule(ctx, module, &path) < 0) {
        mprError(ctx, "Can't find module \"%s\" in search path \"%s\"", moduleName, mprGetModuleSearchPath(ctx));

    } else {
        name = mprGetPathBase(ctx, module);
        path = mprGetPathBase(path, path);

        mprLog(ctx, MPR_INFO, "Loading module %s from %s", name, path);

        if ((handle = GetModuleHandle(name)) == 0 && (handle = LoadLibrary(path)) == 0) {
            mprError(ctx, "Can't load module %s\nReason: \"%d\"\n",  path, mprGetOsError());

        } else if (initFunction) {
            if ((fn = (MprModuleEntry) GetProcAddress((HINSTANCE) handle, initFunction)) != 0) {
                if ((mp = (fn)(ctx, path)) == 0) {
                    mprError(ctx, "Initialization for module %s failed", name);
                    FreeLibrary((HINSTANCE) handle);

                } else {
                    mp->handle = handle;
                }

            } else {
                mprError(ctx, "Can't load module %s\nReason: can't find function \"%s\"\n",  name, initFunction);
                FreeLibrary((HINSTANCE) handle);

            }
        }
    }
    mprFree(name);
    mprFree(path);
    mprFree(module);
    return mp;
}


#if KEEP
/*
 *  Determine the registry hive by the first portion of the path. Return 
 *  a pointer to the rest of key path after the hive portion.
 */ 
static cchar *getHive(cchar *keyPath, HKEY *hive)
{
    char    key[MPR_MAX_STRING], *cp;
    int     len;

    mprAssert(keyPath && *keyPath);

    *hive = 0;

    mprStrcpy(key, sizeof(key), keyPath);
    key[sizeof(key) - 1] = '\0';

    if (cp = strchr(key, '\\')) {
        *cp++ = '\0';
    }
    if (cp == 0 || *cp == '\0') {
        return 0;
    }

    if (!mprStrcmpAnyCase(key, "HKEY_LOCAL_MACHINE")) {
        *hive = HKEY_LOCAL_MACHINE;
    } else if (!mprStrcmpAnyCase(key, "HKEY_CURRENT_USER")) {
        *hive = HKEY_CURRENT_USER;
    } else if (!mprStrcmpAnyCase(key, "HKEY_USERS")) {
        *hive = HKEY_USERS;
    } else if (!mprStrcmpAnyCase(key, "HKEY_CLASSES_ROOT")) {
        *hive = HKEY_CLASSES_ROOT;
    } else {
        return 0;
    }

    if (*hive == 0) {
        return 0;
    }
    len = (int) strlen(key) + 1;
    return keyPath + len;
}


int mprReadRegistry(MprCtx ctx, char **buf, int max, cchar *key, cchar *name)
{
    HKEY        top, h;
    LPWSTR      wkey, wname;
    char        *value;
    ulong       type, size;

    mprAssert(key && *key);
    mprAssert(buf);

    if ((key = getHive(key, &top)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }

    wkey = mprToUni(ctx, key);
    if (RegOpenKeyEx(top, wkey, 0, KEY_READ, &h) != ERROR_SUCCESS) {
        mprFree(wkey);
        return MPR_ERR_CANT_ACCESS;
    }
    mprFree(wkey);

    /*
     *  Get the type
     */
    wname = mprToUni(ctx, name);
    if (RegQueryValueEx(h, wname, 0, &type, 0, &size) != ERROR_SUCCESS) {
        RegCloseKey(h);
        mprFree(wname);
        return MPR_ERR_CANT_READ;
    }

    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        RegCloseKey(h);
        mprFree(wname);
        return MPR_ERR_BAD_TYPE;
    }

    value = (char*) mprAlloc(ctx, size);
    if ((int) size > max) {
        RegCloseKey(h);
        mprFree(wname);
        return MPR_ERR_WONT_FIT;
    }
    if (RegQueryValueEx(h, wname, 0, &type, (uchar*) value, &size) != ERROR_SUCCESS) {
        mprFree(value);
        mprFree(wname);
        RegCloseKey(h);
        return MPR_ERR_CANT_READ;
    }

    mprFree(wname);
    RegCloseKey(h);
    *buf = value;
    return 0;
}


void mprSetInst(Mpr *mpr, long inst)
{
    mpr->appInstance = inst;
}


void mprSetHwnd(MprCtx ctx, HWND h)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mpr->waitService->hwnd = h;
}


void mprSetSocketMessage(MprCtx ctx, int socketMessage)
{
    Mpr     *mpr;

    mpr = mprGetMpr(ctx);
    mpr->waitService->socketMessage = socketMessage;
}
#endif /* WINCE */


void mprSleep(MprCtx ctx, int milliseconds)
{
    Sleep(milliseconds);
}


uni *mprToUni(MprCtx ctx, cchar* a)
{
    uni     *wstr;
    int     len;

    len = MultiByteToWideChar(CP_ACP, 0, a, -1, NULL, 0);
    wstr = (uni*) mprAlloc(ctx, (len+1) * sizeof(uni));
    if (wstr) {
        MultiByteToWideChar(CP_ACP, 0, a, -1, wstr, len);
    }
    return wstr;
}


char *mprToAsc(MprCtx ctx, cuni *w)
{
    char    *str;
    int     len;

    len = WideCharToMultiByte(CP_ACP, 0, w, -1, NULL, 0, NULL, NULL);
    if ((str = mprAlloc(ctx, len + 1)) != 0) {
        WideCharToMultiByte(CP_ACP, 0, w, -1, str, (DWORD) len, NULL, NULL);
    }
    return str;
}


void mprUnloadModule(MprModule *mp)
{
    mprAssert(mp->handle);

    mprStopModule(mp);
    mprRemoveItem(mprGetMpr(mp)->moduleService->modules, mp);
    FreeLibrary((HINSTANCE) mp->handle);
}


#if KEEP
void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
    HKEY        hkey;
    void        *event;
    long        errorType;
    ulong       exists;
    char        buf[MPR_MAX_STRING], logName[MPR_MAX_STRING], *lines[9], *cp, *value;
    int         type;
    static int  once = 0;

    mprStrcpy(buf, sizeof(buf), message);
    cp = &buf[strlen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
    }

    type = EVENTLOG_ERROR_TYPE;

    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        mprSprintf(logName, sizeof(logName), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
            mprGetAppName(ctx));
        hkey = 0;

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) == ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                    (uchar*) value, (int) strlen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }

            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }
            RegCloseKey(hkey);
        }
    }

    event = RegisterEventSource(0, mprGetAppName(ctx));
    if (event) {
        /*
         *  3299 is the event number for the generic message in netmsg.dll.
         *  "%1 %2 %3 %4 %5 %6 %7 %8 %9" -- thanks Apache for the tip
         */
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
}

int mprWriteRegistry(MprCtx ctx, cchar *key, cchar *name, cchar *value)
{
    HKEY    top, h, subHandle;
    ulong   disposition;

    mprAssert(key && *key);
    mprAssert(name && *name);
    mprAssert(value && *value);

    /*
     *  Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }

    if (name) {
        /*
         *  Write a registry string value
         */
        if (RegOpenKeyEx(top, key, 0, KEY_ALL_ACCESS, &h) != ERROR_SUCCESS) {
            return MPR_ERR_CANT_ACCESS;
        }
        if (RegSetValueEx(h, name, 0, REG_SZ, value, (int) strlen(value) + 1) != ERROR_SUCCESS) {
            RegCloseKey(h);
            return MPR_ERR_CANT_READ;
        }

    } else {
        /*
         *  Create a new sub key
         */
        if (RegOpenKeyEx(top, key, 0, KEY_CREATE_SUB_KEY, &h) != ERROR_SUCCESS){
            return MPR_ERR_CANT_ACCESS;
        }
        if (RegCreateKeyEx(h, name, 0, NULL, REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS, NULL, &subHandle, &disposition) != ERROR_SUCCESS) {
            return MPR_ERR_CANT_ACCESS;
        }
        RegCloseKey(subHandle);
    }
    RegCloseKey(h);
    return 0;
}
#endif



int access(cchar *path, int flags)
{
    char    *tmpPath;
    int     rc;

    if (!mprIsAbsPath(MPR, path)) {
        path = (cchar*) tmpPath = mprJoinPath(MPR, currentDir, path);
    } else {
        tmpPath = 0;
    }
    rc = GetFileAttributesA(path) != -1 ? 0 : -1;

    mprFree(tmpPath);
    return rc;
}


int chdir(cchar *dir)
{
    char    *newDir;

    newDir = mprGetAbsPath(MPR, dir);
    mprFree(currentDir);
    currentDir = newDir;

    return 0;
}


int chmod(cchar *path, int mode)
{
    /* CE has no such permissions */
    return 0;
}


int close(int fd)
{
    int     rc;

    //  LOCKING
    rc = CloseHandle(getHandle(fd));
    mprSetItem(files, fd, NULL);
    return (rc != 0) ? 0 : -1;
}


long _get_osfhandle(int handle)
{
    return (long) handle;
}


char *getenv(cchar *key)
{
    return 0;
}


char *getcwd(char *buf, int size)
{
    mprStrcpy(buf, size, currentDir);
    return buf;
}


uint getpid() {
    return 0;
}


long lseek(int handle, long offset, int origin)
{
    switch (origin) {
        case SEEK_SET: offset = FILE_BEGIN; break;
        case SEEK_CUR: offset = FILE_CURRENT; break;
        case SEEK_END: offset = FILE_END; break;
    }
    return SetFilePointer((HANDLE) handle, offset, NULL, origin);
}


int mkdir(cchar *dir, int mode)
{
    char    *tmpDir;
    uni     *wdir;
    int     rc;

    if (!mprIsAbsPath(MPR, dir)) {
        dir = (cchar*) tmpDir = mprJoinPath(MPR, currentDir, dir);
    } else {
        tmpDir = 0;
    }

    wdir = mprToUni(MPR, dir);
    rc = CreateDirectoryW(wdir, NULL);
    mprFree(wdir);
    mprFree(tmpDir);
    return (rc != 0) ? 0 : -1;
}


static HANDLE getHandle(int fd)
{
    //  LOCKING
    return (HANDLE) mprGetItem(files, fd);
}


static int addHandle(HANDLE h)
{
    int     i;

    //  LOCKING
    for (i = 0; i < files->length; i++) {
        if (files->items[i] == 0) {
            mprSetItem(files, i, h);
            return i;
        }
    }
    return mprAddItem(files, h);
}


int _open_osfhandle(int *handle, int flags)
{
    return addHandle((HANDLE) handle);
}


uint open(cchar *path, int mode, va_list arg)
{
    uni     *wpath;
    char    *tmpPath;
    DWORD   accessFlags, shareFlags, createFlags;
    HANDLE  h;

    if (!mprIsAbsPath(MPR, path)) {
        path = (cchar*) tmpPath = mprGetAbsPath(MPR, path);
    } else {
        tmpPath = 0;
    }

    shareFlags = FILE_SHARE_READ;
    accessFlags = 0;
    createFlags = 0;

    if ((mode & O_RDWR) != 0) {
        accessFlags = GENERIC_READ | GENERIC_WRITE;
    } else if ((mode & O_WRONLY) != 0) {
        accessFlags = GENERIC_WRITE;
    } else {
        accessFlags = GENERIC_READ;
    }
    if ((mode & O_CREAT) != 0) {
        createFlags = CREATE_ALWAYS;
    } else {
        createFlags = OPEN_EXISTING;
    }

    wpath = mprToUni(MPR, path);

    h = CreateFileW(wpath, accessFlags, shareFlags, NULL, createFlags, FILE_ATTRIBUTE_NORMAL, NULL);
    mprFree(wpath);
    mprFree(tmpPath);

    return h == INVALID_HANDLE_VALUE ? -1 : addHandle(h);
}


int read(int fd, void *buffer, uint length)
{
    DWORD   dw;

    ReadFile(getHandle(fd), buffer, length, &dw, NULL);
    return (int) dw;
}


int rename(cchar *oldname, cchar *newname)
{
    uni     *from, *to;
    char    *tmpOld, *tmpNew;
    int     rc;

    if (!mprIsAbsPath(MPR, oldname)) {
        oldname = (cchar*) tmpOld = mprJoinPath(MPR, currentDir, oldname);
    } else {
        tmpOld = 0;
    }
    if (!mprIsAbsPath(MPR, newname)) {
        newname = (cchar*) tmpNew = mprJoinPath(MPR, currentDir, newname);
    } else {
        tmpNew = 0;
    }
    from = mprToUni(MPR, oldname);
    to = mprToUni(MPR, newname);

    rc = MoveFileW(from, to);

    mprFree(tmpOld);
    mprFree(tmpNew);
    mprFree(from);
    mprFree(to);

    return rc == 0 ? 0 : -1;
}


int rmdir(cchar *dir)
{
    uni     *wdir;
    char    *tmpDir;
    int     rc;

    if (!mprIsAbsPath(MPR, dir)) {
        dir = (cchar*) tmpDir = mprJoinPath(MPR, currentDir, dir);
    } else {
        tmpDir = 0;
    }
    wdir = mprToUni(MPR, dir);
    rc = RemoveDirectoryW(wdir);

    mprFree(tmpDir);
    mprFree(wdir);

    return rc == 0 ? 0 : -1;
}


int stat(cchar *path, struct stat *sbuf)
{
    WIN32_FIND_DATAW    fd;
    DWORD               attributes;
    HANDLE              h;
    DWORD               dwSizeLow, dwSizeHigh, dwError;
    char                *tmpPath;
    uni                 *wpath;

    dwSizeLow = 0;
    dwSizeHigh = 0;
    dwError = 0;

    memset(sbuf, 0, sizeof(struct stat));

    if (!mprIsAbsPath(MPR, path)) {
        path = (cchar*) tmpPath = mprJoinPath(MPR, currentDir, path);
    } else {
        tmpPath = 0;
    }
    wpath = mprToUni(MPR, path);

    attributes = GetFileAttributesW(wpath);
    if (attributes == -1) {
        mprFree(wpath);
        return -1;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        sbuf->st_mode += S_IFDIR;
    } else {
        sbuf->st_mode += S_IFREG;
    }

    h = FindFirstFileW(wpath, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        if (wpath[wcslen(wpath)-1]  == L'\\') {
            wpath[wcslen(wpath)-1] = L'\0';
            h = FindFirstFileW(wpath, &fd);
            if (h == INVALID_HANDLE_VALUE) {
                mprFree(tmpPath);
                mprFree(wpath);
                return 0;
            }
        } else {
            mprFree(tmpPath);
            mprFree(wpath);
            return 0;
        }
    }

    sbuf->st_atime = (time_t) fileTimeToTime(fd.ftLastAccessTime);
    sbuf->st_mtime = (time_t) fileTimeToTime(fd.ftLastWriteTime);
    sbuf->st_ctime = (time_t) fileTimeToTime(fd.ftCreationTime);
    sbuf->st_size  = fd.nFileSizeLow;

    FindClose(h);
    mprFree(tmpPath);
    mprFree(wpath);

    return 0;
}


/*
 *  Convert time in seconds to a file time
 */
static void timeToFileTime(uint64 t, FILETIME *ft)
{
    t += ORIGIN_GAP;
    t *= WIN_TICKS;
    ft->dwHighDateTime = (DWORD) ((t >> 32) & 0xFFFFFFFF);
    ft->dwLowDateTime  = (DWORD) (t & 0xFFFFFFFF);
}


/*
 *  Get the Julian current year day.
 *
 *  General Julian Day formula:
 *      a = (14 - month) / 12;
 *      y = year + 4800 - a;
 *      m = month + 12 * a - 3;
 *      jd = day + (y * 365) + (y / 4) - (y / 100) + (y / 400) + (m * 153 + 2) / 5 - 32045;
 */
static long getJulianDays(SYSTEMTIME *when)
{
    int     y, m, d, a, day, startYearDay;

    a = (14 - when->wMonth) / 12;
    y = when->wYear + 4800 - a;
    m = when->wMonth + 12 * a - 3;
    d = when->wDay;

    /*
     *  Compute the difference between Julian days for Jan 1 and "when" of the same year
     */
    day = d + (y * 365) + (y / 4) - (y / 100) + (y / 400) + (m * 153 + 2) / 5;
    y = when->wYear + 4799;
    startYearDay = 1 + (y * 365) + (y / 4) - (y / 100) + (y / 400) + 1532 / 5;

    return day - startYearDay;
}


struct tm *gmtime_r(const time_t *when, struct tm *tp)
{
    FILETIME    f;
    SYSTEMTIME  s;
    
    timeToFileTime(*when, &f);
    FileTimeToSystemTime(&f, &s);

    tp->tm_year  = s.wYear - 1900;
    tp->tm_mon   = s.wMonth- 1;
    tp->tm_wday  = s.wDayOfWeek;
    tp->tm_mday  = s.wDay;
    tp->tm_yday  = getJulianDays(&s);
    tp->tm_hour  = s.wHour;
    tp->tm_min   = s.wMinute;
    tp->tm_sec   = s.wSecond;
    tp->tm_isdst = 0;

    return tp;
}


struct tm *localtime_r(const time_t *when, struct tm *tp)
{
    FILETIME                f;
    SYSTEMTIME              s;
    TIME_ZONE_INFORMATION   tz;
    int                     bias, rc;

    mprAssert(when);
    mprAssert(tp);

    rc = GetTimeZoneInformation(&tz);
    bias = tz.Bias;
    if (rc == TIME_ZONE_ID_DAYLIGHT) {
        tp->tm_isdst = 1;
        bias += tz.DaylightBias;
    } else {
        tp->tm_isdst = 0;
    }
    bias *= 60;

    timeToFileTime(*when - bias, &f);
    FileTimeToSystemTime(&f, &s);
    
    tp->tm_year   = s.wYear - 1900;
    tp->tm_mon    = s.wMonth- 1;
    tp->tm_wday   = s.wDayOfWeek;
    tp->tm_mday   = s.wDay;
    tp->tm_yday   = getJulianDays(&s);
    tp->tm_hour   = s.wHour;
    tp->tm_min    = s.wMinute;
    tp->tm_sec    = s.wSecond;

    return tp;
}


time_t mktime(struct tm *tp)
{
    TIME_ZONE_INFORMATION   tz;
    SYSTEMTIME              s;
    FILETIME                f;
    time_t                  result;
    int                     rc, bias;

    mprAssert(tp);

    rc = GetTimeZoneInformation(&tz);
    bias = tz.Bias;
    if (rc == TIME_ZONE_ID_DAYLIGHT) {
        tp->tm_isdst = 1;
        bias += tz.DaylightBias;
    }
    bias *= 60;
    
    s.wYear = tp->tm_year + 1900;
    s.wMonth = tp->tm_mon + 1;
    s.wDayOfWeek = tp->tm_wday;
    s.wDay = tp->tm_mday;
    s.wHour = tp->tm_hour;
    s.wMinute = tp->tm_min;
    s.wSecond = tp->tm_sec;

    SystemTimeToFileTime(&s, &f);
    result = (time_t) (fileTimeToTime(f) + tz.Bias * 60);
    if (rc == TIME_ZONE_ID_DAYLIGHT) {
        result -= bias;
    }
    return result;
}


int write(int fd, cvoid *buffer, uint count)
{
    DWORD   dw;

    WriteFile(getHandle(fd), buffer, count, &dw, NULL);
    return (int) dw;
}


int unlink(cchar *file)
{
    uni     *wpath;
    int     rc;

    wpath = mprToUni(MPR, file);
    rc = DeleteFileW(wpath);
    mprFree(wpath);

    return rc == 0 ? 0 : -1;
}



WINBASEAPI HANDLE WINAPI CreateFileA(LPCSTR path, DWORD access, DWORD sharing,
    LPSECURITY_ATTRIBUTES security, DWORD create, DWORD flags, HANDLE template)
{
    LPWSTR  wpath;
    HANDLE  h;

    wpath = mprToUni(MPR, path);
    h = CreateFileW(wpath, access, sharing, security, create, flags, template);
    mprFree(wpath);

    return h;
}


BOOL WINAPI CreateProcessA(LPCSTR app, LPCSTR cmd, LPSECURITY_ATTRIBUTES att, LPSECURITY_ATTRIBUTES threadatt,
    BOOL options, DWORD flags, LPVOID env, LPSTR dir, LPSTARTUPINFO lpsi, LPPROCESS_INFORMATION info)
{
    LPWSTR      wapp, wcmd, wdir;
    int         result;

    wapp  = mprToUni(MPR, app);
    wcmd  = mprToUni(MPR, cmd);
    wdir  = mprToUni(MPR, dir);

    result = CreateProcessW(wapp, wcmd, att, threadatt, options, flags, env, wdir, lpsi, info);

    mprFree(wapp);
    mprFree(wcmd);
    mprFree(wdir);

    return result;
}


HANDLE FindFirstFileA(LPCSTR path, WIN32_FIND_DATAA *data)
{
    WIN32_FIND_DATAW    wdata;
    LPWSTR              wpath;
    HANDLE              h;
    char                *file;

    wpath = mprToUni(MPR, path);
    h = FindFirstFileW(wpath, &wdata);
    mprFree(wpath);
    
    file = mprToAsc(MPR, wdata.cFileName);
    strcpy(data->cFileName, file);
    mprFree(file);
    return h;
}


BOOL FindNextFileA(HANDLE handle, WIN32_FIND_DATAA *data)
{
    WIN32_FIND_DATAW    wdata;
    char                *file;
    BOOL                result;

    result = FindNextFileW(handle, &wdata);
    file = mprToAsc(MPR, wdata.cFileName);
    strcpy(data->cFileName, file);
    mprFree(file);
    return result;
}


DWORD GetFileAttributesA(cchar *path)
{
    LPWSTR      wpath;
    DWORD       result;

    wpath = mprToUni(MPR, path);
    result = GetFileAttributesW(wpath);
    mprFree(wpath);
    return result;
}


DWORD GetModuleFileNameA(HMODULE module, LPSTR buf, DWORD size)
{
    LPWSTR      wpath;
    LPSTR       mb;
    size_t      ret;

    wpath = (LPWSTR) mprAlloc(MPR, size * sizeof(wchar_t));
    ret = GetModuleFileNameW(module, wpath, size);
    mb = mprToAsc(MPR, wpath);
    strcpy(buf, mb);
    mprFree(mb);
    mprFree(wpath);
    return ret;
}


WINBASEAPI HMODULE WINAPI GetModuleHandleA(LPCSTR path)
{
    LPWSTR      wpath;
    HANDLE      result;

    wpath = mprToUni(MPR, path);
    result = GetModuleHandleW(wpath);
    mprFree(wpath);
    return result;
}


void GetSystemTimeAsFileTime(FILETIME *ft)
{
    SYSTEMTIME  s;

    GetSystemTime(&s);
    SystemTimeToFileTime(&s, ft);
}


HINSTANCE WINAPI LoadLibraryA(LPCSTR path)
{
    HINSTANCE   h;
    LPWSTR      wpath;

    wpath = mprToUni(MPR, path);
    h = LoadLibraryW(wpath);
    mprFree(wpath);
    return h;
}

void mprWriteToOsLog(MprCtx ctx, cchar *message, int flags, int level)
{
}

#else
void __dummyMprWince() {}
#endif /* WINCE */
/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprWince.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/mprXml.c"
 */
/************************************************************************/

/**
 *  mprXml.c - A simple SAX style XML parser
 *
 *  This is a recursive descent parser for XML text files. It is a one-pass simple parser that invokes a user 
 *  supplied callback for key tokens in the XML file. The user supplies a read function so that XML files can 
 *  be parsed from disk or in-memory. 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_XML

static int       parseNext(MprXml *xp, int state);
static MprXmlToken getToken(MprXml *xp, int state);
static int       getNextChar(MprXml *xp);
static int       scanFor(MprXml *xp, char *str);
static int       putLastChar(MprXml *xp, int c);
static void      xmlError(MprXml *xp, char *fmt, ...);
static void      trimToken(MprXml *xp);


MprXml *mprXmlOpen(MprCtx ctx, int initialSize, int maxSize)
{
    MprXml  *xp;

    xp = mprAllocObjZeroed(ctx, MprXml);
    
    xp->inBuf = mprCreateBuf(xp, MPR_XML_BUFSIZE, MPR_XML_BUFSIZE);
    xp->tokBuf = mprCreateBuf(xp, initialSize, maxSize);

    return xp;
}


void mprXmlSetParserHandler(MprXml *xp, MprXmlHandler h)
{
    mprAssert(xp);

    xp->handler = h;
}


void mprXmlSetInputStream(MprXml *xp, MprXmlInputStream s, void *arg)
{
    mprAssert(xp);

    xp->readFn = s;
    xp->inputArg = arg;
}


/*
 *  Set the parse arg
 */ 
void mprXmlSetParseArg(MprXml *xp, void *parseArg)
{
    mprAssert(xp);

    xp->parseArg = parseArg;
}


/*
 *  Set the parse arg
 */ 
void *mprXmlGetParseArg(MprXml *xp)
{
    mprAssert(xp);

    return xp->parseArg;
}


/*
 *  Parse an XML file. Return 0 for success, -1 for error.
 */ 
int mprXmlParse(MprXml *xp)
{
    mprAssert(xp);

    return parseNext(xp, MPR_XML_BEGIN);
}


/*
 *  XML recursive descent parser. Return -1 for errors, 0 for EOF and 1 if there is still more data to parse.
 */
static int parseNext(MprXml *xp, int state)
{
    MprXmlHandler   handler;
    MprXmlToken     token;
    MprBuf          *tokBuf;
    char            *tname, *aname;
    int             rc;

    mprAssert(state >= 0);

    tokBuf = xp->tokBuf;
    handler = xp->handler;
    tname = aname = 0;
    rc = 0;
    
    /*
     *  In this parse loop, the state is never assigned EOF or ERR. In such cases we always return EOF or ERR.
     */
    while (1) {

        token = getToken(xp, state);

        if (token == MPR_XMLTOK_TOO_BIG) {
            xmlError(xp, "XML token is too big");
            goto err;
        }

        switch (state) {
        case MPR_XML_BEGIN:     /* ------------------------------------------ */
            /*
             *  Expect to get an element, comment or processing instruction 
             */
            switch (token) {
            case MPR_XMLTOK_EOF:
                goto exit;

            case MPR_XMLTOK_LS:
                /*
                 *  Recurse to handle the new element, comment etc.
                 */
                rc = parseNext(xp, MPR_XML_AFTER_LS);
                if (rc < 0) {
                    goto exit;
                }
                break;

            default:
                xmlError(xp, "Syntax error");
                goto err;
            }
            break;

        case MPR_XML_AFTER_LS: /* ------------------------------------------ */
            switch (token) {
            case MPR_XMLTOK_COMMENT:
                state = MPR_XML_COMMENT;
                rc = (*handler)(xp, state, "!--", 0, mprGetBufStart(tokBuf));
                if (rc < 0) {
                    goto err;
                }
                rc = 1;
                goto exit;

            case MPR_XMLTOK_CDATA:
                state = MPR_XML_CDATA;
                rc = (*handler)(xp, state, "!--", 0, mprGetBufStart(tokBuf));
                if (rc < 0) {
                    goto err;
                }
                rc = 1;
                goto exit;

            case MPR_XMLTOK_INSTRUCTIONS:
                /* Just ignore processing instructions */
                rc = 1;
                goto exit;

            case MPR_XMLTOK_TEXT:
                state = MPR_XML_NEW_ELT;
                tname = mprStrdup(xp, mprGetBufStart(tokBuf));
                if (tname == 0) {
                    rc = MPR_ERR_NO_MEMORY;
                    goto exit;
                }
                rc = (*handler)(xp, state, tname, 0, 0);
                if (rc < 0) {
                    goto err;
                }
                break;

            default:
                xmlError(xp, "Syntax error");
                goto err;
            }
            break;

        case MPR_XML_NEW_ELT:   /* ------------------------------------------ */
            /*
             *  We have seen the opening "<element" for a new element and have not yet seen the terminating 
             *  ">" of the opening element.
             */
            switch (token) {
            case MPR_XMLTOK_TEXT:
                /*
                 *  Must be an attribute name
                 */
                aname = mprStrdup(xp, mprGetBufStart(tokBuf));
                token = getToken(xp, state);
                if (token != MPR_XMLTOK_EQ) {
                    xmlError(xp, "Missing assignment for attribute \"%s\"", aname);
                    goto err;
                }

                token = getToken(xp, state);
                if (token != MPR_XMLTOK_TEXT) {
                    xmlError(xp, "Missing value for attribute \"%s\"", aname);
                    goto err;
                }
                state = MPR_XML_NEW_ATT;
                rc = (*handler)(xp, state, tname, aname, mprGetBufStart(tokBuf));
                if (rc < 0) {
                    goto err;
                }
                state = MPR_XML_NEW_ELT;
                break;

            case MPR_XMLTOK_GR:
                /*
                 *  This is ">" the termination of the opening element
                 */
                if (*tname == '\0') {
                    xmlError(xp, "Missing element name");
                    goto err;
                }

                /*
                 *  Tell the user that the opening element is now complete
                 */
                state = MPR_XML_ELT_DEFINED;
                rc = (*handler)(xp, state, tname, 0, 0);
                if (rc < 0) {
                    goto err;
                }
                state = MPR_XML_ELT_DATA;
                break;

            case MPR_XMLTOK_SLASH_GR:
                /*
                 *  If we see a "/>" then this is a solo element
                 */
                if (*tname == '\0') {
                    xmlError(xp, "Missing element name");
                    goto err;
                }
                state = MPR_XML_SOLO_ELT_DEFINED;
                rc = (*handler)(xp, state, tname, 0, 0);
                if (rc < 0) {
                    goto err;
                }
                rc = 1;
                goto exit;
    
            default:
                xmlError(xp, "Syntax error");
                goto err;
            }
            break;

        case MPR_XML_ELT_DATA:      /* -------------------------------------- */
            /*
             *  We have seen the full opening element "<name ...>" and now await data or another element.
             */
            if (token == MPR_XMLTOK_LS) {
                /*
                 *  Recurse to handle the new element, comment etc.
                 */
                rc = parseNext(xp, MPR_XML_AFTER_LS);
                if (rc < 0) {
                    goto exit;
                }
                break;

            } else if (token == MPR_XMLTOK_LS_SLASH) {
                state = MPR_XML_END_ELT;
                break;

            } else if (token != MPR_XMLTOK_TEXT) {
                goto err;
            }
            if (mprGetBufLength(tokBuf) > 0) {
                /*
                 *  Pass the data between the element to the user
                 */
                rc = (*handler)(xp, state, tname, 0, mprGetBufStart(tokBuf));
                if (rc < 0) {
                    goto err;
                }
            }
            break;

        case MPR_XML_END_ELT:           /* -------------------------------------- */
            if (token != MPR_XMLTOK_TEXT) {
                xmlError(xp, "Missing closing element name for \"%s\"", tname);
                goto err;
            }
            /*
             *  The closing element name must match the opening element name 
             */
            if (strcmp(tname, mprGetBufStart(tokBuf)) != 0) {
                xmlError(xp, "Closing element name \"%s\" does not match on line %d. Opening name \"%s\"",
                    mprGetBufStart(tokBuf), xp->lineNumber, tname);
                goto err;
            }
            rc = (*handler)(xp, state, tname, 0, 0);
            if (rc < 0) {
                goto err;
            }
            if (getToken(xp, state) != MPR_XMLTOK_GR) {
                xmlError(xp, "Syntax error");
                goto err;
            }
            return 1;

        case MPR_XML_EOF:       /* ---------------------------------------------- */
            goto exit;

        case MPR_XML_ERR:   /* ---------------------------------------------- */
        default:
            goto err;
        }
    }
    mprAssert(0);

err:
    rc = -1;

exit:
    mprFree(tname);
    mprFree(aname);

    return rc;
}


/*
 *  Lexical analyser for XML. Return the next token reading input as required. It uses a one token look ahead and 
 *  push back mechanism (LAR1 parser). Text token identifiers are left in the tokBuf parser buffer on exit. This Lex 
 *  has special cases for the states MPR_XML_ELT_DATA where we have an optimized read of element data, and 
 *  MPR_XML_AFTER_LS where we distinguish between element names, processing instructions and comments. 
 */
static MprXmlToken getToken(MprXml *xp, int state)
{
    MprBuf      *tokBuf, *inBuf;
    char        *cp;
    int         c, rc;

    tokBuf = xp->tokBuf;
    inBuf = xp->inBuf;

    mprAssert(state >= 0);

    if ((c = getNextChar(xp)) < 0) {
        return MPR_XMLTOK_EOF;
    }
    mprFlushBuf(tokBuf);

    /*
     *  Special case parsing for names and for element data. We do this for performance so we can return to the caller 
     *  the largest token possible.
     */
    if (state == MPR_XML_ELT_DATA) {
        /*
         *  Read all the data up to the start of the closing element "<" or the start of a sub-element.
         */
        if (c == '<') {
            if ((c = getNextChar(xp)) < 0) {
                return MPR_XMLTOK_EOF;
            }
            if (c == '/') {
                return MPR_XMLTOK_LS_SLASH;
            }
            putLastChar(xp, c);
            return MPR_XMLTOK_LS;
        }
        do {
            if (mprPutCharToBuf(tokBuf, c) < 0) {
                return MPR_XMLTOK_TOO_BIG;
            }
            if ((c = getNextChar(xp)) < 0) {
                return MPR_XMLTOK_EOF;
            }
        } while (c != '<');

        /*
         *  Put back the last look-ahead character
         */
        putLastChar(xp, c);

        /*
         *  If all white space, then zero the token buffer
         */
        for (cp = tokBuf->start; *cp; cp++) {
            if (!isspace((int) *cp)) {
                return MPR_XMLTOK_TEXT;
            }
        }
        mprFlushBuf(tokBuf);
        return MPR_XMLTOK_TEXT;
    }

    while (1) {
        switch (c) {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
            break;

        case '<':
            if ((c = getNextChar(xp)) < 0) {
                return MPR_XMLTOK_EOF;
            }
            if (c == '/') {
                return MPR_XMLTOK_LS_SLASH;
            }
            putLastChar(xp, c);
            return MPR_XMLTOK_LS;
    
        case '=':
            return MPR_XMLTOK_EQ;

        case '>':
            return MPR_XMLTOK_GR;

        case '/':
            if ((c = getNextChar(xp)) < 0) {
                return MPR_XMLTOK_EOF;
            }
            if (c == '>') {
                return MPR_XMLTOK_SLASH_GR;
            }
            return MPR_XMLTOK_ERR;
        
        case '\"':
        case '\'':
            xp->quoteChar = c;
            /* Fall through */

        default:
            /*
             *  We handle element names, attribute names and attribute values 
             *  here. We do NOT handle data between elements here. Read the 
             *  token.  Stop on white space or a closing element ">"
             */
            if (xp->quoteChar) {
                if ((c = getNextChar(xp)) < 0) {
                    return MPR_XMLTOK_EOF;
                }
                while (c != xp->quoteChar) {
                    if (mprPutCharToBuf(tokBuf, c) < 0) {
                        return MPR_XMLTOK_TOO_BIG;
                    }
                    if ((c = getNextChar(xp)) < 0) {
                        return MPR_XMLTOK_EOF;
                    }
                }
                xp->quoteChar = 0;

            } else {
                while (!isspace(c) && c != '>' && c != '/' && c != '=') {
                    if (mprPutCharToBuf(tokBuf, c) < 0) {
                        return MPR_XMLTOK_TOO_BIG;
                    }
                    if ((c = getNextChar(xp)) < 0) {
                        return MPR_XMLTOK_EOF;
                    }
                }
                putLastChar(xp, c);
            }
            if (mprGetBufLength(tokBuf) <= 0) {
                return MPR_XMLTOK_ERR;
            }
            mprAddNullToBuf(tokBuf);

            if (state == MPR_XML_AFTER_LS) {
                /*
                 *  If we are just inside an element "<", then analyze what we have to see if we have an element name, 
                 *  instruction or comment. Tokbuf will hold "?" for instructions or "!--" for comments.
                 */
                if (mprLookAtNextCharInBuf(tokBuf) == '?') {
                    /*  Just ignore processing instructions */
                    rc = scanFor(xp, "?>");
                    if (rc < 0) {
                        return MPR_XMLTOK_TOO_BIG;
                    } else if (rc == 0) {
                        return MPR_XMLTOK_ERR;
                    }
                    return MPR_XMLTOK_INSTRUCTIONS;

                } else if (mprLookAtNextCharInBuf(tokBuf) == '!') {
                    if (strncmp((char*) tokBuf->start, "![CDATA[", 8) == 0) {
                        mprAdjustBufStart(tokBuf, 8);
                        rc = scanFor(xp, "]]>");
                        if (rc < 0) {
                            return MPR_XMLTOK_TOO_BIG;
                        } else if (rc == 0) {
                            return MPR_XMLTOK_ERR;
                        }
                        return MPR_XMLTOK_CDATA;

                    } else {
                        mprFlushBuf(tokBuf);
                        rc = scanFor(xp, "-->");
                        if (rc < 0) {
                            return MPR_XMLTOK_TOO_BIG;
                        } else if (rc == 0) {
                            return MPR_XMLTOK_ERR;
                        }
                        return MPR_XMLTOK_COMMENT;
                    }
                }
            }
            trimToken(xp);
            return MPR_XMLTOK_TEXT;
        }
        if ((c = getNextChar(xp)) < 0) {
            return MPR_XMLTOK_EOF;
        }
    }

    /* Should never get here */
    mprAssert(0);
    return MPR_XMLTOK_ERR;
}


/*
 *  Scan for a pattern. Trim the pattern from the token. Return 1 if the pattern was found, return 0 if not found. 
 *  Return < 0 on errors.
 */
static int scanFor(MprXml *xp, char *pattern)
{
    MprBuf  *tokBuf;
    char    *start, *p, *cp;
    int     c;

    mprAssert(pattern);

    tokBuf = xp->tokBuf;
    mprAssert(tokBuf);

    start = mprGetBufStart(tokBuf);
    while (1) {
        cp = start;
        for (p = pattern; *p; p++) {
            if (cp >= (char*) tokBuf->end) {
                if ((c = getNextChar(xp)) < 0) {
                    return 0;
                }
                if (mprPutCharToBuf(tokBuf, c) < 0) {
                    return -1;
                }
            }
            if (*cp++ != *p) {
                break;
            }
        }
        if (*p == '\0') {
            /*
             *  Remove the pattern from the tokBuf
             */
            mprAdjustBufEnd(tokBuf, -(int) strlen(pattern));
            trimToken(xp);
            return 1;
        }
        start++;
    }
}


/*
 *  Get another character. We read and buffer blocks of data if we need more data to parse.
 */
static int getNextChar(MprXml *xp)
{
    MprBuf  *inBuf;
    int     c, l;

    inBuf = xp->inBuf;
    if (mprGetBufLength(inBuf) <= 0) {
        /*
         *  Flush to reset the servp/endp pointers to the start of the buffer so we can do a maximal read 
         */
        mprFlushBuf(inBuf);
        l = (xp->readFn)(xp, xp->inputArg, mprGetBufStart(inBuf), mprGetBufSpace(inBuf));
        if (l <= 0) {
            return -1;
        }
        mprAdjustBufEnd(inBuf, l);
    }
    c = mprGetCharFromBuf(inBuf);

    if (c == '\n') {
        xp->lineNumber++;
    }
    return c;
}


/*
 *  Put back a character in the input buffer
 */
static int putLastChar(MprXml *xp, int c)
{
    if (mprInsertCharToBuf(xp->inBuf, (char) c) < 0) {
        mprAssert(0);
        return MPR_ERR_BAD_STATE;
    }
    if (c == '\n') {
        xp->lineNumber--;
    }
    return 0;
}


/*
 *  Output a parse message
 */ 
static void xmlError(MprXml *xp, char *fmt, ...)
{
    va_list     args;
    char        *buf;

    mprAssert(fmt);

    va_start(args, fmt);
    buf = mprVasprintf(xp, MPR_MAX_STRING, fmt, args);
    va_end(args);

    mprFree(xp->errMsg);
    xp->errMsg = mprAsprintf(xp, MPR_MAX_STRING, "XML error: %s\nAt line %d\n", buf, xp->lineNumber);
    mprFree(buf);
}


/*
 *  Remove trailing whitespace in a token and ensure it is terminated with a NULL for easy parsing
 */
static void trimToken(MprXml *xp)
{
    while (isspace(mprLookAtLastCharInBuf(xp->tokBuf))) {
        mprAdjustBufEnd(xp->tokBuf, -1);
    }
    mprAddNullToBuf(xp->tokBuf);
}


cchar *mprXmlGetErrorMsg(MprXml *xp)
{
    if (xp->errMsg == 0) {
        return "";
    }
    return xp->errMsg;
}


int mprXmlGetLineNumber(MprXml *xp)
{
    return xp->lineNumber;
}


#else
void __dummyMprXml() {} 
#endif /* BLD_FEATURE_XML */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/mprXml.c"
 */
/************************************************************************/


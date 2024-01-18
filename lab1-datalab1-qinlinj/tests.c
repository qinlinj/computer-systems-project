/* Testing Code */

#include <limits.h>
#include <math.h>

/* Routines used by floation point test code */

/* Convert from bit level representation to floating point number */
float u2f(unsigned u) {
    union {
        unsigned u;
        float f;
    } a;
    a.u = u;
    return a.f;
}

/* Convert from floating point number to bit-level representation */
unsigned f2u(float f) {
    union {
        unsigned u;
        float f;
    } a;
    a.f = f;
    return a.u;
}
long test_bitMatch(long x, long y)
{
    int i;
    long result = 0;
    for (i = 0; i < 64; i++) {
        long mask = 1L << i;
        long bit = (x & mask) == (y & mask);
        result |= bit << i;
    }
    return result;
}
long test_anyOddBit(long x) {
  int i;
  for (i = 1; i < 64; i+=2)
      if (x & (1L<<i))
          return 1L;
  return 0L;
}
long test_ezThreeFourths(long x)
{
  return (x*3L)/4L;
}
long test_bitMask(long highbit, long lowbit)
{
  long result = 0L;
  int i;
  for (i = lowbit; i <= highbit; i++)
    result |= 1L << i;
  return result;
}
long test_howManyBits(long x) {
    long cnt;
    if (x < 0)
        /* Flip the bits if x is negative */
        x = ~x;
    unsigned long a = (unsigned long) x;
    for (cnt=1; a; a>>=1, cnt++)
        ;
    return cnt;
}
long test_hexAllLetters(long x) {
    int i;
    long ok = 1;
    for (i = 0; i < 64; i+=4) {
        long d = (x >> i) & 0xF;
        ok = ok && (d >= 0xa);
    }
    return ok;
}
long test_tmax(void) {
  return 0x7FFFFFFFFFFFFFFFL;
}
long test_isTmin(long x) {
  return x == (-0x7fffffffffffffffL - 1);
}
long test_isNegative(long x) {
  return x < 0;
}
long test_integerLog2(long x) {
  long result;
  /* find the leftmost bit */
  for (result = 63; result >= 0; result--) {
      long val = 1L << result;
      if (x & val)
          break;
  }
  return result;
}
int test_floatFloat2Int(unsigned uf) {
  float f = u2f(uf);
  int x = (int) f;
  return x;
}
unsigned test_floatScale1d4(unsigned uf) {
    float f = u2f(uf);
    float hf = 0.25*f;
    if (isnan(f))
        return uf;
    else
        return f2u(hf);
}
unsigned test_floatNegate(unsigned uf) {
    float f = u2f(uf);
    float nf = -f;
    if (isnan(f))
        return uf;
    else
        return f2u(nf);
}

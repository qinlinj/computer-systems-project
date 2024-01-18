/*
 * CS:APP Data Lab
 *
 * name: Qinlin Jia 
 * userid: qinlinj
 *
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 */

/* Instructions to Students:

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:

  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code
  must conform to the following style:

  long Funct(long arg1, long arg2, ...) {
      // brief description of how your implementation works
      long var1 = Expr1;
      ...
      long varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. (Long) integer constants 0 through 255 (0xFFL), inclusive. You are
      not allowed to use big constants such as 0xffffffffL.
  2. Function arguments and local variables (no global variables).
  3. Local variables of type int and long
  4. Unary integer operations ! ~
     - Their arguments can have types int or long
     - Note that ! always returns int, even if the argument is long
  5. Binary integer operations & ^ | + << >>
     - Their arguments can have types int or long
  6. Casting from int to long and from long to int

  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting other than between int and long.
  7. Use any data type other than int or long.  This implies that you
     cannot use arrays, structs, or unions.

  You may assume that your machine:
  1. Uses 2s complement representations for int and long.
  2. Data type int is 32 bits, long is 64.
  3. Performs right shifts arithmetically.
  4. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31 (int) or 63 (long)

EXAMPLES OF ACCEPTABLE CODING STYLE:
  //
  // pow2plus1 - returns 2^x + 1, where 0 <= x <= 63
  //
  long pow2plus1(long x) {
     // exploit ability of shifts to compute powers of 2
     // Note that the 'L' indicates a long constant
     return (1L << x) + 1L;
  }

  //
  // pow2plus4 - returns 2^x + 4, where 0 <= x <= 63
  //
  long pow2plus4(long x) {
     // exploit ability of shifts to compute powers of 2
     long result = (1L << x);
     result += 4L;
     return result;
  }

NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

CAUTION:
  Do not add an #include of <stdio.h> (or any other C library header)
  to this file.  C library headers almost always contain constructs
  that dlc does not understand.  For debugging, you can use printf,
  which is declared for you just below.  It is normally bad practice
  to declare C library functions by hand, but in this case it's less
  trouble than any alternative.

  dlc will consider each call to printf to be a violation of the
  coding style (function calls, after all, are not allowed) so you
  must remove all your debugging printf's again before submitting your
  code or testing it with dlc or the BDD checker.  */

extern int printf(const char *, ...);

/* Edit the functions below.  Good luck!  */
/*
 * bitMatch - Create mask indicating which bits in x match those in y
 *            using only ~ and &
 *   Example: bitMatch(0x7L, 0xEL) = 0xFFFFFFFFFFFFFFF6L
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
long bitMatch(long x, long y) {
    return ~(~(x & y) & ~(~x & ~y));
}
/*
 * anyOddBit - return 1 if any odd-numbered bit in word set to 1
 *   where bits are numbered from 0 (least significant) to 63 (most significant)
 *   Examples anyOddBit(0x5L) = 0L, anyOddBit(0x7L) = 1L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 14
 *   Rating: 2
 */
long anyOddBit(long x) {
  long mask;
  long slice = 0xAAL;  // 8-bit slice with odd bits set

  // Construct 64-bit mask
  mask = slice;
  mask = (mask << 8) + slice;
  mask = (mask << 16) + mask;  // At this point, mask is 32 bits: 0xAAAAAAAA
  mask = (mask << 32) + mask;  // 64 bits: 0xAAAAAAAAAAAAAAAA

  // If any odd bit in x is set, result will be non-zero
  return !!(x & mask);
}
/*
 * ezThreeFourths - multiplies by 3/4 rounding toward 0,
 *   Should exactly duplicate effect of C expression (x*3L/4L),
 *   including overflow behavior.
 *   Examples:
 *     ezThreeFourths(11L) = 8L
 *     ezThreeFourths(-9L) = -6L
 *     ezThreeFourths(4611686018427387904L) = -1152921504606846976L (overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 3
 */
long ezThreeFourths(long x) {
    long multiplied = (x << 1) + x;
    long mask = (multiplied >> 63) & 3;
    return (multiplied + mask) >> 2;
}
/*
 * bitMask - Generate a mask consisting of all 1's
 *   between lowbit and highbit
 *   Examples: bitMask(5L,3L) = 0x38L
 *   Assume 0 <= lowbit < 64, and 0 <= highbit < 64
 *   If lowbit > highbit, then mask should be all 0's
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
long bitMask(long highbit, long lowbit) { 
    long ones = ~0L; 
    long left = ones << lowbit; 
    long right = ((ones << 1) << highbit) ^ ones; 
    return left & right; 
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12L) = 5L
 *            howManyBits(298L) = 10L
 *            howManyBits(-5L) = 4L
 *            howManyBits(0L)  = 1L
 *            howManyBits(-1L) = 1L
 *            howManyBits(0x8000000000000000L) = 64L
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 70
 *  Rating: 4
 */
long howManyBits(long x) { 
    long sign = x >> 63;
    x = (sign & ~x) | (~sign & x); 
    long b32 = !!(x >> 32) << 5;
    x = x >> b32;
    long b16 = !!(x >> 16) << 4;
    x = x >> b16;
    long b8 = !!(x >> 8) << 3;
    x = x >> b8;
    long b4 = !!(x >> 4) << 2;
    x = x >> b4;
    long b2 = !!(x >> 2) << 1;
    x = x >> b2;
    long b1 = !!(x >> 1);
    x = x >> b1;
    return b32 + b16 + b8 + b4 + b2 + b1 + x + 1;
}
/*
 * hexAllLetters - return 1 if the hex representation of x
 *   contains only characters 'a' through 'f'
 *   Example: hexAllLetters(0xabcdefabcdefabcdL) = 1L.
 *            hexAllLetters(0x4031323536373839L) = 0L.
 *            hexAllLetters(0x00AAABBBCCCDDDEEL) = 0L.
 *   Legal ops: ! ~ & ^ | << >>
 *   Max ops: 30
 *   Rating: 4
 */
long hexAllLetters(long x) {

  long mask = 0xf;
  
  long a = (x >> 0) & mask;
  long b = (x >> 4) & mask;
  long c = (x >> 8) & mask;
  long d = (x >> 12) & mask;
  long e = (x >> 16) & mask;
  long f = (x >> 20) & mask;
  long g = (x >> 24) & mask;
  long h = (x >> 28) & mask;
  long i = (x >> 32) & mask;
  long j = (x >> 36) & mask;
  long k = (x >> 40) & mask;
  long l = (x >> 44) & mask;
  long m = (x >> 48) & mask;
  long n = (x >> 52) & mask;
  long o = (x >> 56) & mask;
  long p = (x >> 60) & mask;
  
  long result = a | b | c | d | e | f | g | h | i | j | k | l | m | n | o | p;

  return !(result ^ 0x0a);
}

/*
 * TMax - return maximum two's complement long integer
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
long tmax(void) {
    return ~(1L << 63);;
}
/*
 * isTmin - returns 1 if x is the minimum, two's complement number,
 *     and 0 otherwise
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
long isTmin(long x) {
    return !(x + x) & !!x;
}
/*
 * isNegative - return 1 if x < 0, return 0 otherwise
 *   Example: isNegative(-1L) = 1L.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
long isNegative(long x) {
    return (x >> 63) & 1L;
}
/*
 * integerLog2 - return floor(log base 2 of x), where x > 0
 *   Example: integerLog2(16L) = 4L, integerLog2(31L) = 4L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 60
 *   Rating: 4
 */
long integerLog2(long x) { 
    long mask, result; 
    mask = !!(x >> 32) << 5; 
    result = mask; 
    mask = !!(x >> (result + 16)) << 4; 
    result += mask; 
    mask = !!(x >> (result + 8)) << 3; 
    result += mask; 
    mask = !!(x >> (result + 4)) << 2; 
    result += mask; 
    mask = !!(x >> (result + 2)) << 1; 
    result += mask; 
    result += !!(x >> (result + 1)); 
    return result; 
}
/*
 * floatFloat2Int - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int floatFloat2Int(unsigned uf) {
    int exp = (uf >> 23) & 0xFF; // Extract 8-bit index
    int sign = uf & 0x80000000; // Extracting Sign Bits
    int frac = uf & 0x007FFFFF; // Extract 23 digits
    int bias = 127;
    if (uf == 0) return 0;
    int e = exp - bias;
    if (exp == 0xFF || e > 30) return 0x80000000u;
    // Add a hidden 1
    frac = frac | (1 << 23);
    // May overflow if e is greater than 23
    if (e < 0) return 0;
    if (e > 23) {
        frac = frac << (e - 23);
    }   else {
        frac = frac >> (23 - e);
    }
    if (sign) return -frac;
    return frac;
}
/*
 * floatScale1d4 - Return bit-level equivalent of expression 0.25*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale1d4(unsigned uf) {
    unsigned mask_exp = 0x7F800000;
    unsigned exp = uf & mask_exp;

    // Case 1: NaN or infinity
    if (exp == mask_exp) {
        return uf;
    }

    // Case 1.5: Negative zero
    if (uf == 0x80000000L) {
        return uf;
    }

    // Case 2: Denormalized or very small normalized numbers
    // When exp is 0 or 0x00800000 (meaning the actual floating point exponent is -126)
    if (exp <= 0x00800000) {
        // Right shift by 2 to multiply by 0.25. Add rounding as well.
        return (uf + (uf & 3)) >> 2;
    }

    // Case 3: Normalized number
    return uf - (2 << 23); // Decrementing the exponent by 2.
}


/*
 * floatNegate - Return bit-level equivalent of expression -f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   Rating: 2
 */
unsigned floatNegate(unsigned uf) {
    int exp = (uf >> 23) & 0xFF;
    int frac = uf & 0x007FFFFF;
    int isNaN = (exp == 0xFF) && frac;
    if(isNaN) {
        return uf;
    }
    return uf ^ (1 << 31);
}

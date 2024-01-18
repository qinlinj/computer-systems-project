// INTEGER CODING RULES:

//   Replace the "return" statement in each function with one
//   or more lines of C code that implements the function. Your code
//   must conform to the following style:

//   long Funct(long arg1, long arg2, ...) {
//       // brief description of how your implementation works
//       long var1 = Expr1;
//       ...
//       long varM = ExprM;

//       varJ = ExprJ;
//       ...
//       varN = ExprN;
//       return ExprR;
//   }

//   Each "Expr" is an expression using ONLY the following:
//   1. (Long) integer constants 0 through 255 (0xFFL), inclusive. You are
//       not allowed to use big constants such as 0xffffffffL.
//   2. Function arguments and local variables (no global variables).
//   3. Local variables of type int and long
//   4. Unary integer operations ! ~
//      - Their arguments can have types int or long
//      - Note that ! always returns int, even if the argument is long
//   5. Binary integer operations & ^ | + << >>
//      - Their arguments can have types int or long
//   6. Casting from int to long and from long to int

//   Some of the problems restrict the set of allowed operators even further.
//   Each "Expr" may consist of multiple operators. You are not restricted to
//   one operator per line.

//   You are expressly forbidden to:
//   1. Use any control constructs such as if, do, while, for, switch, etc.
//   2. Define or use any macros.
//   3. Define any additional functions in this file.
//   4. Call any functions.
//   5. Use any other operations, such as &&, ||, -, or ?:
//   6. Use any form of casting other than between int and long.
//   7. Use any data type other than int or long.  This implies that you
//      cannot use arrays, structs, or unions.

//   You may assume that your machine:
//   1. Uses 2s complement representations for int and long.
//   2. Data type int is 32 bits, long is 64.
//   3. Performs right shifts arithmetically.
//   4. Has unpredictable behavior when shifting if the shift amount
//      is less than 0 or greater than 31 (int) or 63 (long)

// EXAMPLES OF ACCEPTABLE CODING STYLE:
//   //
//   // pow2plus1 - returns 2^x + 1, where 0 <= x <= 63
//   //
//   long pow2plus1(long x) {
//      // exploit ability of shifts to compute powers of 2
//      // Note that the 'L' indicates a long constant
//      return (1L << x) + 1L;
//   }

//   //
//   // pow2plus4 - returns 2^x + 4, where 0 <= x <= 63
//   //
//   long pow2plus4(long x) {
//      // exploit ability of shifts to compute powers of 2
//      long result = (1L << x);
//      result += 4L;
//      return result;
//   }

// NOTES:
//   1. Use the dlc (data lab checker) compiler (described in the handout) to
//      check the legality of your solutions.
//   2. Each function has a maximum number of operations (integer, logical,
//      or comparison) that you are allowed to use for your implementation
//      of the function.  The max operator count is checked by dlc.
//      Note that assignment ('=') is not counted; you may use as many of
//      these as you want without penalty.
//   3. Use the btest test harness to check your functions for correctness.
//   4. Use the BDD checker to formally verify your functions
//   5. The maximum number of ops for each function is given in the
//      header comment for each function. If there are any inconsistencies
//      between the maximum ops in the writeup and in this file, consider
//      this file the authoritative source.

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
// long hexAllLetters(long x) {
// //     * hexAllLetters - return 1 if the hex representation of x
// // *   contains only characters 'a' through 'f'
// // *   Example: hexAllLetters(0xabcdefabcdefabcdL) = 1L.
// // *            hexAllLetters(0x4031323536373839L) = 0L.
// // *            hexAllLetters(0x00AAABBBCCCDDDEEL) = 0L.
// // *   Legal ops: ! ~ & ^ | << >>
// // *   Max ops: 30
// // *   Rating: 4
// // ERROR: Test hexAllLetters(-1L[0xffffffffffffffffL]) failed...
// // ...Gives 0L[0x0L]. Should be 1L[0x1L]
// // ERROR: Test hexAllLetters(-1L[0xffffffffffffffffL]) failed...
// }

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
// unsigned floatScale1d4(unsigned uf) {
//     /*
//     * floatScale1d4 - Return bit-level equivalent of expression 0.25*f for
//     *   floating point argument f.
//     *   Both the argument and result are passed as unsigned int's, but
//     *   they are to be interpreted as the bit-level representation of
//     *   single-precision floating point values.
//     *   When argument is NaN, return argument
//     *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
//     *   Max ops: 30
//     * Input floatScale1d4(0L[0x0L]). Should be 0L[0x0L]
//     * Input floatScale1d4(2147483648L[0x80000000L]). Should be 2147483648L[0x80000000L]
//     * Input floatScale1d4(8388608L[0x800000L]). Should be 2097152L[0x200000L]
//     * Input floatScale1d4(8388608L[0x800000L]). Should be 2097152L[0x200000L]
//     */

// }

unsigned floatScale1d4(unsigned uf) {
    unsigned sign = uf & 0x80000000;  // Extract the sign bit
    unsigned exp = uf & 0x7F800000;   // Extract the exponent
    unsigned frac = uf & 0x007FFFFF;  // Extract the fraction

    // If NaN or +/- infinity, return original value
    if (exp == 0x7F800000) {
        return uf;
    }

    // For denormalized numbers or the value 0
    if (exp == 0 || exp == 0x00800000) {
        // Convert number to denormalized form and divide fraction by 4 (shift right by 2 positions)
        frac |= (exp >> 22);
        return sign | (frac >> 2);
    }
    // For normalized numbers, decrement the exponent by 2 to divide by 4
    // Taking care if the exponent becomes 0 after subtraction, it becomes a denormalized number
    if ((exp >> 23) <= 2) {
        unsigned shiftVal = (1 << (2 - (exp >> 23))) - 1;
        frac = frac | 0x00800000;  // 1.fraction form
        return sign | ((frac + shiftVal) >> (2 - (exp >> 23)));
    }
    return sign | (exp - (2 << 23)) | frac;
}

int main() {
    unsigned test1 = floatScale1d4(0x0L); // Should be 0x0L
    unsigned test2 = floatScale1d4(0x80000000L); // Should be 0x80000000L
    unsigned test3 = floatScale1d4(0x800000L); // Should be 0x200000L
    unsigned test4 = floatScale1d4(0x3f800000L); // Should be 0x3e800000L

    // Print or assert your results as you need.

    return 0;
}





/* Software floating-point emulation.
   Return a * b
   Copyright (C) 1997-2022 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   In addition to the permissions in the GNU Lesser General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file into
   combinations with other programs, and to distribute those
   combinations without any restriction coming from the use of this
   file.  (The Lesser General Public License restrictions do apply in
   other respects; for example, they cover modification of the file,
   and distribution when not linked into a combine executable.)

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include "soft-fp.h"
#include "double.h"

DFtype
__muldf3 (DFtype a, DFtype b)
{
  /* balatro-ps2: the everyday case first (two normal numbers, a normal product), with the four 32x32 products
     the EE has an instruction for; the generic code below was 6 % of a frame (docs/perf.md). Same rounding (to
     nearest, ties to even); runtime/test/softfp.sh compares it with the host's doubles. */
  {
    union { DFtype d; unsigned long long u; } x, y;
    unsigned int ex, ey;
    x.d = a, y.d = b;
    ex = (x.u >> 52) & 0x7ff, ey = (y.u >> 52) & 0x7ff;
    if ((!(x.u << 1) || !(y.u << 1)) && ex != 0x7ff && ey != 0x7ff)
      {
	x.u = (x.u ^ y.u) & 0x8000000000000000ULL; /* a zero by a finite number */
	return x.d;
      }
    if (ex - 1 < 0x7fe && ey - 1 < 0x7fe)
      {
	unsigned long long mx = (x.u & 0xfffffffffffffULL) | 0x10000000000000ULL;
	unsigned long long my = (y.u & 0xfffffffffffffULL) | 0x10000000000000ULL;
	unsigned int xl = mx, xh = mx >> 32, yl = my, yh = my >> 32;
	unsigned long long ll = (unsigned long long) xl * yl;
	unsigned long long mid = (unsigned long long) xl * yh + (unsigned long long) xh * yl;
	unsigned long long lo = ll + (mid << 32);
	unsigned long long hi = (unsigned long long) xh * yh + (mid >> 32) + (lo < ll); /* hi:lo in [2^104, 2^106) */
	int sh = (hi >> 41) & 1;
	int e = (int) (ex + ey) - 1023 + sh;
	if ((unsigned int) (e - 1) < 0x7fd)
	  {
	    unsigned long long m = (hi << (12 - sh)) | (lo >> (52 + sh)); /* the top 53 bits */
	    unsigned long long rest = lo << (12 - sh);			  /* what is below them */
	    m += rest > 0x8000000000000000ULL || (rest == 0x8000000000000000ULL && (m & 1));
	    /* a carry out of the 53 bits lands in the exponent */
	    x.u = ((x.u ^ y.u) & 0x8000000000000000ULL) | (((unsigned long long) e << 52) + (m - 0x10000000000000ULL));
	    return x.d;
	  }
      }
  }
  FP_DECL_EX;
  FP_DECL_D (A);
  FP_DECL_D (B);
  FP_DECL_D (R);
  DFtype r;

  FP_INIT_ROUNDMODE;
  FP_UNPACK_D (A, a);
  FP_UNPACK_D (B, b);
  FP_MUL_D (R, A, B);
  FP_PACK_D (r, R);
  FP_HANDLE_EXCEPTIONS;

  return r;
}

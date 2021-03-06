/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date
 
       @author Cade Brown <cbrow216@vols.utk.edu>

    A wrapper class around hipFloatComplex & hipDoubleComplex due to problems
    with AMD's implementation, namely the function & operator overloads

    See: https://github.com/ROCm-Developer-Tools/HIP/issues/1575

    Eventually, this file shouldn't be needed, but it seems like it will take
      a while for AMD to merge in the solution.

    The original license is noted below:

Copyright (c) 2015 - present Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef MAGMA_HIP_COMPLEX_H
#define MAGMA_HIP_COMPLEX_H

#include "hip/hcc_detail/hip_vector_types.h"

// TODO: Clang has a bug which allows device functions to call std functions
// when std functions are introduced into default namespace by using statement.
// math.h may be included after this bug is fixed.
#if __cplusplus
#include <cmath>
#else
#include "math.h"
#endif

#if __cplusplus
#define COMPLEX_NEG_OP_OVERLOAD(type)                                                              \
    __device__ __host__ static inline type operator-(const type& op) {                             \
        type ret;                                                                                  \
        ret.x = -op.x;                                                                             \
        ret.y = -op.y;                                                                             \
        return ret;                                                                                \
    }

#define COMPLEX_EQ_OP_OVERLOAD(type)                                                               \
    __device__ __host__ static inline bool operator==(const type& lhs, const type& rhs) {          \
        return lhs.x == rhs.x && lhs.y == rhs.y;                                                   \
    }

#define COMPLEX_NE_OP_OVERLOAD(type)                                                               \
    __device__ __host__ static inline bool operator!=(const type& lhs, const type& rhs) {          \
        return !(lhs == rhs);                                                                      \
    }

#define COMPLEX_ADD_OP_OVERLOAD(type)                                                              \
    __device__ __host__ static inline type operator+(const type& lhs, const type& rhs) {           \
        type ret;                                                                                  \
        ret.x = lhs.x + rhs.x;                                                                     \
        ret.y = lhs.y + rhs.y;                                                                     \
        return ret;                                                                                \
    }

#define COMPLEX_SUB_OP_OVERLOAD(type)                                                              \
    __device__ __host__ static inline type operator-(const type& lhs, const type& rhs) {           \
        type ret;                                                                                  \
        ret.x = lhs.x - rhs.x;                                                                     \
        ret.y = lhs.y - rhs.y;                                                                     \
        return ret;                                                                                \
    }

#define COMPLEX_MUL_OP_OVERLOAD(type)                                                              \
    __device__ __host__ static inline type operator*(const type& lhs, const type& rhs) {           \
        type ret;                                                                                  \
        ret.x = lhs.x * rhs.x - lhs.y * rhs.y;                                                     \
        ret.y = lhs.x * rhs.y + lhs.y * rhs.x;                                                     \
        return ret;                                                                                \
    }

#define COMPLEX_DIV_OP_OVERLOAD(type)                                                              \
    __device__ __host__ static inline type operator/(const type& lhs, const type& rhs) {           \
        type ret;                                                                                  \
        ret.x = (lhs.x * rhs.x + lhs.y * rhs.y);                                                   \
        ret.y = (rhs.x * lhs.y - lhs.x * rhs.y);                                                   \
        ret.x = ret.x / (rhs.x * rhs.x + rhs.y * rhs.y);                                           \
        ret.y = ret.y / (rhs.x * rhs.x + rhs.y * rhs.y);                                           \
        return ret;                                                                                \
    }

#define COMPLEX_ADD_PREOP_OVERLOAD(type)                                                           \
    __device__ __host__ static inline type& operator+=(type& lhs, const type& rhs) {               \
        lhs.x += rhs.x;                                                                            \
        lhs.y += rhs.y;                                                                            \
        return lhs;                                                                                \
    }

#define COMPLEX_SUB_PREOP_OVERLOAD(type)                                                           \
    __device__ __host__ static inline type& operator-=(type& lhs, const type& rhs) {               \
        lhs.x -= rhs.x;                                                                            \
        lhs.y -= rhs.y;                                                                            \
        return lhs;                                                                                \
    }

#define COMPLEX_MUL_PREOP_OVERLOAD(type)                                                           \
    __device__ __host__ static inline type& operator*=(type& lhs, const type& rhs) {               \
        lhs = lhs * rhs;                                                                           \
        return lhs;                                                                                \
    }

#define COMPLEX_DIV_PREOP_OVERLOAD(type)                                                           \
    __device__ __host__ static inline type& operator/=(type& lhs, const type& rhs) {               \
        lhs = lhs / rhs;                                                                           \
        return lhs;                                                                                \
    }

#define COMPLEX_SCALAR_PRODUCT(type, type1)                                                        \
    __device__ __host__ static inline type operator*(const type& lhs, type1 rhs) {                 \
        type ret;                                                                                  \
        ret.x = lhs.x * rhs;                                                                       \
        ret.y = lhs.y * rhs;                                                                       \
        return ret;                                                                                \
    }

#endif

typedef struct {

    // anonymous union
    union {
        // vector type
        float2 _;

        // components
        struct {
            float x, y;
        };
    };

} hipFloatComplex;

static_assert(sizeof(hipFloatComplex) == sizeof(float2), "hipFloatComplex should be the same as a float[2]");

__device__ __host__ static inline float hipCrealf(hipFloatComplex z) { return z.x; }

__device__ __host__ static inline float hipCimagf(hipFloatComplex z) { return z.y; }

__device__ __host__ static inline hipFloatComplex make_hipFloatComplex(float a, float b) {
    hipFloatComplex z;
    z.x = a;
    z.y = b;
    return z;
}

__device__ __host__ static inline hipFloatComplex hipConjf(hipFloatComplex z) {
    hipFloatComplex ret;
    ret.x = z.x;
    ret.y = -z.y;
    return ret;
}

__device__ __host__ static inline float hipCsqabsf(hipFloatComplex z) {
    return z.x * z.x + z.y * z.y;
}

__device__ __host__ static inline hipFloatComplex hipCaddf(hipFloatComplex p, hipFloatComplex q) {
    return make_hipFloatComplex(p.x + q.x, p.y + q.y);
}

__device__ __host__ static inline hipFloatComplex hipCsubf(hipFloatComplex p, hipFloatComplex q) {
    return make_hipFloatComplex(p.x - q.x, p.y - q.y);
}

__device__ __host__ static inline hipFloatComplex hipCmulf(hipFloatComplex p, hipFloatComplex q) {
    return make_hipFloatComplex(p.x * q.x - p.y * q.y, p.y * q.x + p.x * q.y);
}

__device__ __host__ static inline hipFloatComplex hipCdivf(hipFloatComplex p, hipFloatComplex q) {
    float sqabs = hipCsqabsf(q);
    hipFloatComplex ret;
    ret.x = (p.x * q.x + p.y * q.y) / sqabs;
    ret.y = (p.y * q.x - p.x * q.y) / sqabs;
    return ret;
}

__device__ __host__ static inline float hipCabsf(hipFloatComplex z) { return sqrtf(hipCsqabsf(z)); }



typedef struct hipDoubleComplex {

    // anonymous union
    union {
        // vector type
        double2 _;

        // components
        struct {
            double x, y;
        };
    };

} hipDoubleComplex;

static_assert(sizeof(hipDoubleComplex) == sizeof(double2), "hipDoubleComplex should be the same as a double[2]");

__device__ __host__ static inline double hipCreal(hipDoubleComplex z) { return z.x; }

__device__ __host__ static inline double hipCimag(hipDoubleComplex z) { return z.y; }

__device__ __host__ static inline hipDoubleComplex make_hipDoubleComplex(double a, double b) {
    hipDoubleComplex z;
    z.x = a;
    z.y = b;
    return z;
}

__device__ __host__ static inline hipDoubleComplex hipConj(hipDoubleComplex z) {
    hipDoubleComplex ret;
    ret.x = z.x;
    ret.y = z.y;
    return ret;
}

__device__ __host__ static inline double hipCsqabs(hipDoubleComplex z) {
    return z.x * z.x + z.y * z.y;
}

__device__ __host__ static inline hipDoubleComplex hipCadd(hipDoubleComplex p, hipDoubleComplex q) {
    return make_hipDoubleComplex(p.x + q.x, p.y + q.y);
}

__device__ __host__ static inline hipDoubleComplex hipCsub(hipDoubleComplex p, hipDoubleComplex q) {
    return make_hipDoubleComplex(p.x - q.x, p.y - q.y);
}

__device__ __host__ static inline hipDoubleComplex hipCmul(hipDoubleComplex p, hipDoubleComplex q) {
    return make_hipDoubleComplex(p.x * q.x - p.y * q.y, p.y * q.x + p.x * q.y);
}

__device__ __host__ static inline hipDoubleComplex hipCdiv(hipDoubleComplex p, hipDoubleComplex q) {
    double sqabs = hipCsqabs(q);
    hipDoubleComplex ret;
    ret.x = (p.x * q.x + p.y * q.y) / sqabs;
    ret.y = (p.y * q.x - p.x * q.y) / sqabs;
    return ret;
}

__device__ __host__ static inline double hipCabs(hipDoubleComplex z) { return sqrtf(hipCsqabs(z)); }


#if __cplusplus


// we don't need these, because MAGMA defines operators, and these cause
//   issues with overloading
/*

COMPLEX_NEG_OP_OVERLOAD(hipFloatComplex)
COMPLEX_EQ_OP_OVERLOAD(hipFloatComplex)
COMPLEX_NE_OP_OVERLOAD(hipFloatComplex)
COMPLEX_ADD_OP_OVERLOAD(hipFloatComplex)
COMPLEX_SUB_OP_OVERLOAD(hipFloatComplex)
COMPLEX_MUL_OP_OVERLOAD(hipFloatComplex)
COMPLEX_DIV_OP_OVERLOAD(hipFloatComplex)
COMPLEX_ADD_PREOP_OVERLOAD(hipFloatComplex)
COMPLEX_SUB_PREOP_OVERLOAD(hipFloatComplex)
COMPLEX_MUL_PREOP_OVERLOAD(hipFloatComplex)
COMPLEX_DIV_PREOP_OVERLOAD(hipFloatComplex)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, unsigned short)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, signed short)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, unsigned int)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, signed int)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, float)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, unsigned long)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, signed long)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, double)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, signed long long)
COMPLEX_SCALAR_PRODUCT(hipFloatComplex, unsigned long long)


COMPLEX_NEG_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_EQ_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_NE_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_ADD_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_SUB_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_MUL_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_DIV_OP_OVERLOAD(hipDoubleComplex)
COMPLEX_ADD_PREOP_OVERLOAD(hipDoubleComplex)
COMPLEX_SUB_PREOP_OVERLOAD(hipDoubleComplex)
COMPLEX_MUL_PREOP_OVERLOAD(hipDoubleComplex)
COMPLEX_DIV_PREOP_OVERLOAD(hipDoubleComplex)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, unsigned short)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, signed short)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, unsigned int)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, signed int)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, float)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, unsigned long)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, signed long)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, double)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, signed long long)
COMPLEX_SCALAR_PRODUCT(hipDoubleComplex, unsigned long long)

*/

#endif


typedef hipFloatComplex hipComplex;

__device__ __host__ static inline hipComplex make_hipComplex(float x, float y) {
    return make_hipFloatComplex(x, y);
}

__device__ __host__ static inline hipFloatComplex hipComplexDoubleToFloat(hipDoubleComplex z) {
    return make_hipFloatComplex((float)z.x, (float)z.y);
}

__device__ __host__ static inline hipDoubleComplex hipComplexFloatToDouble(hipFloatComplex z) {
    return make_hipDoubleComplex((double)z.x, (double)z.y);
}

__device__ __host__ static inline hipComplex hipCfmaf(hipComplex p, hipComplex q, hipComplex r) {
    float real = (p.x * q.x) + r.x;
    float imag = (q.x * p.y) + r.y;

    real = -(p.y * q.y) + real;
    imag = (p.x * q.y) + imag;

    return make_hipComplex(real, imag);
}

__device__ __host__ static inline hipDoubleComplex hipCfma(hipDoubleComplex p, hipDoubleComplex q,
                                                           hipDoubleComplex r) {
    double real = (p.x * q.x) + r.x;
    double imag = (q.x * p.y) + r.y;

    real = -(p.y * q.y) + real;
    imag = (p.x * q.y) + imag;

    return make_hipDoubleComplex(real, imag);
}


// we don't need to define these function overloads, because
//   include/magma_operators.h already does

/*
// Complex functions returning real numbers.
#define __DEFINE_hip_COMPLEX_REAL_FUN(func, hipFun) \
__device__ __host__ inline float func(const hipFloatComplex& z) { return hipFun##f(z); } \
__device__ __host__ inline double func(const hipDoubleComplex& z) { return hipFun(z); }

__DEFINE_hip_COMPLEX_REAL_FUN(abs, hipCabs)
__DEFINE_hip_COMPLEX_REAL_FUN(real, hipCreal)
__DEFINE_hip_COMPLEX_REAL_FUN(imag, hipCimag)

// Complex functions returning complex numbers.
#define __DEFINE_hip_COMPLEX_FUN(func, hipFun) \
__device__ __host__ inline hipFloatComplex func(const hipFloatComplex& z) { return hipFun##f(z); } \
__device__ __host__ inline hipDoubleComplex func(const hipDoubleComplex& z) { return hipFun(z); }

__DEFINE_hip_COMPLEX_FUN(conj, hipConj)
*/

#endif

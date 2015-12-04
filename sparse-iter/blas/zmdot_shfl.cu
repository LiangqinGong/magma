/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @precisions normal z -> c d s
       @author Hartwig Anzt

*/
#include "common_magma.h"

#include "magmasparse_z.h"
#define BLOCK_SIZE 256

#define PRECISION_z


template<typename T>
__inline__ __device__
T warpReduceSum(T val) {
#if MIN_CUDA_ARCH >= 300
    val += __shfl_down(val, 16);
    val += __shfl_down(val, 8);
    val += __shfl_down(val, 4);
    val += __shfl_down(val, 2);
    val += __shfl_down(val, 1);
#endif
    return val;
}

#ifdef PRECISION_z
template<>
__inline__ __device__
magmaDoubleComplex warpReduceSum<magmaDoubleComplex>(magmaDoubleComplex val) {
#if MIN_CUDA_ARCH >= 300
    int4 a = *reinterpret_cast<int4*>(&val);
    a.x += __shfl_down(a.x, 16);
    a.y += __shfl_down(a.y, 16);
    a.z += __shfl_down(a.z, 16);
    a.w += __shfl_down(a.w, 16);
    a.x += __shfl_down(a.x, 8);
    a.y += __shfl_down(a.y, 8);
    a.z += __shfl_down(a.z, 8);
    a.w += __shfl_down(a.w, 8);
    a.x += __shfl_down(a.x, 4);
    a.y += __shfl_down(a.y, 4);
    a.z += __shfl_down(a.z, 4);
    a.w += __shfl_down(a.w, 4);
    a.x += __shfl_down(a.x, 2);
    a.y += __shfl_down(a.y, 2);
    a.z += __shfl_down(a.z, 2);
    a.w += __shfl_down(a.w, 2);
    a.x += __shfl_down(a.x, 1);
    a.y += __shfl_down(a.y, 1);
    a.z += __shfl_down(a.z, 1);
    a.w += __shfl_down(a.w, 1);
#endif
    return val;
}
#endif

#ifdef PRECISION_c
template<>
__inline__ __device__
magmaFloatComplex warpReduceSum<magmaFloatComplex>(magmaFloatComplex val) {
#if MIN_CUDA_ARCH >= 300
    float2 a = *reinterpret_cast<float2*>(&val);
    a.x += __shfl_down(a.x, 16);
    a.y += __shfl_down(a.y, 16);
    a.x += __shfl_down(a.x, 8);
    a.y += __shfl_down(a.y, 8);
    a.x += __shfl_down(a.x, 4);
    a.y += __shfl_down(a.y, 4);
    a.x += __shfl_down(a.x, 2);
    a.y += __shfl_down(a.y, 2);
    a.x += __shfl_down(a.x, 1);
    a.y += __shfl_down(a.y, 1);
#endif
    return val;
}
#endif

template<typename T>
__inline__ __device__
T blockReduceSum(T val) {

    extern __shared__ T shared[]; // Shared mem for 32 partial sums
    int lane = threadIdx.x % warpSize;
    int wid = threadIdx.x / warpSize;

    val = warpReduceSum<T>(val);     // Each warp performs partial reduction

    if (lane==0) shared[threadIdx.y*32+wid]=val; // Write reduced value to shared memory

    __syncthreads();              // Wait for all partial reductions

    //read from shared memory only if that warp existed
    val = (threadIdx.x < blockDim.x / warpSize) ? shared[threadIdx.y*32+lane] : MAGMA_Z_MAKE(0.0,0.0);
    
    if (wid==0) val = warpReduceSum<T>(val); //Final reduce within first warp
    return val;
}

template<typename T> 
__global__ void deviceReduceKernel(T *in, T* out, int N) {
    T sum = MAGMA_Z_MAKE(0.0,0.0);
    //reduce multiple elements per thread
    for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < N; i += blockDim.x * gridDim.x) {
        sum += in[i];
    }
    sum = blockReduceSum<T>(sum);
    if (threadIdx.x==0)
    out[blockIdx.x]=sum;
}

// dot product for multiple vectors using shuffle intrinsics and less shared memory
__global__ void
magma_zblockdot_kernel_shuffle( 
    int n, 
    int k,
    magmaDoubleComplex * v,
    magmaDoubleComplex * r,
    magmaDoubleComplex * vtmp)
{
    int Idx = threadIdx.x;   
    int i   = blockIdx.x * blockDim.x + Idx;
    int j = threadIdx.y;
    magmaDoubleComplex tmp = (i<n)?(v[i+j*n] * r[i]):MAGMA_Z_MAKE(0.0,0.0);
    tmp = blockReduceSum(tmp);
    if ( Idx == 0 ){
        vtmp[ blockIdx.x+j*gridDim.x ] = tmp;
    }
}

/**
    Purpose
    -------

    Computes the scalar product of a set of vectors v_i such that

    skp = ( <v_0,r>, <v_1,r>, .. )

    Returns the vector skp.

    Arguments
    ---------

    @param[in]
    n           int
                length of v_i and r

    @param[in]
    k           int
                # vectors v_i

    @param[in]
    v           magmaDoubleComplex_ptr 
                v = (v_0 .. v_i.. v_k)

    @param[in]
    r           magmaDoubleComplex_ptr 
                r

    @param[in]
    d1          magmaDoubleComplex_ptr 
                workspace

    @param[in]
    d2          magmaDoubleComplex_ptr 
                workspace

    @param[out]
    skp         magmaDoubleComplex_ptr 
                vector[k] of scalar products (<v_i,r>...)

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_zblas
    ********************************************************************/

#define PAD(n,p) (((n)<1 || (p)<1)?(n):(((n) % (p)) ? ((n) + (p) - (n) % (p)) : (n)))
extern "C" magma_int_t
magma_zmdotc_shfl(
    int n, 
    int k, 
    magmaDoubleComplex_ptr v, 
    magmaDoubleComplex_ptr r,
    magmaDoubleComplex_ptr d1,
    magmaDoubleComplex_ptr d2,
    magmaDoubleComplex_ptr skp,
    magma_queue_t queue )
{
#if MIN_CUDA_ARCH < 300
    return magma_zmdotc(n,k,v,r,d1,d2,skp,queue);
#else
    // set queue for old dense routines
    magma_queue_t orig_queue;
    magmablasGetKernelStream( &orig_queue );

    int local_block_size = 1024;
    dim3 block( PAD(magma_ceildiv(local_block_size,k),32),k );
    while (block.x*block.y > 1024) {
        block.x-=32;
    }
    dim3 grid( magma_ceildiv( n, block.x ) );
    magma_zblockdot_kernel_shuffle<<< grid, block, 32*k*sizeof(magmaDoubleComplex), queue->cuda_stream() >>>( n, k, v, r, d1 );
    int j;
    for (j=0; j<k; j++) {
        deviceReduceKernel<magmaDoubleComplex> <<<1,1024,32*sizeof(magmaDoubleComplex),queue->cuda_stream()>>>(d1+grid.x*j,skp+j,grid.x);
    }
   
    magmablasSetKernelStream( orig_queue );
   return MAGMA_SUCCESS;
#endif
}

/**
    Purpose
    -------

    This is an extension of the merged dot product above by chunking
    the set of vectors v_i such that the data always fits into cache.
    It is equivalent to a matrix vecor product Vr where V
    contains few rows and many columns. The computation is the same:

    skp = ( <v_0,r>, <v_1,r>, .. )

    Returns the vector skp.

    Arguments
    ---------

    @param[in]
    n           int
                length of v_i and r

    @param[in]
    k           int
                # vectors v_i

    @param[in]
    v           magmaDoubleComplex_ptr 
                v = (v_0 .. v_i.. v_k)

    @param[in]
    r           magmaDoubleComplex_ptr 
                r

    @param[in]
    d1          magmaDoubleComplex_ptr 
                workspace

    @param[in]
    d2          magmaDoubleComplex_ptr 
                workspace

    @param[out]
    skp         magmaDoubleComplex_ptr 
                vector[k] of scalar products (<v_i,r>...)

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_z
    ********************************************************************/

extern "C" magma_int_t
magma_zgemvmdot_shfl(
    int n, 
    int k, 
    magmaDoubleComplex_ptr v, 
    magmaDoubleComplex_ptr r,
    magmaDoubleComplex_ptr d1,
    magmaDoubleComplex_ptr d2,
    magmaDoubleComplex_ptr skp,
    magma_queue_t queue )
{
#if MIN_CUDA_ARCH < 300
    return magma_zgemvmdot(n,k,v,r,d1,d2,skp,queue);
#else
    int rows_left = k;
    int offset = 0;
    int chunk_size = 16;
    // process in chunks of 10 - has to be adapted to hardware and precision
    while( rows_left > (chunk_size) ) {
        magma_zmdotc_shfl( n, chunk_size, v+offset*n, r, d1, d2, skp+offset, queue );
        offset = offset + chunk_size;
        rows_left = rows_left-chunk_size;
    }
    // process rest
    magma_zmdotc_shfl( n, rows_left, v+offset*n, r, d1, d2, skp+offset, queue ); 

   return MAGMA_SUCCESS;
#endif
}
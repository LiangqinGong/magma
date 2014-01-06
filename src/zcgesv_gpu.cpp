/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @precisions mixed zc -> ds

*/
#include "common_magma.h"

#define BWDMAX 1.0
#define ITERMAX 30

extern "C" magma_int_t
magma_zcgesv_gpu(char trans, magma_int_t n, magma_int_t nrhs,
                 magmaDoubleComplex *dA, magma_int_t ldda,
                 magma_int_t *ipiv,  magma_int_t *dipiv,
                 magmaDoubleComplex *dB, magma_int_t lddb,
                 magmaDoubleComplex *dX, magma_int_t lddx,
                 magmaDoubleComplex *dworkd, magmaFloatComplex *dworks,
                 magma_int_t *iter, magma_int_t *info)
{
/*  -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

    Purpose
    =======
    ZCGESV computes the solution to a complex system of linear equations
       A * X = B or A' * X = B
    where A is an N-by-N matrix and X and B are N-by-NRHS matrices.

    ZCGESV first attempts to factorize the matrix in complex SINGLE PRECISION
    and use this factorization within an iterative refinement procedure
    to produce a solution with complex DOUBLE PRECISION norm-wise backward error
    quality (see below). If the approach fails the method switches to a
    complex DOUBLE PRECISION factorization and solve.

    The iterative refinement is not going to be a winning strategy if
    the ratio complex SINGLE PRECISION performance over complex DOUBLE PRECISION
    performance is too small. A reasonable strategy should take the
    number of right-hand sides and the size of the matrix into account.
    This might be done with a call to ILAENV in the future. Up to now, we
    always try iterative refinement.
    
    The iterative refinement process is stopped if
        ITER > ITERMAX
    or for all the RHS we have:
        RNRM < SQRT(N)*XNRM*ANRM*EPS*BWDMAX
    where
        o ITER is the number of the current iteration in the iterative
          refinement process
        o RNRM is the infinity-norm of the residual
        o XNRM is the infinity-norm of the solution
        o ANRM is the infinity-operator-norm of the matrix A
        o EPS is the machine epsilon returned by DLAMCH('Epsilon')
    The value ITERMAX and BWDMAX are fixed to 30 and 1.0D+00 respectively.

    Arguments
    =========
    TRANS   (input) CHARACTER*1
            Specifies the form of the system of equations:
            = 'N':  A * X = B  (No transpose)
            = 'T':  A'* X = B  (Transpose)
            = 'C':  A'* X = B  (Conjugate transpose = Transpose)

    N       (input) INTEGER
            The number of linear equations, i.e., the order of the
            matrix A.  N >= 0.

    NRHS    (input) INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    dA      (input or input/output) COMPLEX_16 array on the GPU, dimension (ldda,N)
            On entry, the N-by-N coefficient matrix A.
            On exit, if iterative refinement has been successfully used
            (info.EQ.0 and ITER.GE.0, see description below), A is
            unchanged. If double precision factorization has been used
            (info.EQ.0 and ITER.LT.0, see description below), then the
            array dA contains the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    ldda    (input) INTEGER
            The leading dimension of the array dA.  ldda >= max(1,N).

    IPIV    (output) INTEGER array, dimension (N)
            The pivot indices that define the permutation matrix P;
            row i of the matrix was interchanged with row IPIV(i).
            Corresponzc either to the single precision factorization
            (if info.EQ.0 and ITER.GE.0) or the double precision
            factorization (if info.EQ.0 and ITER.LT.0).

    dIPIV   (output) INTEGER array on the GPU, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was moved to row IPIV(i).

    dB      (input) COMPLEX_16 array on the GPU, dimension (lddb,NRHS)
            The N-by-NRHS right hand side matrix B.

    lddb    (input) INTEGER
            The leading dimension of the array dB.  lddb >= max(1,N).

    dX      (output) COMPLEX_16 array on the GPU, dimension (lddx,NRHS)
            If info = 0, the N-by-NRHS solution matrix X.

    lddx    (input) INTEGER
            The leading dimension of the array dX.  lddx >= max(1,N).

    dworkd  (workspace) COMPLEX_16 array on the GPU, dimension (N*NRHS)
            This array is used to hold the residual vectors.

    dworks  (workspace) COMPLEX array on the GPU, dimension (N*(N+NRHS))
            This array is used to store the complex single precision matrix
            and the right-hand sides or solutions in single precision.

    iter    (output) INTEGER
            < 0: iterative refinement has failed, double precision
                 factorization has been performed
                 -1 : the routine fell back to full precision for
                      implementation- or machine-specific reasons
                 -2 : narrowing the precision induced an overflow,
                      the routine fell back to full precision
                 -3 : failure of SGETRF
                 -31: stop the iterative refinement after the 30th iteration
            > 0: iterative refinement has been successfully used.
                 Returns the number of iterations
 
    info   (output) INTEGER
            = 0:  successful exit
            < 0:  if info = -i, the i-th argument had an illegal value
            > 0:  if info = i, U(i,i) computed in DOUBLE PRECISION is
                  exactly zero.  The factorization has been completed,
                  but the factor U is exactly singular, so the solution
                  could not be computed.
    =====================================================================    */

    #define dB(i,j)     (dB + (i) + (j)*lddb)
    #define dX(i,j)     (dX + (i) + (j)*lddx)
    #define dR(i,j)     (dR + (i) + (j)*lddr)
    
    magmaDoubleComplex c_neg_one = MAGMA_Z_NEG_ONE;
    magmaDoubleComplex c_one     = MAGMA_Z_ONE;
    magma_int_t     ione  = 1;
    magmaDoubleComplex *dR;
    magmaFloatComplex  *dSA, *dSX;
    magmaDoubleComplex Xnrmv, Rnrmv;
    double          Anrm, Xnrm, Rnrm, cte, eps;
    magma_int_t     i, j, iiter, lddsa, lddr;
    
    /* Check arguments */
    *iter = 0;
    *info = 0;
    if ( n < 0 )
        *info = -1;
    else if ( nrhs < 0 )
        *info = -2;
    else if ( ldda < max(1,n))
        *info = -4;
    else if ( lddb < max(1,n))
        *info = -8;
    else if ( lddx < max(1,n))
        *info = -10;
    
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    
    if ( n == 0 || nrhs == 0 )
        return *info;

    lddsa = n;
    lddr  = n;
    
    dSA = dworks;
    dSX = dSA + lddsa*n;
    dR  = dworkd;
    
    eps  = lapackf77_dlamch("Epsilon");
    Anrm = magmablas_zlange('I', n, n, dA, ldda, (double*)dworkd );
    cte  = Anrm * eps * pow((double)n, 0.5) * BWDMAX;
    
    /*
     * Convert to single precision
     */
    //magmablas_zlag2c( n, nrhs, dB, lddb, dSX, lddsx, info );  // done inside zcgetrs with pivots
    if (*info != 0) {
        *iter = -2;
        goto FALLBACK;
    }
    
    magmablas_zlag2c( n, n, dA, ldda, dSA, lddsa, info );
    if (*info != 0) {
        *iter = -2;
        goto FALLBACK;
    }
    
    // factor dSA in single precision
    magma_cgetrf_gpu( n, n, dSA, lddsa, ipiv, info );
    if (*info != 0) {
        *iter = -3;
        goto FALLBACK;
    }
    
    // Generate parallel pivots
    {
        magma_int_t *newipiv;
        magma_imalloc_cpu( &newipiv, n );
        if ( newipiv == NULL ) {
            *iter = -3;
            goto FALLBACK;
        }
        swp2pswp( trans, n, ipiv, newipiv );
        magma_setvector( n, sizeof(magma_int_t), newipiv, 1, dipiv, 1 );
        magma_free_cpu( newipiv );
    }
    
    // solve dSA*dSX = dB in single precision
    // converts dB to dSX and applies pivots, solves, then converts result back to dX
    magma_zcgetrs_gpu( trans, n, nrhs, dSA, lddsa, dipiv, dB, lddb, dX, lddx, dSX, info );
    
    // residual dR = dB - dA*dX in double precision
    magmablas_zlacpy( MagmaUpperLower, n, nrhs, dB, lddb, dR, lddr );
    if ( nrhs == 1 ) {
        magma_zgemv( trans, n, n,
                     c_neg_one, dA, ldda,
                                dX, 1,
                     c_one,     dR, 1 );
    }
    else {
        magma_zgemm( trans, MagmaNoTrans, n, nrhs, n,
                     c_neg_one, dA, ldda,
                                dX, lddx,
                     c_one,     dR, lddr );
    }
    
    // TODO: use MAGMA_Z_ABS( dX(i,j) ) instead of zlange?
    for( j=0; j < nrhs; j++ ) {
        i = magma_izamax( n, dX(0,j), 1) - 1;
        magma_zgetmatrix( 1, 1, dX(i,j), 1, &Xnrmv, 1 );
        Xnrm = lapackf77_zlange( "F", &ione, &ione, &Xnrmv, &ione, NULL );
        
        i = magma_izamax ( n, dR(0,j), 1 ) - 1;
        magma_zgetmatrix( 1, 1, dR(i,j), 1, &Rnrmv, 1 );
        Rnrm = lapackf77_zlange( "F", &ione, &ione, &Rnrmv, &ione, NULL );
        
        if ( Rnrm >  Xnrm*cte ) {
            goto REFINEMENT;
        }
    }
    
    *iter = 0;
    return *info;

REFINEMENT:
    for( iiter=1; iiter < ITERMAX; ) {
        *info = 0;
        // convert residual dR to single precision dSX
        // solve dSA*dSX = R in single precision
        // convert result back to double precision dR
        // it's okay that dR is used for both dB input and dX output.
        magma_zcgetrs_gpu( trans, n, nrhs, dSA, lddsa, dipiv, dR, lddr, dR, lddr, dSX, info );
        if (*info != 0) {
            *iter = -3;
            goto FALLBACK;
        }
        
        // Add correction and setup residual
        // dX += dR  --and--
        // dR = dB
        // This saves going through dR a second time (if done with one more kernel).
        // -- not really: first time is read, second time is write.
        for( j=0; j < nrhs; j++ ) {
            magmablas_zaxpycp( n, dR(0,j), dX(0,j), dB(0,j) );
        }
        
        // residual dR = dB - dA*dX in double precision
        if ( nrhs == 1 ) {
            magma_zgemv( trans, n, n,
                         c_neg_one, dA, ldda,
                                    dX, 1,
                         c_one,     dR, 1 );
        }
        else {
            magma_zgemm( trans, MagmaNoTrans, n, nrhs, n,
                         c_neg_one, dA, ldda,
                                    dX, lddx,
                         c_one,     dR, lddr );
        }
        
        /*  Check whether the nrhs normwise backward errors satisfy the
         *  stopping criterion. If yes, set ITER=IITER > 0 and return. */
        for( j=0; j < nrhs; j++ ) {
            i = magma_izamax( n, dX(0,j), 1) - 1;
            magma_zgetmatrix( 1, 1, dX(i,j), 1, &Xnrmv, 1 );
            Xnrm = lapackf77_zlange( "F", &ione, &ione, &Xnrmv, &ione, NULL );
            
            i = magma_izamax ( n, dR(0,j), 1 ) - 1;
            magma_zgetmatrix( 1, 1, dR(i,j), 1, &Rnrmv, 1 );
            Rnrm = lapackf77_zlange( "F", &ione, &ione, &Rnrmv, &ione, NULL );
            
            if ( Rnrm >  Xnrm*cte ) {
                goto L20;
            }
        }
        
        /*  If we are here, the nrhs normwise backward errors satisfy
         *  the stopping criterion, we are good to exit. */
        *iter = iiter;
        return *info;
        
      L20:
        iiter++;
    }
    
    /* If we are at this place of the code, this is because we have
     * performed ITER=ITERMAX iterations and never satisified the
     * stopping criterion. Set up the ITER flag accordingly and follow
     * up on double precision routine. */
    *iter = -ITERMAX - 1;
    
FALLBACK:
    /* Single-precision iterative refinement failed to converge to a
     * satisfactory solution, so we resort to double precision. */
    magma_zgetrf_gpu( n, n, dA, ldda, ipiv, info );
    if (*info == 0) {
        magmablas_zlacpy( MagmaUpperLower, n, nrhs, dB, lddb, dX, lddx );
        magma_zgetrs_gpu( trans, n, nrhs, dA, ldda, ipiv, dX, lddx, info );
    }
    
    return *info;
}

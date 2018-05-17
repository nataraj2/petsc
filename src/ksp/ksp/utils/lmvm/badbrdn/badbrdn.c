#include <../src/ksp/ksp/utils/lmvm/lmvm.h> /*I "petscksp.h" I*/

/*
  Limited-memory modified (aka "bad") Broyden's method for approximating 
  the inverse of a Jacobian.
  
  Broyden's method is not guaranteed to be symmetric or positive definite.
  
  The solution method is constructed from equation (6) on page 307 of 
  Griewank "Broyden Updating, The Good and The Bad!" 
  (http://www.emis.ams.org/journals/DMJDMV/vol-ismp/45_griewank-andreas-broyden.pdf). 
  The given equation is the recursive inverse-Jacobian application via the 
  Sherman-Morrison-Woodbury formula. The implementation here unrolls the recursion 
  into a loop, with the initial vector carrying the J0 inversion/preconditioning. 
  
  dX <- J0^{-1} * F
  
  for i=0,1,2,...,k
    rho = 1 / (Y[i]^T Y[i])
    tau = rho * (Y[i]^T dX)
    dX <- (rho * dX) + (tau * (S[i] - Y[i]))
  end
 */

PetscErrorCode MatSolve_LMVMBadBrdn(Mat B, Vec F, Vec dX)
{
  Mat_LMVM          *lmvm = (Mat_LMVM*)B->data;
  PetscErrorCode    ierr;
  PetscInt          i;
  PetscReal         rho, tau, ytx, yty;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(B, MAT_CLASSID, 1);
  PetscValidHeaderSpecific(F, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(dX, VEC_CLASSID, 3);
  VecCheckSameSize(F, 2, dX, 3);
  VecCheckMatCompatible(B, dX, 3, F, 2);
  
  ierr = MatLMVMApplyJ0Inv(B, F, dX);CHKERRQ(ierr);
  
  for (i = 0; i <= lmvm->k; ++i) {
    ierr = VecDotBegin(lmvm->Y[i], lmvm->Y[i], &yty);CHKERRQ(ierr);
    ierr = VecDotBegin(lmvm->Y[i], dX, &ytx);CHKERRQ(ierr);
    ierr = VecDotEnd(lmvm->Y[i], lmvm->Y[i], &yty);CHKERRQ(ierr);
    ierr = VecDotEnd(lmvm->Y[i], dX, &ytx);CHKERRQ(ierr);
    rho = 1.0/yty; tau = rho * ytx;
    ierr = VecAXPBYPCZ(dX, tau, -tau, rho, lmvm->S[i], lmvm->Y[i]);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/

PetscErrorCode MatCreate_LMVMBadBrdn(Mat B)
{
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = MatCreate_LMVM(B);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)B, MATLMVMBADBRDN);CHKERRQ(ierr);
  B->ops->solve = MatSolve_LMVMBadBrdn;
  Mat_LMVM *lmvm = (Mat_LMVM*)B->data;
  lmvm->square = PETSC_TRUE;
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/

/*@
   MatCreateLMVMBadBrdn - Creates a limited-memory modified (aka "bad") Broyden-type 
   approximation matrix used for a Jacobian. L-BadBrdn is not guaranteed to be 
   symmetric or positive-definite. This implementation only supports the MatSolve() 
   operation, which is an application of the approximate inverse of the Jacobian. 
   
   The provided local and global sizes must match the solution and function vectors 
   used with MatLMVMUpdate() and MatSolve(). The resulting L-BadBrdn matrix will have 
   storage vectors allocated with VecCreateSeq() in serial and VecCreateMPI() in 
   parallel. To use the L-BadBrdn matrix with other vector types, the matrix must be 
   created using MatCreate() and MatSetType(), followed by MatLMVMAllocate(). 
   This ensures that the internal storage and work vectors are duplicated from the 
   correct type of vector.

   Collective on MPI_Comm

   Input Parameters:
+  comm - MPI communicator, set to PETSC_COMM_SELF
.  n - number of local rows for storage vectors
-  N - global size of the storage vectors

   Output Parameter:
.  B - the matrix

   It is recommended that one use the MatCreate(), MatSetType() and/or MatSetFromOptions()
   paradigm instead of this routine directly.

   Options Database Keys:
.   -mat_lmvm_num_vecs - maximum number of correction vectors (i.e.: updates) stored

   Level: intermediate

.seealso: MatCreate(), MATLMVM, MATLMVMBADBRDN, MatCreateLMVMDFP(), MatCreateLMVMSR1(), 
          MatCreateLMVMBFGS(), MatCreateLMVMBrdn(), MatCreateLMVMSymBrdn()
@*/
PetscErrorCode MatCreateLMVMBadBrdn(MPI_Comm comm, PetscInt n, PetscInt N, Mat *B)
{
  PetscErrorCode    ierr;
  
  PetscFunctionBegin;
  ierr = MatCreate(comm, B);CHKERRQ(ierr);
  ierr = MatSetSizes(*B, n, n, N, N);CHKERRQ(ierr);
  ierr = MatSetType(*B, MATLMVMBADBRDN);CHKERRQ(ierr);
  ierr = MatSetUp(*B);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
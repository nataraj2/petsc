
static char help[] = "Tests MatIncreaseOverlap(), MatCreateSubMatrices() for parallel MatSBAIJ format.\n";
/* Example of usage:
      mpiexec -n 2 ./ex92 -nd 2 -ov 3 -mat_block_size 2 -view_id 0 -test_overlap -test_submat
*/
#include <petscmat.h>

int main(int argc,char **args)
{
  Mat            A,Atrans,sA,*submatA,*submatsA;
  PetscErrorCode ierr;
  PetscMPIInt    size,rank;
  PetscInt       bs=1,mbs=10,ov=1,i,j,k,*rows,*cols,nd=2,*idx,rstart,rend,sz,M,N,Mbs;
  PetscScalar    *vals,rval,one=1.0;
  IS             *is1,*is2;
  PetscRandom    rand;
  PetscBool      flg,TestOverlap,TestSubMat,TestAllcols,test_sorted=PETSC_FALSE;
  PetscInt       vid = -1;
#if defined(PETSC_USE_LOG)
  PetscLogStage  stages[2];
#endif

  ierr = PetscInitialize(&argc,&args,(char*)0,help);if (ierr) return ierr;
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRMPI(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRMPI(ierr);

  ierr = PetscOptionsGetInt(NULL,NULL,"-mat_block_size",&bs,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL,NULL,"-mat_mbs",&mbs,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL,NULL,"-ov",&ov,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL,NULL,"-nd",&nd,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL,NULL,"-view_id",&vid,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(NULL,NULL, "-test_overlap", &TestOverlap);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(NULL,NULL, "-test_submat", &TestSubMat);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(NULL,NULL, "-test_allcols", &TestAllcols);CHKERRQ(ierr);
  ierr = PetscOptionsGetBool(NULL,NULL,"-test_sorted",&test_sorted,NULL);CHKERRQ(ierr);

  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatSetSizes(A,mbs*bs,mbs*bs,PETSC_DECIDE,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = MatSetType(A,MATBAIJ);CHKERRQ(ierr);
  ierr = MatSeqBAIJSetPreallocation(A,bs,PETSC_DEFAULT,NULL);CHKERRQ(ierr);
  ierr = MatMPIBAIJSetPreallocation(A,bs,PETSC_DEFAULT,NULL,PETSC_DEFAULT,NULL);CHKERRQ(ierr);

  ierr = PetscRandomCreate(PETSC_COMM_WORLD,&rand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(rand);CHKERRQ(ierr);

  ierr = MatGetOwnershipRange(A,&rstart,&rend);CHKERRQ(ierr);
  ierr = MatGetSize(A,&M,&N);CHKERRQ(ierr);
  Mbs  = M/bs;

  ierr = PetscMalloc1(bs,&rows);CHKERRQ(ierr);
  ierr = PetscMalloc1(bs,&cols);CHKERRQ(ierr);
  ierr = PetscMalloc1(bs*bs,&vals);CHKERRQ(ierr);
  ierr = PetscMalloc1(M,&idx);CHKERRQ(ierr);

  /* Now set blocks of values */
  for (j=0; j<bs*bs; j++) vals[j] = 0.0;
  for (i=0; i<Mbs; i++) {
    cols[0] = i*bs; rows[0] = i*bs;
    for (j=1; j<bs; j++) {
      rows[j] = rows[j-1]+1;
      cols[j] = cols[j-1]+1;
    }
    ierr = MatSetValues(A,bs,rows,bs,cols,vals,ADD_VALUES);CHKERRQ(ierr);
  }
  /* second, add random blocks */
  for (i=0; i<20*bs; i++) {
    ierr    = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
    cols[0] = bs*(PetscInt)(PetscRealPart(rval)*Mbs);
    ierr    = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
    rows[0] = rstart + bs*(PetscInt)(PetscRealPart(rval)*mbs);
    for (j=1; j<bs; j++) {
      rows[j] = rows[j-1]+1;
      cols[j] = cols[j-1]+1;
    }

    for (j=0; j<bs*bs; j++) {
      ierr    = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
      vals[j] = rval;
    }
    ierr = MatSetValues(A,bs,rows,bs,cols,vals,ADD_VALUES);CHKERRQ(ierr);
  }

  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* make A a symmetric matrix: A <- A^T + A */
  ierr = MatTranspose(A,MAT_INITIAL_MATRIX, &Atrans);CHKERRQ(ierr);
  ierr = MatAXPY(A,one,Atrans,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatDestroy(&Atrans);CHKERRQ(ierr);
  ierr = MatTranspose(A,MAT_INITIAL_MATRIX, &Atrans);CHKERRQ(ierr);
  ierr = MatEqual(A, Atrans, &flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatSetOption(A,MAT_SYMMETRIC,PETSC_TRUE);CHKERRQ(ierr);
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_PLIB,"A+A^T is non-symmetric");
  ierr = MatDestroy(&Atrans);CHKERRQ(ierr);

  /* create a SeqSBAIJ matrix sA (= A) */
  ierr = MatConvert(A,MATSBAIJ,MAT_INITIAL_MATRIX,&sA);CHKERRQ(ierr);
  if (vid >= 0 && vid < size) {
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD,"A:\n");CHKERRQ(ierr);
    ierr = MatView(A,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD,"sA:\n");CHKERRQ(ierr);
    ierr = MatView(sA,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }

  /* Test sA==A through MatMult() */
  ierr = MatMultEqual(A,sA,10,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Error in MatConvert(): A != sA");

  /* Test MatIncreaseOverlap() */
  ierr = PetscMalloc1(nd,&is1);CHKERRQ(ierr);
  ierr = PetscMalloc1(nd,&is2);CHKERRQ(ierr);

  for (i=0; i<nd; i++) {
    if (!TestAllcols) {
      ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
      sz   = (PetscInt)((0.5+0.2*PetscRealPart(rval))*mbs); /* 0.5*mbs < sz < 0.7*mbs */

      for (j=0; j<sz; j++) {
        ierr      = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
        idx[j*bs] = bs*(PetscInt)(PetscRealPart(rval)*Mbs);
        for (k=1; k<bs; k++) idx[j*bs+k] = idx[j*bs]+k;
      }
      ierr = ISCreateGeneral(PETSC_COMM_SELF,sz*bs,idx,PETSC_COPY_VALUES,is1+i);CHKERRQ(ierr);
      ierr = ISCreateGeneral(PETSC_COMM_SELF,sz*bs,idx,PETSC_COPY_VALUES,is2+i);CHKERRQ(ierr);
      if (rank == vid) {
        ierr = PetscPrintf(PETSC_COMM_SELF," [%d] IS sz[%D]: %D\n",rank,i,sz);CHKERRQ(ierr);
        ierr = ISView(is2[i],PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
      }
    } else { /* Test all rows and columns */
      sz   = M;
      ierr = ISCreateStride(PETSC_COMM_SELF,sz,0,1,is1+i);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,sz,0,1,is2+i);CHKERRQ(ierr);

      if (rank == vid) {
        PetscBool colflag;
        ierr = ISIdentity(is2[i],&colflag);CHKERRQ(ierr);
        ierr = PetscPrintf(PETSC_COMM_SELF,"[%d] is2[%D], colflag %D\n",rank,i,colflag);CHKERRQ(ierr);
        ierr = ISView(is2[i],PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
      }
    }
  }

  ierr = PetscLogStageRegister("MatOv_SBAIJ",&stages[0]);CHKERRQ(ierr);
  ierr = PetscLogStageRegister("MatOv_BAIJ",&stages[1]);CHKERRQ(ierr);

  /* Test MatIncreaseOverlap */
  if (TestOverlap) {
    ierr = PetscLogStagePush(stages[0]);CHKERRQ(ierr);
    ierr = MatIncreaseOverlap(sA,nd,is2,ov);CHKERRQ(ierr);
    ierr = PetscLogStagePop();CHKERRQ(ierr);

    ierr = PetscLogStagePush(stages[1]);CHKERRQ(ierr);
    ierr = MatIncreaseOverlap(A,nd,is1,ov);CHKERRQ(ierr);
    ierr = PetscLogStagePop();CHKERRQ(ierr);

    if (rank == vid) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"\n[%d] IS from BAIJ:\n",rank);CHKERRQ(ierr);
      ierr = ISView(is1[0],PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_SELF,"\n[%d] IS from SBAIJ:\n",rank);CHKERRQ(ierr);
      ierr = ISView(is2[0],PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
    }

    for (i=0; i<nd; ++i) {
      ierr = ISEqual(is1[i],is2[i],&flg);CHKERRQ(ierr);
      if (!flg) {
        if (rank == 0) {
          ierr = ISSort(is1[i]);CHKERRQ(ierr);
          ierr = ISSort(is2[i]);CHKERRQ(ierr);
        }
        SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_PLIB,"i=%D, is1 != is2",i);
      }
    }
  }

  /* Test MatCreateSubmatrices */
  if (TestSubMat) {
    if (test_sorted) {
      for (i = 0; i < nd; ++i) {
        ierr = ISSort(is1[i]);CHKERRQ(ierr);
      }
    }
    ierr = MatCreateSubMatrices(A,nd,is1,is1,MAT_INITIAL_MATRIX,&submatA);CHKERRQ(ierr);
    ierr = MatCreateSubMatrices(sA,nd,is1,is1,MAT_INITIAL_MATRIX,&submatsA);CHKERRQ(ierr);

    ierr = MatMultEqual(A,sA,10,&flg);CHKERRQ(ierr);
    if (!flg) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"A != sA");

    /* Now test MatCreateSubmatrices with MAT_REUSE_MATRIX option */
    ierr = MatCreateSubMatrices(A,nd,is1,is1,MAT_REUSE_MATRIX,&submatA);CHKERRQ(ierr);
    ierr = MatCreateSubMatrices(sA,nd,is1,is1,MAT_REUSE_MATRIX,&submatsA);CHKERRQ(ierr);
    ierr = MatMultEqual(A,sA,10,&flg);CHKERRQ(ierr);
    if (!flg) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"MatCreateSubmatrices(): A != sA");

    ierr = MatDestroySubMatrices(nd,&submatA);CHKERRQ(ierr);
    ierr = MatDestroySubMatrices(nd,&submatsA);CHKERRQ(ierr);
  }

  /* Free allocated memory */
  for (i=0; i<nd; ++i) {
    ierr = ISDestroy(&is1[i]);CHKERRQ(ierr);
    ierr = ISDestroy(&is2[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(is1);CHKERRQ(ierr);
  ierr = PetscFree(is2);CHKERRQ(ierr);
  ierr = PetscFree(idx);CHKERRQ(ierr);
  ierr = PetscFree(rows);CHKERRQ(ierr);
  ierr = PetscFree(cols);CHKERRQ(ierr);
  ierr = PetscFree(vals);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = MatDestroy(&sA);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

/*TEST

   test:
      args: -ov {{1 3}} -mat_block_size {{2 8}} -test_overlap -test_submat
      output_file: output/ex92_1.out

   test:
      suffix: 2
      nsize: {{3 4}}
      args: -ov {{1 3}} -mat_block_size {{2 8}} -test_overlap -test_submat
      output_file: output/ex92_1.out

   test:
      suffix: 3
      nsize: {{3 4}}
      args: -ov {{1 3}} -mat_block_size {{2 8}} -test_overlap -test_allcols
      output_file: output/ex92_1.out

   test:
      suffix: 3_sorted
      nsize: {{3 4}}
      args: -ov {{1 3}} -mat_block_size {{2 8}} -test_overlap -test_allcols -test_sorted
      output_file: output/ex92_1.out

   test:
      suffix: 4
      nsize: {{3 4}}
      args: -ov {{1 3}} -mat_block_size {{2 8}} -test_submat -test_allcols
      output_file: output/ex92_1.out

TEST*/

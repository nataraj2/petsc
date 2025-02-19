/*T
   Concepts: KSP^solving a system of linear equations
   Concepts: KSP^Laplacian, 2d
   Processors: n
T*/

/*
Laplacian in 2D. Modeled by the partial differential equation

   div  grad u = f,  0 < x,y < 1,

with forcing function

   f = e^{-(1 - x)^2/\nu} e^{-(1 - y)^2/\nu}

with pure Neumann boundary conditions

The functions are cell-centered

This uses multigrid to solve the linear system

	   Contributed by Andrei Draganescu <aidraga@sandia.gov>

Note the nice multigrid convergence despite the fact it is only using
peicewise constant interpolation/restriction. This is because cell-centered multigrid
does not need the same rule:

	polynomial degree(interpolation) + polynomial degree(restriction) + 2 > degree of PDE

that vertex based multigrid needs.
*/

static char help[] = "Solves 2D inhomogeneous Laplacian using multigrid.\n\n";

#include <petscdm.h>
#include <petscdmda.h>
#include <petscksp.h>
#include <iostream>

extern PetscErrorCode ComputeMatrix(KSP,Mat,Mat,void*);
extern PetscErrorCode ComputeRHS(KSP,Vec,void*);

typedef enum {DIRICHLET, NEUMANN} BCType;

typedef struct {
  PetscScalar nu;
  BCType	  bcType;
} UserContext;


void output_solution_to_file(KSP &ksp, Vec &x)
{
	PetscErrorCode ierr;
	DM			 dm;
	PetscScalar	**barray;
	PetscScalar	Hx, Hy;
	PetscInt	   i,j,mx,my,xm,ym,xs,ys;
	int rank;
	
	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

	PetscFunctionBeginUser;
	ierr  = KSPGetDM(ksp,&dm);
	ierr  = DMDAGetInfo(dm, 0, &mx, &my, 0,0,0,0,0,0,0,0,0,0);
	Hx	= 1.0 / (PetscReal)(mx);
	Hy	= 1.0 / (PetscReal)(my);
	ierr  = DMDAGetCorners(dm,&xs,&ys,0,&xm,&ym,0);
	ierr  = DMDAVecGetArray(dm,x,&barray);

	std::cout << "Rank is " << rank << " " << xs << " " << xs+xm << " " << ys << " " << ys+ym << "\n";	

	FILE *sol_file;
	
	std::string filename;
	
	filename = "sol_file" + std::to_string(rank);
	filename = filename + ".txt";

	sol_file = fopen(filename.c_str(),"w");

	for (j=ys; j<ys+ym; j++) {
	   for (i=xs; i<xs+xm; i++) {
		   fprintf(sol_file, "%d %d %0.15g %0.15g %0.15g\n", i, j, ((PetscReal)i+0.5)*Hx, ((PetscReal)j+0.5)*Hy, barray[j][i]);
	   }
	}

	fclose(sol_file);

	DMDAVecRestoreArray(dm,x,&barray);


}

int main(int argc,char **argv)
{
  KSP			ksp;
  DM			 da;
  UserContext	user;
  const char	 *bcTypes[2] = {"dirichlet","neumann"};
  PetscErrorCode ierr;
  PetscInt	   bc;
  Vec 			  x;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);if (ierr) return ierr;
  ierr = KSPCreate(PETSC_COMM_WORLD,&ksp);CHKERRQ(ierr);
  ierr = DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE,DMDA_STENCIL_STAR,200,100,PETSC_DECIDE,PETSC_DECIDE,1,1,0,0,&da);CHKERRQ(ierr);
  ierr = DMSetFromOptions(da);CHKERRQ(ierr);
  ierr = DMSetUp(da);CHKERRQ(ierr);
  ierr = DMDASetInterpolationType(da, DMDA_Q0);CHKERRQ(ierr);

  ierr = KSPSetDM(ksp,da);CHKERRQ(ierr);

  ierr		= PetscOptionsBegin(PETSC_COMM_WORLD, "", "Options for the inhomogeneous Poisson equation", "DM");CHKERRQ(ierr);
  user.nu	 = 0.1;
  ierr		= PetscOptionsScalar("-nu", "The width of the Gaussian source", "ex29.c", 0.1, &user.nu, NULL);CHKERRQ(ierr);
  bc		  = (PetscInt)NEUMANN;
  ierr		= PetscOptionsEList("-bc_type","Type of boundary condition","ex29.c",bcTypes,2,bcTypes[0],&bc,NULL);CHKERRQ(ierr);
  user.bcType = (BCType)bc;
  ierr		= PetscOptionsEnd();CHKERRQ(ierr);

  ierr = KSPSetComputeRHS(ksp,ComputeRHS,&user);CHKERRQ(ierr);
  ierr = KSPSetComputeOperators(ksp,ComputeMatrix,&user);CHKERRQ(ierr);
  ierr = KSPSetFromOptions(ksp);CHKERRQ(ierr);
  ierr = KSPSolve(ksp,NULL,NULL);CHKERRQ(ierr);
  ierr = KSPGetSolution(ksp,&x);CHKERRQ(ierr);

  output_solution_to_file(ksp,x);

  ierr = KSPDestroy(&ksp);CHKERRQ(ierr);
  ierr = DMDestroy(&da);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

PetscErrorCode ComputeRHS(KSP ksp,Vec b,void *ctx)
{
  UserContext	*user = (UserContext*)ctx;
  PetscErrorCode ierr;
  PetscInt	   i,j,mx,my,xm,ym,xs,ys;
  PetscScalar	Hx,Hy;
  PetscScalar	**array;
  DM			 da;

  PetscFunctionBeginUser;
  ierr = KSPGetDM(ksp,&da);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da, 0, &mx, &my, 0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  Hx   = 1.0 / (PetscReal)(mx);
  Hy   = 1.0 / (PetscReal)(my);
  ierr = DMDAGetCorners(da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(da, b, &array);CHKERRQ(ierr);
  for (j=ys; j<ys+ym; j++) {
	for (i=xs; i<xs+xm; i++) {
	  //array[j][i] = PetscExpScalar(-(((PetscReal)i+0.5)*Hx)*(((PetscReal)i+0.5)*Hx)/user->nu)*PetscExpScalar(-(((PetscReal)j+0.5)*Hy)*(((PetscReal)j+0.5)*Hy)/user->nu)*Hx*Hy;
		PetscReal x = ((PetscReal)i+0.5)*Hx;
		PetscReal y = ((PetscReal)j+0.5)*Hy;

	   array[j][i] = 8.0*3.1415*3.1415*cos(2.0*3.1415*x)*cos(2.0*3.1415*y)*Hx*Hy;
	  
	}
  }
  ierr = DMDAVecRestoreArray(da, b, &array);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(b);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(b);CHKERRQ(ierr);

  /* force right hand side to be consistent for singular matrix */
  /* note this is really a hack, normally the model would provide you with a consistent right handside */
  if (user->bcType == NEUMANN) {
	MatNullSpace nullspace;

	ierr = MatNullSpaceCreate(PETSC_COMM_WORLD,PETSC_TRUE,0,0,&nullspace);CHKERRQ(ierr);
	ierr = MatNullSpaceRemove(nullspace,b);CHKERRQ(ierr);
	ierr = MatNullSpaceDestroy(&nullspace);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode ComputeMatrix(KSP ksp, Mat J,Mat jac, void *ctx)
{
  UserContext	*user = (UserContext*)ctx;
  PetscErrorCode ierr;
  PetscInt	   i,j,mx,my,xm,ym,xs,ys,num, numi, numj;
  PetscScalar	v[5],Hx,Hy,HydHx,HxdHy;
  MatStencil	 row, col[5];
  DM			 da;

  PetscFunctionBeginUser;
  ierr  = KSPGetDM(ksp,&da);CHKERRQ(ierr);
  ierr  = DMDAGetInfo(da,0,&mx,&my,0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  Hx	= 1.0 / (PetscReal)(mx);
  Hy	= 1.0 / (PetscReal)(my);
  HxdHy = Hx/Hy;
  HydHx = Hy/Hx;
  ierr  = DMDAGetCorners(da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  for (j=ys; j<ys+ym; j++) {
	for (i=xs; i<xs+xm; i++) {
	  row.i = i; row.j = j;
	  if (i==0 || j==0 || i==mx-1 || j==my-1) {
		if (user->bcType == DIRICHLET) {
		  v[0] = 2.0*(HxdHy + HydHx);
		  ierr = MatSetValuesStencil(jac,1,&row,1,&row,v,INSERT_VALUES);CHKERRQ(ierr);
		  SETERRQ(PETSC_COMM_WORLD,PETSC_ERR_SUP,"Dirichlet boundary conditions not supported !\n");
		} else if (user->bcType == NEUMANN) {
		  num = 0; numi=0; numj=0;
		  if (j!=0) {
			v[num] = -HxdHy;
			col[num].i = i;
			col[num].j = j-1;
			num++; numj++;
		  }
		  if (i!=0) {
			v[num]	 = -HydHx;
			col[num].i = i-1;
			col[num].j = j;
			num++; numi++;
		  }
		  if (i!=mx-1) {
			v[num]	 = -HydHx;
			col[num].i = i+1;
			col[num].j = j;
			num++; numi++;
		  }
		  if (j!=my-1) {
			v[num]	 = -HxdHy;
			col[num].i = i;
			col[num].j = j+1;
			num++; numj++;
		  }
		  v[num] = (PetscReal)(numj)*HxdHy + (PetscReal)(numi)*HydHx; col[num].i = i;   col[num].j = j;
		  num++;
		  ierr = MatSetValuesStencil(jac,1,&row,num,col,v,INSERT_VALUES);CHKERRQ(ierr);
		}
	  } else {
		v[0] = -HxdHy;			  col[0].i = i;   col[0].j = j-1;
		v[1] = -HydHx;			  col[1].i = i-1; col[1].j = j;
		v[2] = 2.0*(HxdHy + HydHx); col[2].i = i;   col[2].j = j;
		v[3] = -HydHx;			  col[3].i = i+1; col[3].j = j;
		v[4] = -HxdHy;			  col[4].i = i;   col[4].j = j+1;
		ierr = MatSetValuesStencil(jac,1,&row,5,col,v,INSERT_VALUES);CHKERRQ(ierr);
	  }
	}
  }
  ierr = MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (user->bcType == NEUMANN) {
	MatNullSpace nullspace;

	ierr = MatNullSpaceCreate(PETSC_COMM_WORLD,PETSC_TRUE,0,0,&nullspace);CHKERRQ(ierr);
	ierr = MatSetNullSpace(J,nullspace);CHKERRQ(ierr);
	ierr = MatNullSpaceDestroy(&nullspace);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*TEST

   test:
	  args: -pc_type mg -pc_mg_type full -ksp_type fgmres -ksp_monitor_short -pc_mg_levels 3 -mg_coarse_pc_factor_shift_type nonzero

TEST*/

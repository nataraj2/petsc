/*$Id: ex2.c,v 1.25 2000/01/11 21:03:15 bsmith Exp bsmith $*/

static char help[] = 
"Reads a a simple unstructured grid from a file, partitions it,\n\
and distributes the grid data accordingly\n\n";

/*T
   Concepts: Mat^Partitioning a matrix;
   Processors: n
T*/

/*
    Updates of this example MAY be found at 
       http://www.mcs.anl.gov/petsc/src/dm/ao/examples/tutorials/ex2.c

    This is a very basic, even crude, example of managing an unstructured
    grid in parallel.

    This particular code is for a Galerkin-style finite element method. 

    After the calls below, each processor will have 
      1) a list of elements it "owns"; for each "owned" element it will have the global 
         numbering of the three vertices; stored in gdata->ele;
      2) a list of vertices it "owns". For each owned it will have the x and y 
         coordinates; stored in gdata->vert

    It will not have 
      1) list of ghost elements (since they are not needed for traditional 
         Galerkin style finite element methods). For various finite volume methods
         you may need the ghost element lists, these may be generated using the 
         element neighbor information given in the file database.

    To compute the local element stiffness matrix and load vector, each processor
    will need the vertex coordinates for all of the vertices on the locally 
    "owned" elements.  This data could be obtained by doing the appropriate vector
    scatter on the data stored in gdata->vert; we haven't had time to demonstrate this.

    Clearly writing a complete parallel unstructured grid code with PETSc is still
    a good deal of work and requires a lot of application level coding.  BUT, at least
    PETSc can manage all of the nonlinear and linear solvers (including matrix assembly 
    etc.), which allows the programmer to concentrate his or her efforts on managing 
    the unstructured grid. The PETSc team is developing additional library objects
    to help manage parallel unstructured grid computations.  Unfortunately we have 
    not had time to complete these yet, so the application programmer still must
    manage much of the parallel grid manipulation as indicated below.

*/

/* 
  Include "mat.h" so that we can use matrices.
  automatically includes:
     petsc.h  - base PETSc routines   vec.h    - vectors
     sys.h    - system routines       mat.h    - matrices
     is.h     - index sets            viewer.h - viewers               

  Include "ao.h" allows use of the AO (application ordering) commands,
  used below for renumbering the vertex numbers after the partitioning.

  Include "bitarray.h" for managing logical bit arrays that are used to 
  conserve space. Note that the code does use order N bit arrays on each 
  processor so is theoretically not scalable, but even with 64 million 
  vertices it will only need temporarily 8 megabytes of memory for the 
  bit array so one can still do very large problems with this approach, 
  since the bit arrays are freed before the vectors and matrices are
  created.
*/
#include "mat.h"
#include "ao.h"
#include "bitarray.h"

/* 
    This is the user-defined grid data context 
*/
typedef struct {
  int    n_vert,n_ele;
  int    mlocal_vert,mlocal_ele;
  int    *ele;
  double *vert;
  int    *ia,*ja;
  IS     isnewproc;
  int    *localvert,nlocal; /* used to stash temporarily old global vertex number of new vertex */
} GridData;

/*
   Variables on all processors:
      n_vert          - total number of vertices
      mlocal_vert     - number of vertices on this processor
      vert            - x,y coordinates of local vertices

      n_ele           - total number of elements
      mlocal_ele      - number of vertices on this processor
      ele             - vertices of elements on this processor

      ia, ja          - adjacency graph of elements (for partitioning)
    
   Variables on processor 0 during data reading from file:
      mmlocal_vert[i] - number of vertices on each processor
      tmpvert         - x,y coordinates of vertices on any processor (as read in)

      mmlocal_ele[i]  - number of elements on each processor

      tmpia, tmpja    - adjacency graph of elements for other processors

   Notes:
   The code below has a great deal of IO (print statements). This is to allow one to track 
   the renumbering and movement of data among processors. In an actual 
   production run, IO of this type would be deactivated.

   To use the ParMETIS partitioner run with the option -mat_partitioning_type parmetis
   otherwise it defaults to the initial element partitioning induced when the data 
   is read in.

   To understand the parallel performance of this type of code, it is important
   to profile the time spent in various events in the code; running with the
   option -log_summary will indicate how much time is spent in the routines 
   below.   Of course, for very small problems, such as the sample grid used here,
   the profiling results are meaningless.
*/

extern int DataRead(GridData *);
extern int DataPartitionElements(GridData *);
extern int DataMoveElements(GridData *);
extern int DataPartitionVertices(GridData *);
extern int DataMoveVertices(GridData *);
extern int DataDestroy(GridData *);

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **args)
{
  int          ierr;
  int          READ_EVENT,PARTITION_ELEMENT_EVENT,MOVE_ELEMENT_EVENT;
  int          PARTITION_VERTEX_EVENT,MOVE_VERTEX_EVENT;
  GridData     gdata;

  PetscInitialize(&argc,&args,(char *)0,help);

  PLogEventRegister(&READ_EVENT,             "Read Data","red");
  PLogEventRegister(&PARTITION_ELEMENT_EVENT,"Partition elemen","blue");
  PLogEventRegister(&MOVE_ELEMENT_EVENT,     "Move elements","green");
  PLogEventRegister(&PARTITION_VERTEX_EVENT, "Partition vertic","orange");
  PLogEventRegister(&MOVE_VERTEX_EVENT,      "Move vertices","yellow");

  PLogEventBegin(READ_EVENT,0,0,0,0);
  ierr = DataRead(&gdata);CHKERRA(ierr);
  PLogEventEnd(READ_EVENT,0,0,0,0);
  PLogEventBegin(PARTITION_ELEMENT_EVENT,0,0,0,0);
  ierr = DataPartitionElements(&gdata);CHKERRA(ierr);
  PLogEventEnd(PARTITION_ELEMENT_EVENT,0,0,0,0);
  PLogEventBegin(MOVE_ELEMENT_EVENT,0,0,0,0);
  ierr = DataMoveElements(&gdata);CHKERRA(ierr);
  PLogEventEnd(MOVE_ELEMENT_EVENT,0,0,0,0);
  PLogEventBegin(PARTITION_VERTEX_EVENT,0,0,0,0);
  ierr = DataPartitionVertices(&gdata);CHKERRA(ierr);
  PLogEventEnd(PARTITION_VERTEX_EVENT,0,0,0,0);
  PLogEventBegin(MOVE_VERTEX_EVENT,0,0,0,0);
  ierr = DataMoveVertices(&gdata);CHKERRA(ierr);
  PLogEventEnd(MOVE_VERTEX_EVENT,0,0,0,0);
  ierr = DataDestroy(&gdata);CHKERRA(ierr);

  PetscFinalize();
  return 0;
}


#undef __FUNC__
#define __FUNC__ "DataRead"
/*
     Reads in the grid data from a file; each processor is naively 
  assigned a continuous chunk of vertex and element data. Later the data
  will be partitioned and moved to the appropriate processor.
*/
int DataRead(GridData *gdata)
{
  int          rank,size,n_vert,*mmlocal_vert,mlocal_vert,i,*ia,*ja,cnt,j;
  int          mlocal_ele,*mmlocal_ele,*ele,*tmpele,n_ele,net,a1,a2,a3;
  int          *iatmp,*jatmp,ierr;
  char         msg[128];
  double       *vert,*tmpvert;
  MPI_Status   status;

  PetscFunctionBegin;
  /*
     Processor 0 opens the file, reads in chunks of data and sends a portion off to
   each other processor.

     Note: For a truely scalable IO portion of the code, one would store
   the grid data in a binary file and use MPI-IO commands to have each 
   processor read in the parts that it needs. However in most circumstances
   involving up to a say a million nodes and 100 processors this approach 
   here is fine.
  */
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

  if (!rank) {
    FILE *fd;
    fd = fopen("usgdata","r"); if (!fd) SETERRQ(1,1,"Cannot open grid file");

    /* read in number of vertices */
    fgets(msg,128,fd);
    printf("File msg:%s",msg);
    fscanf(fd,"Number Vertices = %d\n",&n_vert);
    printf("Number of grid vertices %d\n",n_vert);

    /* broadcast number of vertices to all processors */
    ierr = MPI_Bcast(&n_vert,1,MPI_INT,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    mlocal_vert  = n_vert/size + ((n_vert % size) > 0);

    /* 
      allocate enough room for the first processor to keep track of how many 
      vertices are assigned to each processor. Splitting vertices equally amoung
      all processors.
    */ 
    mmlocal_vert = (int*)PetscMalloc(size*sizeof(int));CHKPTRA(mmlocal_vert);
    for (i=0; i<size; i++) {
      mmlocal_vert[i] = n_vert/size + ((n_vert % size) > i);
      printf("Processor %d assigned %d vertices\n",i,mmlocal_vert[i]);
    }

    /*
       Read in vertices assigned to first processor
    */ 
    vert = (double*)PetscMalloc(2*mmlocal_vert[0]*sizeof(double));CHKPTRA(vert);   
    printf("Vertices assigned to processor 0\n");
    for (i=0; i<mlocal_vert; i++) {
      fscanf(fd,"%d %lf %lf\n",&cnt,vert+2*i,vert+2*i+1);
      printf("%d %g %g\n",cnt,vert[2*i],vert[2*i+1]);
    }

    /* 
       Read in vertices for all the other processors 
    */
    tmpvert = (double*)PetscMalloc(2*mmlocal_vert[0]*sizeof(double));CHKPTRA(tmpvert);
    for (j=1; j<size; j++) {
      printf("Vertices assigned to processor %d\n",j);
      for (i=0; i<mmlocal_vert[j]; i++) {
        fscanf(fd,"%d %lf %lf\n",&cnt,tmpvert+2*i,tmpvert+2*i+1);
        printf("%d %g %g\n",cnt,tmpvert[2*i],tmpvert[2*i+1]);
      }
      ierr = MPI_Send(tmpvert,2*mmlocal_vert[j],MPI_DOUBLE,j,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    }
    ierr = PetscFree(tmpvert);CHKERRQ(ierr);
    ierr = PetscFree(mmlocal_vert);CHKERRQ(ierr);

    fscanf(fd,"Number Elements = %d\n",&n_ele);
    printf("Number of grid elements %d\n",n_ele);

    /* 
       Broadcast number of elements to all processors
    */
    ierr = MPI_Bcast(&n_ele,1,MPI_INT,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    mlocal_ele  = n_ele/size + ((n_ele % size) > 0);

    /* 
      Allocate enough room for the first processor to keep track of how many 
      elements are assigned to each processor.
    */ 
    mmlocal_ele = (int*)PetscMalloc(size*sizeof(int));CHKPTRA(mmlocal_ele);
    for (i=0; i<size; i++) {
      mmlocal_ele[i] = n_ele/size + ((n_ele % size) > i);
      printf("Processor %d assigned %d elements\n",i,mmlocal_ele[i]);
    }
 
    /*
        read in element information for the first processor
    */
    ele = (int*)PetscMalloc(3*mmlocal_ele[0]*sizeof(int));CHKPTRA(ele);   
    printf("Elements assigned to processor 0\n");
    for (i=0; i<mlocal_ele; i++) {
      fscanf(fd,"%d %d %d %d\n",&cnt,ele+3*i,ele+3*i+1,ele+3*i+2);
      printf("%d %d %d %d\n",cnt,ele[3*i],ele[3*i+1],ele[3*i+2]);
    }

    /* 
       Read in elements for all the other processors 
    */
    tmpele = (int*)PetscMalloc(3*mmlocal_ele[0]*sizeof(int));CHKPTRA(tmpele);
    for (j=1; j<size; j++) {
      printf("Elements assigned to processor %d\n",j);
      for (i=0; i<mmlocal_ele[j]; i++) {
        fscanf(fd,"%d %d %d %d\n",&cnt,tmpele+3*i,tmpele+3*i+1,tmpele+3*i+2);
        printf("%d %d %d %d\n",cnt,tmpele[3*i],tmpele[3*i+1],tmpele[3*i+2]);
      }
      ierr = MPI_Send(tmpele,3*mmlocal_ele[j],MPI_INT,j,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    }
    ierr = PetscFree(tmpele);CHKERRQ(ierr);

    /* 
         Read in element neighbors for processor 0 
         We don't know how many spaces in ja[] to allocate so we allocate 
       3*the number of local elements, this is the maximum it could be
    */
    ia    = (int*)PetscMalloc((mlocal_ele+1)*sizeof(int));CHKPTRA(ia);
    ja    = (int*)PetscMalloc((3*mlocal_ele+1)*sizeof(int));CHKPTRA(ja);
    net   = 0;
    ia[0] = 0;
    printf("Element neighbors on processor 0\n");
    fgets(msg,128,fd);
    for (i=0; i<mlocal_ele; i++) {
      fscanf(fd,"%d %d %d %d\n",&cnt,&a1,&a2,&a3);
      printf("%d %d %d %d\n",cnt,a1,a2,a3);
      if (a1 >= 0) {ja[net++] = a1;}
      if (a2 >= 0) {ja[net++] = a2;}
      if (a3 >= 0) {ja[net++] = a3;}
      ia[i+1] = net;
    }

    printf("ia values for processor 0\n");
    for (i=0; i<mlocal_ele+1; i++) {
      printf("%d ",ia[i]);
    }
    printf("\n");
    printf("ja values for processor 0\n");
    for (i=0; i<ia[mlocal_ele]; i++) {
      printf("%d ",ja[i]);
    }
    printf("\n");

    /*
       Read in element neighbor information for all other processors
    */
    iatmp    = (int*)PetscMalloc((mlocal_ele+1)*sizeof(int));CHKPTRA(iatmp);
    jatmp    = (int*)PetscMalloc((3*mlocal_ele+1)*sizeof(int));CHKPTRA(jatmp);
    for (j=1; j<size; j++) {
      net   = 0;
      iatmp[0] = 0;
      printf("Element neighbors on processor %d\n",j);
      for (i=0; i<mmlocal_ele[j]; i++) {
        fscanf(fd,"%d %d %d %d\n",&cnt,&a1,&a2,&a3);
        printf("%d %d %d %d\n",cnt,a1,a2,a3);
        if (a1 >= 0) {jatmp[net++] = a1;}
        if (a2 >= 0) {jatmp[net++] = a2;}
        if (a3 >= 0) {jatmp[net++] = a3;}
        iatmp[i+1] = net;
      }

      printf("ia values for processor %d\n",j);
      for (i=0; i<mmlocal_ele[j]+1; i++) {
        printf("%d ",iatmp[i]);
      }
      printf("\n");
      printf("ja values for processor %d\n",j);
      for (i=0; i<iatmp[mmlocal_ele[j]]; i++) {
        printf("%d ",jatmp[i]);
      }
      printf("\n");

      /* send graph off to appropriate processor */
      ierr = MPI_Send(iatmp,mmlocal_ele[j]+1,MPI_INT,j,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
      ierr = MPI_Send(jatmp,iatmp[mmlocal_ele[j]],MPI_INT,j,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    }
    ierr = PetscFree(iatmp);CHKERRQ(ierr);
    ierr = PetscFree(jatmp);CHKERRQ(ierr);
    ierr = PetscFree(mmlocal_ele);CHKERRQ(ierr);

    fclose(fd);
  } else {
    /*
        We are not the zeroth processor so we do not open the file
      rather we wait for processor 0 to send us our data.
    */

    /* receive total number of vertices */
    ierr = MPI_Bcast(&n_vert,1,MPI_INT,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    mlocal_vert = n_vert/size + ((n_vert % size) > rank);

    /* receive vertices */
    vert = (double*)PetscMalloc(2*(mlocal_vert+1)*sizeof(double));CHKPTRQ(vert);
    ierr = MPI_Recv(vert,2*mlocal_vert,MPI_DOUBLE,0,0,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);

    /* receive total number of elements */
    ierr = MPI_Bcast(&n_ele,1,MPI_INT,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
    mlocal_ele = n_ele/size + ((n_ele % size) > rank);

    /* receive elements */
    ele = (int*)PetscMalloc(3*(mlocal_ele+1)*sizeof(int));CHKPTRQ(ele);
    ierr = MPI_Recv(ele,3*mlocal_ele,MPI_INT,0,0,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);

    /* receive element adjacency graph */
    ia    = (int*)PetscMalloc((mlocal_ele+1)*sizeof(int));CHKPTRA(ia);
    ierr = MPI_Recv(ia,mlocal_ele+1,MPI_INT,0,0,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);

    ja    = (int*)PetscMalloc((ia[mlocal_ele]+1)*sizeof(int));CHKPTRA(ja);
    ierr = MPI_Recv(ja,ia[mlocal_ele],MPI_INT,0,0,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);
  }

  gdata->n_vert      = n_vert;
  gdata->n_ele       = n_ele;
  gdata->mlocal_vert = mlocal_vert;
  gdata->mlocal_ele  = mlocal_ele;
  gdata->ele         = ele;
  gdata->vert        = vert;

  gdata->ia          = ia;
  gdata->ja          = ja;

  PetscFunctionReturn(0);
}


#undef __FUNC__
#define __FUNC__ "DataPartitionElements"
/*
         Given the grid data spread across the processors, determines a
   new partitioning of the CELLS to reduce the number of cut edges between
   cells.
*/
int DataPartitionElements(GridData *gdata)
{
  Mat          Adj;                /* adjacency matrix */
  int          *ia,*ja;
  int          mlocal_ele,n_ele,ierr;
  MatPartitioning part;
  IS           isnewproc; 

  PetscFunctionBegin;
  n_ele       = gdata->n_ele;
  mlocal_ele  = gdata->mlocal_ele;

  ia          = gdata->ia;
  ja          = gdata->ja;

  /*
      Create the adjacency graph matrix
  */
  ierr = MatCreateMPIAdj(PETSC_COMM_WORLD,mlocal_ele,n_ele,ia,ja,PETSC_NULL,&Adj);CHKERRQ(ierr);

  /*
      Create the partioning object
  */
  ierr = MatPartitioningCreate(PETSC_COMM_WORLD,&part);CHKERRQ(ierr);
  ierr = MatPartitioningSetAdjacency(part,Adj);CHKERRQ(ierr);
  ierr = MatPartitioningSetFromOptions(part);CHKERRQ(ierr);
  ierr = MatPartitioningApply(part,&isnewproc);CHKERRQ(ierr);
  ierr = MatPartitioningDestroy(part);CHKERRQ(ierr);

  /*
       isnewproc - indicates for each local element the new processor it is assigned to
  */
  ierr = PetscPrintf(PETSC_COMM_WORLD,"New processor assignment for each element\n");CHKERRQ(ierr);
  ierr = ISView(isnewproc,VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  gdata->isnewproc = isnewproc;

  /*
      Free the adjacency graph data structures
  */
  ierr = MatDestroy(Adj);CHKERRQ(ierr);


  PetscFunctionReturn(0);
}

#undef __FUNC__
#define __FUNC__ "DataMoveElements"
/*
      Moves the grid element data to be on the correct processor for the new
   element partitioning.
*/
int DataMoveElements(GridData *gdata)
{
  int        ierr,*counts,rank,size,i,*idx;
  Vec        vele,veleold;
  Scalar     *array;
  IS         isscat,isnum;
  VecScatter vecscat;

  PetscFunctionBegin;

  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

  /* 
      Determine how many elements are assigned to each processor 
  */
  counts = (int*)PetscMalloc(size*sizeof(int));CHKPTRQ(counts);
  ierr   = ISPartitioningCount(gdata->isnewproc,counts);CHKERRQ(ierr);

  /* 
     Create a vector to contain the newly ordered element information 

     Note: we use vectors to communicate this data since we can use the powerful
     VecScatter routines to get the data to the correct location. This is a little
     wasteful since the vectors hold double precision numbers instead of integers,
     but since this is just a setup phase in the entire numerical computation that 
     is only called once it is not a measureable performance bottleneck.
  */
  ierr = VecCreateMPI(PETSC_COMM_WORLD,3*counts[rank],PETSC_DECIDE,&vele);CHKERRQ(ierr);

  /* 
      Create an index set from the isnewproc index set to indicate the mapping TO 
  */
  ierr = ISPartitioningToNumbering(gdata->isnewproc,&isnum);CHKERRQ(ierr);
  ierr = ISDestroy(gdata->isnewproc);
  /* 
      There are three data items per cell, the integer vertex numbers of its three 
    coordinates (we convert to double to use the scatter) (one can think 
    of the vectors of having a block size of 3 and there is one index in idx[] for each block)
  */
  ierr = ISGetIndices(isnum,&idx);CHKERRQ(ierr);
  for (i=0; i<gdata->mlocal_ele; i++) {
    idx[i] *= 3;
  }
  ierr = ISCreateBlock(PETSC_COMM_WORLD,3,gdata->mlocal_ele,idx,&isscat);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isnum,&idx);CHKERRQ(ierr);
  ierr = ISDestroy(isnum);CHKERRQ(ierr);

  /* 
     Create a vector to contain the old ordered element information
  */
  ierr = VecCreateSeq(PETSC_COMM_SELF,3*gdata->mlocal_ele,&veleold);CHKERRQ(ierr);
  ierr = VecGetArray(veleold,&array);CHKERRQ(ierr);
  for (i=0; i<3*gdata->mlocal_ele; i++) {
    array[i] = gdata->ele[i];
  }
  ierr = VecRestoreArray(veleold,&array);CHKERRQ(ierr);
  
  /* 
     Scatter the element vertex information to the correct processor
  */
  ierr = VecScatterCreate(veleold,PETSC_NULL,vele,isscat,&vecscat);CHKERRQ(ierr);
  ierr = ISDestroy(isscat);CHKERRQ(ierr);
  ierr = VecScatterBegin(veleold,vele,INSERT_VALUES,SCATTER_FORWARD,vecscat);CHKERRQ(ierr);
  ierr = VecScatterEnd(veleold,vele,INSERT_VALUES,SCATTER_FORWARD,vecscat);CHKERRQ(ierr);
  ierr = VecScatterDestroy(vecscat);CHKERRQ(ierr);
  ierr = VecDestroy(veleold);CHKERRQ(ierr);

  /* 
     Put the element vertex data into a new allocation of the gdata->ele 
  */
  ierr = PetscFree(gdata->ele);CHKERRQ(ierr);
  gdata->mlocal_ele = counts[rank];
  ierr = PetscFree(counts);CHKERRQ(ierr);
  gdata->ele = (int*)PetscMalloc(3*gdata->mlocal_ele*sizeof(int));CHKERRQ(ierr);
  ierr       = VecGetArray(vele,&array);CHKERRQ(ierr);
  for (i=0; i<3*gdata->mlocal_ele; i++) {
    gdata->ele[i] = (int)array[i];
  }
  ierr = VecRestoreArray(vele,&array);CHKERRQ(ierr);
  ierr = VecDestroy(vele);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_WORLD,"Old vertex numbering in new element ordering\n");CHKERRQ(ierr);
  ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"Processor %d\n",rank);CHKERRQ(ierr);
  for (i=0; i<gdata->mlocal_ele; i++) {
    ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"%d %d %d %d\n",i,gdata->ele[3*i],gdata->ele[3*i+1],
                            gdata->ele[3*i+2]);CHKERRQ(ierr);
  } 
  ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNC__
#define __FUNC__ "DataPartitionVertice"
/*
         Given the newly partitioned cells, this routine partitions the 
     vertices.

     The code is not completely scalable since it requires
     1) O(n_vert) bits per processor memory
     2) uses O(size) stages of communication; each of size O(n_vert) bits
     3) it is sequential (each processor marks it vertices ONLY after all processors
        to the left have marked theirs.
     4) the amount of work on the last processor is O(n_vert)

     The algorithm also does not take advantage of vertices that are "interior" to a
     processors elements (that is; is not contained in any element on another processor).
     A better algorithm would first have all processors determine "interior" vertices and
     make sure they are retained on that processor before listing "boundary" vertices.

     The algorithm is:
     a) each processor waits for a message from the left containing mask of all marked vertices
     b) it loops over all local elements, generating a list of vertices it will 
        claim (not claiming ones that have already been marked in the bit-array)
        it claims at most n_vert/size vertices
     c) it sends to the right the mask

     This is a quick-and-dirty implementation; it should work fine for many problems,
     but will need to be replaced once profiling shows that it takes a large amount of
     time. An advantage is it requires no searching or sorting.
     
*/
int DataPartitionVertices(GridData *gdata)
{
  int        n_vert = gdata->n_vert,ierr,*localvert;
  int        rank,size,mlocal_ele = gdata->mlocal_ele,*ele = gdata->ele,i,j,nlocal = 0,nmax;
  PetscBT    mask;
  MPI_Status status;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);

  /*
      Allocated space to store bit-array indicting vertices marked
  */
  ierr = PetscBTCreate(n_vert,mask);CHKERRQ(ierr);

  /*
     All processors except last can have a maximum of n_vert/size vertices assigned
     (because last processor needs to handle any leftovers)
  */
  nmax = n_vert/size;
  if (rank == size-1) {
    nmax = n_vert;
  }

  /* 
     Receive list of marked vertices from left 
  */
  if (rank) {
    ierr = MPI_Recv(mask,PetscBTLength(n_vert),MPI_CHAR,rank-1,0,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);
  }

  if (rank == size-1) {
    /* last processor gets all the rest */
    for (i=0; i<n_vert; i++) {
      if (!PetscBTLookup(mask,i)) {
        nlocal++;
      }
    }
    nmax = nlocal;
  }

  /* 
     Now we know how many are local, allocated enough space for them and mark them 
  */
  localvert = (int*)PetscMalloc((nmax+1)*sizeof(int));CHKPTRQ(localvert);

  /* generate local list and fill in mask */
  nlocal = 0;
  if (rank < size-1) {
    /* count my vertices */
    for (i=0; i<mlocal_ele; i++) {
      for (j=0; j<3; j++) {
        if (!PetscBTLookupSet(mask,ele[3*i+j])) {
          localvert[nlocal++] = ele[3*i+j];
          if (nlocal >= nmax) goto foundenough2;
        }
      }
    }
    foundenough2:;
  } else {
    /* last processor gets all the rest */
    for (i=0; i<n_vert; i++) {
      if (!PetscBTLookup(mask,i)) {
        localvert[nlocal++] = i;
      }
    }
  }
  /* 
      Send bit mask on to next processor
  */
  if (rank < size-1) {
    ierr = MPI_Send(mask,PetscBTLength(n_vert),MPI_CHAR,rank+1,0,PETSC_COMM_WORLD);CHKERRQ(ierr);
  }
  ierr = PetscBTDestroy(mask);CHKERRQ(ierr);

  gdata->localvert = localvert;
  gdata->nlocal    = nlocal;

  /* print lists of owned vertices */
  PetscSynchronizedPrintf(PETSC_COMM_WORLD,"[%d] Number vertices assigned %d\n",rank,nlocal);
  PetscSynchronizedFlush(PETSC_COMM_WORLD);
  ierr = PetscIntView(nlocal,localvert,VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNC__
#define __FUNC__ "DataMoveVertices"
/*
     Given the partitioning of the vertices; renumbers the element vertex lists for the 
     new vertex numbering and moves the vertex coordinate values to the correct processor
*/
int DataMoveVertices(GridData *gdata)
{
  AO         ao;
  int        ierr,rank,i;
  Vec        vert,overt;
  VecScatter vecscat;
  IS         isscat;
  double     *avert;

  PetscFunctionBegin;

  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

  /* ---------------------------------------------------------------------
      Create a global reodering of the vertex numbers
  */
  ierr = AOCreateBasic(PETSC_COMM_WORLD,gdata->nlocal,gdata->localvert,PETSC_NULL,&ao);CHKERRQ(ierr);

  /*
     Change the element vertex information to the new vertex numbering
  */
  ierr = AOApplicationToPetsc(ao,3*gdata->mlocal_ele,gdata->ele);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"New vertex numbering in new element ordering\n");CHKERRQ(ierr);
  ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"Processor %d\n",rank);CHKERRQ(ierr);
  for (i=0; i<gdata->mlocal_ele; i++) {
    ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"%d %d %d %d\n",i,gdata->ele[3*i],gdata->ele[3*i+1],
                            gdata->ele[3*i+2]);CHKERRQ(ierr);
  } 
  ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD);CHKERRQ(ierr);

  /*
     Destroy the AO that is no longer needed
  */
  ierr = AODestroy(ao);CHKERRQ(ierr);

  /* --------------------------------------------------------------------
      Finally ship the vertex coordinate information to its owning process
      note, we do this in a way very similar to what was done for the element info
  */
  /* create a vector to contain the newly ordered vertex information */
  ierr = VecCreateSeq(PETSC_COMM_SELF,2*gdata->nlocal,&vert);CHKERRQ(ierr);

  /* create a vector to contain the old ordered vertex information */
  ierr = VecCreateMPIWithArray(PETSC_COMM_WORLD,2*gdata->mlocal_vert,PETSC_DECIDE,gdata->vert,
                               &overt);CHKERRQ(ierr);

  /* 
      There are two data items per vertex, the x and y coordinates (i.e. one can think 
    of the vectors of having a block size of 2 and there is one index in localvert[] for each block)
  */
  for (i=0; i<gdata->nlocal; i++) gdata->localvert[i] *= 2;
  ierr = ISCreateBlock(PETSC_COMM_WORLD,2,gdata->nlocal,gdata->localvert,&isscat);CHKERRQ(ierr);
  ierr = PetscFree(gdata->localvert);CHKERRQ(ierr);

  /* 
      Scatter the element vertex information to the correct processor
  */
  ierr = VecScatterCreate(overt,isscat,vert,PETSC_NULL,&vecscat);CHKERRQ(ierr);
  ierr = ISDestroy(isscat);CHKERRQ(ierr);
  ierr = VecScatterBegin(overt,vert,INSERT_VALUES,SCATTER_FORWARD,vecscat);CHKERRQ(ierr);
  ierr = VecScatterEnd(overt,vert,INSERT_VALUES,SCATTER_FORWARD,vecscat);CHKERRQ(ierr);
  ierr = VecScatterDestroy(vecscat);CHKERRQ(ierr);

  ierr = VecDestroy(overt);CHKERRQ(ierr);
  ierr = PetscFree(gdata->vert);CHKERRQ(ierr);
 
  /*
        Put resulting vertex information into gdata->vert array
  */
  gdata->vert = (double*)PetscMalloc(2*gdata->nlocal*sizeof(double));
  ierr = VecGetArray(vert,&avert);CHKERRQ(ierr);
  ierr = PetscMemcpy(gdata->vert,avert,2*gdata->nlocal*sizeof(double));CHKERRQ(ierr);
  ierr = VecRestoreArray(vert,&avert);CHKERRQ(ierr);
  gdata->mlocal_vert = gdata->nlocal;
  ierr = VecDestroy(vert);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_WORLD,"Vertex coordinates in new numbering\n");CHKERRQ(ierr);
  for (i=0; i<2*gdata->mlocal_vert; i++) {
    ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"%g\n",gdata->vert[i]);CHKERRQ(ierr);
  }
  ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}  


#undef __FUNC__
#define __FUNC__ "DataDestroy"
int DataDestroy(GridData *gdata)
{
  int ierr;

  PetscFunctionBegin;
  ierr = PetscFree(gdata->ele);CHKERRQ(ierr);
  ierr = PetscFree(gdata->vert);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


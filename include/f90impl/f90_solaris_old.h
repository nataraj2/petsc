/* $Id: f90_solaris.h,v 1.3 1998/09/25 00:09:49 balay Exp bsmith $ */

#if !defined (__F90_SOLARIS_H)
#define __F90_SOLARIS_H
 
typedef struct {
  long lower;   /* starting index of the fortran array */
  long extent;  /* length of the array */
  long mult;    /* in bytes for char,32 bit words for others. Why???? */
} tripple;

/*
  The following constants are just
  guesses. The program behavies strangly
  if these constants are not set in
  the f90 pointer
*/
#define F90_CHAR_ID    100665344
#define F90_INT_ID     35659784
#define F90_DOUBLE_ID  58736640
#define F90_COMPLEX_ID 68190216
#define F90_COOKIE     36864

#if !defined (PETSC_COMPLEX)
#define F90_SCALAR_ID F90_DOUBLE_ID
#else
#define F90_SCALAR_ID F90_COMPLEX_ID
#endif


#define f90_header() \
void*          addr;    /* Pointer to the data */ \
int            sd;      /* sizeof(DataType) in bits */  \
unsigned short cookie;  /* a wierd f90 cookie */ \
unsigned short ndim;    /* No of dimensions */          \
int            id;      /* integer id representing the datatype */ \
int            a,b;     /* unknown stuff - always zero. */

typedef struct {
  f90_header()
  tripple dim[1];
}array1d;

typedef struct {
  f90_header()
  tripple dim[2];   /* dim1,dim2 */
}array2d;

typedef struct {
  f90_header()
  tripple dim[3];   /* dim1,dim2,dim3 */
}array3d;

typedef struct {
  f90_header()
  tripple dim[4];   /* dim1,dim2,dim3,dim4 */
}array4d;


#endif

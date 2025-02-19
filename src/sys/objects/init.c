/*

   This file defines part of the initialization of PETSc

  This file uses regular malloc and free because it cannot be known
  what malloc is being used until it has already processed the input.
*/
#include <petsc/private/petscimpl.h> /*I  "petscsys.h"   I*/

#if defined(PETSC_HAVE_SYS_SYSINFO_H)
#include <sys/sysinfo.h>
#endif
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif

/* ------------------------Nasty global variables -------------------------------*/
/*
     Indicates if PETSc started up MPI, or it was
   already started before PETSc was initialized.
*/
PetscBool   PetscBeganMPI                 = PETSC_FALSE;
PetscBool   PetscErrorHandlingInitialized = PETSC_FALSE;
PetscBool   PetscInitializeCalled         = PETSC_FALSE;
PetscBool   PetscFinalizeCalled           = PETSC_FALSE;

PetscMPIInt PetscGlobalRank               = -1;
PetscMPIInt PetscGlobalSize               = -1;

#if defined(PETSC_HAVE_KOKKOS)
PetscBool   PetscBeganKokkos              = PETSC_FALSE;
#endif

#if defined(PETSC_HAVE_NVSHMEM)
PetscBool   PetscBeganNvshmem             = PETSC_FALSE;
PetscBool   PetscNvshmemInitialized       = PETSC_FALSE;
#endif

PetscBool   use_gpu_aware_mpi             = PetscDefined(HAVE_MPIUNI) ? PETSC_FALSE : PETSC_TRUE;

#if defined(PETSC_HAVE_COMPLEX)
#if defined(PETSC_COMPLEX_INSTANTIATE)
template <> class std::complex<double>; /* instantiate complex template class */
#endif

/*MC
   PETSC_i - the imaginary number i

   Synopsis:
   #include <petscsys.h>
   PetscComplex PETSC_i;

   Level: beginner

   Note:
   Complex numbers are automatically available if PETSc located a working complex implementation

.seealso: PetscRealPart(), PetscImaginaryPart(), PetscRealPartComplex(), PetscImaginaryPartComplex()
M*/
PetscComplex PETSC_i;
MPI_Datatype MPIU___COMPLEX128 = 0;
#endif /* PETSC_HAVE_COMPLEX */
#if defined(PETSC_USE_REAL___FLOAT128)
MPI_Datatype MPIU___FLOAT128 = 0;
#elif defined(PETSC_USE_REAL___FP16)
MPI_Datatype MPIU___FP16 = 0;
#endif
MPI_Datatype MPIU_2SCALAR = 0;
MPI_Datatype MPIU_REAL_INT = 0;
MPI_Datatype MPIU_SCALAR_INT = 0;
#if defined(PETSC_USE_64BIT_INDICES)
MPI_Datatype MPIU_2INT = 0;
#endif
MPI_Datatype MPI_4INT = 0;
MPI_Datatype MPIU_4INT = 0;
MPI_Datatype MPIU_BOOL;
MPI_Datatype MPIU_ENUM;
MPI_Datatype MPIU_FORTRANADDR;
MPI_Datatype MPIU_SIZE_T;

/*
       Function that is called to display all error messages
*/
PetscErrorCode (*PetscErrorPrintf)(const char [],...)          = PetscErrorPrintfDefault;
PetscErrorCode (*PetscHelpPrintf)(MPI_Comm,const char [],...)  = PetscHelpPrintfDefault;
PetscErrorCode (*PetscVFPrintf)(FILE*,const char[],va_list)    = PetscVFPrintfDefault;

/* ------------------------------------------------------------------------------*/
/*
   Optional file where all PETSc output from various prints is saved
*/
PETSC_INTERN FILE *petsc_history;
FILE *petsc_history = NULL;

PetscErrorCode  PetscOpenHistoryFile(const char filename[],FILE **fd)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank,size;
  char           pfile[PETSC_MAX_PATH_LEN],pname[PETSC_MAX_PATH_LEN],fname[PETSC_MAX_PATH_LEN],date[64];
  char           version[256];

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRMPI(ierr);
  if (rank == 0) {
    char        arch[10];
    int         err;

    ierr = PetscGetArchType(arch,10);CHKERRQ(ierr);
    ierr = PetscGetDate(date,64);CHKERRQ(ierr);
    ierr = PetscGetVersion(version,256);CHKERRQ(ierr);
    ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRMPI(ierr);
    if (filename) {
      ierr = PetscFixFilename(filename,fname);CHKERRQ(ierr);
    } else {
      ierr = PetscGetHomeDirectory(pfile,sizeof(pfile));CHKERRQ(ierr);
      ierr = PetscStrlcat(pfile,"/.petschistory",sizeof(pfile));CHKERRQ(ierr);
      ierr = PetscFixFilename(pfile,fname);CHKERRQ(ierr);
    }

    *fd = fopen(fname,"a");
    if (!fd) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Cannot open file: %s",fname);

    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"----------------------------------------\n");CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"%s %s\n",version,date);CHKERRQ(ierr);
    ierr = PetscGetProgramName(pname,sizeof(pname));CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"%s on a %s, %d proc. with options:\n",pname,arch,size);CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"----------------------------------------\n");CHKERRQ(ierr);

    err = fflush(*fd);
    if (err) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"fflush() failed on file");
  }
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscCloseHistoryFile(FILE **fd)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank;
  char           date[64];
  int            err;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRMPI(ierr);
  if (rank == 0) {
    ierr = PetscGetDate(date,64);CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"----------------------------------------\n");CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"Finished at %s\n",date);CHKERRQ(ierr);
    ierr = PetscFPrintf(PETSC_COMM_SELF,*fd,"----------------------------------------\n");CHKERRQ(ierr);
    err  = fflush(*fd);
    if (err) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"fflush() failed on file");
    err = fclose(*fd);
    if (err) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"fclose() failed on file");
  }
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------------*/

/*
   This is ugly and probably belongs somewhere else, but I want to
  be able to put a true MPI abort error handler with command line args.

    This is so MPI errors in the debugger will leave all the stack
  frames. The default MP_Abort() cleans up and exits thus providing no useful information
  in the debugger hence we call abort() instead of MPI_Abort().
*/

void Petsc_MPI_AbortOnError(MPI_Comm *comm,PetscMPIInt *flag,...)
{
  PetscFunctionBegin;
  (*PetscErrorPrintf)("MPI error %d\n",*flag);
  abort();
}

void Petsc_MPI_DebuggerOnError(MPI_Comm *comm,PetscMPIInt *flag,...)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  (*PetscErrorPrintf)("MPI error %d\n",*flag);
  ierr = PetscAttachDebugger();
  if (ierr) PETSCABORT(*comm,*flag); /* hopeless so get out */
}

/*@C
   PetscEnd - Calls PetscFinalize() and then ends the program. This is useful if one
     wishes a clean exit somewhere deep in the program.

   Collective on PETSC_COMM_WORLD

   Options Database Keys are the same as for PetscFinalize()

   Level: advanced

   Note:
   See PetscInitialize() for more general runtime options.

.seealso: PetscInitialize(), PetscOptionsView(), PetscMallocDump(), PetscMPIDump(), PetscFinalize()
@*/
PetscErrorCode  PetscEnd(void)
{
  PetscFunctionBegin;
  PetscFinalize();
  exit(0);
  return 0;
}

PetscBool PetscOptionsPublish = PETSC_FALSE;
PETSC_INTERN PetscErrorCode PetscSetUseHBWMalloc_Private(void);
PETSC_INTERN PetscBool      petscsetmallocvisited;
static       char           emacsmachinename[256];

PetscErrorCode (*PetscExternalVersionFunction)(MPI_Comm) = NULL;
PetscErrorCode (*PetscExternalHelpFunction)(MPI_Comm)    = NULL;

#if PetscDefined(USE_LOG)
#include <petscviewer.h>
#endif

/*@C
   PetscSetHelpVersionFunctions - Sets functions that print help and version information
   before the PETSc help and version information is printed. Must call BEFORE PetscInitialize().
   This routine enables a "higher-level" package that uses PETSc to print its messages first.

   Input Parameters:
+  help - the help function (may be NULL)
-  version - the version function (may be NULL)

   Level: developer

@*/
PetscErrorCode  PetscSetHelpVersionFunctions(PetscErrorCode (*help)(MPI_Comm),PetscErrorCode (*version)(MPI_Comm))
{
  PetscFunctionBegin;
  PetscExternalHelpFunction    = help;
  PetscExternalVersionFunction = version;
  PetscFunctionReturn(0);
}

#if defined(PETSC_USE_LOG)
PETSC_INTERN PetscBool   PetscObjectsLog;
#endif

PETSC_INTERN PetscErrorCode  PetscOptionsCheckInitial_Private(const char help[])
{
  char              string[64];
  MPI_Comm          comm = PETSC_COMM_WORLD;
  PetscBool         flg1 = PETSC_FALSE,flg2 = PETSC_FALSE,flg3 = PETSC_FALSE,flag,hasHelp;
  PetscErrorCode    ierr;
  PetscReal         si;
  PetscInt          intensity;
  int               i;
  PetscMPIInt       rank;
  char              version[256];
#if defined(PETSC_USE_LOG)
  char              mname[PETSC_MAX_PATH_LEN];
  PetscViewerFormat format;
  PetscBool         flg4 = PETSC_FALSE;
#endif

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm,&rank);CHKERRMPI(ierr);

  /*
     Setup building of stack frames for all function calls
  */
  if (PetscDefined(USE_DEBUG) && !PetscDefined(HAVE_THREADSAFETY)) {
    ierr = PetscOptionsGetBool(NULL,NULL,"-checkstack",&flg1,NULL);CHKERRQ(ierr);
    ierr = PetscStackSetCheck(flg1);CHKERRQ(ierr);
  }

#if !defined(PETSC_HAVE_THREADSAFETY)
  if (!(PETSC_RUNNING_ON_VALGRIND)) {
    /*
      Setup the memory management; support for tracing malloc() usage
    */
    PetscBool mdebug = PETSC_FALSE, eachcall = PETSC_FALSE, initializenan = PETSC_FALSE, mlog = PETSC_FALSE;

    if (PetscDefined(USE_DEBUG)) {
      mdebug        = PETSC_TRUE;
      initializenan = PETSC_TRUE;
      ierr   = PetscOptionsHasName(NULL,NULL,"-malloc_test",&flg1);CHKERRQ(ierr);
    } else {
      /* don't warn about unused option */
      ierr = PetscOptionsHasName(NULL,NULL,"-malloc_test",&flg1);CHKERRQ(ierr);
      flg1 = PETSC_FALSE;
    }
    ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_debug",&flg2,&flg3);CHKERRQ(ierr);
    if (flg1 || flg2) {
      mdebug        = PETSC_TRUE;
      eachcall      = PETSC_TRUE;
      initializenan = PETSC_TRUE;
    } else if (flg3 && !flg2) {
      mdebug        = PETSC_FALSE;
      eachcall      = PETSC_FALSE;
      initializenan = PETSC_FALSE;
    }

    ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_requested_size",&flg1,&flg2);CHKERRQ(ierr);
    if (flg2) {ierr = PetscMallocLogRequestedSizeSet(flg1);CHKERRQ(ierr);}

    ierr = PetscOptionsHasName(NULL,NULL,"-malloc_view",&mlog);CHKERRQ(ierr);
    if (mlog) {
      mdebug = PETSC_TRUE;
    }
    /* the next line is deprecated */
    ierr = PetscOptionsGetBool(NULL,NULL,"-malloc",&mdebug,NULL);CHKERRQ(ierr);
    ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_dump",&mdebug,NULL);CHKERRQ(ierr);
    ierr = PetscOptionsGetBool(NULL,NULL,"-log_view_memory",&mdebug,NULL);CHKERRQ(ierr);
    if (mdebug) {
      ierr = PetscMallocSetDebug(eachcall,initializenan);CHKERRQ(ierr);
    }
    if (mlog) {
      PetscReal logthreshold = 0;
      ierr = PetscOptionsGetReal(NULL,NULL,"-malloc_view_threshold",&logthreshold,NULL);CHKERRQ(ierr);
      ierr = PetscMallocViewSet(logthreshold);CHKERRQ(ierr);
    }
#if defined(PETSC_USE_LOG)
    ierr = PetscOptionsGetBool(NULL,NULL,"-log_view_memory",&PetscLogMemory,NULL);CHKERRQ(ierr);
#endif
  }

  ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_coalesce",&flg1,&flg2);CHKERRQ(ierr);
  if (flg2) {ierr = PetscMallocSetCoalesce(flg1);CHKERRQ(ierr);}
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_hbw",&flg1,NULL);CHKERRQ(ierr);
  /* ignore this option if malloc is already set */
  if (flg1 && !petscsetmallocvisited) {ierr = PetscSetUseHBWMalloc_Private();CHKERRQ(ierr);}

  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-malloc_info",&flg1,NULL);CHKERRQ(ierr);
  if (!flg1) {
    flg1 = PETSC_FALSE;
    ierr = PetscOptionsGetBool(NULL,NULL,"-memory_view",&flg1,NULL);CHKERRQ(ierr);
  }
  if (flg1) {
    ierr = PetscMemorySetGetMaximumUsage();CHKERRQ(ierr);
  }
#endif

#if defined(PETSC_USE_LOG)
  ierr = PetscOptionsHasName(NULL,NULL,"-objects_dump",&PetscObjectsLog);CHKERRQ(ierr);
#endif

  /*
      Set the display variable for graphics
  */
  ierr = PetscSetDisplay();CHKERRQ(ierr);

  /*
     Print main application help message
  */
  ierr = PetscOptionsHasHelp(NULL,&hasHelp);CHKERRQ(ierr);
  if (help && hasHelp) {
    ierr = PetscPrintf(comm,"%s",help);CHKERRQ(ierr);
    ierr = PetscPrintf(comm,"----------------------------------------\n");CHKERRQ(ierr);
  }

  /*
      Print the PETSc version information
  */
  ierr = PetscOptionsHasName(NULL,NULL,"-version",&flg1);CHKERRQ(ierr);
  if (flg1 || hasHelp) {
    /*
       Print "higher-level" package version message
    */
    if (PetscExternalVersionFunction) {
      ierr = (*PetscExternalVersionFunction)(comm);CHKERRQ(ierr);
    }

    ierr = PetscGetVersion(version,256);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"%s\n",version);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"%s",PETSC_AUTHOR_INFO);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/changes/index.html for recent updates.\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/faq.html for problems.\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/manualpages/index.html for help. \n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"Libraries linked from %s\n",PETSC_LIB_DIR);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"----------------------------------------\n");CHKERRQ(ierr);
  }

  /*
       Print "higher-level" package help message
  */
  if (hasHelp) {
    PetscBool hasHelpIntro;

    if (PetscExternalHelpFunction) {
      ierr = (*PetscExternalHelpFunction)(comm);CHKERRQ(ierr);
    }
    ierr = PetscOptionsHasHelpIntro_Internal(NULL,&hasHelpIntro);CHKERRQ(ierr);
    if (hasHelpIntro) {
      ierr = PetscOptionsDestroyDefault();CHKERRQ(ierr);
      ierr = PetscFreeMPIResources();CHKERRQ(ierr);
      ierr = MPI_Finalize();CHKERRMPI(ierr);
      exit(0);
    }
  }

  /*
      Setup the error handling
  */
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-on_error_abort",&flg1,NULL);CHKERRQ(ierr);
  if (flg1) {
    ierr = MPI_Comm_set_errhandler(comm,MPI_ERRORS_ARE_FATAL);CHKERRMPI(ierr);
    ierr = PetscPushErrorHandler(PetscAbortErrorHandler,NULL);CHKERRQ(ierr);
  }
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-on_error_mpiabort",&flg1,NULL);CHKERRQ(ierr);
  if (flg1) { ierr = PetscPushErrorHandler(PetscMPIAbortErrorHandler,NULL);CHKERRQ(ierr);}
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-mpi_return_on_error",&flg1,NULL);CHKERRQ(ierr);
  if (flg1) {
    ierr = MPI_Comm_set_errhandler(comm,MPI_ERRORS_RETURN);CHKERRMPI(ierr);
  }
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-no_signal_handler",&flg1,NULL);CHKERRQ(ierr);
  if (!flg1) {ierr = PetscPushSignalHandler(PetscSignalHandlerDefault,(void*)0);CHKERRQ(ierr);}

  /*
      Setup debugger information
  */
  ierr = PetscSetDefaultDebugger();CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"-on_error_attach_debugger",string,sizeof(string),&flg1);CHKERRQ(ierr);
  if (flg1) {
    MPI_Errhandler err_handler;

    ierr = PetscSetDebuggerFromString(string);CHKERRQ(ierr);
    ierr = MPI_Comm_create_errhandler(Petsc_MPI_DebuggerOnError,&err_handler);CHKERRMPI(ierr);
    ierr = MPI_Comm_set_errhandler(comm,err_handler);CHKERRMPI(ierr);
    ierr = PetscPushErrorHandler(PetscAttachDebuggerErrorHandler,NULL);CHKERRQ(ierr);
  }
  ierr = PetscOptionsGetString(NULL,NULL,"-debug_terminal",string,sizeof(string),&flg1);CHKERRQ(ierr);
  if (flg1) { ierr = PetscSetDebugTerminal(string);CHKERRQ(ierr); }
  ierr = PetscOptionsGetString(NULL,NULL,"-start_in_debugger",string,sizeof(string),&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"-stop_for_debugger",string,sizeof(string),&flg2);CHKERRQ(ierr);
  if (flg1 || flg2) {
    PetscMPIInt    size;
    PetscInt       lsize,*ranks;
    MPI_Errhandler err_handler;
    /*
       we have to make sure that all processors have opened
       connections to all other processors, otherwise once the
       debugger has stated it is likely to receive a SIGUSR1
       and kill the program.
    */
    ierr = MPI_Comm_size(comm,&size);CHKERRMPI(ierr);
    if (size > 2) {
      PetscMPIInt dummy = 0;
      MPI_Status  status;
      for (i=0; i<size; i++) {
        if (rank != i) {
          ierr = MPI_Send(&dummy,1,MPI_INT,i,109,comm);CHKERRMPI(ierr);
        }
      }
      for (i=0; i<size; i++) {
        if (rank != i) {
          ierr = MPI_Recv(&dummy,1,MPI_INT,i,109,comm,&status);CHKERRMPI(ierr);
        }
      }
    }
    /* check if this processor node should be in debugger */
    ierr  = PetscMalloc1(size,&ranks);CHKERRQ(ierr);
    lsize = size;
    /* Deprecated in 3.14 */
    ierr  = PetscOptionsGetIntArray(NULL,NULL,"-debugger_nodes",ranks,&lsize,&flag);CHKERRQ(ierr);
    if (flag) {
      const char * const quietopt="-options_suppress_deprecated_warnings";
      char               msg[4096];
      PetscBool          quiet = PETSC_FALSE;

      ierr = PetscOptionsGetBool(NULL,NULL,quietopt,&quiet,NULL);CHKERRQ(ierr);
      if (!quiet) {
        ierr = PetscStrcpy(msg,"** PETSc DEPRECATION WARNING ** : the option ");CHKERRQ(ierr);
        ierr = PetscStrcat(msg,"-debugger_nodes");CHKERRQ(ierr);
        ierr = PetscStrcat(msg," is deprecated as of version ");CHKERRQ(ierr);
        ierr = PetscStrcat(msg,"3.14");CHKERRQ(ierr);
        ierr = PetscStrcat(msg," and will be removed in a future release.");CHKERRQ(ierr);
        ierr = PetscStrcat(msg," Please use the option ");CHKERRQ(ierr);
        ierr = PetscStrcat(msg,"-debugger_ranks");CHKERRQ(ierr);
        ierr = PetscStrcat(msg," instead.");CHKERRQ(ierr);
        ierr = PetscStrcat(msg," (Silence this warning with ");CHKERRQ(ierr);
        ierr = PetscStrcat(msg,quietopt);CHKERRQ(ierr);
        ierr = PetscStrcat(msg,")\n");CHKERRQ(ierr);
        ierr = PetscPrintf(comm,"%s",msg);CHKERRQ(ierr);
      }
    } else {
      lsize = size;
      ierr  = PetscOptionsGetIntArray(NULL,NULL,"-debugger_ranks",ranks,&lsize,&flag);CHKERRQ(ierr);
    }
    if (flag) {
      for (i=0; i<lsize; i++) {
        if (ranks[i] == rank) { flag = PETSC_FALSE; break; }
      }
    }
    if (!flag) {
      ierr = PetscSetDebuggerFromString(string);CHKERRQ(ierr);
      ierr = PetscPushErrorHandler(PetscAbortErrorHandler,NULL);CHKERRQ(ierr);
      if (flg1) {
        ierr = PetscAttachDebugger();CHKERRQ(ierr);
      } else {
        ierr = PetscStopForDebugger();CHKERRQ(ierr);
      }
      ierr = MPI_Comm_create_errhandler(Petsc_MPI_AbortOnError,&err_handler);CHKERRMPI(ierr);
      ierr = MPI_Comm_set_errhandler(comm,err_handler);CHKERRMPI(ierr);
    } else {
      ierr = PetscWaitOnError();CHKERRQ(ierr);
    }
    ierr = PetscFree(ranks);CHKERRQ(ierr);
  }

  ierr = PetscOptionsGetString(NULL,NULL,"-on_error_emacs",emacsmachinename,sizeof(emacsmachinename),&flg1);CHKERRQ(ierr);
  if (flg1 && rank == 0) {ierr = PetscPushErrorHandler(PetscEmacsClientErrorHandler,emacsmachinename);CHKERRQ(ierr);}

  /*
        Setup profiling and logging
  */
#if defined(PETSC_USE_INFO)
  {
    ierr = PetscInfoSetFromOptions(NULL);CHKERRQ(ierr);
  }
#endif
  ierr = PetscDetermineInitialFPTrap();
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-fp_trap",&flg1,&flag);CHKERRQ(ierr);
  if (flag) {ierr = PetscSetFPTrap((PetscFPTrap)flg1);CHKERRQ(ierr);}
  ierr = PetscOptionsGetInt(NULL,NULL,"-check_pointer_intensity",&intensity,&flag);CHKERRQ(ierr);
  if (flag) {ierr = PetscCheckPointerSetIntensity(intensity);CHKERRQ(ierr);}
#if defined(PETSC_USE_LOG)
  mname[0] = 0;
  ierr = PetscOptionsGetString(NULL,NULL,"-history",mname,sizeof(mname),&flg1);CHKERRQ(ierr);
  if (flg1) {
    if (mname[0]) {
      ierr = PetscOpenHistoryFile(mname,&petsc_history);CHKERRQ(ierr);
    } else {
      ierr = PetscOpenHistoryFile(NULL,&petsc_history);CHKERRQ(ierr);
    }
  }

  ierr = PetscOptionsGetBool(NULL,NULL,"-log_sync",&PetscLogSyncOn,NULL);CHKERRQ(ierr);

#if defined(PETSC_HAVE_MPE)
  flg1 = PETSC_FALSE;
  ierr = PetscOptionsHasName(NULL,NULL,"-log_mpe",&flg1);CHKERRQ(ierr);
  if (flg1) {ierr = PetscLogMPEBegin();CHKERRQ(ierr);}
#endif
  flg1 = PETSC_FALSE;
  flg3 = PETSC_FALSE;
  ierr = PetscOptionsGetBool(NULL,NULL,"-log_all",&flg1,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(NULL,NULL,"-log_summary",&flg3);CHKERRQ(ierr);
  if (flg1)                      { ierr = PetscLogAllBegin();CHKERRQ(ierr); }
  else if (flg3)                 { ierr = PetscLogDefaultBegin();CHKERRQ(ierr);}

  ierr = PetscOptionsGetString(NULL,NULL,"-log_trace",mname,sizeof(mname),&flg1);CHKERRQ(ierr);
  if (flg1) {
    char name[PETSC_MAX_PATH_LEN],fname[PETSC_MAX_PATH_LEN];
    FILE *file;
    if (mname[0]) {
      PetscSNPrintf(name,PETSC_MAX_PATH_LEN,"%s.%d",mname,rank);
      ierr = PetscFixFilename(name,fname);CHKERRQ(ierr);
      file = fopen(fname,"w");
      if (!file) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_FILE_OPEN,"Unable to open trace file: %s",fname);
    } else file = PETSC_STDOUT;
    ierr = PetscLogTraceBegin(file);CHKERRQ(ierr);
  }

  ierr = PetscOptionsGetViewer(comm,NULL,NULL,"-log_view",NULL,&format,&flg4);CHKERRQ(ierr);
  if (flg4) {
    if (format == PETSC_VIEWER_ASCII_XML || format == PETSC_VIEWER_ASCII_FLAMEGRAPH) {
      ierr = PetscLogNestedBegin();CHKERRQ(ierr);
    } else {
      ierr = PetscLogDefaultBegin();CHKERRQ(ierr);
    }
  }
  if (flg4 && (format == PETSC_VIEWER_ASCII_XML || format == PETSC_VIEWER_ASCII_FLAMEGRAPH)) {
    PetscReal threshold = PetscRealConstant(0.01);
    ierr = PetscOptionsGetReal(NULL,NULL,"-log_threshold",&threshold,&flg1);CHKERRQ(ierr);
    if (flg1) {ierr = PetscLogSetThreshold((PetscLogDouble)threshold,NULL);CHKERRQ(ierr);}
  }
#endif

  ierr = PetscOptionsGetBool(NULL,NULL,"-saws_options",&PetscOptionsPublish,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetBool(NULL,NULL,"-use_gpu_aware_mpi",&use_gpu_aware_mpi,NULL);CHKERRQ(ierr);

  /*
       Print basic help message
  */
  if (hasHelp) {
    ierr = (*PetscHelpPrintf)(comm,"Options for all PETSc programs:\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -version: prints PETSc version\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -help intro: prints example description and PETSc version, and exits\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -help: prints example description, PETSc version, and available options for used routines\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_abort: cause an abort when an error is detected. Useful \n ");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       only when run in the debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_attach_debugger [gdb,dbx,xxgdb,ups,noxterm]\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       start the debugger in new xterm\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       unless noxterm is given\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -start_in_debugger [gdb,dbx,xxgdb,ups,noxterm]\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       start all processes in the debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_emacs <machinename>\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"    emacs jumps to error file\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -debugger_ranks [n1,n2,..] Ranks to start in debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -debugger_pause [m] : delay (in seconds) to attach debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -stop_for_debugger : prints message on how to attach debugger manually\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"                      waits the delay for you to attach\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -display display: Location where X window graphics and debuggers are displayed\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -no_signal_handler: do not trap error signals\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -mpi_return_on_error: MPI returns error code, rather than abort on internal error\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -fp_trap: stop on floating point exceptions\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"           note on IBM RS6000 this slows run greatly\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc_dump <optional filename>: dump list of unfreed memory at conclusion\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc: use PETSc error checking malloc (deprecated, use -malloc_debug)\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc no: don't use PETSc error checking malloc (deprecated, use -malloc_debug no)\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc_info: prints total memory usage\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc_view <optional filename>: keeps log of all memory allocations, displays in PetscFinalize()\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -malloc_debug <true or false>: enables or disables extended checking for memory corruption\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_view: dump list of options inputted\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_left: dump list of unused options\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_left no: don't dump list of unused options\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -tmp tmpdir: alternative /tmp directory\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -shared_tmp: tmp directory is shared by all processors\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -not_shared_tmp: each processor has separate tmp directory\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -memory_view: print memory usage at end of run\n");CHKERRQ(ierr);
#if defined(PETSC_USE_LOG)
    ierr = (*PetscHelpPrintf)(comm," -get_total_flops: total flops over all processors\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_view [:filename:[format]]: logging objects and events\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_trace [filename]: prints trace of all PETSc calls\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_exclude <list,of,classnames>: exclude given classes from logging\n");CHKERRQ(ierr);
#if defined(PETSC_HAVE_MPE)
    ierr = (*PetscHelpPrintf)(comm," -log_mpe: Also create logfile viewable through Jumpshot\n");CHKERRQ(ierr);
#endif
#endif
#if defined(PETSC_USE_INFO)
    ierr = (*PetscHelpPrintf)(comm," -info [filename][:[~]<list,of,classnames>[:[~]self]]: print verbose information\n");CHKERRQ(ierr);
#endif
    ierr = (*PetscHelpPrintf)(comm," -options_file <file>: reads options from file\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_monitor: monitor options to standard output, including that set previously e.g. in option files\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_monitor_cancel: cancels all hardwired option monitors\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -petsc_sleep n: sleeps n seconds before running program\n");CHKERRQ(ierr);
  }

#if defined(PETSC_HAVE_POPEN)
  {
  char machine[128];
  ierr = PetscOptionsGetString(NULL,NULL,"-popen_machine",machine,sizeof(machine),&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscPOpenSetMachine(machine);CHKERRQ(ierr);
  }
  }
#endif

  ierr = PetscOptionsGetReal(NULL,NULL,"-petsc_sleep",&si,&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscSleep(si);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

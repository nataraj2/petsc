
#include <petscsys.h>

#if defined(PETSC_NEEDS_UTYPE_TYPEDEFS)
/* Some systems have inconsistent include files that use but do not
   ensure that the following definitions are made */
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned short  ushort;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
#endif

#include <errno.h>
#include <ctype.h>
#if defined(PETSC_HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>
#endif
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(PETSC_HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif
#if defined(PETSC_HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(PETSC_HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif
#if defined(PETSC_HAVE_NETDB_H)
#include <netdb.h>
#endif
#if defined(PETSC_HAVE_FCNTL_H)
#include <fcntl.h>
#endif
#if defined(PETSC_HAVE_IO_H)
#include <io.h>
#endif
#if defined(PETSC_HAVE_WINSOCK2_H)
#include <Winsock2.h>
#endif
#include <sys/stat.h>
#include <../src/sys/classes/viewer/impls/socket/socket.h>

#if defined(PETSC_NEED_CLOSE_PROTO)
PETSC_EXTERN int close(int);
#endif
#if defined(PETSC_NEED_SOCKET_PROTO)
PETSC_EXTERN int socket(int,int,int);
#endif
#if defined(PETSC_NEED_SLEEP_PROTO)
PETSC_EXTERN int sleep(unsigned);
#endif
#if defined(PETSC_NEED_CONNECT_PROTO)
PETSC_EXTERN int connect(int,struct sockaddr*,int);
#endif

/*--------------------------------------------------------------*/
static PetscErrorCode PetscViewerDestroy_Socket(PetscViewer viewer)
{
  PetscViewer_Socket *vmatlab = (PetscViewer_Socket*)viewer->data;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  if (vmatlab->port) {
#if defined(PETSC_HAVE_CLOSESOCKET)
    ierr = closesocket(vmatlab->port);
#else
    ierr = close(vmatlab->port);
#endif
    if (ierr) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"System error closing socket");
  }
  ierr = PetscFree(vmatlab);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*--------------------------------------------------------------*/
/*@C
    PetscSocketOpen - handles connected to an open port where someone is waiting.

    Input Parameters:
+    url - for example www.mcs.anl.gov
-    portnum - for example 80

    Output Parameter:
.    t - the socket number

    Notes:
    Use close() to close the socket connection

    Use read() or PetscHTTPRequest() to read from the socket

    Level: advanced

.seealso:   PetscSocketListen(), PetscSocketEstablish(), PetscHTTPRequest(), PetscHTTPSConnect()
@*/
PetscErrorCode  PetscOpenSocket(const char hostname[],int portnum,int *t)
{
  struct sockaddr_in sa;
  struct hostent     *hp;
  int                s = 0;
  PetscErrorCode     ierr;
  PetscBool          flg = PETSC_TRUE;
  static int         refcnt = 0;

  PetscFunctionBegin;
  if (!(hp=gethostbyname(hostname))) {
    perror("SEND: error gethostbyname: ");
    SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_SYS,"system error open connection to %s",hostname);
  }
  ierr = PetscMemzero(&sa,sizeof(sa));CHKERRQ(ierr);
  ierr = PetscMemcpy(&sa.sin_addr,hp->h_addr_list[0],hp->h_length);CHKERRQ(ierr);

  sa.sin_family = hp->h_addrtype;
  sa.sin_port   = htons((u_short) portnum);
  while (flg) {
    if ((s=socket(hp->h_addrtype,SOCK_STREAM,0)) < 0) {
      perror("SEND: error socket");  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"system error");
    }
    if (connect(s,(struct sockaddr*)&sa,sizeof(sa)) < 0) {
#if defined(PETSC_HAVE_WSAGETLASTERROR)
      ierr = WSAGetLastError();
      if (ierr == WSAEADDRINUSE)    (*PetscErrorPrintf)("SEND: address is in use\n");
      else if (ierr == WSAEALREADY) (*PetscErrorPrintf)("SEND: socket is non-blocking \n");
      else if (ierr == WSAEISCONN) {
        (*PetscErrorPrintf)("SEND: socket already connected\n");
        Sleep((unsigned) 1);
      } else if (ierr == WSAECONNREFUSED) {
        /* (*PetscErrorPrintf)("SEND: forcefully rejected\n"); */
        Sleep((unsigned) 1);
      } else {
        perror(NULL); SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"system error");
      }
#else
      if (errno == EADDRINUSE)    (*PetscErrorPrintf)("SEND: address is in use\n");
      else if (errno == EALREADY) (*PetscErrorPrintf)("SEND: socket is non-blocking \n");
      else if (errno == EISCONN) {
        (*PetscErrorPrintf)("SEND: socket already connected\n");
        sleep((unsigned) 1);
      } else if (errno == ECONNREFUSED) {
        refcnt++;
        if (refcnt > 5) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_SYS,"Connection refused by remote host %s port %d",hostname,portnum);
        ierr = PetscInfo(NULL,"Connection refused in attaching socket, trying again\n");CHKERRQ(ierr);
        sleep((unsigned) 1);
      } else {
        perror(NULL); SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"system error");
      }
#endif
      flg = PETSC_TRUE;
#if defined(PETSC_HAVE_CLOSESOCKET)
      closesocket(s);
#else
      close(s);
#endif
    } else flg = PETSC_FALSE;
  }
  *t = s;
  PetscFunctionReturn(0);
}

/*@C
   PetscSocketEstablish - starts a listener on a socket

   Input Parameters:
.    portnumber - the port to wait at

   Output Parameters:
.     ss - the socket to be used with PetscSocketListen()

    Level: advanced

.seealso:   PetscSocketListen(), PetscOpenSocket()

@*/
PETSC_INTERN PetscErrorCode PetscSocketEstablish(int portnum,int *ss)
{
  static size_t      MAXHOSTNAME = 100;
  char               myname[MAXHOSTNAME+1];
  int                s;
  PetscErrorCode     ierr;
  struct sockaddr_in sa;
  struct hostent     *hp;

  PetscFunctionBegin;
  ierr = PetscGetHostName(myname,sizeof(myname));CHKERRQ(ierr);

  ierr = PetscMemzero(&sa,sizeof(struct sockaddr_in));CHKERRQ(ierr);

  hp = gethostbyname(myname);
  if (!hp) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"Unable to get hostent information from system");

  sa.sin_family = hp->h_addrtype;
  sa.sin_port   = htons((u_short)portnum);

  if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"Error running socket() command");
#if defined(PETSC_HAVE_SO_REUSEADDR)
  {
    int optval = 1; /* Turn on the option */
    ierr = setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char*)&optval,sizeof(optval));CHKERRQ(ierr);
  }
#endif

  while (bind(s,(struct sockaddr*)&sa,sizeof(sa)) < 0) {
#if defined(PETSC_HAVE_WSAGETLASTERROR)
    ierr = WSAGetLastError();
    if (ierr != WSAEADDRINUSE) {
#else
    if (errno != EADDRINUSE) {
#endif
      close(s);
      SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"Error from bind()");
    }
  }
  listen(s,0);
  *ss = s;
  return(0);
}

/*@C
   PetscSocketListen - Listens at a socket created with PetscSocketEstablish()

   Input Parameter:
.    listenport - obtained with PetscSocketEstablish()

   Output Parameter:
.     t - pass this to read() to read what is passed to this connection

    Level: advanced

.seealso:   PetscSocketEstablish()
@*/
PETSC_INTERN PetscErrorCode PetscSocketListen(int listenport,int *t)
{
  struct sockaddr_in isa;
#if defined(PETSC_HAVE_ACCEPT_SIZE_T)
  size_t             i;
#else
  int                i;
#endif

  PetscFunctionBegin;
  /* wait for someone to try to connect */
  i = sizeof(struct sockaddr_in);
  if ((*t = accept(listenport,(struct sockaddr*)&isa,(socklen_t*)&i)) < 0) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SYS,"error from accept()\n");
  PetscFunctionReturn(0);
}

/*@C
   PetscViewerSocketOpen - Opens a connection to a MATLAB or other socket based server.

   Collective

   Input Parameters:
+  comm - the MPI communicator
.  machine - the machine the server is running on,, use NULL for the local machine, use "server" to passively wait for
             a connection from elsewhere
-  port - the port to connect to, use PETSC_DEFAULT for the default

   Output Parameter:
.  lab - a context to use when communicating with the server

   Level: intermediate

   Notes:
   Most users should employ the following commands to access the
   MATLAB PetscViewers
$
$    PetscViewerSocketOpen(MPI_Comm comm, char *machine,int port,PetscViewer &viewer)
$    MatView(Mat matrix,PetscViewer viewer)
$
$                or
$
$    PetscViewerSocketOpen(MPI_Comm comm,char *machine,int port,PetscViewer &viewer)
$    VecView(Vec vector,PetscViewer viewer)

   Options Database Keys:
   For use with  PETSC_VIEWER_SOCKET_WORLD, PETSC_VIEWER_SOCKET_SELF,
   PETSC_VIEWER_SOCKET_() or if
    NULL is passed for machine or PETSC_DEFAULT is passed for port
$    -viewer_socket_machine <machine>
$    -viewer_socket_port <port>

   Environmental variables:
+   PETSC_VIEWER_SOCKET_PORT - portnumber
-   PETSC_VIEWER_SOCKET_MACHINE - machine name

     Currently the only socket client available is MATLAB. See
     src/dm/tests/ex12.c and ex12.m for an example of usage.

   Notes:
    The socket viewer is in some sense a subclass of the binary viewer, to read and write to the socket
          use PetscViewerBinaryRead(), PetscViewerBinaryWrite(), PetscViewerBinarWriteStringArray(), PetscViewerBinaryGetDescriptor().

     Use this for communicating with an interactive MATLAB session, see PETSC_VIEWER_MATLAB_() for writing output to a
     .mat file. Use PetscMatlabEngineCreate() or PETSC_MATLAB_ENGINE_(), PETSC_MATLAB_ENGINE_SELF, or PETSC_MATLAB_ENGINE_WORLD
     for communicating with a MATLAB Engine

.seealso: MatView(), VecView(), PetscViewerDestroy(), PetscViewerCreate(), PetscViewerSetType(),
          PetscViewerSocketSetConnection(), PETSC_VIEWER_SOCKET_, PETSC_VIEWER_SOCKET_WORLD,
          PETSC_VIEWER_SOCKET_SELF, PetscViewerBinaryWrite(), PetscViewerBinaryRead(), PetscViewerBinaryWriteStringArray(),
          PetscBinaryViewerGetDescriptor(), PetscMatlabEngineCreate()
@*/
PetscErrorCode  PetscViewerSocketOpen(MPI_Comm comm,const char machine[],int port,PetscViewer *lab)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscViewerCreate(comm,lab);CHKERRQ(ierr);
  ierr = PetscViewerSetType(*lab,PETSCVIEWERSOCKET);CHKERRQ(ierr);
  ierr = PetscViewerSocketSetConnection(*lab,machine,port);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscViewerSetFromOptions_Socket(PetscOptionItems *PetscOptionsObject,PetscViewer v)
{
  PetscErrorCode ierr;
  PetscInt       def = -1;
  char           sdef[256];
  PetscBool      tflg;

  PetscFunctionBegin;
  /*
       These options are not processed here, they are processed in PetscViewerSocketSetConnection(), they
    are listed here for the GUI to display
  */
  ierr = PetscOptionsHead(PetscOptionsObject,"Socket PetscViewer Options");CHKERRQ(ierr);
  ierr = PetscOptionsGetenv(PetscObjectComm((PetscObject)v),"PETSC_VIEWER_SOCKET_PORT",sdef,16,&tflg);CHKERRQ(ierr);
  if (tflg) {
    ierr = PetscOptionsStringToInt(sdef,&def);CHKERRQ(ierr);
  } else def = PETSCSOCKETDEFAULTPORT;
  ierr = PetscOptionsInt("-viewer_socket_port","Port number to use for socket","PetscViewerSocketSetConnection",def,NULL,NULL);CHKERRQ(ierr);

  ierr = PetscOptionsString("-viewer_socket_machine","Machine to use for socket","PetscViewerSocketSetConnection",sdef,NULL,sizeof(sdef),NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetenv(PetscObjectComm((PetscObject)v),"PETSC_VIEWER_SOCKET_MACHINE",sdef,sizeof(sdef),&tflg);CHKERRQ(ierr);
  if (!tflg) {
    ierr = PetscGetHostName(sdef,sizeof(sdef));CHKERRQ(ierr);
  }
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscViewerBinaryGetSkipHeader_Socket(PetscViewer viewer,PetscBool  *skip)
{
  PetscViewer_Socket *vsocket = (PetscViewer_Socket*)viewer->data;

  PetscFunctionBegin;
  *skip = vsocket->skipheader;
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscViewerBinarySetSkipHeader_Socket(PetscViewer viewer,PetscBool skip)
{
  PetscViewer_Socket *vsocket = (PetscViewer_Socket*)viewer->data;

  PetscFunctionBegin;
  vsocket->skipheader = skip;
  PetscFunctionReturn(0);
}

static PetscErrorCode  PetscViewerBinaryGetFlowControl_Socket(PetscViewer viewer,PetscInt *fc)
{
  PetscFunctionBegin;
  *fc = 0;
  PetscFunctionReturn(0);
}

/*MC
   PETSCVIEWERSOCKET - A viewer that writes to a Unix socket

.seealso:  PetscViewerSocketOpen(), PetscViewerDrawOpen(), PETSC_VIEWER_DRAW_(),PETSC_VIEWER_DRAW_SELF, PETSC_VIEWER_DRAW_WORLD,
           PetscViewerCreate(), PetscViewerASCIIOpen(), PetscViewerBinaryOpen(), PETSCVIEWERBINARY, PETSCVIEWERDRAW,
           PetscViewerMatlabOpen(), VecView(), DMView(), PetscViewerMatlabPutArray(), PETSCVIEWERASCII, PETSCVIEWERMATLAB,
           PetscViewerFileSetName(), PetscViewerFileSetMode(), PetscViewerFormat, PetscViewerType, PetscViewerSetType()

  Level: beginner
M*/

PETSC_EXTERN PetscErrorCode PetscViewerCreate_Socket(PetscViewer v)
{
  PetscViewer_Socket *vmatlab;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  ierr                   = PetscNewLog(v,&vmatlab);CHKERRQ(ierr);
  vmatlab->port          = 0;
  v->data                = (void*)vmatlab;
  v->ops->destroy        = PetscViewerDestroy_Socket;
  v->ops->flush          = NULL;
  v->ops->setfromoptions = PetscViewerSetFromOptions_Socket;

  /* lie and say this is a binary viewer; then all the XXXView_Binary() methods will work correctly on it */
  ierr = PetscObjectChangeTypeName((PetscObject)v,PETSCVIEWERBINARY);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)v,"PetscViewerBinarySetSkipHeader_C",PetscViewerBinarySetSkipHeader_Socket);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)v,"PetscViewerBinaryGetSkipHeader_C",PetscViewerBinaryGetSkipHeader_Socket);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)v,"PetscViewerBinaryGetFlowControl_C",PetscViewerBinaryGetFlowControl_Socket);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*@C
      PetscViewerSocketSetConnection - Sets the machine and port that a PETSc socket
             viewer is to use

  Logically Collective on PetscViewer

  Input Parameters:
+   v - viewer to connect
.   machine - host to connect to, use NULL for the local machine,use "server" to passively wait for
             a connection from elsewhere
-   port - the port on the machine one is connecting to, use PETSC_DEFAULT for default

    Level: advanced

.seealso: PetscViewerSocketOpen()
@*/
PetscErrorCode  PetscViewerSocketSetConnection(PetscViewer v,const char machine[],int port)
{
  PetscErrorCode     ierr;
  PetscMPIInt        rank;
  char               mach[256];
  PetscBool          tflg;
  PetscViewer_Socket *vmatlab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,PETSC_VIEWER_CLASSID,1);
  if (machine) PetscValidCharPointer(machine,2);
  vmatlab = (PetscViewer_Socket*)v->data;
  /* PetscValidLogicalCollectiveInt(v,port,3); not a PetscInt */
  if (port <= 0) {
    char portn[16];
    ierr = PetscOptionsGetenv(PetscObjectComm((PetscObject)v),"PETSC_VIEWER_SOCKET_PORT",portn,16,&tflg);CHKERRQ(ierr);
    if (tflg) {
      PetscInt pport;
      ierr = PetscOptionsStringToInt(portn,&pport);CHKERRQ(ierr);
      port = (int)pport;
    } else port = PETSCSOCKETDEFAULTPORT;
  }
  if (!machine) {
    ierr = PetscOptionsGetenv(PetscObjectComm((PetscObject)v),"PETSC_VIEWER_SOCKET_MACHINE",mach,sizeof(mach),&tflg);CHKERRQ(ierr);
    if (!tflg) {
      ierr = PetscGetHostName(mach,sizeof(mach));CHKERRQ(ierr);
    }
  } else {
    ierr = PetscStrncpy(mach,machine,sizeof(mach));CHKERRQ(ierr);
  }

  ierr = MPI_Comm_rank(PetscObjectComm((PetscObject)v),&rank);CHKERRMPI(ierr);
  if (rank == 0) {
    ierr = PetscStrcmp(mach,"server",&tflg);CHKERRQ(ierr);
    if (tflg) {
      int listenport;
      ierr = PetscInfo1(v,"Waiting for connection from socket process on port %d\n",port);CHKERRQ(ierr);
      ierr = PetscSocketEstablish(port,&listenport);CHKERRQ(ierr);
      ierr = PetscSocketListen(listenport,&vmatlab->port);CHKERRQ(ierr);
      close(listenport);
    } else {
      ierr = PetscInfo2(v,"Connecting to socket process on port %d machine %s\n",port,mach);CHKERRQ(ierr);
      ierr = PetscOpenSocket(mach,port,&vmatlab->port);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------------------*/
/*
    The variable Petsc_Viewer_Socket_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
PetscMPIInt Petsc_Viewer_Socket_keyval = MPI_KEYVAL_INVALID;

/*@C
     PETSC_VIEWER_SOCKET_ - Creates a socket viewer shared by all processors in a communicator.

     Collective

     Input Parameter:
.    comm - the MPI communicator to share the socket PetscViewer

     Level: intermediate

   Options Database Keys:
   For use with the default PETSC_VIEWER_SOCKET_WORLD or if
    NULL is passed for machine or PETSC_DEFAULT is passed for port
$    -viewer_socket_machine <machine>
$    -viewer_socket_port <port>

   Environmental variables:
+   PETSC_VIEWER_SOCKET_PORT - portnumber
-   PETSC_VIEWER_SOCKET_MACHINE - machine name

     Notes:
     Unlike almost all other PETSc routines, Petsc_VIEWER_SOCKET_ does not return
     an error code, it returns NULL if it fails. The socket PetscViewer is usually used in the form
$       XXXView(XXX object,PETSC_VIEWER_SOCKET_(comm));

     Currently the only socket client available is MATLAB. See
     src/dm/tests/ex12.c and ex12.m for an example of usage.

     Connects to a waiting socket and stays connected until PetscViewerDestroy() is called.

     Use this for communicating with an interactive MATLAB session, see PETSC_VIEWER_MATLAB_() for writing output to a
     .mat file. Use PetscMatlabEngineCreate() or PETSC_MATLAB_ENGINE_(), PETSC_MATLAB_ENGINE_SELF, or PETSC_MATLAB_ENGINE_WORLD
     for communicating with a MATLAB Engine

.seealso: PETSC_VIEWER_SOCKET_WORLD, PETSC_VIEWER_SOCKET_SELF, PetscViewerSocketOpen(), PetscViewerCreate(),
          PetscViewerSocketSetConnection(), PetscViewerDestroy(), PETSC_VIEWER_SOCKET_(), PetscViewerBinaryWrite(), PetscViewerBinaryRead(),
          PetscViewerBinaryWriteStringArray(), PetscViewerBinaryGetDescriptor(), PETSC_VIEWER_MATLAB_()
@*/
PetscViewer  PETSC_VIEWER_SOCKET_(MPI_Comm comm)
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscViewer    viewer;
  MPI_Comm       ncomm;

  PetscFunctionBegin;
  ierr = PetscCommDuplicate(comm,&ncomm,NULL);if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," ");PetscFunctionReturn(NULL);}
  if (Petsc_Viewer_Socket_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN,MPI_COMM_NULL_DELETE_FN,&Petsc_Viewer_Socket_keyval,NULL);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," ");PetscFunctionReturn(NULL);}
  }
  ierr = MPI_Comm_get_attr(ncomm,Petsc_Viewer_Socket_keyval,(void**)&viewer,(int*)&flg);
  if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," ");PetscFunctionReturn(NULL);}
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscViewerSocketOpen(ncomm,NULL,0,&viewer);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_REPEAT," ");PetscFunctionReturn(NULL);}
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_REPEAT," ");PetscFunctionReturn(NULL);}
    ierr = MPI_Comm_set_attr(ncomm,Petsc_Viewer_Socket_keyval,(void*)viewer);
    if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_INITIAL," ");PetscFunctionReturn(NULL);}
  }
  ierr = PetscCommDestroy(&ncomm);
  if (ierr) {PetscError(PETSC_COMM_SELF,__LINE__,"PETSC_VIEWER_SOCKET_",__FILE__,PETSC_ERR_PLIB,PETSC_ERROR_REPEAT," ");PetscFunctionReturn(NULL);}
  PetscFunctionReturn(viewer);
}

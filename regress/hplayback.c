#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "harness.h"
static FILE *Tinputfile, *Treportfile;
static vbuf vb2;
static void Tensurereportfile(void) {
  const char *fdstr;
  int fd;
  Treportfile= stderr;
  fdstr= getenv("ADNS_TEST_REPORT_FD"); if (!fdstr) return;
  fd= atoi(fdstr);
  Treportfile= fdopen(fd,"a"); if (!Treportfile) Tfailed("fdopen ADNS_TEST_REPORT_FD");
}
static void Psyntax(const char *where) {
  fprintf(stderr,"adns test harness: syntax error in test log input file: %s\n",where);
  exit(-1);
}
static void Pcheckinput(void) {
  if (ferror(Tinputfile)) Tfailed("read test log input file");
  if (feof(Tinputfile)) Psyntax("eof at syscall reply");
}
static void Tensureinputfile(void) {
  const char *fdstr;
  int fd;
  int chars;
  unsigned long sec, usec;
  if (Tinputfile) return;
  Tinputfile= stdin;
  fdstr= getenv("ADNS_TEST_IN_FD");
  if (fdstr) {
    fd= atoi(fdstr);
    Tinputfile= fdopen(fd,"r"); if (!Tinputfile) Tfailed("fdopen ADNS_TEST_IN_FD");
  }
  if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
  fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
  chars= -1;
  sscanf(vb2.buf," start %lu.%lu%n",&sec,&usec,&chars);
  if (chars==-1) Psyntax("start time invalid");
  currenttime.tv_sec= sec;
  currenttime.tv_usec= usec;
  if (vb2.buf[chars] != '\n') Psyntax("not newline after start time");
}
static void Parg(const char *argname) {
  int l;
  if (vb2.buf[vb2.used++] != ' ') Psyntax("not a space before argument");
  l= strlen(argname);
  if (memcmp(vb2.buf+vb2.used,argname,l)) Psyntax("argument name wrong");
  vb2.used+= l;
  if (vb2.buf[vb2.used++] != '=') Psyntax("not = after argument name");
}
static int Perrno(const char *stuff) {
  const struct Terrno *te;
  int r;
  char *ep;
  for (te= Terrnos; te->n && strcmp(te->n,stuff); te++);
  if (te->n) return te->v;
  r= strtoul(stuff+1,&ep,10);
  if (ep) Psyntax("errno value not recognised, not numeric");
  return r;
}
static void P_updatetime(void) {
  int chars;
  unsigned long sec, usec;
  if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
  fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
  chars= -1;
  sscanf(vb2.buf," +%lu.%lu%n",&sec,&usec,&chars);
  if (chars==-1) Psyntax("update time invalid");
  currenttime.tv_sec+= sec;
  currenttime.tv_usec+= usec;
  if (currenttime.tv_usec > 1000000) {
    currenttime.tv_sec++;
    currenttime.tv_usec -= 1000000;
  }
  if (vb2.buf[chars] != '\n') Psyntax("not newline after update time");
}
static void Pfdset(fd_set *set, int max) {
  int r, c;
  char *ep;
  if (vb2.buf[vb2.used++] != '[') Psyntax("fd set start not [");
  FD_ZERO(set);
  for (;;) {
    r= strtoul(vb2.buf+vb2.used,&ep,10);
    if (r>=max) Psyntax("fd set member > max");
    FD_SET(r,set);
    vb2.used= ep - (char*)vb2.buf;
    c= vb2.buf[vb2.used++];
    if (c == ']') break;
    if (c != ',') Psyntax("fd set separator not ,");
  }
}
static void Paddr(struct sockaddr *addr, int *lenr) {
  struct sockaddr_in *sa= (struct sockaddr_in*)addr;
  char *p, *ep;
  long ul;
  assert(*lenr >= sizeof(*sa));
  p= strchr(vb2.buf+vb2.used,':');
  if (!p) Psyntax("no port on address");
  *p++= 0;
  memset(sa,0,sizeof(*sa));
  sa->sin_family= AF_INET;
  if (!inet_aton(vb2.buf+vb2.used,&sa->sin_addr)) Psyntax("invalid address");
  ul= strtoul(p,&ep,10);
  if (*ep && *ep != ' ') Psyntax("invalid port (bad syntax)");
  if (ul >= 65536) Psyntax("port too large");
  sa->sin_port= htons(ul);
  *lenr= sizeof(*sa);
  vb2.used= ep - (char*)vb2.buf;
}
static int Pbytes(byte *buf, int maxlen) {
  static const char hexdigits[]= "0123456789abcdef";
  int c, v, done;
  const char *pf;
  done= 0;
  for (;;) {
    c= getc(Tinputfile); Pcheckinput();
    if (c=='\n' || c==' ' || c=='\t') continue;
    if (c=='.') break;
    pf= strchr(hexdigits,c); if (!pf) Psyntax("invalid first hex digit");
    v= (pf-hexdigits)<<4;
    c= getc(Tinputfile); Pcheckinput();
    pf= strchr(hexdigits,c); if (!pf) Psyntax("invalid second hex digit");
    v |= (pf-hexdigits);
    if (maxlen<=0) Psyntax("buffer overflow in bytes");
    *buf++= v;
    maxlen--; done++;
  }
  for (;;) {
    c= getc(Tinputfile); Pcheckinput();
    if (c=='\n') return done;
  }
}
void Q_vb(void) {
  int r;
  Tensureinputfile();
  if (!adns__vbuf_ensure(&vb2,vb.used+2)) Tnomem();
  r= fread(vb2.buf,1,vb.used+2,Tinputfile);
  if (feof(Tinputfile)) {
    fprintf(stderr,"adns test harness: input ends prematurely; program did:\n %.*s\n",
           vb.used,vb.buf);
    exit(-1);
  }
  Pcheckinput();
  if (vb2.buf[0] != ' ') Psyntax("not space before call");
  if (memcmp(vb.buf,vb2.buf+1,vb.used) ||
      vb2.buf[vb.used+1] != '\n') {
    fprintf(stderr,
            "adns test harness: program did unexpected:\n %.*s\n"
            "was expecting:\n %.*s\n",
            vb.used,vb.buf, vb.used,vb2.buf+1);
    exit(1);
  }
}
int Hselect(
	int max , fd_set *rfds , fd_set *wfds , fd_set *efds , struct timeval *to 
	) {
 int r;
 char *ep;
 Qselect(
	max , rfds , wfds , efds , to 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," select=",8)) Psyntax("syscall reply mismatch");
 if (vb2.buf[8] == 'E') {
  int e;
  e= Perrno(vb2.buf+8);
  P_updatetime();
  errno= e;
  return -1;
 }
  r= strtoul(vb2.buf+8,&ep,10);
  if (*ep && *ep!=' ') Psyntax("return value not E* or positive number");
  vb2.used= ep - (char*)vb2.buf;
	Parg("rfds"); Pfdset(rfds,max); 
	Parg("wfds"); Pfdset(wfds,max); 
	Parg("efds"); Pfdset(efds,max); 
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hsocket(
	int domain , int type , int protocol 
	) {
 int r;
 char *ep;
	Tmust("socket","domain",domain==AF_INET); 
  Tmust("socket","type",type==SOCK_STREAM || type==SOCK_DGRAM); 
 Qsocket(
	 type 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," socket=",8)) Psyntax("syscall reply mismatch");
 if (vb2.buf[8] == 'E') {
  int e;
  e= Perrno(vb2.buf+8);
  P_updatetime();
  errno= e;
  return -1;
 }
  r= strtoul(vb2.buf+8,&ep,10);
  if (*ep && *ep!=' ') Psyntax("return value not E* or positive number");
  vb2.used= ep - (char*)vb2.buf;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hfcntl(
	int fd , int cmd , ... 
	) {
 int r;
 char *ep;
	va_list al; long arg; 
  Tmust("fcntl","cmd",cmd==F_SETFL || cmd==F_GETFL);
  if (cmd == F_SETFL) {
    va_start(al,cmd); arg= va_arg(al,long); va_end(al);
  } else {
    arg= 0;
  } 
 Qfcntl(
	fd , cmd , arg 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," fcntl=",7)) Psyntax("syscall reply mismatch");
 if (vb2.buf[7] == 'E') {
  int e;
  e= Perrno(vb2.buf+7);
  P_updatetime();
  errno= e;
  return -1;
 }
  r= strtoul(vb2.buf+7,&ep,10);
  if (*ep && *ep!=' ') Psyntax("return value not E* or positive number");
  vb2.used= ep - (char*)vb2.buf;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hconnect(
	int fd , const struct sockaddr *addr , int addrlen 
	) {
 int r;
 Qconnect(
	fd , addr , addrlen 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," connect=",9)) Psyntax("syscall reply mismatch");
 if (vb2.buf[9] == 'E') {
  int e;
  e= Perrno(vb2.buf+9);
  P_updatetime();
  errno= e;
  return -1;
 }
  if (memcmp(vb2.buf+9,"OK",2)) Psyntax("success/fail not E* or OK");
  vb2.used= 9+2;
  r= 0;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hclose(
	int fd 
	) {
 int r;
 Qclose(
	fd 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," close=",7)) Psyntax("syscall reply mismatch");
 if (vb2.buf[7] == 'E') {
  int e;
  e= Perrno(vb2.buf+7);
  P_updatetime();
  errno= e;
  return -1;
 }
  if (memcmp(vb2.buf+7,"OK",2)) Psyntax("success/fail not E* or OK");
  vb2.used= 7+2;
  r= 0;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hsendto(
	int fd , const void *msg , int msglen , unsigned int flags , const struct sockaddr *addr , int addrlen 
	) {
 int r;
 char *ep;
	Tmust("sendto","flags",flags==0); 
 Qsendto(
	fd , msg , msglen , addr , addrlen 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," sendto=",8)) Psyntax("syscall reply mismatch");
 if (vb2.buf[8] == 'E') {
  int e;
  e= Perrno(vb2.buf+8);
  P_updatetime();
  errno= e;
  return -1;
 }
  r= strtoul(vb2.buf+8,&ep,10);
  if (*ep && *ep!=' ') Psyntax("return value not E* or positive number");
  vb2.used= ep - (char*)vb2.buf;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}
int Hrecvfrom(
	int fd , void *buf , int buflen , unsigned int flags , struct sockaddr *addr , int *addrlen 
	) {
 int r;
	Tmust("recvfrom","flags",flags==0); 
	Tmust("recvfrom","*addrlen",*addrlen>=sizeof(struct sockaddr_in)); 
 Qrecvfrom(
	fd , buflen , *addrlen 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," recvfrom=",10)) Psyntax("syscall reply mismatch");
 if (vb2.buf[10] == 'E') {
  int e;
  e= Perrno(vb2.buf+10);
  P_updatetime();
  errno= e;
  return -1;
 }
  if (memcmp(vb2.buf+10,"OK",2)) Psyntax("success/fail not E* or OK");
  vb2.used= 10+2;
  r= 0;
	Parg("addr"); Paddr(addr,addrlen); 
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
	r= Pbytes(buf,buflen); 
 P_updatetime();
 return r;
}
int Hread(
	int fd , void *buf , size_t buflen 
	) {
 int r;
 Qread(
	fd , buflen 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," read=",6)) Psyntax("syscall reply mismatch");
 if (vb2.buf[6] == 'E') {
  int e;
  e= Perrno(vb2.buf+6);
  P_updatetime();
  errno= e;
  return -1;
 }
  if (memcmp(vb2.buf+6,"OK",2)) Psyntax("success/fail not E* or OK");
  vb2.used= 6+2;
  r= 0;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
	r= Pbytes(buf,buflen); 
 P_updatetime();
 return r;
}
int Hwrite(
	int fd , const void *buf , size_t len 
	) {
 int r;
 char *ep;
 Qwrite(
	fd , buf , len 
	);
 if (!adns__vbuf_ensure(&vb2,1000)) Tnomem();
 fgets(vb2.buf,vb2.avail,Tinputfile); Pcheckinput();
 Tensurereportfile();
 fprintf(Treportfile,"syscallr %s",vb2.buf);
 vb2.avail= strlen(vb2.buf);
 if (vb.avail<=0 || vb2.buf[--vb2.avail]!='\n')
  Psyntax("badly formed line");
 vb2.buf[vb2.avail]= 0;
 if (memcmp(vb2.buf," write=",7)) Psyntax("syscall reply mismatch");
 if (vb2.buf[7] == 'E') {
  int e;
  e= Perrno(vb2.buf+7);
  P_updatetime();
  errno= e;
  return -1;
 }
  r= strtoul(vb2.buf+7,&ep,10);
  if (*ep && *ep!=' ') Psyntax("return value not E* or positive number");
  vb2.used= ep - (char*)vb2.buf;
 if (vb2.used != vb2.avail) Psyntax("junk at end of line");
 P_updatetime();
 return r;
}

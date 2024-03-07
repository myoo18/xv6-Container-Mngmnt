struct stat;
struct rtcdate;

// system calls
int   fork(void);
int   exit(void) __attribute__((noreturn));
int   wait(void);
int   pipe(int *);
int   write(int, void *, int);
int   read(int, void *, int);
int   close(int);
int   kill(int);
int   exec(char *, char **);
int   open(char *, int);
int   mknod(char *, short, short);
int   unlink(char *);
int   fstat(int fd, struct stat *);
int   link(char *, char *);
int   mkdir(char *);
int   chdir(char *);
int   dup(int);
int   getpid(void);
char *sbrk(int);
int   sleep(int);
int   uptime(void);
//container manager syscalls
int  cm_maxproc(int nproc);
int  cm_setroot(char *path, int path_len);
int  test_newroot(char *path, int path_len);
int  test_dotdot(void);
int  cm_create_and_enter(void);

//mutex.h
void mutex_init(void);
int mutex_create(char *name);
int mutex_delete(int muxid);
int mutex_lock(int muxid);
int mutex_unlock(int muxid);
int cv_wait(int muxid);
int cv_signal(int muxid);

/*------------------------------------SHARED MEM---------------------------------------------*/
char* shm_get(char *name);
int   shm_rem(char *name);
/*------------------------------------SHARED MEM---------------------------------------------*/
int	  prio_set(int, int); 

// ulib.c
int   stat(char *, struct stat *);
char *strcpy(char *, char *);
void *memmove(void *, void *, int);
char *strchr(const char *, char c);
int   strcmp(const char *, const char *);
void  printf(int, char *, ...);
char *gets(char *, int max);
uint  strlen(char *);
void *memset(void *, int, uint);
void *malloc(uint);
void  free(void *);
int   atoi(const char *);
//container manager userlvl
int   strncmp( const char *, const char *, int);
void safe_strcpy(char *dest, const char *src, int n);
void strconcat(char *dest, const char *s1, const char *s2);
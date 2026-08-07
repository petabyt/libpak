#ifndef	_UNISTD_H
#define	_UNISTD_H	1

/* Sleep USECONDS microseconds, or until a signal arrives that is not blocked
   or ignored.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int usleep (unsigned long long __useconds);

#endif /* unistd.h  */

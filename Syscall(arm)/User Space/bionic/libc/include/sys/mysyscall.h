#ifndef _MY_SYS_CALL_H_
#define _MY_SYS_CALL_H_

#include <sys/cdefs.h> // define C common macro like '__BEGIN_DECLS'

__BEGIN_DECLS

extern int mysyscall(int param);
extern int __mysyscall(int param);

__END_DECLS
#endif /* _MY_SYS_CALL_H_ */
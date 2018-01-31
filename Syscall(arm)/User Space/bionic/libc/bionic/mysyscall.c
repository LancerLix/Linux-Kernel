#include <unistd.h>
#include <sys/mysyscall.h>

int mysyscall(int param)
{
    return __mysyscall(param);
}
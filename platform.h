
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "isitypes.h"
typedef uint64_t isi_time_t;

#if defined(_MSC_VER)
#include <WinSock2.h>
#include <WS2tcpip.h>

constexpr fdesc_t fdesc_empty = { ~0 };

bool operator==(const fdesc_t &, const fdesc_t &);
bool operator!=(const fdesc_t &, const fdesc_t &);

typedef signed __int64 ssize_t;
int read(fdesc_t fdesc, char *buf, int len);
int write(fdesc_t fdesc, const char *buf, int len);
fdesc_t open(const char *path, int flags);
fdesc_t open(const char *path, int flags, int mode);
int lseek(fdesc_t fdesc, ssize_t ofs, int how);
int close(fdesc_t fdesc);
int setsockopt(fdesc_t s, int level, int optname, const int * optval, int optlen);
int sleep(int);
int fstat(fdesc_t, struct stat *);
int poll(pollfd *, uint32_t, int);
constexpr int SHUT_RDWR = SD_RECEIVE | SD_SEND;
extern fdesc_t STDERR_FILENO;
extern fdesc_t STDIN_FILENO;
struct dirent {
	ino_t d_ino;
	char d_name[260];
};
struct DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *);
int closedir(DIR *);
#elif defined(POSIX)
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/time.h>
#include <dirent.h>
#endif

int platform_startup();
fdesc_t platform_socket(int, int, int);
int get_net_error();

int set_nonblock(fdesc_t fdesc);
void enter_service();

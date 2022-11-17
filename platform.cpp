
#include "platform.h"
#include <string>

#if defined(_MSC_VER)
#include <Windows.h>
#pragma comment(lib, "WS2_32.lib")
#include <immintrin.h>
#include <intrin.h>

static LARGE_INTEGER pfreq;
static int64_t pfreqm;
static int64_t pfreqr;
extern fdesc_t fdlog;
fdesc_t STDERR_FILENO;
fdesc_t STDIN_FILENO;
bool operator==(const fdesc_t &a, const fdesc_t &b) { return a.v == b.v; }
bool operator!=(const fdesc_t &a, const fdesc_t &b) { return a.v != b.v; }
fdesc_t &fdesc_s::operator=(const SOCKET s) {
	this->v = s;
	return *this;
}
int platform_startup() {
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	STDERR_FILENO.h = fdlog.h = GetStdHandle(STD_ERROR_HANDLE);
	STDIN_FILENO.h = GetStdHandle(STD_INPUT_HANDLE);
	pfreqm = 1000000000ll / f.QuadPart;
	pfreqr = 1000000000ll % f.QuadPart;
	if(!pfreqm) pfreqm = 1;
	pfreq.QuadPart = f.QuadPart;
	WSAData wsd;
	int r = WSAStartup(0x202, &wsd);
	if(r) {
		isilogneterr("Startup");
		return 1;
	}
	return 0;
}
int get_net_error() { return WSAGetLastError(); }
fdesc_t platform_socket(int af, int s, int p) {
	return { socket(af, s, p) };
}
void isi_fetch_time(isi_time_t *t) {
	LARGE_INTEGER qf;
	QueryPerformanceCounter(&qf);
	qf.QuadPart *= pfreqm;
	if(pfreqr) qf.QuadPart /= pfreqr;
	*t = qf.QuadPart;
}
void platform_test() {}
int set_nonblock(fdesc_t fdesc) {
	u_long mode = 1;
	return ioctlsocket(fdesc, FIONBIO, &mode);
}
int setsockopt(fdesc_t s, int level, int optname, const int *optval, int optlen) {
	return setsockopt(s.v, level, optname, (const char *)optval, optlen);
}
/* this is not a proper fstat, it's just enough for what isi needs */
int fstat(fdesc_t fd, struct stat *sti) {
	LARGE_INTEGER fs;
	if(!sti) return -1;
	if(!GetFileSizeEx(fd.h, &fs)) {
		return -1;
	}
	sti->st_size = fs.QuadPart;
	return 0;
}
int read(fdesc_t fdesc, char *buf, int len) {
	DWORD wt;
	if(!ReadFile(fdesc.h, buf, len, &wt, nullptr)) {
		return -1;
	}
	return wt;
}
int write(fdesc_t fdesc, const char *buf, int len) {
	DWORD wt;
	if(!WriteFile(fdesc.h, buf, len, &wt, nullptr)) {
		return -1;
	}
	return wt;
}
fdesc_t open(const char *path, int flags) { return fdesc_empty; }
fdesc_t open(const char *path, int flags, int mode) { return fdesc_empty; }
int lseek(fdesc_t fdesc, ssize_t ofs, int how) {
	LARGE_INTEGER fs;
	fs.QuadPart = ofs;
	if(!SetFilePointerEx(fdesc.h, fs, nullptr, how)) {
		return -1;
	}
	return 0;
}
int send(fdesc_t fdesc, const unsigned char *buf, int len, int flags) {
	return -1;
}
int close(fdesc_t fdesc) {
	return closesocket(fdesc.v);
	if(!CloseHandle(fdesc.h))
		return -1;
	return 0;
}
int sleep(int ms) {
	Sleep(ms);
	return 0;
}
int poll(pollfd *fd, uint32_t nfd, int timeout) {
	return WSAPoll(fd, nfd, timeout);
}
void enter_service() {} // can't do this on win yet
struct DIR {
	std::wstring cpath;
	HANDLE findhandle;
	WIN32_FIND_DATAW finddata;
	struct dirent ent;
};

DIR *opendir(const char *path) {
	if(path == nullptr || path[0] == 0) return nullptr;
	std::wstring wpath;
	std::wstring wdir;
	size_t l = strlen(path);
	wpath.reserve(l);
	for(size_t i = 0; i < l; i++) {
		char c = path[i];
		if(c == '/') c = '\\';
		wpath.push_back(c);
	}
	l = 1023;
	wchar_t *filepart = nullptr;
	do {
		wdir.resize(l);
		l = GetFullPathNameW(wpath.c_str(), (DWORD)wdir.size(), &wdir[0], &filepart);
	} while((l != 0) && (l > wdir.size()));
	if(l == 0) return nullptr;
	wdir.resize(l);
	if(filepart != nullptr) {
		wdir.push_back(L'\\');
	}
	wdir.push_back(L'*');
	DIR *d = new DIR;
	d->cpath = std::move(wdir);
	d->findhandle = INVALID_HANDLE_VALUE;
	return d;
}
struct dirent *readdir(DIR *d) {
	if(d == nullptr) return nullptr;
	if(d->findhandle == INVALID_HANDLE_VALUE) {
		d->findhandle = FindFirstFileExW(d->cpath.c_str(), FindExInfoBasic, &d->finddata, FindExSearchNameMatch, NULL, 0);
		if(d->findhandle == INVALID_HANDLE_VALUE) {
			return nullptr;
		}
	} else {
		if(!FindNextFileW(d->findhandle, &d->finddata)) {
			FindClose(d->findhandle);
			d->findhandle = INVALID_HANDLE_VALUE;
			return nullptr;
		}
	}
	size_t i = 0;
	wchar_t wc;
	while(i < sizeof(dirent::d_name) && (wc = d->finddata.cFileName[i])) {
		if((unsigned)wc > 0x80u) {
			d->ent.d_name[i] = '?';
		} else {
			d->ent.d_name[i] = (char)wc;
		}
		i++;
	}
	if(i < sizeof(dirent::d_name)) {
		d->ent.d_name[i++] = 0;
	}
	return &d->ent;
}
int closedir(DIR *d) {
	if(!d) return -1;
	if(d->findhandle != INVALID_HANDLE_VALUE) {
		FindClose(d->findhandle);
		d->findhandle = INVALID_HANDLE_VALUE;
	}
	delete d;
	return 0;
}
#endif
#ifdef POSIX
int platform_startup() { return 0; }
int get_net_error() { return errno; }
fdesc_t platform_socket(int af, int s, int p) {
	return socket(af, s, p);
}
int set_nonblock(fdesc_t fdesc) {
	return fcntl(fdesc, F_SETFL, O_NONBLOCK);
}

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

void isi_fetch_time(isi_time_t *t) {
	struct timespec mono;
	clock_gettime(CLOCK_MONOTONIC_RAW, &mono);
	*t = mono.tv_sec * 1000000000 + mono.tv_nsec;
}

void enter_service() {
	pid_t pid, sid;
	pid = fork();
	if(pid < 0) {
		isilogerr("fork");
		exit(1);
	}
	if(pid > 0) {
		isilog(L_INFO, "entering service\n");
		exit(0);
	}
	umask(0);
	open_log();
	/*
	if( (chdir("/")) < 0 ) {
	isilogerr("chdir");
	}*/
	sid = setsid();
	if(sid < 0) {
		isilogerr("setsid");
		exit(1);
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}
#endif

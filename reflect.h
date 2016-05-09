
#include <stdint.h>

#define ISIR_void(o,n,l,tl)  fprintf(stderr, "%%+%02x VOID-" #n "\n", o);
#define ISIR_char(o,n,l,tl)  fprintf(stderr, "%%+%02x CHAR[%d]-" #n "\n", o, l / tl);
#define ISIR_int(o,n,l,tl)  fprintf(stderr, "%%+%02x INT[%d]-" #n "\n", o, l / tl);

#define offsetof(s,m)  ((uintptr_t)( &(((s*)0)->m) ))
#define ISIR_TEST(s,a,t)  ISIR_##t(offsetof(s,a),a,sizeof(((s*)0)->a),sizeof(t))

#define ISIR(s,t,n) {1,sizeof(t),offsetof(struct s,n),#n},
#define _ISI_REFL_struct

#define ISIREFNAME(sname) _ISI_REFL_##sname##_REFLECT
#ifndef ISIREFLECT
#define ISIREFLECT(sname,vdef) \
	struct isiReflEntry _ISI_REFL_##sname##_REFLTABLE[] = {vdef{0,0,0,0}}; \
	struct isiReflection _ISI_REFL_##sname##_REFLECT = { sizeof(sname), _ISI_REFL_##sname##_REFLTABLE };
#endif

struct isiReflEntry {
	const int type;
	const int tlen;
	const int offset;
	const char * const ident;
};
struct isiReflection {
	const uint32_t length;
	struct isiReflEntry const * const ent;
};


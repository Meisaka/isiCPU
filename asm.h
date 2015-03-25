
typedef struct stOPTABLE {
	char * nmo;
	int opu;
	int opb;
	int opc;
	int cyc;
} OpTable;

typedef struct stASMSYM {
	char * ident;
	unsigned short value;
	int scope;
	int blockid;
	void * next;
} AsmSym;

typedef struct stASMENT {
	int line;
	unsigned short sym;
	unsigned short addr;
	int op;
	unsigned short ptypex;
	unsigned short pvalx;
	unsigned short ptypey;
	unsigned short pvaly;
	void * next;
} AsmEnt;

int DCPUASM_asm(const char *, int, short*);


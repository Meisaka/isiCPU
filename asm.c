
#include "opcode.h"
#include "asm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static OpTable ALLI[] = {
{"DAT", -2, 0, -1, 0},
{".ORG",0, 0, 1, 0},
{".EQU",0, 0, 1, 0},
{"BRK", 0, 0, 0, 9},
{"SET", OP_SET, 0, 2, 1},
{"ADD", OP_ADD, 0, 2, 2},
{"SUB", OP_SUB, 0, 2, 2},
{"MUL", OP_MUL, 0, 2, 2},
{"MLI", OP_MLI, 0, 2, 2},
{"DIV", OP_DIV, 0, 2, 3},
{"DVI", OP_DVI, 0, 2, 3},
{"MOD", OP_MOD, 0, 2, 3},
{"MDI", OP_MDI, 0, 2, 3},
{"AND", OP_AND, 0, 2, 1},
{"BOR", OP_BOR, 0, 2, 1},
{"XOR", OP_XOR, 0, 2, 1},
{"SHR", OP_SHR, 0, 2, 1},
{"ASR", OP_ASR, 0, 2, 1},
{"SHL", OP_SHL, 0, 2, 1},
{"IFB", OP_IFB, 0, 2, 2},
{"IFC", OP_IFC, 0, 2, 2},
{"IFE", OP_IFE, 0, 2, 2},
{"IFN", OP_IFN, 0, 2, 2},
{"IFG", OP_IFG, 0, 2, 2},
{"IFA", OP_IFA, 0, 2, 2},
{"IFL", OP_IFL, 0, 2, 2},
{"IFU", OP_IFU, 0, 2, 2},
{"ADX", OP_ADX, 0, 2, 3},
{"SBX", OP_SBX, 0, 2, 3},
{"STI", OP_STI, 0, 2, 2},
{"STD", OP_STD, 0, 2, 2},
{"JSR", SOP_JSR << 5, 0, 1, 3},
{"INT", SOP_INT << 5, 0, 1, 4},
{"IAG", SOP_IAG << 5, 0, 1, 1},
{"IAS", SOP_IAS << 5, 0, 1, 1},
{"RFI", SOP_RFI << 5, 0, 1, 3},
{"IAQ", SOP_IAQ << 5, 0, 1, 2},
{"HWN", SOP_HWN << 5, 0, 1, 2},
{"HWQ", SOP_HWQ << 5, 0, 1, 4},
{"HWI", SOP_HWI << 5, 0, 1, 4},
{0, 0, 0, 0, 0}
};

static char * ramblock;
static int ramsize;
static int ramused;
static char * endostrings;
static AsmSym* symbols;
static AsmEnt* program;


#define ERCDEF(a) static char * ERC_##a

ERCDEF(TOOMANY) = "Error: Too many Errors\n";
ERCDEF(PASSERR) = "Pass completed with errors!\n";
ERCDEF(GENSYN2) = "Line %d: Syntax Error '%c'\n";
ERCDEF(INVDLM1) = "Line %d: Syntax Error - invalid label marker ':'\n";
ERCDEF(INDOUT1) = "Line %d: Syntax Error - charactors outside indirection\n";
/*
ERCDEF() = ;
ERCDEF() = ;
ERCDEF() = ;
ERCDEF() = ;
*/
/*
 * Modes:
 * 0 start of line (Text treated as labels, or placement error)
 * 1 white space1
 * 2 label (via :)
 * 3 white space2
 * 4 instruction
 * 5 macro
 * 6 special
 * 7 white space (after instruction)
 * 8 operands / symbols / flag '[]'
 *
 * 9 space/ operator +-
 * 10 operand 1 symbol2 flag ']' at end
 * 11 space/comma no '[' before and no ']' after
 * 12 operand 2 symbol1
 * 13 space/ operator +-
 * 14 operand 2 symbol2
 * 15 space/]
 *
 * 100 comments
 *
 */

// strnequ[u] test (str != str)
// test string a with string b (size n) for non-equality
// return:
// 0 - equal
//-1 - not equal
// 1 - size not equal
int strnequu(const char *a, const char *b, int n)
{
	int i;
	if(!a || !b || n < 0) return -2;
	if(!n) return 0;
	for(i=0; i < n && toupper(a[i]) == toupper(b[i]); i++) {
		if(!a[i]) return -1;
		if(!b[i]) return 1;
	}
	if(!a[i] && !b[i])
		return 0;
	else
		return 1;
}

int DCPUASM_asm(const char * txt, int txtsize, short* ram)
{
	int i,j,k,l,ll,n;
	int erc, wnc;
	int mode, lmode;
	int indrc;
	int rval;
	int base;
	int lws;
	int reg;
	int ws;
	int wtin, lwtin, lblc ;
	int param, lparam, nmod;
	int opin;
	unsigned short PC;
	char * ttoc;
	void * tmpp;
	ramsize = 1000;
	ramused = 0;
	symbols = 0;
	program = 0;
	ramblock = (char*)malloc(ramsize);
	if(!ramblock) return -5;
	endostrings = ramblock;

	PC = 0; // Virtual Intruction count
	l = 1; // Line count
	ll = 0;
	erc = 0; // error count
	wnc = 0; // Warning count

	mode = 0; // line mode
	indrc = 0; // indirection mark [ ]
	reg = 0;
	rval = 0;
	ws = 1; // Whitespece or seperator
	lws = 0; // last whitespace
	wtin = 0;
	lwtin = 0; // object type
	k = 0;
	nmod = 0; // modifier count
	opin = 0;
	lblc = 0;
	lmode = 0; // last mode
	lparam = 0; // section parameter count
	param = 0;

	for(i = 0; i < txtsize; i++)
	{
		if(mode == 100)
		{
		switch(txt[i])
		{
			case 10:
			case 13:
				mode = 0;
			break;
		}
		} else if( mode == 99) {
		switch(txt[i]) {
		case 13:
		case 10:
			mode = 0;
			break;
		case '"':
			mode = lmode;
			lmode = 99;
		default:
			break;
		}
		}
		if(mode < 80) {

		switch(txt[i])
		{
		case ':':
			// label
			if(mode == 0 || mode == 1)
			{
				mode = 2;
				k = 0; wtin = 1;
				lwtin = 1;
				lblc = 1;
			} else if(mode == 2 && lblc == 0) {
				mode = 3;
				lblc++;
			} else {
				fprintf(stderr, ERC_INVDLM1, l); erc++;
			}
			ws = 1; break;
		case ' ':
		case '\t':
			if(mode == 0) mode = 1;
			if(mode == 2) mode = 3;
			if(mode == 4) { mode = 7; }
			if(mode == 8) { lwtin = 5;mode = 7; }
			wtin = 0;
			// white space
			ws = 1; break;
		case 13: // CR is ignored (reset mode but same line)
		case ';': // Comments run up to the end of the line
		case '#': // different comment char
			if(txt[i] == '#' || txt[i] == ';') { // A comment will terminate the line
			// comment
			mode = 100;
			}
			break;
		case 10:
			ws = 1; break;
		case '[':
			if(mode < 7) {
			fprintf(stderr, ERC_GENSYN2, l, txt[i]); erc++;
			} else {
				if(lparam) {
			fprintf(stderr, ERC_GENSYN2, l, txt[i]); erc++;
				} else if(indrc) {
			fprintf(stderr, "Line %d: Syntax Error - multiple '['\n", l); erc++;
				} else {
				indrc = 1;
				if(nmod || lparam) {
			fprintf(stderr, ERC_INDOUT1, l); erc++;
				}
				lparam++;
				nmod++;
				}
			}
			ws = 1; break;
		case '+':
			nmod++;
			ws = 1; break;
		case ']':
			if(mode < 7) {
			fprintf(stderr, ERC_GENSYN2, l, txt[i]); erc++;
			} else {
				if(indrc == 1) {
					indrc = 2;
				} else {
					if(indrc == 2) {
			fprintf(stderr, "Line %d: Syntax Error - multiple ']'\n", l); erc++;
					} else {
			fprintf(stderr, "Line %d: Syntax Error - unmatched ']'\n", l); erc++;
					}
				}
			}
			ws = 1; break;
		case '"':
			if(lmode != 99) {
			lmode = mode;
			mode = 99;
			lparam++;
			} else {
				//fprintf(stderr, "<.STR>");
				lmode = 0;
				if(opin != 1) {
					fprintf(stderr, "Line %d: Error - string only allowed with \"DAT\"\n",l);
				}
			}
			ws = 0; break;
		case '*':
			ws = 1; break;
		case '.':
			if(mode == 0 || mode == 1 || mode == 3)
			{
				mode = 5;
				k = 0; wtin = 1;
				lwtin = 2;
				endostrings[k++] = txt[i];
			} else {
			fprintf(stderr, ERC_GENSYN2, l, txt[i]); erc++;
			}
			lws = 0; ws = 0; break;
		case ',':
			if(mode < 7) {
			fprintf(stderr, ERC_GENSYN2, l, txt[i]); erc++;
			} else {
				if(indrc == 1) {
			fprintf(stderr, "Line %d: Syntax Error - unmatched '['\n", l); erc++;
				}
				if(!lparam) {
			fprintf(stderr, "Line %d: Syntax Error - missing parameter before ','\n", l);
				}
				if(lparam > nmod+1) {
			fprintf(stderr, "Line %d: Syntax Error - Missing ','\n", l);
				}
			}
			indrc = 0;
			wtin = 0;
			nmod = 0;
			lparam = 0;
			param++;
			ws = 1; break;
		default:
			if(indrc == 2) {
				fprintf(stderr, ERC_INDOUT1, l); erc++;
			} else {
			if(txt[i] == '-' || (txt[i] >= '0' && txt[i] <= '9')) {
				if(ws) { // Interpret number
					if(wtin) { // continue read
						endostrings[k++] = txt[i];
					} else { // start reading
						k = 0; wtin = 1;
						lparam++;lwtin = 5;
						endostrings[k++] = txt[i];
					}
				} else { // Part of something else
					if(wtin) { // read it?
						endostrings[k++] = txt[i];
					} else { // no, we don't want this
			fprintf(stderr, "Line %d: (NI)Error unexpected charactor: '%c'\n",l, txt[i]); erc++;
					}
				}
			} else if(txt[i] == '_' || ((txt[i] | 0x20) >= 'a' && (txt[i] | 0x20) <= 'z')) // read labels... for the most part.
			{
				if(mode == 0) { // no label marker ':'
					mode = 2; // this is still valid Asm
					k = 0; wtin = 1;
					lwtin = 1;
					//fprintf(stderr, "Line %d: Warning - implicit label\n", l);
				}
				if(mode == 1 || mode == 3) { // label marked
					mode = 4;
					lwtin = 4;
					k = 0; wtin = 1;
				}
				if((mode == 7 || mode == 8) && (!wtin))
				{ // identifiers in the args
					//fprintf(stderr, "<II%d,%d %c>", l, mode, txt[i]);
					mode = 8;lwtin = 5;
					k = 0; wtin = 1;
					lparam++;
				}
				if(wtin) { // read chars?
					endostrings[k++] = txt[i];
				}
			} else { // doesn't match anything... it's no good
				fprintf(stderr, "Line %d: (DF)Error unknown charactor: '%c'\n",l, txt[i]); erc++;
			}
			}
			ws = 0; break;
		}
		if(k + ramused >= ramsize) { // got RAM?
			tmpp = realloc(ramblock, ramsize + 1000); // get some
			if(!tmpp) { // we got it?
				fprintf(stderr, "Fatal Error: Out of Memory!\n"); erc++;
				goto lstopasm; // XXX: evil
			}
			ramblock = (char*)tmpp;
			ramsize += 1000;
		}

		if(lws != ws) { // whitespace seperation?
			if(lws == 0 && lwtin && k) {
				endostrings[k] = 0; // make a C-style string
				//fprintf(stderr, "<%d'%d:%s>",mode,lwtin,endostrings);
				// process it!!!
				if(lwtin == 1) { // labels
					if(k == 3) {
					for(j = 0; ALLI[j].nmo; j++) {
						ttoc = ALLI[j].nmo;
						for(n=0; ttoc[n] != 0 && ttoc[n] == toupper(endostrings[n]) && n < k; n++) { }
						if(ttoc[n] == 0 && n == k) { break; }
					}
					}
					if(k==3 && ALLI[j].nmo) {
						opin = j + 1;
						mode = 7;
					fprintf(stderr, "Line %d: Warning - opcode at beginning of line\n", l);
					} else {
						// process label
						if(!lblc) {
				// TODO this should be an optional message
				fprintf(stderr, "Line %d: Unmarked label - expected ':'\n", l);
						}
					}
				}
				if(lwtin == 2) { // .directives
				fprintf(stderr, "|LMM%s|\n", endostrings);
					mode = 8; 
				}
				if(lwtin == 4) { // opcodes
					for(j = 0; ALLI[j].nmo; j++) {
						ttoc = ALLI[j].nmo;
						for(n=0; ttoc[n] != 0 && ttoc[n] == toupper(endostrings[n]) && n < k; n++) {
						}
						if(ttoc[n] == 0 && n == k) {
							// opcode success
//fprintf(stderr, "Line %d: (OI)opcode %04x '%s'\n",l, ALLI[j].opu | (ALLI[j].opb << 5), endostrings);
							break;
						}
					}

					if(!ALLI[j].nmo) {
				fprintf(stderr, "Line %d: (OI)Error unknown opcode '%s'\n",l, endostrings); erc++;
					} else {
						opin = j + 1;
				//fprintf(stderr, "<OI:%s:%d>",endostrings, opin);
					}
					mode = 7;
				}
				if(lwtin == 5) { // identifiers/numbers
					wtin = 0;
					mode = 7;
					if(!strnequu(endostrings, "PiCK", 4)) {
					fprintf(stderr, "%i: %s is not currently supported.\n", l, endostrings);
					lparam--;
					}
				}
				lwtin = 0;
			}
			//wtin = 0;
			lws = ws;
		}

		if(txt[i] == 10)
		{
			// validate line
			if(indrc == 1) {
			fprintf(stderr, "Line %d: (LE) Syntax Error - unmatched '['\n", l); erc++;
			}
			if(lparam > nmod+1) {
			fprintf(stderr, "Line %d: Syntax Error - Missing ','\n", l);
			}
			param++;

			if(!opin) {
			// labels/comments/blank lines
			//fprintf(stderr, "%d: no opcode\n",l);
			} else { // active lines
				if(ALLI[opin-1].opc > -1 && param > ALLI[opin-1].opc) {
		fprintf(stderr, "Line %d: (LE) Syntax Error - moo many arguments\n", l); erc++;
				}
			}
			// Process line

			// Finish line processing here //
			// Reset for next line
			l++; // increase line count
			//fprintf(stderr, "\n", ll, l);
			ll = i;

			// new line
			mode = 0;
			indrc = 0;
			reg = 0;
			rval = 0;
			ws = 0;
			wtin = 0;
			lparam = 0;
			param = 0;
			nmod = 0;
			opin = 0;
			lblc = 0;
		}
		if(erc > 50) {
			fprintf(stderr, ERC_TOOMANY); erc++;
			break;
		}
		// End of loop
		}
	}
	if(erc)
	{
		fprintf(stderr, ERC_PASSERR);
		return -1;
	}

lstopasm:
	free(ramblock);
	return 0;
}

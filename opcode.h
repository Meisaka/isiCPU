
// Basic opcodes:

#define OP_SET 0x01
#define OP_ADD 0x02
#define OP_SUB 0x03
#define OP_MUL 0x04
#define OP_MLI 0x05
#define OP_DIV 0x06
#define OP_DVI 0x07
#define OP_MOD 0x08
#define OP_MDI 0x09
#define OP_AND 0x0A
#define OP_BOR 0x0B
#define OP_XOR 0x0C
#define OP_SHR 0x0D
#define OP_ASR 0x0E
#define OP_SHL 0x0F
#define OP_IFB 0x10
#define OP_IFC 0x11
#define OP_IFE 0x12
#define OP_IFN 0x13
#define OP_IFG 0x14
#define OP_IFA 0x15
#define OP_IFL 0x16
#define OP_IFU 0x17
#define OPN_IFMIN 0x10
#define OPN_IFMAX 0x17
//#define OP_--- 0x18
//#define OP_--- 0x19
#define OP_ADX 0x1A
#define OP_SBX 0x1B
//#define OP_--- 0x1C
//#define OP_--- 0x1D
#define OP_STI 0x1E
#define OP_STD 0x1F

// Special 1 Opcodes
#define SOP_JSR 0x01
//#define SOP_--- 0x02
//#define SOP_--- 0x03
//#define SOP_--- 0x04
//#define SOP_--- 0x05
//#define SOP_--- 0x06
#define SOP_HCF 0x07
#define SOP_INT 0x08
#define SOP_IAG 0x09
#define SOP_IAS 0x0A
#define SOP_RFI 0x0B
#define SOP_IAQ 0x0C
//#define SOP_--- 0x0D
//#define SOP_--- 0x0E
//#define SOP_--- 0x0F
#define SOP_HWN 0x10
#define SOP_HWQ 0x11
#define SOP_HWI 0x12


#ifndef RESERVED_H
#define RESERVED_H

// Control codes used by part buffering / code generation
// from /usr/piece/tools/mucc/reserved.h
enum CTRLCODE {
    CCD_END = 0,
    CCD_LEX = 127,
    CCD_RRR = 0xE0 + 0,
    CCD_GAT = 0xE0 + 1,
    CCD_JMP = 0xE0 + 2,
    CCD_CAL = 0xE0 + 3,
    CCD_RPT = 0xE0 + 4,
    CCD_NXT = 0xE0 + 5,
    CCD_TRS = 0xE0 + 6,
    CCD_TMP = 0xE0 + 7,
    CCD_INO = 0xE0 + 8,
    CCD_VOL = 0xE0 + 9,
    CCD_ENV = 0xE0 + 10,
    CCD_DTN = 0xE0 + 11,
    CCD_NOT = 0xE0 + 12,
    CCD_PPA = 0xE0 + 13,
    CCD_PON = 0xE0 + 14,
    CCD_POF = 0xE0 + 15,
    CCD_TAT = 0xE0 + 16,
    CCD_VIB = 0xE0 + 17,
    CCD_MVL = 0xE0 + 18,
    CCD_MFD = 0xE0 + 19,
    CCD_PFD = 0xE0 + 20,
    CCD_BND = 0xE0 + 21,
    CCD_BRK = 0xF6,
    CCD_NOP = 0xF7,
    CCD_LEG = 0xF8,
    CCD_LOF = 0xF9,
    CCD_EXP = 0xFA,
    CCD_EXR = 0xFB,
};

#endif // RESERVED_H

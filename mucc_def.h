#ifndef MUCC_NEXT_MUCC_DEF_H
#define MUCC_NEXT_MUCC_DEF_H

#define MACRONEST       16
#define MAXNEST         8           // muslib nests repeats and drum calls in 24 words, 3 per level
#define MAXPART         32

#define MAXLINE         1024
#define MAXPARTNAME     32
#define MAXMACRONAME    256

#define MAXDRUMMACRO    96          // derived from command definition

#define TPQN            24          // ticks per quarter note
#define TIMEBASE        (TPQN * 4)  // clocks per whole note (24 clocks per quarter note * 4)
#define MAXLENGTH       (126 + 256) // 1-126 as is, 127-382 by CCD_LEX and a byte

#endif //MUCC_NEXT_MUCC_DEF_H

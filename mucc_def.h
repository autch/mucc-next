#ifndef MUCC_NEXT_MUCC_DEF_H
#define MUCC_NEXT_MUCC_DEF_H

#define MACRONEST       16
#define MAXNEST         32
#define MAXPART         32

#define MAXLINE         1024
#define MAXPARTNAME     32
#define MAXMACRONAME    256

#define MAXDRUMMACRO    96          // derived from command definition

#define TPQN            24          // ticks per quarter note
#define TIMEBASE        (TPQN * 4)  // clocks per whole note (24 clocks per quarter note * 4)

#endif //MUCC_NEXT_MUCC_DEF_H

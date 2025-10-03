#include "part.h"

int mml_ctx::parse_partdef(char* part_name, int lineno, mml_part &mp, part_buffer &pb)
{
    while(true) {
        int c = static_cast<unsigned char>(getchar()), rel = 0;
        if(c == '\0' || c == '\n')
            break; // end of line
        switch(c) {
            case ' ':
            case '\t':
                // skip whitespace
                break;
            case '<':
                // relative octave change
                mp.oct--;
                break;
            case '>':
                // relative octave change
                mp.oct++;
                break;
            case 'o':
            {
                // octave change, followed by octave number 0-9, if number has +/-, change relative to current octave
                int oct = 0;
                read_number(&oct, &rel);
                if(rel)
                    mp.oct1 = oct;
                else
                    mp.oct = oct;
                break;
            }
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            {
                // note, optional sharp/flat, optional length, optional dot, optional tie
                static char tbl[] = {9, 11, 0, 2, 4, 5, 7};
                int note = tbl[c - 'a'] + (mp.oct + mp.oct1) * 12;
                while(true) {
                    if ( peek() == '+' ) note++;
                    else if ( peek() == '-' ) note--;
                    else break;
                    getchar();
                }

                if ( !(mp.tr_attr & 1) ) mp.xnote = note;
                gen_note(mp, mp.xnote, pb);
                break;
            }
#if 0 /* stray numbers should not be allowed, while original mucc allowed them */
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
                p--;
                printf("note same as previous: [%s]\n", p);
                gen_note(mp, mp.xnote, pb);
                break;
#endif
            case 'r':
                // rest, optional length, optional dot
                gen_note(mp, -1, pb);
                break;
            case 'x':
            {
                // note, the same as previous, optional length, optional dot
                gen_note(mp, mp.xnote, pb);
                break;
            }
            case '(':
            case ')':
            {
                // relative expression change, optional number 0-127
                int expr = mp.def_exp;
                if(isdigit(peek()))
                    read_number(&expr, &rel);
                if(c == '(')
                    expr = -expr;
                pb.write(CCD_EXR);
                pb.write(expr);
                break;
            }
            case 'v':
            {
                // absolute expression change, followed by number 0-127; if that number has +/-, change relative to the current expression
                int expr = 0;
                read_number(&expr, &rel);
                if(rel) {
                    pb.write(CCD_EXR);
                    pb.write(expr);
                } else {
                    pb.write(CCD_EXP);
                    pb.write(expr);
                }
                break;
            }
            case 'V':
            {
                // absolute volume change, followed by number 0-127; if that number has +/-, change relative to the current volume
                int vol = 0;
                read_number(&vol, &rel);
                if(rel)
                    mp.vol1 = vol;
                else
                    mp.vol = vol;
                pb.write(CCD_VOL);
                pb.write(mp.vol + mp.vol1);
                break;
            }
            case 'l':
            {
                // default note length, length number or direct clock number by '%'
                int length = 0;
                read_length(&length, &rel);
                if(length == 0) {
                    printf("Warning: parameter of l cannot be zero, line %d\n", lineno);
                    break;
                }
                if(rel)
                    mp.len1 = length;
                else
                    mp.len = length;
                break;
            }
            case 'q':
            case 'Q':
            {
                int gate = 0;
                read_number(&gate, &rel);
                if(c == 'q') {
                    gate = ((24 - gate) * 99) / 24;
                } else {
                    gate = (gate * 99) / 8;
                }
                if(gate < 1) gate = 1;
                if(gate > 99) gate = 99;

                pb.write2(CCD_GAT, gate);
                break;
            }
            case 't':
            case 'T':
            {
                // tempo change, followed by number, if number has +/-, change relative to the current tempo
                int t = 0;
                read_number(&t, &rel);
                if(rel) {
                    tempo1 = t;
                } else {
                    tempo = t;
                }
                pb.write2(CCD_TMP, tempo + tempo1);

                break;
            }
            case 'D':
            {
                // detune, if another 'D' follows, value is relative to the current detune, followed by number -128 to 127
                int det = 0;
                if (peek() == 'D') {
                    getchar();
                    read_number(&det, &rel);
                    mp.det1 += det;
                } else {
                    read_number(&det, &rel);
                    mp.det = det;
                }
                pb.write2(CCD_DTN, mp.det + mp.det1);
                break;
            }
            case '[':
            {
                // loop start
                mp.xlen = 0;
                pb.write(CCD_RPT);
                mp.nestdata[mp.nestlevel++] = pb.addr(); // current code address
                pb.write(0); // placeholder for loop count

                mp.tickstack[++mp.ticklevel] = 0;
                mp.tickstack[++mp.ticklevel] = 0;
                break;
            }
            case ']':
            {
                // loop end, followed by an optional number of times to repeat (default infinite)
                if(mp.nestlevel == 0) {
                    printf("Warning: ']' found without matching '['.\n");
                    break;
                }

                int times = 0;
                read_number(&times, &rel);

                pb.write_at(mp.nestdata[--mp.nestlevel], times);
                pb.write(CCD_NXT);

                unsigned loop_ticks = mp.tickstack[mp.ticklevel--];
                unsigned till_break = mp.tickstack[mp.ticklevel--];

                if(till_break > 0) {
                    mp.current_tick() += loop_ticks * (times - 1) + till_break;
                } else {
                    mp.current_tick() += loop_ticks * times;
                }

                mp.xlen = 0;
                break;
            }
            case ':':
                // loop exit, only valid inside loop, and its last iteration
                if(mp.nestlevel == 0) {
                    printf("Warning: ':' found outside of loop.\n");
                    break;
                }
                pb.write(CCD_BRK);
                mp.ticks_till_break() = mp.current_tick();
                break;
            case '_':
            {
                // transpose, if another '_' follows, its value is relative to the current transpose, followed by number -24 to 24
                int trs = 0;
                if(peek() == '_') {
                    getchar();
                    read_number(&trs, &rel);
                    mp.trs += trs;
                } else {
                    read_number(&trs, &rel);
                    mp.trs = trs;
                }
                pb.write2(CCD_TRS, mp.trs);
                break;
            }
            case '!':
            {
                // call macro, followed by macro name
                if (call_macro(mp, pb) < 0) return -1;
                break;
            }
            case 'L':
                // loop point set
                mp.tick_at_loop = mp.current_tick();
                pb.loop_pos = pb.addr();
                break;
            case '=':
            {
                // track attribute, followed by attribute value in number
                int tr_attr = 0;
                read_number(&tr_attr, &rel);
                mp.tr_attr = tr_attr;
                pb.write2(CCD_TAT, mp.tr_attr);
                break;
            }
            case '@':
            {
                // change instrument, followed by instrument number 0-255
                int inst = 0;
                read_number(&inst, &rel);
                pb.write2(CCD_INO, inst);
                break;
            }
            case 'E':
            {
                // envelope, followed by envelope parameters, ar, dr, sl, sr
                int env[4];
                read_numbers(env, 4);
                pb.write_n(CCD_ENV, env, 4);
                break;
            }
            case 'Y':
            {
                // vibrato, followed by vibrato parameters, depth, speed, delay, rate
                int vib[4];
                read_numbers(vib, 4);
                pb.write_n(CCD_VIB, vib, 4);
                break;
            }
            case 'Z':
            {
                // portamento, followed by portamento parameters, note distance, speed
                int por[2];
                read_numbers(por, 2);
                pb.write_n(CCD_PPA, por, 2);
                break;
            }
            case '?':
                // alternative note, immediately followed by a note
                pb.write(CCD_NOT);
                break;
            case '{':
                // begin portamento
                mp.porsw = 1;
                break;
            case '}':
            {
                // end portamento, followed by optional length, optional dot, optional tie
                mp.porsw = 2;
                gen_note(mp, mp.pornote1, pb);
                pb.write(CCD_POF);
                break;
            }
            case '/':
                // ignored, used for visual alignment.  other drivers may use this for loop break (MUCOM88)
                break;
            case 'C':
            {
                // ignored, followed by number.  other drivers may use this for channel control (MML2MID) or clock change (MUCOM88)
                int ignored = 0;
                read_number(&ignored, &rel);
                break;
            }

            // ****
            // mucc-next extensions
            // ****
            case 'S':
            {
                // report the current tick position for synchronization
                printf(
                    "PART %s: LINE %d TICK %u\n"
                    "        OCT %d+%d LEN %d+%d VOL %d+%d DET %d+%d\n",
                    part_name, lineno, mp.current_tick(),
                    mp.oct, mp.oct1, mp.len, mp.len1, mp.vol, mp.vol1, mp.det, mp.det1);
                break;
            }
            case 'W':
            {
                // default step of `(` / `)` command, followed by number
                int step = 0;
                read_number(&step, &rel);
                if(step < 1) step = 1;
                mp.def_exp = step;
                break;
            }

            default:
                printf("Unknown command: [%c] in line %d [%s]\n", c, lineno, p);
                return -1;
        }
    }
    if(macro_sp > 0) {
        // still in macro
        printf("Warning: still in macro after part line parsing.\n");
    }
    return 0;
}

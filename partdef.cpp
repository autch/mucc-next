#include "part.h"

void mml_ctx::parse_partdef(const std::string_view part_name, mml_part &mp, part_buffer &pb)
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
                need_number(c, oct, rel);
                if(rel)
                    mp.oct1 = static_cast<int8_t>(check_s8(c, oct));
                else
                    mp.oct = static_cast<int8_t>(check_s8(c, oct));
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
                static const char tbl[] = {9, 11, 0, 2, 4, 5, 7};
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
                pos--;
                gen_note(mp, mp.xnote, pb);
                break;
            case 'r':
                // rest, optional length, optional dot
                gen_note(mp, NOTE_REST, pb);
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
                if(isdigit(peek())) {
                    read_number(expr, rel);
                    expr *= mp.exp_mul;
                }
                if(c == '(')
                    expr = -expr;
                check_s8(c, expr);
                pb.write(CCD_EXR);
                pb.write(expr);
                mp.exp += expr;
                break;
            }
            case 'v':
            {
                // absolute expression change, followed by number 0-127; if that number has +/-, change relative to the current expression
                int expr = 0;
                need_number(c, expr, rel);
                if(rel) {
                    check_s8(c, expr);
                    pb.write(CCD_EXR);
                    pb.write(expr);
                    mp.exp += expr;
                } else {
                    check_u8(c, expr);
                    pb.write(CCD_EXP);
                    pb.write(expr);
                    mp.exp = expr;
                }
                break;
            }
            case 'V':
            {
                // absolute volume change, followed by number 0-127; if that number has +/-, change relative to the current volume
                int vol = 0;
                need_number(c, vol, rel);
                if(rel)
                    mp.vol1 = static_cast<int8_t>(check_s8(c, vol));
                else
                    mp.vol = static_cast<uint8_t>(check_u8(c, vol));
                pb.write(CCD_VOL);
                pb.write(check_u8(c, mp.vol + mp.vol1));
                mp.exp = 0; // reset expression on volume change
                break;
            }
            case 'l':
            {
                // default note length, length number or direct clock number by '%'
                int length = 0;
                if(!isdigit(peek()) && peek() != '%')
                    error("missing parameter of `l`");
                read_length(length, rel);
                if(!rel)
                    check_length(length);
                if(rel)
                    mp.len1 = static_cast<int8_t>(length);
                else
                    mp.len = length;
                break;
            }
            case 'q':
            case 'Q':
            {
                int gate = 0;
                need_number(c, gate, rel);
                // ranges from pmml.txt. mucc makes q25 and over the same as q24
                check_param(c, gate, 0, c == 'q' ? 100 : 8);
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
                need_number(c, t, rel);
                if(rel) {
                    tempo1 = t;
                } else {
                    tempo = t;
                }
                pb.write2(CCD_TMP, check_u8(c, tempo + tempo1));

                break;
            }
            case 'D':
            {
                // detune, if another 'D' follows, value is relative to the current detune, followed by number -128 to 127
                int det = 0;
                if (peek() == 'D') {
                    getchar();
                    need_number(c, det, rel);
                    mp.det1 = static_cast<int8_t>(check_s8(c, mp.det1 + det));
                } else {
                    need_number(c, det, rel);
                    mp.det = static_cast<int8_t>(check_s8(c, det));
                }
                pb.write2(CCD_DTN, check_s8(c, mp.det + mp.det1));
                break;
            }
            case '[':
            {
                // loop start
                if(mp.loop_nest_level() >= MAXNEST)
                    error("repeats nested too deep (max %d levels)", MAXNEST);
                mp.xlen = 0;
                pb.write(CCD_RPT);
                mp.push_loop_addr(pb.addr(), lineno); // current code address
                pb.write(0); // placeholder for loop count

                mp.push_tick();
                break;
            }
            case ']':
            {
                // loop end, followed by an optional number of times to repeat (default infinite)
                if(mp.loop_nest_level() == 0)
                    error("`]` without `[`");

                int times = 0;
                read_number(times, rel);
                check_u8(c, times);

                pb.write_at(mp.pop_loop_addr(), times);
                pb.write(CCD_NXT);

                auto [loop_ticks, till_break] = mp.pop_tick();

                if(till_break != mml_part::NO_BREAK) {
                    mp.current_tick() += loop_ticks * (times - 1) + till_break;
                } else {
                    mp.current_tick() += loop_ticks * times;
                }

                mp.xlen = 0;
                break;
            }
            case ':':
                // loop exit, only valid inside loop, and its last iteration
                if(mp.loop_nest_level() == 0)
                    error("`:` outside of repeat");
                // muslib exits at the first `:` in the last iteration, the others are never taken
                if(mp.ticks_till_break() != mml_part::NO_BREAK)
                    error("second `:` in the same repeat");
                pb.write(CCD_BRK);
                mp.ticks_till_break() = mp.current_tick();
                break;
            case '_':
            {
                // transpose, if another '_' follows, its value is relative to the current transpose, followed by number -24 to 24
                int trs = 0;
                if(peek() == '_') {
                    getchar();
                    need_number(c, trs, rel);
                    mp.trs = static_cast<int8_t>(check_s8(c, mp.trs + trs));
                } else {
                    need_number(c, trs, rel);
                    mp.trs = static_cast<int8_t>(check_s8(c, trs));
                }
                pb.write2(CCD_TRS, mp.trs);
                break;
            }
            case '!':
            {
                // call macro, followed by macro name
                call_macro(mp, pb);
                break;
            }
            case 'L':
                // loop point set
                mp.save_tick_at_loop();
                pb.has_loop = true;
                pb.loop_pos = pb.addr();
                mp.loop_len_pending = true;
                mp.xlen_at_loop = 0;
                mp.leg_at_loop = mp.leg & 1;
                break;
            case '=':
            {
                // track attribute, followed by attribute value in number
                int tr_attr = 0;
                need_number(c, tr_attr, rel);
                mp.tr_attr = static_cast<uint8_t>(check_u8(c, tr_attr));
                pb.write2(CCD_TAT, mp.tr_attr);
                break;
            }
            case '@':
            {
                // change instrument, followed by instrument number 0-255
                int inst = 0;
                need_number(c, inst, rel);
                pb.write2(CCD_INO, check_u8(c, inst));
                break;
            }
            case 'E':
            {
                // envelope, followed by envelope parameters, ar, dr, sl, sr
                int env[4];
                need_numbers(c, env, 4);
                for(int v : env) check_u8(c, v);
                pb.write_n(CCD_ENV, env, 4);
                break;
            }
            case 'Y':
            {
                // vibrato, followed by vibrato parameters, depth, speed, delay, rate
                int vib[4];
                need_numbers(c, vib, 4);
                for(int v : vib) check_u8(c, v);
                pb.write_n(CCD_VIB, vib, 4);
                break;
            }
            case 'Z':
            {
                // portamento, followed by portamento parameters, note distance, speed
                int por[2];
                need_numbers(c, por, 2);
                check_s8(c, por[0]);
                check_u8(c, por[1]);
                pb.write_n(CCD_PPA, por, 2);
                break;
            }
            case '?':
                // alternative note, followed by a note. CCD_NOT is written just before the note byte
                mp.explicit_not = true;
                break;
            case '{':
                // begin portamento
                if(mp.porsw)
                    error("`{` inside `{}`");
                mp.porsw = 1;
                mp.porta_lineno = lineno;
                mp.porta_notes = 0;
                break;
            case '}':
            {
                // end portamento, followed by optional length, optional dot, optional tie
                if(mp.porsw != 1)
                    error("`}` without `{`");
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
                read_number(ignored, rel);
                break;
            }

            // ****
            // mucc-next extensions
            // ****
            case 'S':
            {
                // report the current tick position for synchronization
                printf(
                    "line %4d: PART %s TICK %5u  TR_ATTR %d OCT %d+%d LEN %d+%d VOL %d+%d EXP %d DETUNE %d+%d XPOSE %d REP_NEST %zd\n",
                    lineno, part_name.data(), mp.current_tick(),
                    mp.tr_attr,
                    mp.oct, mp.oct1, mp.len, mp.len1, mp.vol, mp.vol1, mp.exp, mp.det, mp.det1,
                    mp.trs, mp.loop_nest_level());
                break;
            }
            case 'W':
            {
                // default step of `(` / `)` command, followed by number
                bool exp_mul = false;
                if (peek() == '*') {
                    getchar();
                    exp_mul = true;
                }
                int step = 0;
                need_number(c, step, rel);
                check_param(c, step, 1, 127);
                if (!exp_mul)
                    mp.def_exp = step;
                else
                    mp.exp_mul = step;
                break;
            }
            case '|':
                // measure bar, ignored
                break;

            default:
                error("unknown command `%c` [%s]", c, std::string(line.substr(pos)).c_str());
        }
    }
    if(!macro_stack.empty()) {
        // still in macro
        printf("[BUG] line %d: still in macro after part line parsing.\n", lineno);
    }
}

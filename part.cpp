#include <cctype>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "part.h"
#include "codegen.h"

char mml_ctx::getchar()
{
    char c;
    while(true) {
        if(pos < line.size() && (c = line[pos++]) != '\0')
            return c;
        if(macro_stack.empty())
            return 0;
        pop_macro();
    }
}

int mml_ctx::read_number(int& out, int& is_relative)
{
    int sign = 1;
    is_relative = 0;
    if(peek() == '+' || peek() == '-') {
        if(peek() == '-')
            sign = -1;
        is_relative = 1;
        getchar();
    }
    if(!isdigit(peek())) {
        return 0; // no number found
    }
    int value = 0;
    while(isdigit(peek())) {
        value = value * 10 + (getchar() - '0');
    }
    out = value * sign;
    while(isspace(peek()))
        getchar(); // skip whitespace
    return 1;
}

int mml_ctx::read_numbers(int* out, int count)
{
    int rel = 0, nums_read = 0;
    for(int i = 0; i < count; i++) {
        if(!read_number(out[i], rel)) {
            return nums_read; // failed to read number
        }
        nums_read++;
        if(peek() == ',' && i < count - 1)
            getchar(); // skip comma
        while(isspace(peek()))
            getchar(); // skip whitespace
    }
    return nums_read;
}

int convert_length_to_clock(int len)
{
    // assuming 4/4 time signature and 24 clocks per quarter note
    if(len <= 0) 
    {
        printf("warning: length too short (%d), setting to 1\n", len);
        return 1;
    }
    if(len > TIMEBASE) {
        printf("warning: length too long (%d), limiting to 96\n", len);
        len = TIMEBASE;
    }
    return TIMEBASE / len; // 96 = 24 * 4
}

int mml_ctx::read_length(int& out, int& is_relative)
{
    if(peek() == '%') {
        // direct clock number
        getchar(); // consume '%'
        return read_number(out, is_relative);
    } else {
        int total = 0;
        while(true) {
            int value = 0;
            if(isdigit(peek())) {
                while(isdigit(peek())) {
                    value = value * 10 + (getchar() - '0');
                }
                value = convert_length_to_clock(value);
            } else {
                value = out;
            }
            if(peek() == '.') {
                // dot length
                int x = value;
                do {
                    value += (x >>= 1);
                    getchar();
                }while(peek() == '.');
            }
            total += value;
            if(peek() == '^') {
                getchar();
                continue;
            }
            break;
        }
        out = total;
        is_relative = 0;
        while(isspace(peek()))
            getchar(); // skip whitespace
        return 1;
    }
}

// note codes are o0c = 0. `0x80 + code` covers 0..95; `CCD_NOT` followed by
// a byte reaches -12..127 (muslib reads 0x00-0x7F as code + 12, 0x80-0xFF as code + 128)
void mml_ctx::write_note(mml_part &mp, int code, part_buffer &pb)
{
    bool explicit_not = mp.explicit_not;
    mp.explicit_not = false;

    if(code == NOTE_REST) {
        pb.write(CCD_RRR);
        return;
    }
    if(!explicit_not && code >= 0 && code < 96) {
        pb.write(128 + code);
        return;
    }
    if(code < -12 || code > 127) {
        printf("Warning: note out of range (%d), replaced by rest in line %d\n", code, lineno);
        pb.write(CCD_RRR);
        return;
    }
    if(!explicit_not) {
        printf("Warning: note out of range (%d), written with `?` in line %d\n", code, lineno);
        if(code >= 96)
            printf("\tnote: original muslib may play it an octave lower with @3-@5\n");
    }
    pb.write(CCD_NOT);
    pb.write(code >= 0 ? 128 + code : code + 12);
}

void mml_ctx::gen_note(mml_part &mp, int code, part_buffer &pb, unsigned steps)
{
    int len = mp.len + mp.len1, rel;
    if(int ret = read_length(len, rel); ret == 0)
        len = mp.len + mp.len1;

    if(peek() == '&') {
        getchar();
        if(mp.xxleg < 2) mp.xxleg = 2; // tie
    }

    if(mp.porsw == 2) {
		int x = static_cast<int>(log(fabs(mp.pornote2 - mp.pornote1) / len) / log(2.0));
		x <<= 4;
		x += 128;
        
        pb.write(CCD_PPA);
        pb.write(mp.pornote2 - mp.pornote1);
        pb.write(x);

        mp.porsw = 0;
        
        pb.write(CCD_PON);
    }

    if(mp.xxleg) mp.xxleg--;
    if(mp.xxleg) {
        if(!(mp.leg & 1)) {
            pb.write(CCD_LEG);
            mp.leg |= 1;
        }
    } else {
        if(mp.leg & 1) {
            pb.write(CCD_LOF);
            mp.leg &= ~1;
        }
    }

    if ( !mp.porsw ) {
        if(mp.loop_len_pending) {
            // the first note after `L` omits its length: the player must have it at the jump
            mp.loop_len_pending = false;
            if(mp.xlen == len) mp.xlen_at_loop = len;
        }
		if ( mp.xlen != len ) {
			mp.xlen = len;
            pb.write_length(len);
		}
        mp.current_tick() += mp.count_steps ? 1 : len * steps;

        write_note(mp, code, pb);
	} else {
		mp.pornote2 = mp.pornote1;
		mp.pornote1 = code;
	}
}

int mml_ctx::call_macro(mml_part &mp, part_buffer &pb)
{
    char c;
    char macro_name[MAXMACRONAME];
    int i = 0;
    while((c = peek()) != '\0' && isalpha(c)) {
        macro_name[i++] = c;
        getchar();
    }
    macro_name[i] = '\0';
    if(auto it = macro.find(std::string(macro_name)); it != macro.end()) {
        if(macro_stack.size() >= MACRONEST) {
            printf("Macro stack overflow.\n");
            return -1;
        }
        push_macro();
        set_p(const_cast<char *>(it->second.definition.c_str()), it->second.lineno);
    } else if(mp.tr_attr & 1) {
        if(auto dit = drummacro.find(std::string(macro_name)); dit != drummacro.end()) {
            int index = dit->second;
            compile_drummacro(index);
            gen_note(mp, index, pb, drum_items[index].steps);
            mp.xnote = index;
        } else {
            printf("Drum macro not found: [%s]\n", macro_name);
        }
    } else {
        printf("Macro isn't found: [%s], note: cannot call drum macro in tracks other than =1'ed\n", macro_name);
    }
    return 0;
}

void mml_ctx::register_macro(int lineno, const std::string& name, const std::string& definition)
{
    if(auto [iter, success] = macro.insert_or_assign(name, macro_item{lineno, definition});
        !success) {
        printf("Warning: macro redefined! [%s] in line %d\n", name.c_str(), lineno);
    }
}

int mml_ctx::register_drummacro(int lineno, const std::string& name, const std::string& definition)
{
    if(drum_items.size() >= MAXDRUMMACRO) {
        printf("Error: too many drum macros (max %d) in line %d\n", MAXDRUMMACRO, lineno);
        return -1;
    }
    if(auto [iter, success] = drummacro.insert_or_assign(name, static_cast<int>(drum_items.size()));
        !success) {
        printf("Warning: drum macro redefined! [%s] in line %d\n", name.c_str(), lineno);
    }
    drum_items.push_back({name, lineno, definition, false, 0, {}});
    return 0;
}

// compiles a drum pattern on its first call, as the caller needs its steps to count ticks
void mml_ctx::compile_drummacro(int index)
{
    drummacro_item& item = drum_items[index];
    if(item.compiled) return;
    item.compiled = true;

    // save the context of the calling line. the pattern must not return into the caller's macros
    auto saved_line = line;
    int saved_pos = pos, saved_lineno = lineno;
    std::stack<macro_stack_item> saved_macro_stack;
    std::swap(macro_stack, saved_macro_stack);

    mml_part dummy_part; // dummy part context for drum pattern
    dummy_part.len = 0;  // notes in a pattern take the length of the calling part, so never write lengths
    dummy_part.count_steps = true;
    part_buffer& pb = item.pb;

    set_p(item.definition, item.lineno);
    parse_partdef("!!" + item.name, dummy_part, pb);

    pb.write(CCD_END);
    pb.size = pb.pos;
    item.steps = dummy_part.current_tick();

    std::swap(macro_stack, saved_macro_stack);
    line = saved_line;
    pos = saved_pos;
    lineno = saved_lineno;
}

int mml_ctx::parse_partline(std::string_view part_token, int lineno, std::string_view line, codegen& cg)
{
    char part_name[2];

    for(char pt : part_token) {
        if(0 && strchr(part_token.data(), pt) != &pt) {
            printf("duplicate part definition: [%c][%s]\n", pt, part_token.data());
            return -1;
        }
        char part = pt;
        partflags |= 1 << (part - 'A');

        part_name[0] = part;
        part_name[1] = '\0';
        // printf("\tParsing for part [%c][%s]\n", part, line.data());

        // set part context to p
        mml_part& mp = parts[part - 'A'];
        part_buffer& pb = cg.get_part(part - 'A');
        set_p(line, lineno);
        
        parse_partdef(part_name, mp, pb);
        pb.length_written = mp.current_tick();
    }
    return 0;
}

int mml_ctx::parse_wildcardline(int lineno, std::string_view line, codegen& cg)
{
    char part_name[32 + 1];
    uint32_t pf = partflags;
    char* pt = part_name;

    for (int part = 0; part < MAXPART; part++) {
        if (pf & 1) {
            *pt++ = 'A' + part;
        }
        pf >>= 1;
    }
    *pt = '\0';
    // printf("\tWILDCARD: Parsing for part [%s][%s]\n", part_name, line.data());
    return parse_partline(part_name, lineno, line, cg);
}

int mml_ctx::parse_drumline(codegen& cg)
{
    std::vector<part_buffer> drum_buffers;

    // patterns never called are compiled here
    for(size_t index = 0; index < drum_items.size(); index++) {
        compile_drummacro(static_cast<int>(index));
        drum_buffers.push_back(std::move(drum_items[index].pb));
    }
    cg.set_drum_patterns(std::move(drum_buffers));
    return 0;
}

int mml_ctx::end_mml(codegen& cg)
{
    // the length and legato at the end of the part are in effect after jumping back to `L`,
    // so restore what the code after `L` assumes
    for(int part = 0; part < MAXPART; part++) {
        mml_part& mp = parts[part];
        part_buffer& pb = cg.get_part(part);
        if(!pb.has_loop) continue;
        if(mp.xlen_at_loop != 0 && mp.xlen != mp.xlen_at_loop)
            pb.write_length(mp.xlen_at_loop);
        if((mp.leg & 1) != mp.leg_at_loop)
            pb.write(mp.leg_at_loop ? CCD_LEG : CCD_LOF);
    }

    parse_drumline(cg);
    return 0;
}

void mml_ctx::report_macro(FILE *fp)
{
    fprintf(fp, "MACRO definitions:\n");
    for(auto& [key, value]: macro) {
        fprintf(fp, "\tline %4d: !%s = [%s]\n", value.lineno, key.c_str(), value.definition.c_str());
    }
}

void mml_ctx::report_drummacro(FILE *fp)
{
    fprintf(fp, "DRUMMACRO definitions:\n");
    for(auto& [key, index]: drummacro) {
        const drummacro_item& item = drum_items[index];
        fprintf(fp, "\tline %4d: !!%s = (%2d, %u steps)[%s]\n",
            item.lineno, key.c_str(), index, item.steps, item.definition.c_str());
    }
}

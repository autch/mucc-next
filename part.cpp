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

void mml_ctx::gen_note(mml_part &mp, int code, part_buffer &pb)
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
		if ( mp.xlen != len ) {
			mp.xlen = len;
            pb.write_length(len);
		}
        mp.current_tick() += len;

        pb.write((code >= 0 && code < 96) ? 128 + code : CCD_RRR);
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
            gen_note(mp, dit->second.index, pb);
            mp.xnote = dit->second.index;
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

void mml_ctx::register_drummacro(int lineno, const std::string& name, const std::string& definition)
{
    if(drummacro_count >= MAXDRUMMACRO) {
        printf("Too many drum macros (max %d).\n", MAXDRUMMACRO);
        return;
    }
    if(auto [iter, success] = drummacro.insert_or_assign(
        name, drummacro_item{drummacro_count, lineno, definition});
        !success) {
        printf("Warning: drum macro redefined! [%s] in line %d\n", name.c_str(), lineno);
    }
    drummacro_count++;
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
    char drum_token[MAXMACRONAME];
    std::vector<part_buffer> drum_buffers;
    drum_buffers.resize(drummacro_count);

    for(auto &[name, item] : drummacro) {
        const std::string& definition = item.definition;
        mml_part dummy_part; // dummy part context for drum pattern
        part_buffer& pb = drum_buffers[item.index];

        set_p(const_cast<char *>(definition.c_str()), item.lineno);

        snprintf(drum_token, sizeof drum_token, "!!%s", name.c_str());
        parse_partdef(drum_token, dummy_part, pb);

        pb.write(CCD_END);
        pb.size = pb.pos;
        pb.length_written = dummy_part.current_tick();

    }
    cg.set_drum_patterns(std::move(drum_buffers));
    return 0;
}

int mml_ctx::end_mml(codegen& cg)
{
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
    for(auto& [key, value]: drummacro) {
        fprintf(fp, "\tline %4d: !!%s = (%2d)[%s]\n", value.lineno, key.c_str(), value.index, value.definition.c_str());
    }
}

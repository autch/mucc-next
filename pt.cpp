#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <regex>

#include "mucc_def.h"
#include "error.h"
#include "file.h"
#include "part.h"
#include "codegen.h"

// a line of part names only, like `A`, is an empty part line
std::regex re_part_line(R"(^([A-Z]*|[?]+)(?:\s+(.*))?$)", std::regex::ECMAScript | std::regex::optimize);
std::regex re_meta_line(R"(^#([a-zA-Z0-9]+)(?:\s+(.*))?$)", std::regex::ECMAScript | std::regex::optimize);
std::regex re_macro_line(R"(^(!!?)([a-zA-Z]+)\s+(.*)$)", std::regex::ECMAScript | std::regex::optimize);

std::string part_token;

void parse_line(file &f, mml_ctx& mml, codegen& cg)
{
    std::smatch match;
    std::string_view line = f.get_p();

    if(line.starts_with('\0') || line.starts_with('\n') || line.starts_with('\r')) {
        return; // skip empty or whitespace-only lines
    }
    if(std::regex_match(f.get_buffer(), match, re_part_line)) {
        std::string new_part_token = match[1];
        std::string content = match[2];

        if(new_part_token == "?") {
            mml.parse_wildcardline(f.get_line_number(), content, cg);
            return;
        }
        if(!new_part_token.empty()) {
            part_token = new_part_token;
        }

        // printf("\tPART: [%s][%s]\n", part_token.c_str(), content.c_str());
        mml.parse_partline(part_token, f.get_line_number(), content, cg);
    } else if(std::regex_match(f.get_buffer(), match, re_meta_line)) {
        cg.register_meta(f.get_line_number(), std::string(match[1]), std::string(match[2]));
    } else if(std::regex_match(f.get_buffer(), match, re_macro_line)) {
        if(match[1] == "!!") {
            mml.register_drummacro(f.get_line_number(), std::string(match[2]), std::string(match[3]));
            return;
        }
        mml.register_macro(f.get_line_number(), std::string(match[2]), std::string(match[3]));
    } else {
        fatal(f.get_line_number(), "unknown line format [%s]", f.get_buffer().c_str());
    }
}

// returns the size of the pmd written
int compile(char* in_filename, const std::string& out_filename, mml_ctx& mml, codegen& cg)
{
    {
        file f;

        if (f.open(in_filename) != 0) {
            perror("file_open");
            throw compile_error("cannot open " + std::string(in_filename));
        }

        while (f.readline().data() != nullptr) {
            if (f.get_p().empty())
                continue;
            // printf("READ[%3d]: [%s]\n", f.line_number, f.p);
            parse_line(f, mml, cg);
        }
        f.close();
    }

    mml.end_mml(cg);

    FILE* out_fp = fopen(out_filename.c_str(), "wb");
    if(!out_fp) {
        perror("fopen for output");
        throw compile_error("cannot open " + out_filename);
    }
    try {
        int bytes_written = cg.write_pmd(out_fp);
        fclose(out_fp);
        return bytes_written;
    } catch(...) {
        fclose(out_fp);
        std::filesystem::remove(out_filename);
        throw;
    }
}

int usage(int ac, char** av)
{
    fprintf(stderr, "Usage: %s filename.mml [out_filename.pmd]\n", av[0]);
    return EXIT_FAILURE;
}

int main(int ac, char** av)
{
    std::string out_filename;

    part_token[0] = '\0';

    if (ac < 2) {
        return usage(ac, av);
    }
    if(ac >= 3) {
        out_filename = std::string(av[2]);
    } else {
        std::filesystem::path p(av[1]);
        out_filename = p.replace_extension(".pmd").string();
    }
    if(out_filename.empty()) {
        return usage(ac, av);
    }

    codegen cg;
    mml_ctx mml;
    int bytes_written;
    try {
        bytes_written = compile(av[1], out_filename, mml, cg);
    } catch(const compile_error& e) {
        printf("Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    cg.report_meta(stdout);
    mml.report_macro(stdout);
    mml.report_drummacro(stdout);

    for(int part = 0; part < MAXPART; part++) {
        mml_part& mp = mml.get_part(part);
        part_buffer& pb = cg.get_part(part);
        if(unsigned ticks = mp.current_tick(); ticks > 0 || pb.size > 0) {
            if('A' + part <= 'Z') {
                printf("PART %c: total %4u, at loop %4u, %5llu bytes\n",
                    'A' + part, ticks, mp.get_tick_at_loop(), pb.size);
            } else {
                printf("PART (%d): total %4u, at loop %4u, %5llu bytes\n",
                    part, ticks, mp.get_tick_at_loop(), pb.size);
            }
        }
    }
    printf("Total %d bytes written.\n", bytes_written);

    return EXIT_SUCCESS;
}

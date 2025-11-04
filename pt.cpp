#include <cstdio>
#include <cstdlib>
#include <cctype>

#include "mucc_def.h"
#include "file.h"
#include "part.h"
#include "codegen.h"

int parse_meta(file &f, codegen& cg)
{
    if(f.read_token(is_meta_token) == 1) {
        // printf("\tMETA: [%s][%s]\n", f->last_token, f->p);
        cg.register_meta(std::string(f.get_last_token()), std::string(f.get_p()));
        return 1;
    } else {
        printf("No meta token found.\n");
    }
    return 0;
}

int parse_macro(file &f, mml_ctx& mml)
{
    if(f.read_token(is_macro_name) == 1) {
        // printf("\tMACDEF: [%s][%s]\n", f->last_token, f->p);
        mml.register_macro(f.get_line_number(), std::string(f.get_last_token()), std::string(f.get_p()));
        return 1;
    } else {
        printf("No macro token found.\n");
    }
    return 0;
}

int is_part_line(char* p)
{
    char* pp = p;
    char* part_end = nullptr;

    while(*pp && isupper(static_cast<uint8_t>(*pp)))
        pp++;
    
    if(pp == p)
        return 0; // no uppercase letters at start

    part_end = pp;

    while(*pp && isspace(static_cast<uint8_t>(*pp)))
        pp++;

    if(pp == part_end && *pp != '\0')
        return 0; // no space after part token, but not end of line

    return static_cast<int>(part_end - p); // return length of part-line token
}

char part_token[MAXPARTNAME];

int parse_line(file &f, mml_ctx& mml, codegen& cg)
{
    switch(f.peek()) {
        case '\0':
            break;
        case '\t':
        case ' ':
            f.skip_while(isspace);
            mml.parse_partline(part_token, f.get_line_number(), f.get_buffer(), cg);
            break;
        case '#':
            parse_meta(f, cg);
            break;
        case '!':
            parse_macro(f, mml);
            break;
        case '?':
        {
            // printf("WILDCARD: [%s]\n", f.p);
            while (f.peek() == '?')
                f.getchar();
            f.skip_while(isspace);
            mml.parse_wildcardline(f.getchar(), f.get_p(), cg);
            break;
        }
        default:
        {
            if(int len = is_part_line(f.get_p()); len > 0) {
                snprintf(part_token, sizeof part_token, "%.*s", static_cast<int>(len), f.get_p());
                f.skip_p(len);
                f.skip_while(isspace);
                // printf("\tPART: [%s][%s]\n", part_token, f->p);
                mml.parse_partline(part_token, f.get_line_number(), f.get_p(), cg);

                if(f.get_p() > f.get_buffer() + sizeof(f.get_buffer()) || f.get_p() < f.get_buffer()) {
                    // f->p may point to somewhere in macro definition
                    printf("Error: pointer out of buffer range.\n");
                    return -1;
                }
            } else {
                printf("Unknown line format: [%s]\n", reinterpret_cast<const char *>(f.get_p()));
            }
        }
    }
    return 0;
}

int usage(int ac, char** av)
{
    fprintf(stderr, "Usage: %s filename.mml [out_filename.pmd]\n", av[0]);
    return EXIT_FAILURE;
}

int main(int ac, char** av)
{
    file f;
    codegen cg;
    mml_ctx mml;
    std::string out_filename;

    part_token[0] = '\0';

    if (ac < 2) {
        return usage(ac, av);
    }
    if (f.open(av[1]) != 0) {
        perror("file_open");
        return EXIT_FAILURE;
    }
    if(ac >= 3) {
        out_filename = std::string(av[2]);
    } else {
        out_filename = std::string(av[1]);
        if(auto dot = out_filename.find_last_of('.'); dot != std::string::npos) {
            out_filename = out_filename.substr(0, dot) + ".pmd";
        } else {
            out_filename += ".pmd";
        }
    }

    while (f.readline() != nullptr) {
        if(f.peek() == '\0')
            continue; // skip empty lines
        // printf("READ[%3d]: [%s]\n", f.line_number, f.p);
        parse_line(f, mml, cg);
    }
    f.close();

    mml.end_mml(cg);

    int bytes_written;
    {
        FILE* out_fp = nullptr;
        if(!out_filename.empty()) {
            out_fp = fopen(out_filename.c_str(), "wb");
            if(!out_fp) {
                perror("fopen for output");
                return EXIT_FAILURE;
            }
        }

        bytes_written = cg.write_pmd(out_fp);
        fclose(out_fp);
    }

    cg.report_meta(stdout);
    mml.report_macro(stdout);
    mml.report_drummacro(stdout);

    for(int part = 0; part < MAXPART; part++) {
        mml_part& mp = mml.get_part(part);
        part_buffer& pb = cg.get_part(part);
        if(unsigned ticks = mp.current_tick(); ticks > 0 || pb.size > 0) {
            printf("PART %c: total %4u, at loop %4u, %5llu bytes\n",
                'A' + part, ticks, mp.get_tick_at_loop(), pb.size);
        }
    }
    printf("Total %d bytes written.\n", bytes_written);

    return EXIT_SUCCESS;
}

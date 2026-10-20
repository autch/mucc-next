#include "file.h"

#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdint>

int file::open(char* filename)
{
    if(fp) return -1; // already opened
    fp = fopen(filename, "r");
    if (!fp) return -1;
    buffer.assign({});
    view = buffer;
    line_number = 0;
    return 0;
}

void file::close()
{
    if (fp) fclose(fp);
    fp = nullptr;
}

std::string_view file::readline()
{
    char buf[MAXLINE];

    if (!fp)
        return {}; // not opened
    if (fgets(buf, sizeof buf, fp) == nullptr)
        return {};

    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';

    if(buf[0] != '#') {
        if(char* sc = strchr(buf, ';'))
            *sc = '\0'; // remove comment
    }
    buffer.assign(buf);
    view = buffer;
    line_number++;

    return view;
}


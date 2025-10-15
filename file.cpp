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
    memset(buffer, 0, sizeof buffer);
    p = buffer;
    line_number = 0;
    last_token[0] = '\0';
    return 0;
}

void file::close()
{
    if (fp) fclose(fp);
    fp = nullptr;
}

char* file::readline()
{
    if (!fp)
        return nullptr; // not opened
    if (fgets(buffer, sizeof buffer, fp) == nullptr)
        return nullptr;

    if (char* nl = strchr(buffer, '\n')) *nl = '\0'; // remove newline

    if(buffer[0] != '#') {
        if(char* sc = strchr(buffer, ';')) *sc = '\0'; // remove comment
    }
    p = buffer;
    line_number++;
    last_token[0] = '\0';

    return p;
}

int file::read_token(int (*token_type)(size_t, char*))
{
    char* start = p;

    if(*p == '\0')
        return EOF; // end of line or empty

    while (*p && token_type(p - start, p))
        p++;
    if (start == p) return 0; // no token found
    
    size_t len = p - start;
    snprintf(last_token, sizeof last_token, "%.*s", static_cast<int>(len), start);

    while(*p && isspace(static_cast<uint8_t>(*p)))
        p++; // skip whitespace

    return 1; // token found
}

int is_meta_token(size_t pos, char* c)
{
    if(pos == 0)
        return *c == '#';
    return isalnum(static_cast<uint8_t>(*c));
}

int is_macro_name(size_t pos, char* c)
{
    if(pos == 0)
        return *c == '!';
    if(pos == 1)
        return *c == '!' || isalpha(static_cast<uint8_t>(*c));
    return isalpha(static_cast<uint8_t>(*c));
}

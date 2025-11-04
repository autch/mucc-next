#if !defined(file_h)
#define file_h

#include <cstdio>
#include <functional>
#include <string>
#include "mucc_def.h"

class file
{
    FILE* fp{nullptr};
    char* p{nullptr}; // current position in buffer
    char buffer[MAXLINE]{0};
    int line_number{0};
    char last_token[MAXPARTNAME]{0};

public:
    ~file() { close(); }

    int open(char* filename);
    void close();
    char* readline();
    int read_token(const std::function<int(size_t, char)>& predicate);

    std::string_view get_last_token() const { return last_token; }
    int get_line_number() const { return line_number; }
    char* get_buffer() { return buffer; }
    char* get_p() { return p; }
    void skip_p(size_t n) { p += n; }
    void skip_while(const std::function<int(char)>& predicate)
    {
        while(char c = peek()) {
            if(!predicate(c))
                break;
            p++;
        }
    }

    char peek() const { return *p; }
    char getchar()
    {
        if(peek())
            return *p++;
        return '\0';        // do not advance on end of line
    }
};

// predicate functions for read_token
int is_meta_token(size_t pos, char c);
int is_macro_name(size_t pos, char c);

#endif // !defined(file_h)

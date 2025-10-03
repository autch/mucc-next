#if !defined(file_h)
#define file_h

#include <cstdio>
#include "mucc_def.h"

class file
{
    FILE* fp{nullptr};

public:
    char* p{nullptr}; // current position in buffer
    char buffer[MAXLINE]{0};
    int line_number{0};
    char last_token[MAXPARTNAME]{0};

    ~file() { close(); }

    int open(char* filename);
    void close();
    char* readline();
    int read_token(int (*token_type)(size_t, char*));

};

// predicate functions for read_token
int is_meta_token(size_t pos, char* c);
int is_macro_name(size_t pos, char* c);

#endif // !defined(file_h)

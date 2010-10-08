#include "strings.h"
#include <cstdio>

int main(int argc, char* argv[])
{
    DS::String str = "blah";
    printf("%d|%s|\n", str.length(), str.c_str());
    str += "halb";
    str = "0";
    printf("%d|%s|\n", str.length(), str.c_str());
    return 0;
}

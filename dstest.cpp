#include "strings.h"
#include <cstdio>

int main(int argc, char* argv[])
{
    DS::String str = "blah";
    printf("%d,%d|%s|\n", str.length(), str.toRaw().length(), str.toRaw().data());
    str += DS::String::FromUtf8(reinterpret_cast<const chr8_t*>("\xef\xbf\xbd"));
    printf("%d,%d|%s|\n", str.length(), str.toRaw().length(), str.toUtf8().data());
    return 0;
}

#include <stdio.h>

int main() {
    int a = 7;
    printf("Hello\n");
    printf("num is %d\n", a);
    //fflush(stdout);

    return 0;
}

/*
 compile in python kernel:
 !gcc test_simple.c -o simpleTest
 To run : !simpleTest
*/

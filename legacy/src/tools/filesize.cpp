#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2)
    {
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fclose(fp);

    printf("%d", size);
}
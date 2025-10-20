#include <stdio.h>

int main() {
    FILE *file = fopen("matrix.txt", "w");

    // Create a 3x3 matrix
    int matrix[3][3] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    // Write dimensions
    fprintf(file, "3,3\n");

    // Write matrix values
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            fprintf(file, "%d", matrix[i][j]);
            if (j < 2) fprintf(file, ",");
        }
        fprintf(file, "\n");
    }

    fclose(file);
    printf("Matrix written to matrix.txt\n");
    return 0;
}

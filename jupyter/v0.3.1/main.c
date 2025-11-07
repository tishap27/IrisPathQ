/**
 * IrisPathQ Route Optimizer
 * Main program - data loading, route generation, and cost matrix creation
*/

#include "data_structures.h"
//function declaration

int main() {
    printf("====================================\n");
    printf("IrisPathQ Route Optimization\n");
    printf("====================================\n\n");

    ProblemInstance problem;
    memset(&problem, 0, sizeof(ProblemInstance));

    printf("Loading data...\n");
    load_flights("data/flights.csv", &problem);
    load_waypoints("data/waypoints.csv", &problem);
    load_weather("data/weather.csv", &problem);

    printf("\n====================================\n");
    printf("Generating Routes\n");
    printf("====================================\n\n");

    int num_alternative_routes = 5;
    for (int i = 0; i < problem.num_flights; i++) {
        generate_alternative_routes(&problem, i, num_alternative_routes);
    }

    printf("\n====================================\n");
    printf("Building Cost Matrix\n");
    printf("====================================\n\n");

    double *cost_matrix = NULL;
    int matrix_size = 0;
    build_cost_matrix(&problem, &cost_matrix, &matrix_size);

    export_cost_matrix(cost_matrix, matrix_size, "output/cost_matrix.txt");
    export_full_matrix(cost_matrix, matrix_size, "output/full_matrix.txt");

    printf("\n====================================\n");
    printf("Summary\n");
    printf("====================================\n");
    printf("Total flights: %d\n", problem.num_flights);
    printf("Total routes generated: %d\n", matrix_size);
    printf("Cost matrix size: %d x %d\n", matrix_size, matrix_size);
    printf("\nReady for quantum optimization!\n");

    free(cost_matrix);
    return 0;
}


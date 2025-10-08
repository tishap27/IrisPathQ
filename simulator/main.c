
/**
 * Route Optimizer
*/
#include "data_structures.h"



int main() {
    printf("====================================\n");
    printf("Route Optimization\n");
    printf("====================================\n\n");
    
    // Initialize problem instance
    ProblemInstance problem;
    memset(&problem, 0, sizeof(ProblemInstance));
    
    // Load data
    printf("Loading data...\n");
    load_flights("data/flights.csv", &problem);
    load_waypoints("data/waypoints.csv", &problem);
    load_weather("data/weather.csv", &problem);
    
    printf("\n====================================\n");
    printf("Generating Routes\n");
    printf("====================================\n\n");
    
    // Generate 5 alternative routes for each flight
    int num_alternative_routes = 5;
    for (int i = 0; i < problem.num_flights; i++) {
        generate_alternative_routes(&problem, i, num_alternative_routes);
        printf("\n");
    }
    
    printf("\n====================================\n");
    printf("Building Cost Matrix \n");
    printf("====================================\n\n");
    
    // Build cost matrix
    double *cost_matrix = NULL;
    int matrix_size = 0;
    build_cost_matrix(&problem, &cost_matrix, &matrix_size);
    
    // Export for quantum processing
    export_cost_matrix(cost_matrix, matrix_size, "output/cost_matrix.txt");


    export_full_matrix(cost_matrix, matrix_size, "output/full_matrix.txt");
    
    printf("\n====================================\n");
    printf("Summary\n");
    printf("====================================\n");
    printf("Total flights: %d\n", problem.num_flights);
    printf("Total routes generated: %d\n", matrix_size);
    printf("Cost matrix size: %d x %d\n", matrix_size, matrix_size);
    printf("\n on this matrix now quantum!\n");
    
    free(cost_matrix);
    return 0;
}
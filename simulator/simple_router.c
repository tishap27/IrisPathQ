/**
 * Route Optimizer
*/
//Generates routes and builds a cost matrix 
#include "data_structures.h"
#include <float.h>

// Generating routes with hardcoded costs as of now based on calculated distance now
int generate_alternative_routes(ProblemInstance *problem, int flight_idx, int num_routes) {
    Flight *flight = &problem->flights[flight_idx];
    
    // Find origin and destination waypoint indices
    int origin_idx = -1, dest_idx = -1;
    for (int i = 0; i < problem->num_waypoints; i++) {
        if (strcmp(problem->waypoints[i].id, flight->origin) == 0) {
            origin_idx = i;
        }
        if (strcmp(problem->waypoints[i].id, flight->destination) == 0) {
            dest_idx = i;
        }
    }
    
    if (origin_idx == -1 || dest_idx == -1) {
        printf("  Error: Waypoints not found for %s\n", flight->flight_id);
        return -1;
    }
    
    printf("  %s (%s -> %s): ", flight->flight_id, flight->origin, flight->destination);
    
    // Calculate straight-line distance
    double distance = calculate_distance(
        problem->waypoints[origin_idx].latitude,
        problem->waypoints[origin_idx].longitude,
        problem->waypoints[dest_idx].latitude,
        problem->waypoints[dest_idx].longitude
    );
    
    // Generate multiple route variants
    for (int r = 0; r < num_routes && r < MAX_ROUTES_PER_FLIGHT; r++) {
        Route *route = &problem->routes[flight_idx][r];
        
        // Simple route: just origin and destination
        route->num_waypoints = 2;
        route->waypoint_indices[0] = origin_idx;
        route->waypoint_indices[1] = dest_idx;
        
        // Base distance
        route->total_distance = distance;
        
        // Variant: add 2% more distance per alternative hence right now every flight will choose R0
        if (r > 0) {
            route->total_distance *= (1.0 + (r * 0.02));
        }
        
        // Calculate fuel and time
        route->fuel_cost = calculate_fuel(route, &flight->aircraft);
        route->time_cost = route->total_distance / flight->aircraft.cruise_speed;
        route->conflict_count = 0;
        
        problem->num_routes_per_flight[flight_idx]++;
    }
    
    printf("%d routes\n", num_routes);
    return problem->num_routes_per_flight[flight_idx];
}

// Building cost matrix (diagonal only for now - no conflicts)
void build_cost_matrix(ProblemInstance *problem, double **cost_matrix, int *matrix_size) {
    // Calculate total number of variables
    int total_vars = 0;
    for (int i = 0; i < problem->num_flights; i++) {
        total_vars += problem->num_routes_per_flight[i];
    }
    
    *matrix_size = total_vars;
    
    // Allocate matrix
    *cost_matrix = (double*)calloc(total_vars * total_vars, sizeof(double));
    
    printf("\n  Building cost matrix (%d x %d)...\n", total_vars, total_vars);
    
    // Fill diagonal with route costs
    int var_index = 0;
    for (int flight = 0; flight < problem->num_flights; flight++) {
        for (int route = 0; route < problem->num_routes_per_flight[flight]; route++) {
            double fuel_cost = problem->routes[flight][route].fuel_cost;
            (*cost_matrix)[var_index * total_vars + var_index] = fuel_cost;
            var_index++;
        }
    }
    
    printf(" Cost matrix built\n");
}

// Exporting sparse cost matrix 
void export_cost_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf(" Cannot open %s\n", filename);
        return;
    }
    
    fprintf(file, "%d\n", matrix_size);  //just to make sure
    
    // Write only non-zero entries (sparse format)
    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            double value = cost_matrix[i * matrix_size + j];
            if (value != 0.0) {
                fprintf(file, "%d,%d,%.2f\n", i, j, value);
            }
        }
    }
    
    fclose(file);
    printf(" Exported to %s\n", filename);
}

// Export full dense matrix for visualization
void export_full_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf(" Cannot open %s\n", filename);
        return;
    }
    
    fprintf(file, "Full Cost Matrix (%d x %d)\n", matrix_size, matrix_size);
    fprintf(file, "================================\n\n");
    
    
    fprintf(file, "  - Diagonal values = Fuel cost (kg) for that route\n");
    fprintf(file, "  - Off-diagonal = 0 (no conflicts yet)\n\n");
    fprintf(file, "Row 0 to Row 4 is for flight 0 everytime chooses a different route until col4(each flight has 5 different routes) and so on...");
    
    fprintf(file, "================================\n\n");
    
    // Column headers
    fprintf(file, "      ");
    for (int j = 0; j < matrix_size; j++) {
        fprintf(file, "Col%-5d ", j);
    }
    fprintf(file, "\n");
    
    fprintf(file, "      ");
    for (int j = 0; j < matrix_size; j++) {
        fprintf(file, "-------- ");
    }
    fprintf(file, "\n");
    
    // Matrix rows
    for (int i = 0; i < matrix_size; i++) {
        fprintf(file, "Row%-2d | ", i);
        
        for (int j = 0; j < matrix_size; j++) {
            double value = cost_matrix[i * matrix_size + j];
            
            if (value == 0.0) {
                fprintf(file, "    0    ");
            } else if (i == j) {
                fprintf(file, "%8.0f ", value);  // Diagonal: fuel cost
            } else if (value >= 10000.0) {
                fprintf(file, " CONFLICT");  // Future: conflicts
            } else {
                fprintf(file, "%8.0f ", value);
            }
        }
        fprintf(file, "\n");
    }
    
    fprintf(file, "\n================================\n");
    
    fclose(file);
    printf(" Full matrix exported to %s\n", filename);
}

// Greedy solver - picks cheapest route for each flight
/*void solve_greedy(double *cost_matrix, int matrix_size, int num_flights, int routes_per_flight) {
    printf("\nGREEDY ALGORITHM SOLUTION\n");
    
    
    const char *flight_names[] = {
        "AC101 (YYZ->YVR)",
        "WS202 (YUL->YYC)", 
        "AC303 (YEG->YOW)",
        "WS404 (YWG->YHZ)",
        "AC505 (YYJ->YYT)"
    };
    
    double total_fuel = 0.0;
    
    for (int flight = 0; flight < num_flights; flight++) {
        int start = flight * routes_per_flight;
        int end = start + routes_per_flight;
        
        // Find cheapest route
        double min_cost = DBL_MAX;    // from float.h
        int best_route = 0;
        
        for (int route = start; route < end; route++) {
            double cost = cost_matrix[route * matrix_size + route];
            if (cost < min_cost) {
                min_cost = cost;
                best_route = route % routes_per_flight;
            }
        }
        
        total_fuel += min_cost;
        
        printf("  Flight %d (%s):\n", flight, flight_names[flight]);
        printf("    Selected Route %d → %.0f kg\n\n", best_route, min_cost);
    }
    
    printf("------------------------------------\n");
    printf("TOTAL FUEL: %.0f kg\n", total_fuel);
    printf("====================================\n");
}*/

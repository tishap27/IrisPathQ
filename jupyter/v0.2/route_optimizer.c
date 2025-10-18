/**
 * IrisPathQ Route Optimizer
 * Generate alternative routes and build cost matrix both full and sparse for now
*/

#include "data_structures.h"
#include <float.h>

int generate_alternative_routes(ProblemInstance *problem, int flight_idx, int num_routes) {
    Flight *flight = &problem->flights[flight_idx];

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

    double distance = calculate_distance(
        problem->waypoints[origin_idx].latitude,
        problem->waypoints[origin_idx].longitude,
        problem->waypoints[dest_idx].latitude,
        problem->waypoints[dest_idx].longitude
    );

    for (int r = 0; r < num_routes && r < MAX_ROUTES_PER_FLIGHT; r++) {
        Route *route = &problem->routes[flight_idx][r];

        route->num_waypoints = 2;
        route->waypoint_indices[0] = origin_idx;
        route->waypoint_indices[1] = dest_idx;

        route->total_distance = distance;

        if (r > 0) {
            route->total_distance *= (1.0 + (r * 0.02));
        }

        route->fuel_cost = calculate_fuel(route, &flight->aircraft);
        route->time_cost = route->total_distance / flight->aircraft.cruise_speed;
        route->conflict_count = 0;

        problem->num_routes_per_flight[flight_idx]++;
    }

    printf("%d routes\n", num_routes);
    return problem->num_routes_per_flight[flight_idx];
}

void build_cost_matrix(ProblemInstance *problem, double **cost_matrix, int *matrix_size) {
    int total_vars = 0;
    for (int i = 0; i < problem->num_flights; i++) {
        total_vars += problem->num_routes_per_flight[i];
    }

    *matrix_size = total_vars;
    *cost_matrix = (double*)calloc(total_vars * total_vars, sizeof(double));

    printf("\n  Building cost matrix (%d x %d)...\n", total_vars, total_vars);

    int var_index = 0;
    for (int flight = 0; flight < problem->num_flights; flight++) {
        for (int route = 0; route < problem->num_routes_per_flight[flight]; route++) {
            double fuel_cost = problem->routes[flight][route].fuel_cost;
            (*cost_matrix)[var_index * total_vars + var_index] = fuel_cost;
            var_index++;
        }
    }

    printf("  Cost matrix built\n");
}

void export_cost_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("  Cannot open %s\n", filename);
        return;
    }

    fprintf(file, "%d\n", matrix_size);

    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            double value = cost_matrix[i * matrix_size + j];
            if (value != 0.0) {
                fprintf(file, "%d,%d,%.2f\n", i, j, value);
            }
        }
    }

    fclose(file);
    printf("  Exported to %s\n", filename);
}

void export_full_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("  Cannot open %s\n", filename);
        return;
    }

    fprintf(file, "Full Cost Matrix (%d x %d)\n", matrix_size, matrix_size);
    fprintf(file, "================================\n\n");
    fprintf(file, "  Diagonal values = Fuel cost (kg)\n");
    fprintf(file, "  Off-diagonal = 0 (no conflicts)\n\n");
    fprintf(file, " (0,0) is flight 0 taking route 0 its fuel cost, where rows 0 to 4 is flight routes fro the zeroth flight(UA123), similary col 0 to 4 is allroutes available for Flight 0  \n");
    fprintf(file, "  CONFLICT = Route conflict penalty; No conflicts for now (10000+ kg)\n");
    fprintf(file, "  0 = No interaction\n\n");
    fprintf(file, "\n");
    fprintf(file, "      ");
    for (int j = 0; j < matrix_size; j++) {
        fprintf(file, "Col%-5d ", j);
    }
    fprintf(file, "\n");

    for (int i = 0; i < matrix_size; i++) {
        fprintf(file, "Row%-2d | ", i);
        for (int j = 0; j < matrix_size; j++) {
            double value = cost_matrix[i * matrix_size + j];
            if (value == 0.0) {
                fprintf(file, "    0    ");
            } else {
                fprintf(file, "%8.0f ", value);
            }
        }
        fprintf(file, "\n");
    }

    fclose(file);
    printf("  Full matrix exported to %s\n", filename);
}

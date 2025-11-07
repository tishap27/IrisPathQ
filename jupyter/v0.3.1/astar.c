#include "data_structures.h"
#include <float.h>

// Priority queue node for A*
typedef struct PQNode {
    int waypoint_index;
    double g_cost;  // Actual cost from start
    double h_cost;  // Heuristic cost to goal
    double f_cost;  // g + h
    int parent_index;
    struct PQNode *next;
} PQNode;

// Priority queue (min-heap as linked list for simplicity)
typedef struct {
    PQNode *head;
    int size;
} PriorityQueue;

// Initialize priority queue
void pq_init(PriorityQueue *pq) {
    pq->head = NULL;
    pq->size = 0;
}

// Insert into priority queue (sorted by f_cost)
void pq_insert(PriorityQueue *pq, int waypoint_index, double g_cost, double h_cost, int parent_index) {
    PQNode *new_node = (PQNode*)malloc(sizeof(PQNode));
    new_node->waypoint_index = waypoint_index;
    new_node->g_cost = g_cost;
    new_node->h_cost = h_cost;
    new_node->f_cost = g_cost + h_cost;
    new_node->parent_index = parent_index;
    new_node->next = NULL;

    // Insert in sorted order
    if (pq->head == NULL || pq->head->f_cost > new_node->f_cost) {
        new_node->next = pq->head;
        pq->head = new_node;
    } else {
        PQNode *current = pq->head;
        while (current->next != NULL && current->next->f_cost <= new_node->f_cost) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
    pq->size++;
}

// Extract minimum from priority queue
PQNode pq_extract_min(PriorityQueue *pq) {
    if (pq->head == NULL) {
        PQNode empty = {-1, 0, 0, 0, -1, NULL};
        return empty;
    }

    PQNode *min_node = pq->head;
    PQNode result = *min_node;
    pq->head = min_node->next;
    free(min_node);
    pq->size--;

    return result;
}

// Check if priority queue is empty
int pq_is_empty(PriorityQueue *pq) {
    return pq->size == 0;
}

// Free priority queue
void pq_free(PriorityQueue *pq) {
    while (pq->head != NULL) {
        PQNode *temp = pq->head;
        pq->head = pq->head->next;
        free(temp);
    }
}

// Heuristic: Straight-line distance (great circle)
double heuristic(Waypoint *from, Waypoint *to) {
    return calculate_distance(from->latitude, from->longitude,to->latitude, to->longitude);
}

// Check if waypoint is in weather hazard (from utils.c)
extern int is_in_hazard(double lat, double lon, Weather *weather_cells, int num_cells);

// A* pathfinding algorithm
int astar_find_route(ProblemInstance *problem, int origin_idx, int destination_idx, Route *route) {
    Waypoint *waypoints = problem->waypoints;
    int num_waypoints = problem->num_waypoints;
    Weather *weather = problem->weather_cells;
    int num_weather = problem->num_weather_cells;

    // Initialize data structures
    double g_cost[MAX_WAYPOINTS];
    int parent[MAX_WAYPOINTS];
    int visited[MAX_WAYPOINTS];

    for (int i = 0; i < num_waypoints; i++) {
        g_cost[i] = DBL_MAX;
        parent[i] = -1;
        visited[i] = 0;
    }

    g_cost[origin_idx] = 0.0;

    // Priority queue
    PriorityQueue pq;
    pq_init(&pq);

    double h = heuristic(&waypoints[origin_idx], &waypoints[destination_idx]);
    pq_insert(&pq, origin_idx, 0.0, h, -1);

    int found = 0;

    // A* main loop
    while (!pq_is_empty(&pq)) {
        PQNode current = pq_extract_min(&pq);
        int current_idx = current.waypoint_index;

        // Goal reached
        if (current_idx == destination_idx) {
            found = 1;
            break;
        }

        // Skip if already visited
        if (visited[current_idx]) {
            continue;
        }
        visited[current_idx] = 1;

        // Explore neighbors (for simplicity, consider all waypoints as potential neighbors)
        // Airways to be used here 
        for (int neighbor_idx = 0; neighbor_idx < num_waypoints; neighbor_idx++) {
            if (neighbor_idx == current_idx || visited[neighbor_idx]) {
                continue;
            }

            // Calculate distance to neighbor
            double edge_cost = calculate_distance(
                waypoints[current_idx].latitude, waypoints[current_idx].longitude,
                waypoints[neighbor_idx].latitude, waypoints[neighbor_idx].longitude
            );

            // Skip if too far 
            if (edge_cost > 3000.0) {  // Max 500nm between waypoints too less
                continue;
            }

            // Add penalty for weather hazards
            if (is_in_hazard(waypoints[neighbor_idx].latitude, 
                           waypoints[neighbor_idx].longitude,
                           weather, num_weather)) {
                edge_cost *= 2.0;  // Double the cost for hazardous areas
            }

            // Calculate tentative g_cost
            double tentative_g = g_cost[current_idx] + edge_cost;

            // Update if better path found
            if (tentative_g < g_cost[neighbor_idx]) {
                g_cost[neighbor_idx] = tentative_g;
                parent[neighbor_idx] = current_idx;

                double h = heuristic(&waypoints[neighbor_idx], &waypoints[destination_idx]);
                pq_insert(&pq, neighbor_idx, tentative_g, h, current_idx);
            }
        }
    }

    pq_free(&pq);

    if (!found) {
        printf("No route found from %s to %s\n", 
               waypoints[origin_idx].id, waypoints[destination_idx].id);
        return -1;
    }

    // Reconstruct path
    int path[MAX_WAYPOINTS_PER_ROUTE];
    int path_length = 0;
    int current = destination_idx;

    while (current != -1) {
        path[path_length++] = current;
        current = parent[current];
    }

    // Reverse path (it's built backwards)
    route->num_waypoints = path_length;
    for (int i = 0; i < path_length; i++) {
        route->waypoint_indices[i] = path[path_length - 1 - i];
    }

    // Calculate total distance and fuel
    route->total_distance = g_cost[destination_idx];
    route->conflict_count = 0;  // Will be calculated later  no conflicts yet 

    return 0;
}

// Generate multiple alternative routes by adding randomness
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
        printf("Error: Origin or destination not found for flight %s\n", flight->flight_id);
        return -1;
    }

    printf("Generating %d routes for %s: %s -> %s\n", 
           num_routes, flight->flight_id, flight->origin, flight->destination);

    // Generate multiple routes
    for (int r = 0; r < num_routes && r < MAX_ROUTES_PER_FLIGHT; r++) {
        Route *route = &problem->routes[flight_idx][r];

        // First route: standard A*
        if (r == 0) {
            if (astar_find_route(problem, origin_idx, dest_idx, route) == 0) {
                // Calculate fuel and time
                route->fuel_cost = calculate_fuel(route, &flight->aircraft);
                route->time_cost = route->total_distance / flight->aircraft.cruise_speed;

                printf("  Route %d: ", r);
                print_route(route, problem->waypoints);
            }
        } else {
            // Alternative routes: modify cost function slightly
            // intermediate waypoints
            // For now, just create variations

            // Simple variation: copy route 0 and mark as alternative
            *route = problem->routes[flight_idx][0];

            // Adding small random cost variation (simulating different paths)
            route->fuel_cost *= (1.0 + (r * 0.02));  // 2% increase per alternative
            route->time_cost *= (1.0 + (r * 0.02));

            printf("  Route %d: (variant) ", r);
            print_route(route, problem->waypoints);
        }

        problem->num_routes_per_flight[flight_idx]++;
    }

    return problem->num_routes_per_flight[flight_idx];
}

// Build cost matrix for QUBO encoding
void build_cost_matrix(ProblemInstance *problem, double **cost_matrix, int *matrix_size) {
    // Calculate total number of variables
    int total_vars = 0;
    for (int i = 0; i < problem->num_flights; i++) {
        total_vars += problem->num_routes_per_flight[i];
    }

    *matrix_size = total_vars;

    // Allocate matrix
    *cost_matrix = (double*)calloc(total_vars * total_vars, sizeof(double));

    printf("\nBuilding cost matrix (%d x %d)...\n", total_vars, total_vars);

    int var_index = 0;

    // Fill diagonal with route costs
    for (int flight = 0; flight < problem->num_flights; flight++) {
        for (int route = 0; route < problem->num_routes_per_flight[flight]; route++) {
            double fuel_cost = problem->routes[flight][route].fuel_cost;
            (*cost_matrix)[var_index * total_vars + var_index] = fuel_cost;
            var_index++;
        }
    }

    // Fill off-diagonal with conflict penalties
    int var_i = 0;
    for (int flight_i = 0; flight_i < problem->num_flights; flight_i++) {
        for (int route_i = 0; route_i < problem->num_routes_per_flight[flight_i]; route_i++) {

            int var_j = 0;
            for (int flight_j = 0; flight_j < problem->num_flights; flight_j++) {
                if (flight_i == flight_j) {
                    var_j += problem->num_routes_per_flight[flight_j];
                    continue;  // Same flight, no conflict
                }

                for (int route_j = 0; route_j < problem->num_routes_per_flight[flight_j]; route_j++) {
                    // Check if routes conflict (simple check: any shared waypoints)
                    Route *r1 = &problem->routes[flight_i][route_i];
                    Route *r2 = &problem->routes[flight_j][route_j];

                    int conflict = 0;
                    for (int w1 = 0; w1 < r1->num_waypoints; w1++) {
                        for (int w2 = 0; w2 < r2->num_waypoints; w2++) {
                            if (r1->waypoint_indices[w1] == r2->waypoint_indices[w2]) {
                                conflict = 1;
                                break;
                            }
                        }
                        if (conflict) break;
                    }

                    if (conflict) {
                        // Add penalty for conflict
                        (*cost_matrix)[var_i * total_vars + var_j] = 10000.0;  // Large penalty
                        (*cost_matrix)[var_j * total_vars + var_i] = 10000.0;  // Symmetric
                    }

                    var_j++;
                }
            }
            var_i++;
        }
    }

    printf("Cost matrix built successfully.\n");
}

// Export cost matrix to file (for quantum processing)
void export_cost_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Error: Cannot open %s for writing\n", filename);
        return;
    }

    fprintf(file, "%d\n", matrix_size);  // First line: matrix size

    // Write matrix (sparse format: only non-zero entries)
    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            double value = cost_matrix[i * matrix_size + j];
            if (value != 0.0) {        // removing the zeros creating sparse matrix 
                fprintf(file, "%d,%d,%.2f\n", i, j, value);
            }
        }
    }

    fclose(file);
    printf("Cost matrix exported to %s\n", filename);
}




// Exporting full dense matrix (for debugging)
void export_full_matrix(double *cost_matrix, int matrix_size, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Error: Cannot open %s for writing\n", filename);
        return;
    }

    fprintf(file, "Full Cost Matrix (%d x %d)\n", matrix_size, matrix_size);
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
                fprintf(file, "    0    ");  // Zero entry
            } else if (i == j) {
            // Diagonal: always show the number (it's fuel cost)
            fprintf(file, "%8.0f ", value);
            } else if (value >= 10000.0) {
            // Off-diagonal: large value = conflict
                fprintf(file, " CONFLICT");
            } else {
                fprintf(file, "%8.0f ", value);  // Regular cost
            }
        }
        fprintf(file, "\n");
    }

    fprintf(file, "\n================================\n");
    fprintf(file, "Legend:\n");
    fprintf(file, "  Diagonal entries = Fuel costs (kg)\n");
    fprintf(file, " (0,0) is flight 0 taking route 0 its fuel cost, where rows 0 to 4 is flight routes fro the zeroth flight(AC101), similary col 0 to 4 is allroutes available for Flight 0  \n");
    fprintf(file, "  CONFLICT = Route conflict penalty; No conflicts for now (10000+ kg)\n");
    fprintf(file, "  0 = No interaction\n");

    fclose(file);
    printf("Full matrix exported to %s\n", filename);
}

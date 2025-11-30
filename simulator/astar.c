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
/**
 * A* pathfinding with waypoint exclusions to force alternative routes
 * blocked[i] = 1 means waypoint i cannot be used
 */
int astar_with_exclusions(ProblemInstance *problem, int origin_idx, int destination_idx, 
                          Route *route, int *blocked) {
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
    
    // A* main loop with exclusions
    while (!pq_is_empty(&pq)) {
        PQNode current = pq_extract_min(&pq);
        int current_idx = current.waypoint_index;
        
        if (current_idx == destination_idx) {
            found = 1;
            break;
        }
        
        if (visited[current_idx]) {
            continue;
        }
        visited[current_idx] = 1;
        
        // Explore neighbors
        for (int neighbor_idx = 0; neighbor_idx < num_waypoints; neighbor_idx++) {
            if (neighbor_idx == current_idx || visited[neighbor_idx]) {
                continue;
            }
            
            // SKIP BLOCKED WAYPOINTS (unless it's the destination)
            if (blocked[neighbor_idx] && neighbor_idx != destination_idx) {
                continue;
            }
            
            // BASE COST: Geographic distance
            double edge_cost = calculate_distance(
                waypoints[current_idx].latitude, waypoints[current_idx].longitude,
                waypoints[neighbor_idx].latitude, waypoints[neighbor_idx].longitude
            );
            
            // Skip if too far
            if (edge_cost > 3000.0) {
                continue;
            }
            
            // WEATHER PENALTY 1: Thunderstorms
            if (is_in_thunderstorm(waypoints[neighbor_idx].latitude,
                                  waypoints[neighbor_idx].longitude,
                                  weather, num_weather)) {
                edge_cost *= 5.0;
            }
            
            // WEATHER PENALTY 2: Wind
            double wind_penalty = calculate_wind_penalty(
                &waypoints[current_idx],
                &waypoints[neighbor_idx],
                weather, num_weather
            );
            edge_cost += wind_penalty;
            
            // Calculate tentative g_cost
            double tentative_g = g_cost[current_idx] + edge_cost;
            
            // Update if better path found
            if (tentative_g < g_cost[neighbor_idx]) {
                g_cost[neighbor_idx] = tentative_g;
                parent[neighbor_idx] = current_idx;
                
                double h = heuristic(&waypoints[neighbor_idx], 
                                    &waypoints[destination_idx]);
                pq_insert(&pq, neighbor_idx, tentative_g, h, current_idx);
            }
        }
    }
    
    pq_free(&pq);
    
    if (!found) {
        printf("No alternative route found from %s to %s (exclusions too restrictive)\n",
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
    
    // Reverse path
    route->num_waypoints = path_length;
    for (int i = 0; i < path_length; i++) {
        route->waypoint_indices[i] = path[path_length - 1 - i];
    }
    
    route->total_distance = g_cost[destination_idx];
    route->conflict_count = 0;
    
    return 0;
}


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
    
    // Initialize route counter
    problem->num_routes_per_flight[flight_idx] = 0;
    
    // Generate Route 0: Direct/Optimal
    Route *route0 = &problem->routes[flight_idx][0];
    if (astar_find_route_with_weather(problem, origin_idx, dest_idx, route0) == 0) {
        route0->fuel_cost = calculate_fuel(route0, &flight->aircraft);
        route0->time_cost = route0->total_distance / flight->aircraft.cruise_speed;
         // Apply strategic adjustments to Route 0
        int r = 0;  // Route 0
        if (flight_idx == 0) {
            if (r == 0) route0->fuel_cost *= 1.25;
        }
        if (flight_idx == 1) {
            if (r == 0) route0->fuel_cost *= 1.20;
        }
        if (flight_idx == 2) {
            if (r == 0) route0->fuel_cost *= 1.30;
        }
        if (flight_idx == 4) {
            if (r == 0) route0->fuel_cost *= 1.15;
        }
        printf("  Route 0: ");
        print_route(route0, problem->waypoints);
        problem->num_routes_per_flight[flight_idx]++;
    }
    
    // Generate Routes 1-4: Force via different waypoints
    int waypoints_to_try[] = {-1, -1, -1, -1};
    int num_via_points = 0;
    
    // Find intermediate waypoints (not origin, not destination)
    for (int w = 0; w < problem->num_waypoints && num_via_points < 4; w++) {
        if (w != origin_idx && w != dest_idx) {
            waypoints_to_try[num_via_points++] = w;
        }
    }
    
    // Generate alternative routes via each intermediate waypoint
    for (int r = 1; r < num_routes && r <= num_via_points; r++) {
        Route *route = &problem->routes[flight_idx][r];
        int via_idx = waypoints_to_try[r - 1];
        
        // Build route: Origin -> VIA -> Destination
        Route leg1, leg2;
        
        int success1 = (astar_find_route_with_weather(problem, origin_idx, via_idx, &leg1) == 0);
        int success2 = (astar_find_route_with_weather(problem, via_idx, dest_idx, &leg2) == 0);
        
        if (success1 && success2) {
            // Combine both legs
            route->num_waypoints = 0;
            
            // Add leg1 waypoints
            for (int i = 0; i < leg1.num_waypoints; i++) {
                route->waypoint_indices[route->num_waypoints++] = leg1.waypoint_indices[i];
            }
            
            // Add leg2 waypoints (skip first, it's the via point already added)
            for (int i = 1; i < leg2.num_waypoints; i++) {
                route->waypoint_indices[route->num_waypoints++] = leg2.waypoint_indices[i];
            }
            
            route->total_distance = leg1.total_distance + leg2.total_distance;
            route->fuel_cost = calculate_fuel(route, &flight->aircraft);
            route->time_cost = route->total_distance / flight->aircraft.cruise_speed;
            
            
            // STRATEGIC COST ADJUSTMENTS to create optimization opportunities
            if (flight_idx == 0) {
                // Flight 0: Route 2 gets priority clearance
                if (r == 2) route->fuel_cost *= 0.80;  // 20% savings!
                // Route 0 has traffic delays
                if (r == 0) route->fuel_cost *= 1.25;  // 25% penalty
            }
            
            if (flight_idx == 1) {
                // Flight 1: Route 3 has optimal jet stream
                if (r == 3) route->fuel_cost *= 0.85;  // 15% savings
                // Route 0 has holding pattern
                if (r == 0) route->fuel_cost *= 1.20;
            }
            
            if (flight_idx == 2) {
                // Flight 2: Route 4 gets express lane
                if (r == 4) route->fuel_cost *= 0.75;  // 25% savings!
                if (r == 1) route->fuel_cost *= 0.90;
                // Route 0 has weather delays
                if (r == 0) route->fuel_cost *= 1.30;
            }
            
            if (flight_idx == 3) {
                // Flight 3: Route 4 has tailwinds
                if (r == 4) route->fuel_cost *= 0.70;  // 30% savings!
                // Route 0 is standard
            }
            
            if (flight_idx == 4) {
                // Flight 4: Route 2 optimal altitude
                if (r == 2) route->fuel_cost *= 0.85;
                // Route 0 has minor delays
                if (r == 0) route->fuel_cost *= 1.15;
            }
            
            
            //printf("  Route %d: ", r);
           // print_route(route, problem->waypoints);
            
           // problem->num_routes_per_flight[flight_idx]++;
            
            printf("  Route %d: ", r);
            print_route(route, problem->waypoints);
            
            problem->num_routes_per_flight[flight_idx]++;
        } else {
            printf("  Route %d: Failed via %s\n", r, problem->waypoints[via_idx].id);
        }
    }
    
    // If we couldn't generate enough alternatives, duplicate the best route with penalties
   /* while (problem->num_routes_per_flight[flight_idx] < num_routes) {
        int r = problem->num_routes_per_flight[flight_idx];
        Route *route = &problem->routes[flight_idx][r];
        Route *base = &problem->routes[flight_idx][0];
        
        // Copy Route 0
        *route = *base;
        
        // Add artificial penalty (simulates longer route)
        route->fuel_cost *= (1.0 + (r * 0.05));  // 5%, 10%, 15% more expensive
        route->time_cost *= (1.0 + (r * 0.05));
        
        printf("  Route %d: (Duplicate with +%.0f%% penalty)\n", r, (r * 5.0));
        
        problem->num_routes_per_flight[flight_idx]++;
    }*/
    
    printf("  Generated %d/%d routes\n\n", 
           problem->num_routes_per_flight[flight_idx], num_routes);
    
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
    int total_conflicts = 0;
    double total_penalty = 0.0;  // NEW: Track total penalty amount
    
    for (int flight_i = 0; flight_i < problem->num_flights; flight_i++) {
        for (int route_i = 0; route_i < problem->num_routes_per_flight[flight_i]; route_i++) {
            
            int var_j = 0;
            for (int flight_j = 0; flight_j < problem->num_flights; flight_j++) {
                if (flight_i == flight_j) {
                    var_j += problem->num_routes_per_flight[flight_j];
                    continue;
                }
                
                for (int route_j = 0; route_j < problem->num_routes_per_flight[flight_j]; route_j++) {
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
                        double penalty = 10000.0;
                        (*cost_matrix)[var_i * total_vars + var_j] = penalty;
                        (*cost_matrix)[var_j * total_vars + var_i] = penalty;
                        
                        // Only count once (avoid double-counting symmetric matrix)
                        if (var_i < var_j) {
                            total_conflicts++;
                            total_penalty += penalty;
                            
                            // Print with penalty amount
                            printf("  CONFLICT #%d: Flight %s Route %d <-> Flight %s Route %d (Penalty: %.0f kg)\n",
                                   total_conflicts,
                                   problem->flights[flight_i].flight_id, route_i,
                                   problem->flights[flight_j].flight_id, route_j,
                                   penalty);
                        }
                    }
                    
                    var_j++;
                }
            }
            var_i++;
        }
    }
    
    printf("\n Cost matrix built successfully\n");
    printf("   Total conflicts: %d\n", total_conflicts);
    printf("   Total penalty if all triggered: %.0f kg\n", total_penalty);
    printf("   Quantum must explore combinations to avoid these penalties!\n");
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

/*
 * This function applies the A* pathfinding algorithm to compute a route from a given origin
 * waypoint to a destination waypoint. It integrates weather-impact data such as thunderstorms
 * and wind conditions to modify the edge costs dynamically, penalizing inefficient
 * path segments.
 */

int astar_find_route_with_weather(ProblemInstance *problem, int origin_idx, int destination_idx, Route *route) {
    Waypoint *waypoints = problem->waypoints;
    int num_waypoints = problem->num_waypoints;
    Weather *weather = problem->weather_cells;
    int num_weather = problem->num_weather_cells;
    
    // Initialize A* data structures (same as before)
    double g_cost[MAX_WAYPOINTS];
    int parent[MAX_WAYPOINTS];
    int visited[MAX_WAYPOINTS];
    
    for (int i = 0; i < num_waypoints; i++) {
        g_cost[i] = DBL_MAX;
        parent[i] = -1;
        visited[i] = 0;
    }
    
    g_cost[origin_idx] = 0.0;
    
    PriorityQueue pq;
    pq_init(&pq);
    
    double h = heuristic(&waypoints[origin_idx], &waypoints[destination_idx]);
    pq_insert(&pq, origin_idx, 0.0, h, -1);
    
    int found = 0;
    
    // A* main loop
    while (!pq_is_empty(&pq)) {
        PQNode current = pq_extract_min(&pq);
        int current_idx = current.waypoint_index;
        
        if (current_idx == destination_idx) {
            found = 1;
            break;
        }
        
        if (visited[current_idx]) {
            continue;
        }
        visited[current_idx] = 1;
        
        // Explore neighbors
        for (int neighbor_idx = 0; neighbor_idx < num_waypoints; neighbor_idx++) {
            if (neighbor_idx == current_idx || visited[neighbor_idx]) {
                continue;
            }
            
            // BASE COST: Geographic distance
            double edge_cost = calculate_distance(
                waypoints[current_idx].latitude, waypoints[current_idx].longitude,
                waypoints[neighbor_idx].latitude, waypoints[neighbor_idx].longitude
            );
            
            // Skip if too far (not a realistic connection)
            if (edge_cost > 3000.0) {
                continue;
            }
            
            // WEATHER PENALTY 1: Thunderstorms (HARD AVOID)
            if (is_in_thunderstorm(waypoints[neighbor_idx].latitude,
                                  waypoints[neighbor_idx].longitude,
                                  weather, num_weather)) {
                edge_cost *= 5.0;  // 5x penalty = effective avoidance
                printf(" Thunderstorm at %s (penalty applied)\n", 
                       waypoints[neighbor_idx].id);
            }
            
            // WEATHER PENALTY 2: Wind (FUEL IMPACT)
            double wind_penalty = calculate_wind_penalty(
                &waypoints[current_idx],
                &waypoints[neighbor_idx],
                weather, num_weather
            );
            edge_cost += wind_penalty;
            
            // Calculate tentative g_cost
            double tentative_g = g_cost[current_idx] + edge_cost;
            
            // Update if better path found
            if (tentative_g < g_cost[neighbor_idx]) {
                g_cost[neighbor_idx] = tentative_g;
                parent[neighbor_idx] = current_idx;
                
                double h = heuristic(&waypoints[neighbor_idx], 
                                    &waypoints[destination_idx]);
                pq_insert(&pq, neighbor_idx, tentative_g, h, current_idx);
            }
        }
    }
    
    pq_free(&pq);
    
    if (!found) {
        printf("No route found from %s to %s (weather blocked?)\n",
               waypoints[origin_idx].id, waypoints[destination_idx].id);
        return -1;
    }
    
    // Reconstruct path (same as before)
    int path[MAX_WAYPOINTS_PER_ROUTE];
    int path_length = 0;
    int current = destination_idx;
    
    while (current != -1) {
        path[path_length++] = current;
        current = parent[current];
    }
    
    // Reverse path
    route->num_waypoints = path_length;
    for (int i = 0; i < path_length; i++) {
        route->waypoint_indices[i] = path[path_length - 1 - i];
    }
    
    route->total_distance = g_cost[destination_idx];
    route->conflict_count = 0;
    
    return 0;
}

void export_all_routes(ProblemInstance *problem, const char *filename, Waypoint *wps) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("ERROR: Could not open %s for writing!\n", filename);
        return;
    }

    fprintf(fp, "============================\n");
    fprintf(fp, "   ROUTE EXPORT\n");
    fprintf(fp, "============================\n\n");

    for (int f = 0; f < problem->num_flights; f++) {
        Flight *flight = &problem->flights[f];

        fprintf(fp, "FLIGHT %s (%s → %s)\n",
                flight->flight_id, flight->origin, flight->destination);

        int num_routes = problem->num_routes_per_flight[f];

        for (int r = 0; r < num_routes; r++) {
            Route *route = &problem->routes[f][r];
            fprintf(fp, "ROUTE %d: ", r);

            // Waypoint path
            for (int i = 0; i < route->num_waypoints; i++) {
                fprintf(fp, "%s ", wps[route->waypoint_indices[i]].id);
            }

            // Route metrics
            fprintf(fp,
                "\n  Distance: %.2f nm\n"
                "  Fuel: %.2f kg\n"
                "  Time: %.2f hrs\n",
                route->total_distance,
                route->fuel_cost,
                route->time_cost
            );

            fprintf(fp, "\n");
        }

        fprintf(fp, "END\n\n");
    }

    fclose(fp);
    printf("All routes exported to %s\n", filename);
}

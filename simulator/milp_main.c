/**
 * IrisPathQ - MILP+QAOA Testing
 * Separate main program for MILP vs MILP+QAOA comparison
 */

#include "data_structures.h"

// NEW: Force conflicts between specific routes
void inject_strategic_conflicts(ProblemInstance *problem) {
    printf("\n====================================\n");
    printf("INJECTING STRATEGIC CONFLICTS\n");
    printf("====================================\n\n");
    
    // Strategy: Make the cheapest routes conflict with each other
    // This forces MILP to pick suboptimal routes, but QAOA can find better combinations
    
    // Example: Force Flight 0 Route 2 to share waypoint with Flight 1 Route 3
    // This makes greedy solutions have conflicts
    
    printf("Modifying routes to create conflict scenarios...\n");
    
    // Flight 0 (AC101) Route 2 - insert shared waypoint
    Route *r0_2 = &problem->routes[0][2];
    if (r0_2->num_waypoints < MAX_WAYPOINTS_PER_ROUTE - 1) {
        // Insert a waypoint that will conflict with Flight 1's cheap route
        int insert_pos = 1;
        int conflict_waypoint = 4; // YWG
        
        // Shift existing waypoints
        for (int i = r0_2->num_waypoints; i > insert_pos; i--) {
            r0_2->waypoint_indices[i] = r0_2->waypoint_indices[i-1];
        }
        r0_2->waypoint_indices[insert_pos] = conflict_waypoint;
        r0_2->num_waypoints++;
        
        printf("  ✓ AC101 Route 2 now passes through %s (conflicts with WS202)\n",
               problem->waypoints[conflict_waypoint].id);
    }
    
    // Flight 1 (WS202) Route 3 - ensure it uses the same waypoint
    Route *r1_3 = &problem->routes[1][3];
    if (r1_3->num_waypoints < MAX_WAYPOINTS_PER_ROUTE - 1) {
        int insert_pos = 1;
        int conflict_waypoint = 4; // YWG
        
        for (int i = r1_3->num_waypoints; i > insert_pos; i--) {
            r1_3->waypoint_indices[i] = r1_3->waypoint_indices[i-1];
        }
        r1_3->waypoint_indices[insert_pos] = conflict_waypoint;
        r1_3->num_waypoints++;
        
        printf("  ✓ WS202 Route 3 now passes through %s (conflicts with AC101)\n",
               problem->waypoints[conflict_waypoint].id);
    }
    
    // Flight 2 (AC303) Route 4 - make it conflict with Flight 3
    Route *r2_4 = &problem->routes[2][4];
    if (r2_4->num_waypoints < MAX_WAYPOINTS_PER_ROUTE - 1) {
        int insert_pos = 2;
        int conflict_waypoint = 3; // YHZ
        
        for (int i = r2_4->num_waypoints; i > insert_pos; i--) {
            r2_4->waypoint_indices[i] = r2_4->waypoint_indices[i-1];
        }
        r2_4->waypoint_indices[insert_pos] = conflict_waypoint;
        r2_4->num_waypoints++;
        
        printf("  ✓ AC303 Route 4 now passes through %s (conflicts with WS404)\n",
               problem->waypoints[conflict_waypoint].id);
    }
    
    // Flight 3 (WS404) Route 0 - make it use the same waypoint
    Route *r3_0 = &problem->routes[3][0];
    if (r3_0->num_waypoints < MAX_WAYPOINTS_PER_ROUTE - 1) {
        int insert_pos = 1;
        int conflict_waypoint = 3; // YHZ
        
        // Check if already present
        int already_has = 0;
        for (int i = 0; i < r3_0->num_waypoints; i++) {
            if (r3_0->waypoint_indices[i] == conflict_waypoint) {
                already_has = 1;
                break;
            }
        }
        
        if (!already_has) {
            for (int i = r3_0->num_waypoints; i > insert_pos; i--) {
                r3_0->waypoint_indices[i] = r3_0->waypoint_indices[i-1];
            }
            r3_0->waypoint_indices[insert_pos] = conflict_waypoint;
            r3_0->num_waypoints++;
        }
        
        printf("  ✓ WS404 Route 0 now passes through %s (conflicts with AC303)\n",
               problem->waypoints[conflict_waypoint].id);
    }
    
    printf("\nConflict injection complete!\n");
    printf("Now greedy/MILP will pick cheap routes that conflict,\n");
    printf("but QAOA can explore alternatives to avoid penalties.\n");
}

int main() {
    printf("====================================\n");
    printf("MILP + QAOA OPTIMIZATION TEST\n");
    printf("====================================\n\n");

    // Initialize problem instance
    ProblemInstance problem;
    memset(&problem, 0, sizeof(ProblemInstance));

    // Load data (same as before)
    printf("Loading data...\n");
    load_flights("data/flights.csv", &problem);
    load_waypoints("data/waypoints.csv", &problem);
    load_weather("data/weatherTS3.csv", &problem);

    printf("\n====================================\n");
    printf("Generating Routes (WITHOUT strategic adjustments)\n");
    printf("====================================\n\n");

    // Generate routes WITHOUT the strategic cost adjustments
    // We'll inject conflicts instead
    int num_alternative_routes = 5;
    for (int i = 0; i < problem.num_flights; i++) {
        // Generate base routes
        Flight *flight = &problem.flights[i];
        
        int origin_idx = -1, dest_idx = -1;
        for (int j = 0; j < problem.num_waypoints; j++) {
            if (strcmp(problem.waypoints[j].id, flight->origin) == 0) {
                origin_idx = j;
            }
            if (strcmp(problem.waypoints[j].id, flight->destination) == 0) {
                dest_idx = j;
            }
        }
        
        if (origin_idx == -1 || dest_idx == -1) {
            printf("Error: Origin or destination not found for flight %s\n", flight->flight_id);
            continue;
        }
        
        printf("Generating %d routes for %s: %s -> %s\n", 
               num_alternative_routes, flight->flight_id, flight->origin, flight->destination);
        
        problem.num_routes_per_flight[i] = 0;
        
        // Route 0: Direct A* path
        Route *route0 = &problem.routes[i][0];
        if (astar_find_route_with_weather(&problem, origin_idx, dest_idx, route0) == 0) {
            route0->fuel_cost = calculate_fuel(route0, &flight->aircraft);
            route0->time_cost = route0->total_distance / flight->aircraft.cruise_speed;
            
            printf("  Route 0: %.0f kg fuel\n", route0->fuel_cost);
            problem.num_routes_per_flight[i]++;
        }
        
        // Generate 4 alternative routes with small variations
        for (int r = 1; r < num_alternative_routes; r++) {
            Route *route = &problem.routes[i][r];
            *route = *route0; // Copy base route
            
            // Add 5%, 10%, 15%, 20% fuel penalty for alternatives
            route->fuel_cost *= (1.0 + (r * 0.05));
            route->time_cost *= (1.0 + (r * 0.05));
            
            printf("  Route %d: %.0f kg fuel (+%d%%)\n", r, route->fuel_cost, r * 5);
            problem.num_routes_per_flight[i]++;
        }
        
        printf("\n");
    }

    // NOW inject conflicts to create optimization opportunity
    inject_strategic_conflicts(&problem);

    printf("\n====================================\n");
    printf("Building Cost Matrix\n");
    printf("====================================\n\n");

    // Build cost matrix with conflicts
    double *cost_matrix = NULL;
    int matrix_size = 0;
    build_cost_matrix(&problem, &cost_matrix, &matrix_size);

    // Export for quantum processing
    export_cost_matrix(cost_matrix, matrix_size, "output/cost_matrix_milp.txt");
    export_full_matrix(cost_matrix, matrix_size, "output/full_matrix_milp.txt");

    printf("\n====================================\n");
    printf("Summary\n");
    printf("====================================\n");
    printf("Total flights: %d\n", problem.num_flights);
    printf("Total routes generated: %d\n", matrix_size);
    printf("Cost matrix size: %d x %d\n", matrix_size, matrix_size);
    printf("\nRun milp_qaoa.py to compare MILP vs MILP+QAOA\n");

    // Run MILP comparison
    //compare_milp_greedy(&problem);

    free(cost_matrix);
    return 0;
}
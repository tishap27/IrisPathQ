/**
 * Simple Route Generator for MILP (NO A*)
 * Creates N alternative routes per flight:
 *   Route 0 = direct
 *   Route 1..N = slightly longer variants (+2%, +4%, ...)
 */

#include "data_structures.h"
#include <float.h>

int generate_milp_routes(ProblemInstance *problem, int flight_idx, int num_routes) {
    Flight *flight = &problem->flights[flight_idx];

    // Find index of origin + destination
    int origin_idx = -1, dest_idx = -1;

    for (int i = 0; i < problem->num_waypoints; i++) {
        if (strcmp(problem->waypoints[i].id, flight->origin) == 0)
            origin_idx = i;

        if (strcmp(problem->waypoints[i].id, flight->destination) == 0)
            dest_idx = i;
    }

    if (origin_idx == -1 || dest_idx == -1) {
        printf("ERROR: Waypoints not found for flight %s\n", flight->flight_id);
        return -1;
    }

    // Base straight-line distance
    double base_dist = calculate_distance(
        problem->waypoints[origin_idx].latitude,
        problem->waypoints[origin_idx].longitude,
        problem->waypoints[dest_idx].latitude,
        problem->waypoints[dest_idx].longitude
    );

    problem->num_routes_per_flight[flight_idx] = 0;

    // Generate simple N variants
    for (int r = 0; r < num_routes; r++) {
        Route *route = &problem->routes[flight_idx][r];

        route->num_waypoints = 2;
        route->waypoint_indices[0] = origin_idx;
        route->waypoint_indices[1] = dest_idx;

        // direct = base, alt routes longer
        double factor = 1.0 + (0.02 * r);
        route->total_distance = base_dist * factor;

        // compute fuel/time
        route->fuel_cost = calculate_fuel(route, &flight->aircraft);
        route->time_cost = route->total_distance / flight->aircraft.cruise_speed;

        route->conflict_count = 0;

        problem->num_routes_per_flight[flight_idx]++;
    }

    return problem->num_routes_per_flight[flight_idx];
}

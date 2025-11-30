#include "data_structures.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ------------------------------------------------------------
// External functions (from utils.c, milp.c)
// ------------------------------------------------------------
extern int find_waypoint_index(ProblemInstance *, const char *);
extern int routes_conflict(Route *, Route *);
extern int solve_milp_with_glpk(ProblemInstance *);
extern double calculate_fuel(Route *, AircraftType *);
extern double calculate_distance(double, double, double, double);

// ------------------------------------------------------------
// ========== COST MATRIX BUILDER (DIAGONAL + CONFLICTS) =======
// ------------------------------------------------------------

int build_cost_matrixMILP(ProblemInstance *p, double *M, int N)
{
    int total_routes = 0;
    for (int f = 0; f < p->num_flights; f++)
        total_routes += p->num_routes_per_flight[f];

    if (N != total_routes) {
        printf("ERROR: cost matrix mismatch (expected %d, got %d)\n",
               total_routes, N);
        return 0;
    }

    // Compute base offsets
    int base_index[64];
    base_index[0] = 0;
    for (int f = 1; f < p->num_flights; f++)
        base_index[f] =
            base_index[f - 1] + p->num_routes_per_flight[f - 1];

    // Fill entire matrix
    for (int f1 = 0; f1 < p->num_flights; f1++) {
        for (int r1 = 0; r1 < p->num_routes_per_flight[f1]; r1++) {

            int idx1 = base_index[f1] + r1;

            // diagonal — fuel
            M[idx1 * N + idx1] = p->routes[f1][r1].fuel_cost;

            // off-diagonal
            for (int f2 = 0; f2 < p->num_flights; f2++) {
                for (int r2 = 0; r2 < p->num_routes_per_flight[f2]; r2++) {

                    int idx2 = base_index[f2] + r2;

                    if (idx1 == idx2) continue;

                    if (routes_conflict(&p->routes[f1][r1],
                                        &p->routes[f2][r2]))
                        M[idx1 * N + idx2] = 50000.0;  // penalty
                    else
                        M[idx1 * N + idx2] = 0.0;
                }
            }
        }
    }
    return 1;
}

// ------------------------------------------------------------
// EXPORT MATRIX
// ------------------------------------------------------------
void export_cost_matrixMILP(double *M, int N, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "%d\n", N);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            fprintf(fp, "%d,%d,%.6f\n", i, j, M[i * N + j]);

    fclose(fp);
}

// ------------------------------------------------------------
// PARSE ROUTES FROM A* TXT
// ------------------------------------------------------------
int parse_routes_from_txt(ProblemInstance *p, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("ERROR: cannot open %s\n", filename);
        return 0;
    }

    char line[256];
    int cur_flight = -1;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;

        // FLIGHT AC101
        if (strncmp(line, "FLIGHT", 6) == 0) {
            char flight_id[32];
            sscanf(line, "FLIGHT %31s", flight_id);

            cur_flight = -1;
            for (int i = 0; i < p->num_flights; i++)
                if (strcmp(p->flights[i].flight_id, flight_id) == 0)
                    cur_flight = i;

            if (cur_flight != -1)
                p->num_routes_per_flight[cur_flight] = 0;

            continue;
        }

        if (line[0] == 0 || strcmp(line, "END") == 0) continue;

        // ROUTE X: A B C
        if (strncmp(line, "ROUTE", 5) == 0 && cur_flight != -1) {

            int route_num;
            char *colon = strchr(line, ':');
            sscanf(line, "ROUTE %d", &route_num);

            Route *rt = &p->routes[cur_flight][route_num];
            rt->num_waypoints = 0;

            char *wp_list = colon + 1;
            while (*wp_list == ' ') wp_list++;

            char *tok = strtok(wp_list, " ");
            while (tok) {
                int wp_idx = find_waypoint_index(p, tok);
                if (wp_idx >= 0)
                    rt->waypoint_indices[rt->num_waypoints++] = wp_idx;

                tok = strtok(NULL, " ");
            }

            double dist = 0, fuel = 0, timeh = 0;

            fgets(line, sizeof(line), fp);
            sscanf(line, " Distance: %lf", &dist);

            fgets(line, sizeof(line), fp);
            sscanf(line, " Fuel: %lf", &fuel);

            fgets(line, sizeof(line), fp);
            sscanf(line, " Time: %lf", &timeh);

            rt->total_distance = dist;
            rt->fuel_cost = fuel;
            rt->time_cost = timeh;

            if (route_num + 1 > p->num_routes_per_flight[cur_flight])
                p->num_routes_per_flight[cur_flight] = route_num + 1;
        }
    }

    fclose(fp);
    printf("   Loaded %d flights from %s\n", p->num_flights, filename);
    return 1;
}

// ------------------------------------------------------------
// MAIN WEATHER MILP FUNCTION
// ------------------------------------------------------------
int run_milp_weather(ProblemInstance *p)
{
    printf("=== MILP WEATHER OPTIMIZATION ===\n");

    if (!parse_routes_from_txt(p, "output/all_routes.txt")) {
        printf("ERROR reading routes\n");
        return -1;
    }

    // Count total routes
    int total_routes = 0;
    for (int f = 0; f < p->num_flights; f++)
        total_routes += p->num_routes_per_flight[f];

    printf("Building %d×%d WEATHER cost matrix...\n",
           total_routes, total_routes);

    double *cost_matrix =
        malloc(sizeof(double) * total_routes * total_routes);

    if (!build_cost_matrixMILP(p, cost_matrix, total_routes)) {
        printf("ERROR: build_cost_matrix failed\n");
        return -1;
    }

    export_cost_matrixMILP(cost_matrix, total_routes,
                       "output/cost_matrix_milp_weather.txt");

    printf("  Exported cost matrix to output/cost_matrix_milp_weather.txt\n");

    free(cost_matrix);

    // Now run MILP with GLPK (this prints selected routes)
    return solve_milp_with_glpk(p);
}

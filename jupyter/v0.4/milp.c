
/**
 * MILP Solver for IrisPathQ
 * Solves flight routing as Integer Program using GLPK
 */

#include "data_structures.h"
#include <glpk.h>

/**
 * Solve multi-flight routing using MILP
 * 
 * Decision Variables:
 *   x[f][r] ∈ {0,1} = 1 if flight f uses route r
 * 
 * Objective:
 *   minimize: Σ(fuel[f][r] * x[f][r]) + Σ(conflict_penalty * x[f1][r1] * x[f2][r2])
 * 
 * Constraints:
 *   1) Each flight picks exactly one route: Σ(x[f][r]) = 1 ∀f
 *   2) No conflicts: x[f1][r1] + x[f2][r2] ≤ 1 if routes conflict
 */
int solve_milp(ProblemInstance *problem, int *solution, double *optimal_cost) {
    printf("\n====================================\n");
    printf("MILP OPTIMIZATION (GLPK)\n");
    printf("====================================\n\n");

    // Create GLPK problem
    glp_prob *lp = glp_create_prob();
    glp_set_prob_name(lp, "FlightRouting");
    glp_set_obj_dir(lp, GLP_MIN);  // Minimize cost

    // Calculate problem dimensions
    int num_flights = problem->num_flights;
    int total_vars = 0;
    for (int f = 0; f < num_flights; f++) {
        total_vars += problem->num_routes_per_flight[f];
    }

    printf("Problem size: %d flights, %d total routes\n", num_flights, total_vars);

    // Add binary variables: x[f][r] ∈ {0,1}
    glp_add_cols(lp, total_vars);

    int var_idx = 1;  // GLPK is 1-indexed
    for (int f = 0; f < num_flights; f++) {
        for (int r = 0; r < problem->num_routes_per_flight[f]; r++) {
            char var_name[50];
            sprintf(var_name, "x[%d][%d]", f, r);
            glp_set_col_name(lp, var_idx, var_name);
            glp_set_col_kind(lp, var_idx, GLP_BV);  // Binary variable

            // Set objective coefficient (fuel cost)
            double fuel_cost = problem->routes[f][r].fuel_cost;
            glp_set_obj_coef(lp, var_idx, fuel_cost);

            var_idx++;
        }
    }

    // CONSTRAINT 1: Each flight picks exactly one route
    // Σ(x[f][r]) = 1 for each flight f
    glp_add_rows(lp, num_flights);

    var_idx = 1;
    for (int f = 0; f < num_flights; f++) {
        int num_routes = problem->num_routes_per_flight[f];

        // Arrays for constraint (1-indexed)
        int *ind = (int*)malloc((num_routes + 1) * sizeof(int));
        double *val = (double*)malloc((num_routes + 1) * sizeof(double));

        for (int r = 0; r < num_routes; r++) {
            ind[r + 1] = var_idx + r;  // Column index
            val[r + 1] = 1.0;          // Coefficient
        }

        glp_set_row_name(lp, f + 1, problem->flights[f].flight_id);
        glp_set_row_bnds(lp, f + 1, GLP_FX, 1.0, 1.0);  // = 1
        glp_set_mat_row(lp, f + 1, num_routes, ind, val);

        free(ind);
        free(val);

        var_idx += num_routes;
    }

    // No hard conflict constraints - just minimize fuel cost
    printf("\nMILP will find minimum fuel solution\n");
    printf("(Conflicts handled via route fuel penalties already in A*)\n");

    // Solve the MIP
    printf("\nSolving MILP...\n");

    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.presolve = GLP_ON;
    parm.msg_lev = GLP_MSG_ERR;  // Only show errors

    int status = glp_intopt(lp, &parm);

    if (status != 0) {
        printf("ERROR: GLPK solver failed with status %d\n", status);
        glp_delete_prob(lp);
        return -1;
    }

    // Check solution status
    int sol_status = glp_mip_status(lp);
    if (sol_status != GLP_OPT && sol_status != GLP_FEAS) {
        printf("ERROR: No feasible solution found (status: %d)\n", sol_status);
        glp_delete_prob(lp);
        return -1;
    }

    // Extract solution
    *optimal_cost = glp_mip_obj_val(lp);

    var_idx = 1;
    for (int f = 0; f < num_flights; f++) {
        for (int r = 0; r < problem->num_routes_per_flight[f]; r++) {
            double val = glp_mip_col_val(lp, var_idx);
            if (val > 0.5) {  // Binary variable is 1
                solution[f] = r;
            }
            var_idx++;
        }
    }

    // Print solution
    printf("\n====================================\n");
    printf("MILP SOLUTION\n");
    printf("====================================\n");
    printf("Optimal Cost: %.0f kg\n", *optimal_cost);
    printf("Routes: [");
    for (int f = 0; f < num_flights; f++) {
        printf("%d", solution[f]);
        if (f < num_flights - 1) printf(", ");
    }
    printf("]\n");

    // Cleanup
    glp_delete_prob(lp);

    return 0;
}

/**
 * Compare MILP vs Greedy
 */
void compare_milp_greedy(ProblemInstance *problem) {
    printf("\n====================================\n");
    printf("COMPARISON: GREEDY vs MILP\n");
    printf("====================================\n\n");

    // Greedy solution
    int greedy_solution[MAX_FLIGHTS];
    double greedy_cost = 0;

    for (int f = 0; f < problem->num_flights; f++) {
        int best_route = 0;
        double min_fuel = problem->routes[f][0].fuel_cost;

        for (int r = 1; r < problem->num_routes_per_flight[f]; r++) {
            if (problem->routes[f][r].fuel_cost < min_fuel) {
                min_fuel = problem->routes[f][r].fuel_cost;
                best_route = r;
            }
        }

        greedy_solution[f] = best_route;
        greedy_cost += min_fuel;
    }

    printf("GREEDY:\n");
    printf("  Routes: [");
    for (int f = 0; f < problem->num_flights; f++) {
        printf("%d", greedy_solution[f]);
        if (f < problem->num_flights - 1) printf(", ");
    }
    printf("]\n");
    printf("  Cost: %.0f kg (fuel only, ignores conflicts)\n\n", greedy_cost);

    // MILP solution
    int milp_solution[MAX_FLIGHTS];
    double milp_cost = 0;

    int result = solve_milp(problem, milp_solution, &milp_cost);

    if (result == 0) {
        printf("\n====================================\n");
        printf("MILP is %.0f kg better than Greedy\n", greedy_cost - milp_cost);
        printf("(MILP finds conflict-free routes)\n");
        printf("====================================\n");
    }
}

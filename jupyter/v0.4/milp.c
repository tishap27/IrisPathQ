
/**
 * MILP Solver for IrisPathQ
 * Solves flight routing as Integer Program using GLPK
 */

#include "data_structures.h"
#include <glpk.h>

// conflict check: share ≥3 consecutive waypoints → conflict
int routes_conflict(Route *a, Route *b) {
    int max_consec = 0;

    for (int i = 0; i < a->num_waypoints - 1; i++) {
        for (int j = 0; j < b->num_waypoints - 1; j++) {
            int consec = 0;

            while (i + consec < a->num_waypoints &&
                   j + consec < b->num_waypoints &&
                   a->waypoint_indices[i + consec] ==
                   b->waypoint_indices[j + consec]) {
                consec++;
            }

            if (consec > max_consec)
                max_consec = consec;
        }
    }

    return (max_consec >= 3);
}

int get_variable_index(ProblemInstance *p, int f, int r) {
    int idx = 1;
    for (int i = 0; i < f; i++)
        idx += p->num_routes_per_flight[i];
    return idx + r;
}

int solve_milp(ProblemInstance *p, int *solution, double *optimal_cost) {
    int num_flights = p->num_flights;

    // Count variables
    int total_vars = 0;
    for (int f = 0; f < num_flights; f++)
        total_vars += p->num_routes_per_flight[f];

    glp_prob *lp = glp_create_prob();
    glp_set_prob_name(lp, "MILP_Routing");
    glp_set_obj_dir(lp, GLP_MIN);

    // Add variables
    glp_add_cols(lp, total_vars);

    int var = 1;
    for (int f = 0; f < num_flights; f++) {
        for (int r = 0; r < p->num_routes_per_flight[f]; r++) {

            char name[32];
            sprintf(name, "x_%d_%d", f, r);

            glp_set_col_name(lp, var, name);
            glp_set_col_kind(lp, var, GLP_BV); // binary

            glp_set_obj_coef(lp, var, p->routes[f][r].fuel_cost);

            var++;
        }
    }

    // Constraint 1: each flight selects exactly one route
    glp_add_rows(lp, num_flights);
    int row = 1;

    for (int f = 0; f < num_flights; f++) {
        int N = p->num_routes_per_flight[f];

        int *ind = malloc((N + 1) * sizeof(int));
        double *val = malloc((N + 1) * sizeof(double));

        for (int r = 0; r < N; r++) {
            ind[r + 1] = get_variable_index(p, f, r);
            val[r + 1] = 1;
        }

        char rowname[32];
        sprintf(rowname, "flight_%d", f);
        glp_set_row_name(lp, row, rowname);
        glp_set_row_bnds(lp, row, GLP_FX, 1, 1);

        glp_set_mat_row(lp, row, N, ind, val);

        free(ind);
        free(val);

        row++;
    }

    // Constraint 2: conflict avoidance
    int conflict_count = 0;
    for (int f1 = 0; f1 < num_flights; f1++)
        for (int r1 = 0; r1 < p->num_routes_per_flight[f1]; r1++)
            for (int f2 = f1 + 1; f2 < num_flights; f2++)
                for (int r2 = 0; r2 < p->num_routes_per_flight[f2]; r2++)
                    if (routes_conflict(&p->routes[f1][r1], &p->routes[f2][r2]))
                        conflict_count++;

    glp_add_rows(lp, conflict_count);
    int cur = row;
    int added = 0;

    for (int f1 = 0; f1 < num_flights; f1++)
        for (int r1 = 0; r1 < p->num_routes_per_flight[f1]; r1++)
            for (int f2 = f1 + 1; f2 < num_flights; f2++)
                for (int r2 = 0; r2 < p->num_routes_per_flight[f2]; r2++)
                    if (routes_conflict(&p->routes[f1][r1],
                                        &p->routes[f2][r2])) {

                        int v1 = get_variable_index(p, f1, r1);
                        int v2 = get_variable_index(p, f2, r2);

                        int ind[3] = {0, v1, v2};
                        double val[3] = {0, 1.0, 1.0};

                        char rn[64];
                        sprintf(rn, "conflict_%d_%d__%d_%d", f1, r1, f2, r2);

                        glp_set_row_name(lp, cur, rn);
                        glp_set_row_bnds(lp, cur, GLP_UP, 0.0, 1.0);
                        glp_set_mat_row(lp, cur, 2, ind, val);

                        cur++;
                        added++;
                    }

    // Solve
    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.presolve = GLP_ON;

    if (glp_intopt(lp, &parm) != 0) {
        printf("MILP solver failed\n");
        return -1;
    }

    *optimal_cost = glp_mip_obj_val(lp);

    // extract solution
    for (int f = 0; f < num_flights; f++) {
        solution[f] = -1;
        for (int r = 0; r < p->num_routes_per_flight[f]; r++) {
            int v = get_variable_index(p, f, r);
            if (glp_mip_col_val(lp, v) > 0.5)
                solution[f] = r;
        }
    }

    glp_delete_prob(lp);
    return 0;
}

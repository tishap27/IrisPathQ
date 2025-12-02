/**
 * TRUE MILP Route Generator (NO DUPLICATED FUNCTIONS!)
 */

#include "data_structures.h"
#include <glpk.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// EXTERNAL FUNCTIONS (implemented in utils.c and astar.c — DO NOT REDEFINE)
// -----------------------------------------------------------------------------
extern double calculate_distance(double, double, double, double);
extern double calculate_fuel(Route *, AircraftType *);
extern int find_waypoint_index(ProblemInstance *, const char *);
extern int astar_find_route_with_weather(ProblemInstance *, int, int, Route *);
extern double calculate_wind_penalty(Waypoint *, Waypoint *, Weather *, int);
extern int is_in_thunderstorm(double, double, Weather *, int);

// ============================================================================
// AVIATION CONSTRAINTS
// ============================================================================

int is_valid_airway_connection(ProblemInstance *problem, int from_idx, int to_idx) {
    double dist = calculate_distance(
        problem->waypoints[from_idx].latitude,
        problem->waypoints[from_idx].longitude,
        problem->waypoints[to_idx].latitude,
        problem->waypoints[to_idx].longitude
    );

    if (dist > 600.0) return 0;

    char *from = problem->waypoints[from_idx].id;
    char *to   = problem->waypoints[to_idx].id;

    int from_hub =
        (!strcmp(from,"YYZ") || !strcmp(from,"YVR") || 
         !strcmp(from,"YUL") || !strcmp(from,"YYC"));

    int to_hub =
        (!strcmp(to,"YYZ") || !strcmp(to,"YVR") ||
         !strcmp(to,"YUL") || !strcmp(to,"YYC"));

    if (from_hub && to_hub) return 1;

    return (dist <= 400.0);
}

double calculate_milp_edge_cost(ProblemInstance *problem, int from_idx, int to_idx,
                                int route_num, int flight_idx)
{
    Waypoint *from = &problem->waypoints[from_idx];
    Waypoint *to   = &problem->waypoints[to_idx];
    Weather *weather = problem->weather_cells;
    int num_weather = problem->num_weather_cells;

    double cost = calculate_distance(from->latitude, from->longitude,
                                     to->latitude, to->longitude);

    // Weather
    if (is_in_thunderstorm(to->latitude, to->longitude, weather, num_weather))
        cost *= 6.0;

    cost += calculate_wind_penalty(from, to, weather, num_weather);

    // Congestion
    if (!strcmp(to->id,"YYZ")) { cost*=1.30; cost+=500; }
    else if (!strcmp(to->id,"YVR")) { cost*=1.25; cost+=450; }
    else if (!strcmp(to->id,"YUL")) { cost*=1.22; cost+=400; }
    else if (!strcmp(to->id,"YYC")) { cost*=1.18; cost+=350; }
    else if (!strcmp(to->id,"YEG")) { cost*=1.15; cost+=300; }

    cost += (route_num * 200.0);  // slot allocation
    cost += 150.0;                // airway user fee
    cost += 100.0;                // ATC coordination

    return cost;
}

// ============================================================================
// MILP CORE
// ============================================================================

int generate_single_milp_route(ProblemInstance *problem, int flight_idx,
                               int origin_idx, int dest_idx, int route_num,
                               int *excluded)
{
    int n = problem->num_waypoints;
    Waypoint *wps = problem->waypoints;

    glp_prob *lp = glp_create_prob();
    glp_set_prob_name(lp, "MILP_ROUTE");
    glp_set_obj_dir(lp, GLP_MIN);

    // --------------------------
    // Build edge list
    // --------------------------
    typedef struct { int from,to,var; double cost; } Edge;
    Edge *edges = malloc(n * n * sizeof(Edge));
    int E = 0;

    for (int i=0;i<n;i++) {
        if (excluded && excluded[i] && i!=origin_idx && i!=dest_idx) continue;

        for (int j=0;j<n;j++) {
            if (i==j) continue;
            if (excluded && excluded[j] && j!=dest_idx) continue;

            if (!is_valid_airway_connection(problem,i,j)) continue;

            edges[E].from = i;
            edges[E].to   = j;
            edges[E].cost = calculate_milp_edge_cost(problem,i,j,route_num,flight_idx);
            edges[E].var  = E+1;
            E++;
        }
    }

    if (E==0) { free(edges); glp_delete_prob(lp); return -1; }

    glp_add_cols(lp,E);
    for (int e=0;e<E;e++) {
        glp_set_col_kind(lp,edges[e].var,GLP_BV);
        glp_set_obj_coef(lp,edges[e].var,edges[e].cost);
    }

    // --------------------------
    // Flow constraints
    //---------------------------
    glp_add_rows(lp,n);

    for (int w=0; w<n; w++) {
        int *ind = malloc((E+1)*sizeof(int));
        double *val = malloc((E+1)*sizeof(double));
        int k=0;

        for (int e=0;e<E;e++) {
            if (edges[e].to == w) { ind[++k]=edges[e].var; val[k]= 1.0; }
            if (edges[e].from== w) { ind[++k]=edges[e].var; val[k]=-1.0; }
        }

        glp_set_mat_row(lp,w+1,k,ind,val);

        if (w==origin_idx)      glp_set_row_bnds(lp,w+1,GLP_FX,-1,-1);
        else if (w==dest_idx)   glp_set_row_bnds(lp,w+1,GLP_FX,1,1);
        else                    glp_set_row_bnds(lp,w+1,GLP_FX,0,0);

        free(ind); free(val);
    }

    // Solve
    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.presolve = GLP_ON;
    parm.msg_lev  = GLP_MSG_OFF;

    int status = glp_intopt(lp,&parm);
    if (status!=0) { free(edges); glp_delete_prob(lp); return -1; }

    // --------------------------
    // Extract route
    // --------------------------
    Route *r = &problem->routes[flight_idx][route_num];
    r->num_waypoints = 0;
    r->total_distance = 0;

    int cur = origin_idx;
    int hops = 0;

    r->waypoint_indices[r->num_waypoints++] = cur;

    while (cur != dest_idx && hops < n) {
        int nxt = -1;
        for (int e=0;e<E;e++) {
            if (edges[e].from==cur && glp_mip_col_val(lp,edges[e].var) > 0.5) {
                nxt = edges[e].to;
                r->total_distance += edges[e].cost;
                break;
            }
        }
        if (nxt==-1) { free(edges); glp_delete_prob(lp); return -1; }
        r->waypoint_indices[r->num_waypoints++] = nxt;
        cur = nxt;
        hops++;
    }

    if (cur != dest_idx) { free(edges); glp_delete_prob(lp); return -1; }

    Flight *flt = &problem->flights[flight_idx];
    r->fuel_cost = calculate_fuel(r,&flt->aircraft);
    r->time_cost = r->total_distance / flt->aircraft.cruise_speed;

    free(edges);
    glp_delete_prob(lp);
    return 0;
}

// ============================================================================
// MULTI-ROUTE GENERATOR
// ============================================================================

int generate_milp_routes(ProblemInstance *problem, int flight_idx, int R)
{
    Flight *f = &problem->flights[flight_idx];
    int o = find_waypoint_index(problem,f->origin);
    int d = find_waypoint_index(problem,f->destination);

    if (o<0 || d<0) return -1;

    problem->num_routes_per_flight[flight_idx] = 0;

    // Route 0
    if (generate_single_milp_route(problem,flight_idx,o,d,0,NULL)==0) {
        problem->num_routes_per_flight[flight_idx]++;
    } else {
        Route *r0 = &problem->routes[flight_idx][0];
        astar_find_route_with_weather(problem,o,d,r0);
        r0->fuel_cost *= 1.15;
        problem->num_routes_per_flight[flight_idx]++;
    }

    // Routes 1..R-1
    for (int r=1;r<R;r++) {
        int excl[MAX_WAYPOINTS]={0};

        for (int p=0;p<r;p++) {
            Route *pr = &problem->routes[flight_idx][p];
            for (int i=1;i<pr->num_waypoints-1;i++)
                excl[pr->waypoint_indices[i]] = 1;
        }

        if (generate_single_milp_route(problem,flight_idx,o,d,r,excl)==0) {
            problem->num_routes_per_flight[flight_idx]++;
        } else {
            Route *fallback = &problem->routes[flight_idx][r];
            *fallback = problem->routes[flight_idx][0];
            fallback->fuel_cost *= (1.0 + r*0.10);
            fallback->fuel_cost += (r*500);
            problem->num_routes_per_flight[flight_idx]++;
        }
    }

    return problem->num_routes_per_flight[flight_idx];
}

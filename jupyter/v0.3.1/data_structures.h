/**
 * IrisPathQ Route Optimizer
 * Data structures for flights, routes, and optimization
*/

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FLIGHTS 500
#define MAX_WAYPOINTS 1000
#define MAX_ROUTES_PER_FLIGHT 10
#define MAX_WAYPOINTS_PER_ROUTE 50
#define EARTH_RADIUS_KM 6371.0
#define M_PI 3.14159265358979323846

typedef struct {
    char id[10];
    char name[50];
    double latitude;
    double longitude;
} Waypoint;

typedef struct {
    double latitude;
    double longitude;
    double wind_speed;
    double wind_direction;
    char weather_type[20];
} Weather;

typedef struct {
    char type[10];
    double cruise_speed;
    double fuel_burn_rate;
    double max_range;
} AircraftType;

typedef struct {
    char flight_id[10];
    char origin[10];
    char destination[10];
    char departure_time[10];
    AircraftType aircraft;
} Flight;

typedef struct {
    int waypoint_indices[MAX_WAYPOINTS_PER_ROUTE];
    int num_waypoints;
    double total_distance;
    double fuel_cost;
    double time_cost;
    int conflict_count;
} Route;

typedef struct {
    Flight flights[MAX_FLIGHTS];
    int num_flights;

    Waypoint waypoints[MAX_WAYPOINTS];
    int num_waypoints;

    Weather weather_cells[100];
    int num_weather_cells;

    Route routes[MAX_FLIGHTS][MAX_ROUTES_PER_FLIGHT];
    int num_routes_per_flight[MAX_FLIGHTS];
} ProblemInstance;

double calculate_distance(double lat1, double lon1, double lat2, double lon2);
double calculate_fuel(Route *route, AircraftType *aircraft);
void print_route(Route *route, Waypoint *waypoints);
int is_in_hazard(double lat, double lon, Weather *weather_cells, int num_cells);

int load_flights(const char *filename, ProblemInstance *problem);
int load_waypoints(const char *filename, ProblemInstance *problem);
int load_weather(const char *filename, ProblemInstance *problem);
int generate_alternative_routes(ProblemInstance *problem, int flight_idx, int num_routes);
void inject_conflicts(ProblemInstance *problem);
void build_cost_matrix(ProblemInstance *problem, double **cost_matrix, int *matrix_size);
void export_cost_matrix(double *cost_matrix, int matrix_size, const char *filename);
void export_full_matrix(double *cost_matrix, int matrix_size, const char *filename);

#endif

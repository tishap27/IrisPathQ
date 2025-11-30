/**
 * Route Optimizer
*/

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Constants
#define MAX_FLIGHTS 500
#define MAX_WAYPOINTS 1000
#define MAX_ROUTES_PER_FLIGHT 10
#define MAX_WAYPOINTS_PER_ROUTE 50
#define EARTH_RADIUS_KM 6371.0
#define M_PI 3.14159265358979323846     //cuz I can use it rather than 3.14!

// Airport/Waypoint structure
typedef struct {
    char id[10];
    char name[50];
    double latitude;
    double longitude;
} Waypoint;

// Weather data
typedef struct {
    double latitude;
    double longitude;
    double wind_speed;      // knots
    double wind_direction;  // degrees
    double radius_nm;
    char severity[20];
    char weather_type[20];  // CLEAR, STORM, etc.
   
 
    
} Weather;

// Aircraft type
typedef struct {
    char type[10];          // B737, A320, etc.
    double cruise_speed;    // knots
    double fuel_burn_rate;  // kg per hour
    double max_range;       // nautical miles
} AircraftType;

// Flight information
typedef struct {
    char flight_id[10];
    char origin[10];
    char destination[10];
    char departure_time[10];
    AircraftType aircraft;
} Flight;

// Route (sequence of waypoints)
typedef struct {
    int waypoint_indices[MAX_WAYPOINTS_PER_ROUTE];
    int num_waypoints;
    double total_distance;  // nautical miles
    double fuel_cost;       // kg
    double time_cost;       // hours
    int conflict_count;     // number of conflicts with other routes
} Route;

// Problem instance
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

// Function declarations
double calculate_distance(double lat1, double lon1, double lat2, double lon2);
double calculate_fuel(Route *route, AircraftType *aircraft);
void print_route(Route *route, Waypoint *waypoints);
int is_in_hazard(double lat, double lon, Weather *weather_cells, int num_cells);
double calculate_wind_penalty(Waypoint *from, Waypoint *to, Weather *weather_cells, int num_cells);
double calculate_bearing(double lat1, double lon1, double lat2, double lon2);
double get_wind_component(double route_bearing, double wind_direction, double wind_speed);
int is_in_thunderstorm(double lat, double lon, Weather *weather_cells, int num_cells);

int load_flights(const char *filename, ProblemInstance *problem);
int load_waypoints(const char *filename, ProblemInstance *problem);
int load_weather(const char *filename, ProblemInstance *problem);
int generate_alternative_routes(ProblemInstance *problem, int flight_idx, int num_routes);
void inject_conflicts(ProblemInstance *problem);
void build_cost_matrix(ProblemInstance *problem, double **cost_matrix, int *matrix_size);
void export_cost_matrix(double *cost_matrix, int matrix_size, const char *filename);
void export_full_matrix(double *cost_matrix, int matrix_size, const char *filename);
int astar_find_route_with_weather(ProblemInstance *problem, int origin_idx, int destination_idx, Route *route);
// MILP
int solve_milp(ProblemInstance *problem, int *solution, double *optimal_cost);
void compare_milp_greedy(ProblemInstance *problem);
int find_waypoint_index(ProblemInstance *problem, const char *id);

#endif
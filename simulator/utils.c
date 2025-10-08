/**
 * Route Optimizer
*/
//All calculations here  
#include "data_structures.h"

// Calculate great circle distance between two points AIRPATH
double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    double distance_km = EARTH_RADIUS_KM * c;
    
    // Convert to nautical miles (1 nm = 1.852 km)
    return distance_km / 1.852;
}

// Calculate fuel consumption for a route
double calculate_fuel(Route *route, AircraftType *aircraft) {
    double time_hours = route->total_distance / aircraft->cruise_speed;
    return time_hours * aircraft->fuel_burn_rate;
}

// Check if a waypoint is in a weather hazard zone
int is_in_hazard(double lat, double lon, Weather *weather_cells, int num_cells) {
    for (int i = 0; i < num_cells; i++) {
        if (strcmp(weather_cells[i].weather_type, "THUNDERSTORM") == 0) {
            double dist = calculate_distance(lat, lon, 
                                            weather_cells[i].latitude, 
                                            weather_cells[i].longitude);
            if (dist < 50.0) {  // Within 50nm of storm
                return 1;
            }
        }
    }
    return 0;
}

// Print route details
void print_route(Route *route, Waypoint *waypoints) {
    printf("Route: ");
    for (int i = 0; i < route->num_waypoints; i++) {
        printf("%s ", waypoints[route->waypoint_indices[i]].id);
        if (i < route->num_waypoints - 1) printf("-> ");
    }
    printf("\n");
    printf("Distance: %.2f nm, Fuel: %.2f kg, Time: %.2f hrs\n",
           route->total_distance, route->fuel_cost, route->time_cost);
}
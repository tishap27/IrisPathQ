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
// Calculate bearing between two points (degrees)
double calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    
    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
    double bearing = atan2(y, x);
    
    bearing = bearing * 180.0 / M_PI;
    bearing = fmod((bearing + 360.0), 360.0);  // Normalize to 0-360
    
    return bearing;
}

// Get wind component for a route segment
double get_wind_component(double route_bearing, double wind_direction, double wind_speed) {
    // Calculate angle between route and wind
    double angle_diff = fmod(fabs(route_bearing - wind_direction), 360.0);
    if (angle_diff > 180.0) {
        angle_diff = 360.0 - angle_diff;
    }
    
    // Headwind component (positive = headwind, negative = tailwind)
    double wind_component = wind_speed * cos(angle_diff * M_PI / 180.0);
    
    return wind_component;
}
// Calculate wind penalty for route segment
double calculate_wind_penalty(Waypoint *from, Waypoint *to, Weather *weather_cells, int num_cells) {
    // Find weather cell affecting this segment (use midpoint)
    double mid_lat = (from->latitude + to->latitude) / 2.0;
    double mid_lon = (from->longitude + to->longitude) / 2.0;
    
    // Find nearest weather cell
    Weather *active_weather = NULL;
    double min_dist = 999999.0;
    
    for (int i = 0; i < num_cells; i++) {
        double dist = calculate_distance(mid_lat, mid_lon, 
                                        weather_cells[i].latitude, 
                                        weather_cells[i].longitude);
        
        if (dist < weather_cells[i].radius_nm && dist < min_dist) {
            min_dist = dist;
            active_weather = &weather_cells[i];
        }
    }
    
    if (active_weather == NULL) {
        return 0.0;  // No weather impact
    }
    
    // Calculate route bearing
    double bearing = calculate_bearing(from->latitude, from->longitude,
                                      to->latitude, to->longitude);
    
    // Get headwind/tailwind component
    double wind_component = get_wind_component(bearing, 
                                              active_weather->wind_direction,
                                              active_weather->wind_speed);
    
    // Convert to fuel penalty
    // Headwind: +50 knots = +5% fuel = +50 kg per 1000 nm
    // Tailwind: -50 knots = -5% fuel = -50 kg per 1000 nm
    double distance_nm = calculate_distance(from->latitude, from->longitude,
                                           to->latitude, to->longitude);
    
    double fuel_penalty = (wind_component / 10.0) * (distance_nm / 1000.0) * 100.0;
    
    return fuel_penalty;  // kg of extra fuel
}

// Check if waypoint is in thunderstorm zone
int is_in_thunderstorm(double lat, double lon, Weather *weather_cells, int num_cells) {
    for (int i = 0; i < num_cells; i++) {
        if (strcmp(weather_cells[i].weather_type, "THUNDERSTORM") != 0) {
            continue;  // Not a thunderstorm
        }
        
        double dist = calculate_distance(lat, lon,
                                        weather_cells[i].latitude,
                                        weather_cells[i].longitude);
        
        if (dist <= weather_cells[i].radius_nm) {
            return 1;  // Inside thunderstorm zone
        }
    }
    return 0;  // Clear
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
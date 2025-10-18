/**
 * IrisPathQ Route Optimizer
 * Load flights, waypoints, and weather data from CSV files
*/

#include "data_structures.h"

int load_flights(const char *filename, ProblemInstance *problem) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return -1;
    }

    char line[256];
    fgets(line, sizeof(line), file);

    problem->num_flights = 0;
    while (fgets(line, sizeof(line), file) && problem->num_flights < MAX_FLIGHTS) {
        Flight *f = &problem->flights[problem->num_flights];

        sscanf(line, "%[^,],%[^,],%[^,],%[^,],%s",
               f->flight_id, f->origin, f->destination, 
               f->departure_time, f->aircraft.type);

        if (strcmp(f->aircraft.type, "B737") == 0) {
            f->aircraft.cruise_speed = 450.0;
            f->aircraft.fuel_burn_rate = 2500.0;
            f->aircraft.max_range = 3000.0;
        } else if (strcmp(f->aircraft.type, "A320") == 0) {
            f->aircraft.cruise_speed = 450.0;
            f->aircraft.fuel_burn_rate = 2400.0;
            f->aircraft.max_range = 3200.0;
        } else {
            f->aircraft.cruise_speed = 450.0;
            f->aircraft.fuel_burn_rate = 2500.0;
            f->aircraft.max_range = 3000.0;
        }

        problem->num_flights++;
    }

    fclose(file);
    printf("Loaded %d flights\n", problem->num_flights);
    return problem->num_flights;
}

int load_waypoints(const char *filename, ProblemInstance *problem) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return -1;
    }

    char line[256];
    fgets(line, sizeof(line), file);

    problem->num_waypoints = 0;
    while (fgets(line, sizeof(line), file) && problem->num_waypoints < MAX_WAYPOINTS) {
        Waypoint *w = &problem->waypoints[problem->num_waypoints];

        sscanf(line, "%[^,],%lf,%lf,%s",
               w->id, &w->latitude, &w->longitude, w->name);

        problem->num_waypoints++;
    }

    fclose(file);
    printf("Loaded %d waypoints\n", problem->num_waypoints);
    return problem->num_waypoints;
}

int load_weather(const char *filename, ProblemInstance *problem) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return -1;
    }

    char line[256];
    fgets(line, sizeof(line), file);

    problem->num_weather_cells = 0;
    while (fgets(line, sizeof(line), file) && problem->num_weather_cells < 100) {
        Weather *w = &problem->weather_cells[problem->num_weather_cells];

        sscanf(line, "%lf,%lf,%lf,%lf,%s",
               &w->latitude, &w->longitude, &w->wind_speed, 
               &w->wind_direction, w->weather_type);

        problem->num_weather_cells++;
    }

    fclose(file);
    printf("Loaded %d weather cells\n", problem->num_weather_cells);
    return problem->num_weather_cells;
}

#include "data_structures.h"
#include <stdio.h>
#include <string.h>

extern int run_milp_weather(ProblemInstance *);

int main() {

    printf("=== WEATHER-AWARE A* + MILP ===\n");

    ProblemInstance p;
    memset(&p, 0, sizeof(p));

    load_flights("data/flights.csv", &p);
    load_waypoints("data/waypoints.csv", &p);
    load_weather("data/weatherTS3.csv", &p);

    printf("Loaded %d flights, %d waypoints, %d weather cells.\n",
           p.num_flights, p.num_waypoints, p.num_weather_cells);

    // This calls parse_routes_from_txt + MILP solver
    run_milp_weather(&p);

    return 0;
}

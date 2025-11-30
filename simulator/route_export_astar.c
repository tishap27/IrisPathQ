#include "data_structures.h"

void export_routes_as_txt(ProblemInstance *p, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("ERROR: Cannot open %s for writing\n", filename);
        return;
    }

    for (int f = 0; f < p->num_flights; f++) {
        fprintf(fp, "FLIGHT %s\n", p->flights[f].flight_id);

        int R = p->num_routes_per_flight[f];
        for (int r = 0; r < R; r++) {
            Route *rt = &p->routes[f][r];

            fprintf(fp, "ROUTE %d ", r);
            for (int w = 0; w < rt->num_waypoints; w++) {
                fprintf(fp, "%s",
                        p->waypoints[rt->waypoint_indices[w]].id);
                if (w < rt->num_waypoints - 1)
                    fprintf(fp, " ");
            }
            fprintf(fp, "\n");
        }

        fprintf(fp, "END\n");
    }

    fclose(fp);
    printf(" A* routes exported to %s\n", filename);
}

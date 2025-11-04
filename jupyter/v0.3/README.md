# IrisPathQ Phase 2: A* Pathfinding Implementation

**Status:** In Progress  
**Date:** November 2025  
**Version:** 0.3  

---

## Overview  
This phase implements the A* pathfinding algorithm for realistic waypoint-based route generation, replacing the simple direct routing from Phase 1 (v0.2).  
The system now navigates through waypoint networks, avoids weather hazards, and generates multiple alternative routes per flight.

---

## Updates in v0.3

### ✅ A* Pathfinding Algorithm
- Priority queue implementation with f-cost ordering  
- Heuristic: Great circle distance (geodesic)  
- Dynamic waypoint network navigation  
- Weather hazard avoidance with cost penalties  

### ✅ Route Alternative Generation
- Multiple route options per flight (configurable)  
- Cost variations for quantum optimization comparison  
- Conflict detection between routes (shared waypoints)  


---

## Architecture

### A* Pathfinding Process
1. **Initialize:** Origin waypoint with `g_cost = 0`  
2. **Priority Queue:** Sort nodes by `f_cost = g_cost + h_cost`  
3. **Expand:** Explore neighbors within 3000nm range  
4. **Penalties:** Double cost for weather hazard waypoints  
5. **Goal Check:** Terminate when destination reached  
6. **Backtrack:** Reconstruct path from parent pointers  

---

## Key Components
### Priority Queue (Min-Heap)

---

## File Structure

---

## Algorithm Complexity

---

## Usage

### 1. Compile C Preprocessing
```
gcc -o route_optimizer main.c data_loader.c utils.c astar.c calculations.c -lm

```

### 2. Generate Routes & Cost Matrix
```
./route_optimizer

```

**Expected Output:**
```
Generating 5 routes for AC101: YYZ -> YVR
  Route 0: Route: YYZ -> YVR 
Distance: 1806.46 nm, Fuel: 10035.91 kg, Time: 4.01 hrs
  Route 1: (variant) Route: YYZ -> YVR 
Distance: 1806.46 nm, Fuel: 10236.63 kg, Time: 4.09 hrs
  Route 2: (variant) Route: YYZ -> YVR 
Building cost matrix (25 x 25)...
Cost matrix exported to output/cost_matrix.txt

```

### 3. Run Quantum Optimization
```
python route_qaoa.py
```

---

## Data Formats

### waypoints.csv
### weather.csv
### flights.csv

---

## Key Improvements Over v0.2
| Feature | v0.2 (Greedy) | v0.3 (A*) |
|----------|----------------|------------|
| Route Type | Direct origin→destination | Waypoint network navigation |
| Weather | ❌ Ignored | ✅ Cost penalties |
| Conflicts | ❌ No detection | ✅ Shared waypoint penalties |
| Alternatives | Fixed +2% increments | Realistic path variations |
| Quantum Advantage | None  | Enabled (conflict penalties) |
---

## Known Limitations
- ⚠️ **Neighbor Exploration:** Considers all waypoints within 3000nm  
  → Future: Predefined airway connections  
-

---

## Next Steps
### Phase 2.1 
Integrate real-time constraints
### Phase 3: MILP Comparison
Formulate as a mixed-integer linear program
Compare: Greedy vs A* vs QAOA vs MILP
Analyze solution quality vs runtime tradeoff

---

## Dependencies

### C Compilation
- GCC 7.0+ with math library (`-lm`)  
- Standard library (`stdio`, `stdlib`, `math`)  

### Python (Quantum)
```
pip install qiskit qiskit-aer numpy scipy matplotlib

```

---

## References

ToDo


---

<br>

**IrisPathQ**  
November 2025


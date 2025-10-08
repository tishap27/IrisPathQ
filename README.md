# IrisPathQ - Quantum Optimized Flight Route 

## Overview
IrisPathQ is a hybrid quantum-classical flight route optimization tool that minimizes fuel consumption for multiple commercial flights while avoiding weather hazards and airspace conflicts. It combines classical pathfinding with quantum optimization to evaluate route combinations globally.

## Key Features
-  Uses the A* algorithm to find the shortest and safest flight routes.
- Weather & Conflict Penalties integrated into cost calculations
- Real Aviation Data (airport coordinates, aircraft fuel burn rates)
- Quantum QAOA and QUBO -- problem(Matrix) will be in QUBO format and QAOA will be applied on that

## Problem
Given *N* flights, each with *M* possible routes, determine the optimal combination of one route per flight that minimizes total fuel use while avoiding:
- Storm detours (fuel penalties)
- Airspace conflicts (waypoint sharing at same time)
- Aircraft limitations

A classical greedy approach is fast because it just picks the cheapest route for each flight, one by one. However, when flights can affect each other (like having route conflicts), this method can miss better overall solutions.

Quantum optimization : Can look at all possible combinations of routes at once and has a better chance of finding the best solution for everyone, even when there are conflicts or complicated rules. This means it sometimes finds lower total fuel usage that a classical method would miss.

## Example: A simple 2×2 matrix

|       | R0 (Route 0) | R1 (Route 1) |
|-------|--------------|--------------|
| **F0** | 4            | 6            |
| **F1** | 5            | 7            |

Conflict penalty: **10** if both flights pick the same route (R0 or R1).

### Classical Greedy:
- Picks the route with the lowest fuel cost for each flight step-by-step, independently:
  - Flight 0 picks Route 0 (cost = 4) because 4 < 6.
  - Flight 1 picks Route 0 (cost = 5) because 5 < 7.
- Conflict penalty (10) is added *after* these choices are made, increasing total unexpectedly.
- Total cost with greedy = 4 + 5 + 10 (penalty) = **19**.
- No backtracking

### Quantum Optimization:
- Incorporates conflict penalties *within* the cost matrix before selecting routes.
- The penalty increases the "cost" of conflicting choices upfront.
- Quantum search automatically avoids combinations that would lead to high penalties.
- Picks Flight 0: Route 1 (cost = 6), Flight 1: Route 0 (cost = 5), with no penalty.
- Total cost with quantum = 6 + 5 = **11**, which is better.
- This method also encountered the exact same routes classical faced However it went ahead and looked if there was a better option. 

## Architecture
- Data Loading (`flights.csv`, `waypoints.csv`, `weather.csv`)
- Route Generation via A* (fuel, distance, penalties)
- Cost Matrix Construction — encodes fuel + conflicts
- Optimization  
  - Classical Greedy → Per-flight cheapest route  
  - Quantum QAOA → Explore all combinations in superposition and measure best

## Technology Stack
- **C** — Data prep, A*, fuel calculations, conflict checks  
- **Python/Qiskit** — Quantum stuff - QAOA solver... 
- **CSV Data Files** — Flights, waypoints, weather cells  

## Future Work
- Use a full CSV of all Canadian airports and waypoints as a routing base  
- Integrate real-time weather data (rather than a static CSV), so route planning uses live weather updates  
- Add realistic airway networks and altitude choices  
- Real quantum hardware runs :) 
- Scaling to 20+ flights, 100+ waypoints :) 

to be continued/corrected...
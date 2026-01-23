# IrisPathQ - Quantum Optimized Flight Route 

## Overview
IrisPathQ is a hybrid quantum-classical flight route optimization tool that minimizes fuel consumption for multiple commercial flights while avoiding weather hazards and airspace conflicts. It combines classical pathfinding with quantum optimization to evaluate route combinations globally.

## Key Features
-  Uses the A* algorithm to find the shortest and safest flight routes.
- Weather & Conflict Penalties integrated into cost calculations
- Real Aviation Data (airport coordinates, aircraft fuel burn rates)
- Quantum QAOA and QUBO -- problem(Matrix) will be in QUBO format and QAOA will be applied on that

## Problem
Given *N* flights, each with *M* possible routes, determine the optimal route selection that minimizes total cost subject to:
- 1. **Fuel Consumption (Primary Cost Driver)**
   - Fuel = (distance_nm / cruise_speed_knots) * fuel_burn_rate_kg_hr
   - B737: 450 kt cruise, 2,500 kg/hr burn
   - Higher cost for longer routes; quantum finds routes that collectively minimize
- 2. **Weather Penalties (Secondary Cost Driver)**
   - THUNDERSTORMS: 5× multiplier on route segments within storm radius
   - WIND: Headwind adds fuel penalty; tailwind reduces penalty
   - Routes through good weather preferred; quantum avoids bad combinations
- 3. **Airspace Conflicts (Tertiary Cost Driver)**
   - WAYPOINT SHARING: If two selected routes use same waypoint → 10,000 kg penalty
   - Quantum naturally avoids high-conflict combinations
   - Greedy/A* discover conflicts after selection; QAOA prevents them upfront

**Goal: Select exactly one route per flight (N total selections) that minimizes:**
```bash
Total Cost = Σ Fuel(selected_route_i) + Σ Conflict_Penalty(route_i, route_j)
             i=1..N                      i,j pairs that conflict
```
**Constraints:** One route per flight (binary selection)
   - Each flight picks exactly 1 of its M routes (binary choice)
   - Fuel cost = (Distance nm / 450 knots) × 2,500 kg/hr (based on aircraft type)
   - Conflict penalty = 10,000 kg if two routes share ANY waypoint
   - Weather adjustments already applied to route costs by A*

The classical greedy approach is fast because it just picks the cheapest route for each flight, one by one. However, when flights can affect each other (like having route conflicts), this method can miss better overall solutions.

Quantum optimization : Can look at all possible combinations of routes at once and has a better chance of finding the best solution for everyone, even when there are conflicts or complicated rules. This means it sometimes finds lower total fuel usage that a classical method would miss.

## Example: A simple 2×2 matrix

|       | R0 (Route 0) | R1 (Route 1) |
|-------|--------------|--------------|
| **F0** | 5 kg        | 6 kg         |
| **F1** | 4 kg        | 7 kg         |

```bash
Waypoint structure:
  Route 0 (Flight 0): NYC → WAYPOINT_A → Toronto
  Route 1 (Flight 0): NYC → WAYPOINT_B → Toronto
  Route 0 (Flight 1): Montreal → WAYPOINT_A → Toronto  ← SHARES WAYPOINT_A
  Route 1 (Flight 1): Montreal → WAYPOINT_C → Toronto

Conflict rule: If both flights use same waypoint → +10 kg penalty
```

### Classical Greedy:
- Picks the route with the lowest fuel cost for each flight step-by-step, independently:
  - Flight 0 picks Route 0 (cost = 5) because 5 < 6.
  - Flight 1 sees: 4 < 7 → picks Route 0 (4 kg) **doesn't know Flight 0 already uses WAYPOINT_A**

**Calculation:**
```bash
Flight 0 fuel:        5 kg (Route 0 via WAYPOINT_A)
Flight 1 fuel:        4 kg (Route 0 via WAYPOINT_A)
Conflict penalty:    +10 kg (both use WAYPOINT_A - OOPS!)
─────────────────────────────
GREEDY TOTAL:        19 kg ❌ (SUBOPTIMAL - conflict created)
```
- Conflict penalty (10) is added *after* these choices are made, increasing total unexpectedly.
- Total cost with greedy = 4 + 5 + 10 (penalty) = **19**.
- No backtracking

### A* Only (Weather-Aware, But Still Local):
**Algorithm:** A* evaluates each route per flight independently, adds small costs for detours
```bash
A* Evaluation per flight (no global conflict awareness):

Flight 0:
  Route 0: 5 kg + minimal weather = 5 kg ✅ (A* picks this)
  Route 1: 6 kg + minor penalty = 6 kg

Flight 1:
  Route 0: 4 kg + minimal weather = 4 kg ✅ (A* picks this)
  Route 1: 7 kg + minor penalty = 7 kg
```
**Calculation:**
```bash
Flight 0 fuel:        5 kg (Route 0 via WAYPOINT_A)
Flight 1 fuel:        4 kg (Route 0 via WAYPOINT_A)
Conflict penalty:    +10 kg (both use WAYPOINT_A - A* DIDN'T AVOID THIS!)
─────────────────────────────
A* TOTAL:            19 kg ❌ (SAME AS GREEDY - no conflict awareness)
```
Why A fails here*: A* optimises each flight independently. It found the locally cheapest routes per flight, but created the same conflict as greedy. A* doesn't look ahead to global combinations.

###  A* + Quantum QAOA (Global Optimisation)
**Algorithm:** QAOA explores ALL 4 combinations simultaneously, weighs them by total cost  

Unlike Greedy and A*, which optimize each flight independently, QAOA sees the problem as a whole:
```bash
QAOA evaluates all combinations:

Combination 1: Flight 0 Route 0 (5kg), Flight 1 Route 0 (4kg)
  Fuel: 5 + 4 = 9 kg
  Conflict: Both use WAYPOINT_A → +10 kg
  TOTAL: 9 + 10 = 19 kg ❌

Combination 2: Flight 0 Route 0 (5kg), Flight 1 Route 1 (7kg)
  Fuel: 5 + 7 = 12 kg
  Conflict: WAYPOINT_A vs WAYPOINT_C → +0 kg
  TOTAL: 12 + 0 = 12 kg ✅ (BEST!)

Combination 3: Flight 0 Route 1 (6kg), Flight 1 Route 0 (4kg)
  Fuel: 6 + 4 = 10 kg
  Conflict: WAYPOINT_B vs WAYPOINT_A → +0 kg
  TOTAL: 10 + 0 = 10 kg (tied for best)

Combination 4: Flight 0 Route 1 (6kg), Flight 1 Route 1 (7kg)
  Fuel: 6 + 7 = 13 kg
  Conflict: WAYPOINT_B vs WAYPOINT_C → +0 kg
  TOTAL: 13 + 0 = 13 kg
```
**How QAOA Works:**
- Incorporates conflict penalties **within the cost matrix BEFORE** selecting routes
- Assigns probability amplitudes to each combination based on total cost
- Low-cost combinations (10-12 kg) get HIGH probability
- High-cost combinations (19 kg) get LOW probability
- When measured: Most likely to pick Combination 2 (12 kg) or Combination 3 (10 kg)

**QAOA Result:**
```bash
QAOA assigns probability amplitudes based on total cost:
- Combination 1 (19 kg): VERY LOW probability (highest cost)
- Combination 2 (12 kg): HIGH probability ← Most likely outcome
- Combination 3 (10 kg): HIGH probability ← Also good
- Combination 4 (13 kg): MEDIUM probability

When circuit is measured (8,192 shots):
  Most likely: Combination 2 or 3 (both avoid conflicts & are cheapest overall)
  
Let's say measured: Combination 3

Flight 0 fuel:        6 kg (Route 1 via WAYPOINT_B)
Flight 1 fuel:        4 kg (Route 0 via WAYPOINT_A)
Conflict penalty:     +0 kg (different waypoints!)
─────────────────────────────
QAOA TOTAL:          10 kg ✅ (OPTIMAL - no conflicts!)
```

#### Final Answer Comparison
```bash
Method         Route Selection           Fuel     Conflicts    TOTAL    vs Greedy
───────────────────────────────────────────────────────────────────────────────
Greedy         [Route0, Route0]          5+4=9    +10 conflict  19 kg   Baseline
A* Only        [Route0, Route0]          5+4=9    +10 conflict  19 kg   0% (Same!)
A*+QAOA        [Route1, Route0]          6+4=10   +0 conflict   10 kg   47% better ✓

```
KEY DIFFERENCE: In this tricky case, A* & Greedy both create conflicts.
QAOA avoids the conflict by exploring globally and picking Combination 3.

**Key Insight:**
- Greedy & A*: Make perfect local decisions but create global problems
- QAOA: Evaluates all combinations globally and avoids conflicts upfront
- Result: QAOA picks a slightly pricier route for Flight 0 (6 kg instead of 5 kg) but saves 10 kg in conflict penalties


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
- Scaling to 20+ flights, 100+ waypoints


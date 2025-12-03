# IrisPathQ v0.4 – MILP Integration 

## Overview

Hybrid classical–quantum system for multi-flight route optimisation.  
Key achievement: QAOA reduces conflict penalties and achieves a 22.2% total cost reduction vs a greedy baseline.

---

## What’s New in v0.4

### MILP Solver (GLPK)

- Exact integer programming with binary decision variables  
- Conflict avoidance via linear constraints  
- Optimal route selection guaranteeing no conflicts

### Strategic Conflict Injection

- Cheapest routes deliberately overlap  
  - Example: AC101 Route 2 shares YEG with WS202 Route 3  
- Creates an optimization challenge: fuel efficiency vs conflict penalties  
- Tests solver intelligence: can they escape local optima?

### Three-Way Comparison

- **MILP Greedy** → Local optimization (cheapest per flight)  
- **MILP Optimal** → Global exact solution (with constraints)  
- **MILP + QAOA** → Quantum exploration (entangled state space)

---

## Architecture

```bash

┌───────────────────────┐
│ Route Generator       │ Simple direct routes with +5%, +10%, +15% variants
│ (route_generator.c)   │
└───────────┬───────────┘
            │
            ↓
┌───────────────────────┐
│  Conflict Injection   │ Strategic: Cheapest routes share waypoints
│                       │ - YEG: AC101 R2 ↔ WS202 R3
└───────────┬───────────┘ - YYC: AC303 R4 ↔ WS404 R0
            │
            ↓
┌───────────────────────┐
│ Cost Matrix           │ 25×25 (5 flights × 5 routes)
│ (sparse)              │ 54 conflicts @ 10,000 kg each
└──────┬────────────────┘
       │
       ├───────────────┬─────────────────┐
       ↓               ↓                 ↓
┌────────────┐ ┌────────────┐ ┌────────────┐
│ Greedy     │ │ MILP       │ │ QAOA       │
│ (Local)    │ │ (Exact)    │ │ (Quantum)  │
└────────────┘ └────────────┘ └────────────┘

```

---

## Results
```bash
┌──────────────┬────────────┬───────────┬────────────┬──────────────┐
│ Method       │ Fuel Cost  │ Conflicts │ Total Cost │ Improvement  │
├──────────────┼────────────┼───────────┼────────────┼──────────────┤
│ MILP Greedy  │ 58,441 kg  │ 30,000 kg │ 88,441 kg  │ Baseline     │
│ MILP + QAOA  │ 58,827 kg  │ 10,000 kg │ 68,827 kg  │ 22.2%        │
└──────────────┴────────────┴───────────┴────────────┴──────────────┘

```

**Key insight:** QAOA sacrifices 386 kg fuel to avoid a 20,000 kg conflict penalty → net savings: 19,614 kg (global vs local optimization).

---

## Mathematical Formulation

### 1. Decision Variables (MILP)

```bash
x[flight][route] ∈ {0,1}

Example: x[0][2] = 1 means "Flight 0 takes Route 2"
```

### 2. Objective Function

```bash
Minimize: Σ FuelCost[f][r] × x[f][r]
          f,r

Subject to:
  1. Exactly one route per flight:    Σ x[f][r] = 1  ∀f
                                        r
  
  2. No conflicts:  x[f1][r1] + x[f2][r2] ≤ 1  for conflicting pairs
```


**Example constraint:**
```bash
x[0][2] + x[1][3] ≤ 1    (AC101 Route 2 conflicts with WS202 Route 3)
```

---

## Conflict Detection Rule
```bash
Routes conflict IF they share ≥3 consecutive waypoints

Example:
  Route A: YYZ → YEG → YYC → YVR  
  Route B: YUL → YEG → YYC → YOW  
           Shared: YEG, YYC (2 consecutive) → NO conflict
  
  Route C: YYZ → YEG → YYC → YWG → YVR
  Route D: YUL → YEG → YYC → YWG → YOW
           Shared: YEG, YYC, YWG (3 consecutive) → CONFLICT!
```

---

## QAOA Cost Hamiltonian

```bash
Ĥ = Σ FuelCost[r] |r⟩⟨r|  +  λ Σ |r1⟩⟨r1| ⊗ |r2⟩⟨r2|
    r                         conflicts

    ↑                         ↑
  Fuel costs              Conflict penalties (λ=10,000 kg)
```

- First term: fuel costs  
- Second term: conflict penalties (λ = 10,000 kg)

---

## Quantum Encoding

```bash
3 qubits per flight → 8 possible states (use 5, penalize 6-7)

Bitstring: |q2 q1 q0⟩ → Route number = 4×q2 + 2×q1 + q0

Examples:
  |001⟩ = Route 1
  |100⟩ = Route 4  
  |111⟩ = Invalid (adds 50,000 kg penalty)
```

---

## Quick Start

### Installation
```bash
Install GLPK
sudo apt-get install libglpk-dev # Ubuntu/Debian
brew install glpk # macOS

Install Python dependencies
pip install qiskit qiskit-aer scipy numpy

```

### Build & Run

1. Compile MILP simulator
```bash
gcc -o route_optimizerMILP milp_main.c data_loader.c
route_generator.c utils.c astar.c milp.c -lm -lglpk 
```

2. Generate cost matrix with strategic conflicts
./route_optimizerMILP

3. Run quantum optimization```python milp_qaoa.py```


---

## Expected Output
```bash
====================================
INJECTING STRATEGIC CONFLICTS
AC101 Route 2 passes through YEG (conflicts with WS202)
WS202 Route 3 passes through YEG (conflicts with AC101)
...

Building Cost Matrix
Total conflicts: 54

====================================
QAOA OPTIMIZATION
Circuit: 15 qubits (3 per flight)

MILP Greedy Routes:
Fuel: 58,441 kg
Conflicts: 3 (penalty: 30,000 kg)
TOTAL: 88,441 kg

Best QAOA Routes:​
Fuel: 58,827 kg
Conflicts: 1 (penalty: 10,000 kg)
TOTAL: 68,827 kg

QAOA BEATS MILP BY 19,614 kg (22.2% improvement)
Conflict reduction: 3 → 1
```

---

## File Structure
```bash
IrisPathQ/
├── simulator/
│ ├── milp.c # GLPK solver (constraints + objective)
│ ├── milp_main.c # Conflict injection + orchestration
│ ├── route_generator.c # Simple route variants (+5%, +10%, ...)
│ ├── astar.c # A* pathfinding (used for weather-aware routes)
│ └── data_structures.h # Shared structs (Route, Flight, Weather)
│
├── milp_qaoa.py # QAOA circuit + optimization
├── route_optimizer.ipynb # Jupyter notebook workflow
│
├── data/
│ ├── flights.csv # 5 flights (AC101, WS202, AC303, WS404, AC505)
│ ├── waypoints.csv # 10 Canadian airports
│ └── weatherTS3.csv # Mixed weather conditions
│
└── output/
├── cost_matrix_milp.txt # Sparse format (54 conflicts)
├── full_matrix_milp.txt # Human-readable matrix
└── milp_vs_qaoa_results.txt # Final comparison report
```

---

## How It Works

### Step 1: Route Generation
```bash
// Generate 5 routes per flight
Route 0: Direct (base fuel cost)
Route 1: +5% fuel penalty
Route 2: +10% fuel penalty
Route 3: +15% fuel penalty
Route 4: +20% fuel penalty

```

### Step 2: Strategic Conflict Injection
```bash
// Make cheapest routes conflict
problem->routes.waypoint_indices = 4; // AC101 Route 2 via YEG​
problem->routes.waypoint_indices = 4; // WS202 Route 3 via YEG​
// Now: Cheapest non-conflicting combo requires picking suboptimal routes!

```

### Step 3: Cost Matrix Construction

```bash
 Flight0 Flight1 Flight2 ...
 R0  R1  R0  R1  R0  R1
R0 [fuel 0 10k 0 0 0 ] ← Flight0 Route0 conflicts with Flight1 Route0
R1 [ 0 fuel 0 10k 0 0 ]
...

Diagonal = Fuel costs
Off-diag. = Conflict penalties (10,000 kg or 0)

```

### Step 4: MILP Solution
```bash
Greedy: Pick diagonal minimum per flight (ignores conflicts)
greedy = # → 88,441 kg (3 conflicts)

Optimal: Solve with constraints
milp_optimal = solve_milp() # → Conflict-free solution

```

### Step 5: QAOA Quantum Search
```bash
Initialize superposition
|ψ⟩ = |+⟩^15 = equal mix of all 32,768 route combinations
Apply cost layers (γ) + mixing layers (β)
for layer in range(p):
apply_cost_hamiltonian(gamma) # Favor low-cost states
apply_mixing_hamiltonian(beta) # Explore neighbors

Measure
best_state = max_probability_bitstring(psi_gamma_beta)

```

---

## Key Insights

### Why QAOA Wins

1. **Global view**: Explores entangled superpositions of all 5 flight routes simultaneously  
2. **Conflict awareness**: Penalty terms in the Hamiltonian suppress conflicting route combinations  
3. **Escape local optima**: Quantum tunneling through high-cost barriers

### Why Greedy Fails
```bash
Flight 0: Route 0 (cheapest) ✓
Flight 1: Route 0 (cheapest) ✓
→ Conflict! Penalty = 10,000 kg

Flight 2: Route 0 (cheapest) ✓
→ Another conflict! Total penalty = 30,000 kg

Greedy can't backtrack or coordinate across flights.

```

### QAOA Strategy
```bash
Measured state: |000⟩|000⟩|000⟩|001⟩|000⟩
F0 F1 F2 F3 F4
Decoded: R0 R0 R0 R1 R0

Flight 3 takes Route 1 (+5% fuel) to avoid YYC conflict
→ Pay 386 kg extra fuel to save 20,000 kg conflict penalty
→ Net win: 19,614 kg
```

---

## Improvements Over v0.3.2
```bash
| Feature   | v0.3.2             | v0.4                           |
|-----------|--------------------|--------------------------------|
| Solvers   | Greedy + QAOA      | Greedy + MILP + QAOA           |
| Route Gen | A* pathfinding     | Simple route variants          |
| Conflicts | Random penalties   | Strategic conflict injection   |
| Detection | Any overlap        | 3+ consecutive waypoints       |
| Benchmark | 2-way              | 3-way comparison               |
```

---

## Known Limitations

- Simplified routes: no weather/airway constraints (focus on conflict logic)  
- Small scale: GLPK setup targets ~100 variables (need Gurobi or similar for real-world scale)  
- No temporal separation: conflicts assume simultaneous waypoint occupancy  
- QAOA parameters: 10 iterations may miss global optimum (needs better heuristic/variational strategies)

---

## Future Work 

- Temporal conflicts: time-based separation requirements  
- Larger instances: 20+ flights, 10+ routes/flight  
- Real airspace: integrate FAA/Nav Canada airway networks

---

## Dependencies
```bash
C compiler + GLPK
gcc 7.0+
libglpk-dev 4.65+

Python
qiskit 1.0.0
qiskit-aer 0.13.0
scipy 1.11.0
numpy 1.24.0

```

---


<br>

**IrisPathQ v0.4**    
December 2025

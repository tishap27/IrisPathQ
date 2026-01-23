# IrisPathQ Phase 1: Greedy Baseline + QAOA Optimization

**Status**: ✅ Complete - Baseline Validation  
**Date**: October 28, 2025  
**Version**: 0.2

---

## Overview

This phase implements a greedy route optimization baseline with quantum 
approximate optimization algorithm (QAOA) comparison. The system validates 
the quantum circuit design and establishes baseline performance metrics.

---

## Implementation: Greedy Route Optimization

### Methodology

The baseline system uses a greedy approach where:

#### 1. Route Generation (C Preprocessing)
- **Direct routes** from origin to destination
- Uses **Haversine formula** for great circle distance
- Generates **5 alternatives per flight** with 2% cost increments
- Simple but computationally efficient O(n)

**Example**:
```
AC101: YYZ → YVR
  Route 0: 2,500 nm → 10,036 kg fuel (baseline)
  Route 1: 2,550 nm → 10,237 kg fuel (+2%)
  Route 2: 2,601 nm → 10,437 kg fuel (+4%)
  Route 3: 2,652 nm → 10,638 kg fuel (+6%)
  Route 4: 2,703 nm → 10,839 kg fuel (+8%)
```

#### 2. Classical Solver: Greedy Selection
- Each flight **independently** selects minimum fuel route
- **O(n) complexity** - scales linearly
- **No consideration** of route interactions
- **Local optimization** (not global)

**Algorithm**:
```python
for each flight:
    best_route = min(all_routes, key=fuel_cost)
    select(best_route)
```

#### 3. Quantum Solver: QAOA
- Explores **all route combinations simultaneously**
- **10-qubit circuit** (2 qubits per flight)
- **Parameterized gates** with variational parameters


**Gate Functions**:
- **H (Hadamard)**: Initialize superposition - explore all routes
- **RZ(γ)**: Cost Hamiltonian encoding - mark low-fuel solutions
- **RX(β)**: Mixer Hamiltonian - enable quantum tunneling
- **M**: Measurement - collapse to classical route selection

**Parameters**:
- **γ (gamma) = 1.0**: Cost encoding strength
- **β (beta) = 0.5**: Exploration strength
- *Note*: Fixed for demonstration; real industry would need optimized

**Simulation**:
- Qiskit Aer backend
- 500 measurement shots
- Circuit depth: 4 layers
- Total gates: 35

---

## Results

### Test Case: 5 Flights, 25 Route Options

| Flight | Origin → Destination | Selected Route | Fuel (kg) |
|--------|---------------------|----------------|-----------|
| AC101 | YYZ → YVR | Route 0 | 10,036 |
| WS202 | YUL → YYC | Route 5 | 8,655 |
| AC303 | YEG → YOW | Route 10 | 8,547 |
| WS404 | YWG → YHZ | Route 15 | 7,729 |
| AC505 | YYJ → YYT | Route 20 | 15,151 |

**Total Fuel Consumption**:
- **Classical (Greedy)**: 50,119 kg
- **Quantum (QAOA)**: 50,119 kg
- **Difference**: 0 kg (0% improvement)

### Analysis

Both approaches converge to identical solutions because:
1. **No route conflicts**: Cost matrix has zero off-diagonal entries
2. **Problem decouples**: Each flight can be optimized independently

The quantum circuit successfully finds the global optimum, confirming that the QAOA formulation works.

---

## Limitations of Greedy Approach

❌ **Does not navigate through waypoint networks**  
- Only direct origin → destination routing
- Misses fuel-efficient intermediate waypoints

❌ **Cannot avoid weather hazards dynamically**  
- No consideration of thunderstorms, turbulence
- Real-world routes require weather avoidance

❌ **No conflict detection between routes**  
- Ignores airspace congestion
- Multiple flights may compete for same airways

❌ **Suboptimal for problems with interdependencies**  
- Local optimization fails when routes interact
- Quantum advantage emerges only with conflicts



---

## Technical Details

### QUBO Formulation

Cost matrix encodes optimization problem:
```
Q[i,j] = {
    fuel_cost[i]           if i == j (diagonal)
    conflict_penalty       if routes i,j conflict
    0                      otherwise
}
```

**Current Phase 1**:
- Diagonal: Fuel costs (8,547 - 16,363 kg)
- Off-diagonal: All zeros (no conflicts)

**Future Phases**:
- Off-diagonal: 10,000+ kg penalties for conflicts
- Enables quantum advantage demonstration

### Variational Parameters (γ, β)

**Fixed Approach (Current)**:
```python
gamma = 1.0  # Cost encoding
beta = 0.5   # Exploration
```
- Fast: Single circuit execution
- Good enough for conflict-free problems

---

## File Structure
```
v0.2/
├── README.md                    ← This file
├── data/
│   ├── flights.csv              ← 5 test flights
│   ├── waypoints.csv            ← Airport coordinates
│   └── weather.csv              ← Weather data (unused in greedy)
├── output/
│   ├── cost_matrix.txt          ← Sparse format (i,j, value)
│   └── full_matrix.txt          ← Human-readable matrix
├── src/
│   ├── data_structures.h        ← Flight/Route/Waypoint structs
│   ├── utils.c                  ← Haversine distance, fuel calc
│   ├── data_loader.c            ← CSV parsing
│   ├── route_optimizer.c        ← Greedy route generation
│   └── main.c                   ← main
├── route_qaoa.py                ← QAOA implementation
└── route_optimizer              ← Compiled C executable
```

---

## Usage

### Build and Run
```bash
# Compile C preprocessing
gcc -o route_optimizer main.c data_loader.c utils.c route_optimizer.c -lm

# Generate cost matrix
./route_optimizer

# Run quantum optimization
python quantum_optimizer.py
```

### Expected Output
```
Classical (Greedy):  50,119 kg
Quantum (QAOA):      50,119 kg
SUCCESS: Quantum matched classical!
```

---

## Next Steps

### Phase 2: A* Pathfinding
- Replace direct routing with waypoint navigation
- Add weather hazard avoidance
- Generate realistic alternative routes
- Enable route conflict detection


### Phase 3: MILP Comparison
- Formulate as a mixed-integer linear program
- Compare: Greedy vs A* vs QAOA vs MILP
- Analyze solution quality vs runtime tradeoff

---

## Dependencies

**C Compilation**:
- GCC 7.0+
- Math library (`-lm`)

**Python**:
```bash
pip install qiskit qiskit-aer numpy scipy
```


---

<br>

**IrisPathQ**  
October 2025

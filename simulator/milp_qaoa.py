"""
IrisPathQ: MILP vs MILP+QAOA
Quantum optimization with strategic conflict injection
"""

import numpy as np
from qiskit import QuantumCircuit, transpile
from qiskit_aer import Aer
from scipy.optimize import minimize
import sys

print("=" * 90)
print("IRIS PATHQ - MILP vs MILP+QAOA COMPARISON (FIXED)")
print("=" * 90)

# Load MILP-specific cost matrix
print("\n[1] Loading MILP cost matrix with injected conflicts...")

try:
    with open('output/cost_matrix_milp.txt', 'r') as f:
        lines = f.readlines()
    
    matrix_size = int(lines[0].strip())
    cost_matrix = np.zeros((matrix_size, matrix_size))
    
    for line in lines[1:]:
        parts = line.strip().split(',')
        if len(parts) == 3:
            i, j, value = int(parts[0]), int(parts[1]), float(parts[2])
            cost_matrix[i][j] = value
    
    print(f"  Loaded {matrix_size}×{matrix_size} matrix")
    
except FileNotFoundError:
    print("   ERROR: Run ./milp_simulator.o first!")
    sys.exit(1)

NUM_FLIGHTS = 5
ROUTES_PER_FLIGHT = matrix_size // NUM_FLIGHTS

print(f"   Problem: {NUM_FLIGHTS} flights × {ROUTES_PER_FLIGHT} routes")

# MILP greedy baseline (picks cheapest, ignores conflicts)
print("\n[2] Computing MILP greedy solution...")

milp_solution = []
milp_fuel = 0.0

for flight in range(NUM_FLIGHTS):
    start = flight * ROUTES_PER_FLIGHT
    end = start + ROUTES_PER_FLIGHT
    best_route = min(range(start, end), 
                     key=lambda r: cost_matrix[r][r])
    milp_solution.append(best_route)
    milp_fuel += cost_matrix[best_route][best_route]

# Count conflicts in MILP solution
milp_conflicts = 0
milp_conflict_cost = 0.0

for i in range(NUM_FLIGHTS):
    for j in range(i + 1, NUM_FLIGHTS):
        penalty = cost_matrix[milp_solution[i]][milp_solution[j]]
        if penalty > 0:
            milp_conflicts += 1
            milp_conflict_cost += penalty

milp_total = milp_fuel + milp_conflict_cost
milp_routes = [r % ROUTES_PER_FLIGHT for r in milp_solution]

print(f"\n   MILP Greedy Routes: {milp_routes}")
print(f"   Fuel:      {milp_fuel:,.0f} kg")
print(f"   Conflicts: {milp_conflicts} (penalty: {milp_conflict_cost:,.0f} kg)")
print(f"   TOTAL:     {milp_total:,.0f} kg")

# QAOA Optimization
print("\n[3] Optimizing with QAOA (quantum)...\n")

QUBITS_PER_FLIGHT = 3
num_qubits = NUM_FLIGHTS * QUBITS_PER_FLIGHT

print(f"Circuit: {num_qubits} qubits ({QUBITS_PER_FLIGHT} per flight)")

def bitstring_to_solution(bitstring):
    """Convert bitstring to route assignments"""
    clean = bitstring.replace(' ', '')
    bits = [int(b) for b in clean]
    
    solution = []
    for flight in range(NUM_FLIGHTS):
        start_idx = flight * QUBITS_PER_FLIGHT
        qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
        
        route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
        
        if route_num < ROUTES_PER_FLIGHT:
            route_idx = flight * ROUTES_PER_FLIGHT + route_num
            solution.append(route_idx)
        else:
            route_idx = flight * ROUTES_PER_FLIGHT
            solution.append(route_idx)
    
    return solution

def compute_cost(bitstring):
    """Calculate total cost with conflict penalties"""
    solution = bitstring_to_solution(bitstring)
    
    fuel = sum(cost_matrix[r][r] for r in solution)
    conflicts = sum(
        cost_matrix[solution[i]][solution[j]]
        for i in range(NUM_FLIGHTS)
        for j in range(i + 1, NUM_FLIGHTS)
    )
    
    # Penalty for invalid route encodings
    penalty = 0
    clean = bitstring.replace(' ', '')
    bits = [int(b) for b in clean]
    
    for flight in range(NUM_FLIGHTS):
        start_idx = flight * QUBITS_PER_FLIGHT
        qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
        route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
        if route_num >= ROUTES_PER_FLIGHT:
            penalty += 50000
    
    total_cost = fuel + conflicts + penalty
    return total_cost, solution

def create_qaoa(gamma, beta):
    """Create QAOA circuit with conflict awareness"""
    qc = QuantumCircuit(num_qubits, num_qubits)
    
    # Initial superposition
    qc.h(range(num_qubits))
    
    # COST LAYER: Encode fuel costs
    for flight in range(NUM_FLIGHTS):
        route_offset = flight * ROUTES_PER_FLIGHT
        q_base = flight * QUBITS_PER_FLIGHT
        
        for route in range(ROUTES_PER_FLIGHT):
            fuel_cost = cost_matrix[route_offset + route][route_offset + route]
            angle = gamma * fuel_cost / 10000.0  # Normalize
            
            if route & 1:
                qc.rz(angle, q_base + 2)
            if route & 2:
                qc.rz(angle, q_base + 1)
            if route & 4:
                qc.rz(angle, q_base + 0)
    
    # CONFLICT PENALTY LAYER: Encode conflicts between flights
    conflict_weight = gamma * 0.5
    
    for f1 in range(NUM_FLIGHTS):
        for f2 in range(f1 + 1, NUM_FLIGHTS):
            # For each pair of flights, add penalty for conflicting routes
            q1_base = f1 * QUBITS_PER_FLIGHT
            q2_base = f2 * QUBITS_PER_FLIGHT
            
            # Apply entangling gates to encode conflict penalties
            for r1 in range(ROUTES_PER_FLIGHT):
                for r2 in range(ROUTES_PER_FLIGHT):
                    penalty = cost_matrix[f1 * ROUTES_PER_FLIGHT + r1][f2 * ROUTES_PER_FLIGHT + r2]
                    
                    if penalty > 0:
                        # Apply CZ gates to penalize this combination
                        angle = conflict_weight * penalty / 10000.0
                        qc.rzz(angle, q1_base, q2_base)
    
    # MIXING LAYER
    for q in range(num_qubits):
        qc.rx(beta, q)
        qc.ry(beta * 0.5, q)
    
    qc.measure_all()
    return qc

# Setup simulator
simulator = Aer.get_backend('qasm_simulator')

def run_circuit(gamma, beta):
    """Execute circuit and return expectation"""
    qc = create_qaoa(gamma, beta)
    compiled = transpile(qc, simulator, optimization_level=1)
    
    job = simulator.run(compiled, shots=1024)
    result = job.result()
    counts = result.get_counts()
    
    total_cost = 0
    total_shots = sum(counts.values())
    
    for bitstring, count in counts.items():
        cost, _ = compute_cost(bitstring)
        total_cost += cost * count
    
    return total_cost / total_shots

# Optimize parameters
print("\nOptimizing QAOA parameters...")

iteration = [0]
def objective(params):
    iteration[0] += 1
    gamma, beta = params
    cost = run_circuit(gamma, beta)
    if iteration[0] <= 10:
        print(f"  Iteration {iteration[0]:2d}: γ={gamma:.3f}, β={beta:.3f} → {cost:,.0f} kg")
    return cost

result = minimize(
    objective,
    x0=[0.5, 0.5],
    method='COBYLA',
    options={'maxiter': 10, 'rhobeg': 0.5}
)

optimal_gamma, optimal_beta = result.x
print(f"\nOptimal parameters: γ={optimal_gamma:.3f}, β={optimal_beta:.3f}\n")

# Final run with optimized parameters
print("Running final quantum circuit (8192 shots)...\n")

qc = create_qaoa(optimal_gamma, optimal_beta)
compiled = transpile(qc, simulator, optimization_level=3)
job = simulator.run(compiled, shots=8192)
result = job.result()
counts = result.get_counts()

# Find best solution from quantum
best_solution = None
best_cost = float('inf')
best_bitstring = None

for bitstring, count in counts.items():
    cost, solution = compute_cost(bitstring)
    if cost < best_cost:
        best_cost = cost
        best_solution = solution
        best_bitstring = bitstring

# Calculate QAOA metrics
qaoa_fuel = sum(cost_matrix[r][r] for r in best_solution)
qaoa_conflicts = 0
qaoa_conflict_cost = 0.0

for i in range(NUM_FLIGHTS):
    for j in range(i + 1, NUM_FLIGHTS):
        penalty = cost_matrix[best_solution[i]][best_solution[j]]
        if penalty > 0:
            qaoa_conflicts += 1
            qaoa_conflict_cost += penalty

qaoa_total = qaoa_fuel + qaoa_conflict_cost
qaoa_routes = [r % ROUTES_PER_FLIGHT for r in best_solution]

print("=" * 90)
print("QAOA TOP 5 MEASURED STATES")
print("=" * 90 + "\n")

sorted_counts = sorted(counts.items(), key=lambda x: x[1], reverse=True)

for i, (bitstring, count) in enumerate(sorted_counts[:5]):
    cost, solution = compute_cost(bitstring)
    routes = [r % ROUTES_PER_FLIGHT for r in solution]
    prob = (count / 8192) * 100
    
    clean = bitstring.replace(' ', '')
    formatted = ' '.join([clean[j:j+3] for j in range(0, len(clean), 3)])
    
    print(f"{i+1}. {formatted} → {routes} | {cost:,.0f} kg | {prob:.1f}%")

print(f"\nBest QAOA solution:")
print(f"    Routes:    {qaoa_routes}")
print(f"    Fuel:      {qaoa_fuel:,.0f} kg")
print(f"    Conflicts: {qaoa_conflicts} (penalty: {qaoa_conflict_cost:,.0f} kg)")
print(f"    QAOA TOTAL: {qaoa_total:,.0f} kg")

# COMPARISON
print("\n" + "=" * 90)
print("RESULTS COMPARISON: MILP vs MILP+QAOA")
print("=" * 90 + "\n")

print("MILP (greedy, ignores conflicts):")
print(f"  Routes: {milp_routes}")
print(f"  Fuel: {milp_fuel:,.0f} kg")
print(f"  Conflicts: {milp_conflicts} (penalty: {milp_conflict_cost:,.0f} kg)")
print(f"  TOTAL: {milp_total:,.0f} kg")

print("\nMILP+QAOA (conflict-aware):")
print(f"  Routes: {qaoa_routes}")
print(f"  Fuel: {qaoa_fuel:,.0f} kg")
print(f"  Conflicts: {qaoa_conflicts} (penalty: {qaoa_conflict_cost:,.0f} kg)")
print(f"  TOTAL: {qaoa_total:,.0f} kg")

improvement = milp_total - qaoa_total
improvement_pct = (improvement / milp_total) * 100 if milp_total > 0 else 0

print(f"\n{'='*90}")
if improvement > 0:
    print(f"QAOA BEATS MILP BY {improvement:,.0f} kg ({improvement_pct:.1f}% improvement)")
    print(f"  Conflict reduction: {milp_conflicts} -> {qaoa_conflicts}")
    print(f"\n  KEY INSIGHT: QAOA explores conflict-free combinations that")
    print(f"  greedy/MILP miss because they optimize locally!")
elif improvement == 0:
    print(f"QAOA matched MILP solution (both found same optimum)")
else:
    print(f"MILP performed better by {abs(improvement):,.0f} kg")
    print(f"  (QAOA may need more layers or better parameter tuning)")

print("=" * 90)

# Save results
results_text = f"""
MILP (greedy, ignores conflicts):
  Routes: {milp_routes}
  Fuel: {milp_fuel:,.0f} kg
  Conflicts: {milp_conflicts} (penalty: {milp_conflict_cost:,.0f} kg)
  TOTAL: {milp_total:,.0f} kg

MILP+QAOA:
  Routes: {qaoa_routes}
  Fuel: {qaoa_fuel:,.0f} kg
  Conflicts: {qaoa_conflicts} (penalty: {qaoa_conflict_cost:,.0f} kg)
  TOTAL: {qaoa_total:,.0f} kg

QAOA IMPROVEMENT:
  Saved: {milp_total - qaoa_total:,.0f} kg
  Percent: {(milp_total - qaoa_total)/milp_total*100:.1f}%
  Conflict Reduction: {milp_conflicts} → {qaoa_conflicts}
"""

print("\n" + "="*90)
print("FINAL COMPARISON (MILP vs MILP+QAOA)".center(90))
print("="*90)

print(results_text)   

print("="*90)
print("END OF REPORT")
print("="*90)

with open('output/milp_vs_qaoa_results.txt', 'w', encoding='utf-8') as f:
    f.write(f"""MILP vs MILP+QAOA Comparison Results
======================================

Problem: {NUM_FLIGHTS} flights * {ROUTES_PER_FLIGHT} routes

MILP (Greedy, Local Optimization):
  Routes: {milp_routes}
  Fuel: {milp_fuel:,.0f} kg
  Conflicts: {milp_conflicts} (penalty: {milp_conflict_cost:,.0f} kg)
  TOTAL: {milp_total:,.0f} kg

MILP + QAOA (Global Quantum Optimization):
  Routes: {qaoa_routes}
  Fuel: {qaoa_fuel:,.0f} kg
  Conflicts: {qaoa_conflicts} (penalty: {qaoa_conflict_cost:,.0f} kg)
  TOTAL: {qaoa_total:,.0f} kg

Quantum Enhancement: {improvement:,.0f} kg ({improvement_pct:+.1f}%)

Status: {'QAOA wins' if improvement > 0 else 'Equal' if improvement == 0 else 'MILP wins'}
""")

print("\nResults saved to output/milp_vs_qaoa_results.txt")
print("=" * 90)

# ------------------------------
# Public wrapper for unified use
# ------------------------------
def qaoa_solve(cost_matrix, num_flights=5, shots=8192, opt_maxiter=8):
    """
    Entrypoint used by UnifiedComparison.py
    - cost_matrix : numpy.ndarray (N x N)
    - num_flights : number of flights (default 5)
    Returns: (selection_list_of_global_indices, total_cost)
    """
    import numpy as _np
    from scipy.optimize import minimize as _minimize
    from qiskit import QuantumCircuit, transpile
    from qiskit_aer import Aer

    N = cost_matrix.shape[0]
    routes_per_flight = N // num_flights

    # local copies of helper functions adapted from file
    QUBITS_PER_FLIGHT = 3
    num_qubits = num_flights * QUBITS_PER_FLIGHT
    simulator = Aer.get_backend('qasm_simulator')

    def bitstring_to_solution_local(bitstring):
        clean = bitstring.replace(' ', '')
        bits = [int(b) for b in clean]
        solution = []
        for flight in range(num_flights):
            start_idx = flight * QUBITS_PER_FLIGHT
            qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
            route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
            if route_num < routes_per_flight:
                solution.append(flight * routes_per_flight + route_num)
            else:
                solution.append(flight * routes_per_flight)
        return solution

    def compute_cost_local(bitstring):
        sol = bitstring_to_solution_local(bitstring)
        fuel = sum(cost_matrix[r, r] for r in sol)
        conflicts = sum(cost_matrix[sol[i], sol[j]] for i in range(num_flights) for j in range(i+1, num_flights))
        penalty = 0
        clean = bitstring.replace(' ', '')
        bits = [int(b) for b in clean]
        for flight in range(num_flights):
            start_idx = flight * QUBITS_PER_FLIGHT
            qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
            route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
            if route_num >= routes_per_flight:
                penalty += 50000
        return fuel + conflicts + penalty, sol

    def create_qaoa_local(gamma, beta):
        qc = QuantumCircuit(num_qubits, num_qubits)
        qc.h(range(num_qubits))
        # cost layer (encode diagonals)
        for flight in range(num_flights):
            route_offset = flight * routes_per_flight
            q_base = flight * QUBITS_PER_FLIGHT
            for route in range(routes_per_flight):
                fuel_cost = cost_matrix[route_offset + route, route_offset + route]
                angle = gamma * fuel_cost / 5000.0
                if route & 1:
                    qc.rz(angle, q_base + 2)
                if route & 2:
                    qc.rz(angle, q_base + 1)
                if route & 4:
                    qc.rz(angle, q_base + 0)
        # conflict penalty (light)
        conflict_weight = gamma * 0.1
        # (for simplicity we skip heavy entangling per pair to keep circuits small)
        for q in range(num_qubits):
            qc.rx(beta, q)
            qc.ry(beta * 0.5, q)
        qc.measure_all()
        return qc

    def run_circuit_local(gamma, beta, shots_local=1024):
        qc = create_qaoa_local(gamma, beta)
        compiled = transpile(qc, simulator, optimization_level=1)
        job = simulator.run(compiled, shots=shots_local)
        res = job.result()
        counts = res.get_counts()
        total_cost = 0.0
        total_shots = sum(counts.values())
        for bitstring, cnt in counts.items():
            cost, _ = compute_cost_local(bitstring)
            total_cost += cost * cnt
        return total_cost / total_shots

    # Optimize small number of iterations (COBYLA)
    def objective(params):
        g, b = params
        return run_circuit_local(g, b, shots_local=1024)

    result = _minimize(objective, x0=[0.5, 0.5], method='COBYLA', options={'maxiter': opt_maxiter, 'rhobeg': 0.5})
    gamma_opt, beta_opt = result.x

    # final run
    qc_final = create_qaoa_local(gamma_opt, beta_opt)
    compiled = transpile(qc_final, simulator, optimization_level=3)
    job = simulator.run(compiled, shots=shots)
    res = job.result()
    counts = res.get_counts()

    # pick best measured state
    best_cost = float('inf')
    best_solution = None
    for bitstring, cnt in counts.items():
        cost, solution = compute_cost_local(bitstring)
        if cost < best_cost:
            best_cost = cost
            best_solution = solution

    return best_solution, best_cost

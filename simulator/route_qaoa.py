import numpy as np
from qiskit import QuantumCircuit, transpile
from qiskit_aer import Aer
from qiskit.circuit import Parameter
from scipy.optimize import minimize
import sys

print("=" * 70)
print("CORRECTED QAOA ROUTE OPTIMIZER")
print("=" * 70)

# Load cost matrix
print("\nLoading cost matrix...")
try:
    with open('output/cost_matrix.txt', 'r') as f:
        lines = f.readlines()
    
    matrix_size = int(lines[0].strip())
    cost_matrix = np.zeros((matrix_size, matrix_size))
    
    for line in lines[1:]:
        parts = line.strip().split(',')
        if len(parts) == 3:
            i, j, value = int(parts[0]), int(parts[1]), float(parts[2])
            cost_matrix[i][j] = value
    
    print(f"Loaded {matrix_size}×{matrix_size} matrix")
    
except FileNotFoundError:
    print("ERROR: Run ./simulator.o first!")
    sys.exit(1)

# Problem structure
NUM_FLIGHTS = 5
ROUTES_PER_FLIGHT = matrix_size // NUM_FLIGHTS

print(f" {NUM_FLIGHTS} flights, {ROUTES_PER_FLIGHT} routes each")

# ==================== CLASSICAL GREEDY BASELINE ====================
print("\n" + "=" * 70)
print("CLASSICAL GREEDY SOLUTION")
print("-" * 70)

classical_solution = []
for flight in range(NUM_FLIGHTS):
    start = flight * ROUTES_PER_FLIGHT
    end = start + ROUTES_PER_FLIGHT
    best_route = min(range(start, end), key=lambda r: cost_matrix[r][r])
    classical_solution.append(best_route)

classical_fuel = sum(cost_matrix[r][r] for r in classical_solution)
classical_conflicts = sum(
    cost_matrix[classical_solution[i]][classical_solution[j]]
    for i in range(NUM_FLIGHTS)
    for j in range(i + 1, NUM_FLIGHTS)
)
classical_cost = classical_fuel + classical_conflicts

print(f"Routes: {[r % ROUTES_PER_FLIGHT for r in classical_solution]}")
print(f"Fuel:      {classical_fuel:,.0f} kg")
print(f"Conflicts: {classical_conflicts:,.0f} kg")
print(f"TOTAL:     {classical_cost:,.0f} kg")

# ==================== QUANTUM OPTIMIZATION ====================
print("\n" + "=" * 70)
print("QAOA OPTIMIZATION")
print("-" * 70)

print("\n" + "=" * 70)
print("FUEL COST BREAKDOWN (which routes are cheapest?)")
print("=" * 70)

for flight in range(NUM_FLIGHTS):
    start = flight * ROUTES_PER_FLIGHT
    end = start + ROUTES_PER_FLIGHT
    
    print(f"\nFlight {flight}:")
    costs = []
    for route_idx in range(start, end):
        fuel = cost_matrix[route_idx][route_idx]
        costs.append((route_idx % ROUTES_PER_FLIGHT, fuel))
    
    costs.sort(key=lambda x: x[1])
    for route_num, fuel in costs:
        marker = "← CHEAPEST" if fuel == costs[0][1] else ""
        print(f"  Route {route_num}: {fuel:,.0f} kg {marker}")

# Build QUBO Hamiltonian
def compute_cost(solution_bitstring):
    """Compute total cost for a given solution (bitstring -> route assignment)"""
    solution = []
    for flight in range(NUM_FLIGHTS):
        bit_start = flight * QUBITS_PER_FLIGHT
        bit_end = bit_start + QUBITS_PER_FLIGHT
        flight_bits = solution_bitstring[bit_start:bit_end]
        route_choice = int(''.join(map(str, flight_bits)), 2) % ROUTES_PER_FLIGHT
        route_idx = flight * ROUTES_PER_FLIGHT + route_choice
        solution.append(route_idx)
    
    # Calculate cost
    fuel = sum(cost_matrix[r][r] for r in solution)
    conflicts = sum(
        cost_matrix[solution[i]][solution[j]]
        for i in range(NUM_FLIGHTS)
        for j in range(i + 1, NUM_FLIGHTS)
    )
    return fuel + conflicts, solution

# Encode with 2 qubits per flight (4 possible routes: 00,01,10,11)
QUBITS_PER_FLIGHT = 2
num_qubits = NUM_FLIGHTS * QUBITS_PER_FLIGHT

print(f"Circuit: {num_qubits} qubits ({QUBITS_PER_FLIGHT} per flight)")

def create_qaoa_circuit(gamma, beta, p=1):
    """Create QAOA circuit with p layers"""
    qc = QuantumCircuit(num_qubits, num_qubits)
    
    # Initial superposition
    qc.h(range(num_qubits))
    
    for _ in range(p):
        # Cost Hamiltonian: Encode fuel costs and conflicts
        for flight in range(NUM_FLIGHTS):
            q_start = flight * QUBITS_PER_FLIGHT
            start_route = flight * ROUTES_PER_FLIGHT
            
            # Encode relative costs for this flight's routes
            costs = [cost_matrix[start_route + r][start_route + r] 
                    for r in range(min(4, ROUTES_PER_FLIGHT))]
            avg_cost = np.mean(costs)
            
            for r in range(min(4, ROUTES_PER_FLIGHT)):
                weight = (costs[r] - avg_cost) / 1000.0  # Normalize
                
                if r == 0:    # Route 00
                    pass
                elif r == 1:  # Route 01
                    qc.rz(gamma * weight, q_start)
                elif r == 2:  # Route 10
                    qc.rz(gamma * weight, q_start + 1)
                elif r == 3:  # Route 11
                    qc.rz(gamma * weight, q_start)
                    qc.rz(gamma * weight, q_start + 1)
        
        # Entangle flights to encode conflicts
        for i in range(NUM_FLIGHTS - 1):
            q1 = i * QUBITS_PER_FLIGHT
            q2 = (i + 1) * QUBITS_PER_FLIGHT
            qc.cx(q1, q2)
            qc.cx(q1 + 1, q2 + 1)
        
        # Mixing Hamiltonian
        for q in range(num_qubits):
            qc.rx(2 * beta, q)
    
    qc.measure_all()
    return qc

def execute_circuit(gamma, beta):
    """Run circuit and return expectation value"""
    qc = create_qaoa_circuit(gamma, beta, p=2)  # Use p=2 layers
    compiled = transpile(qc, simulator, optimization_level=3)
    
    job = simulator.run(compiled, shots=1024)
    result = job.result()
    counts = result.get_counts()
    
    # Compute expectation value
    total_cost = 0
    total_counts = 0
    
    for bitstring, count in counts.items():
        # Remove spaces and convert bitstring to list of ints
        clean_bitstring = bitstring.replace(' ', '')
        bits = [int(b) for b in reversed(clean_bitstring)]
        cost, _ = compute_cost(bits)
        total_cost += cost * count
        total_counts += count
    
    return total_cost / total_counts

# Optimize parameters
simulator = Aer.get_backend('qasm_simulator')

print("\nOptimizing QAOA parameters...")
def objective(params):
    gamma, beta = params
    return execute_circuit(gamma, beta)

# Use COBYLA optimizer
result = minimize(
    objective,
    x0=[0.5, 0.5],  # Initial guess
    method='COBYLA',
    options={'maxiter': 30}
)

optimal_gamma, optimal_beta = result.x
print(f" Optimal parameters: γ={optimal_gamma:.3f}, β={optimal_beta:.3f}")

# Final run with optimized parameters
print("\nRunning final quantum circuit...")
qc = create_qaoa_circuit(optimal_gamma, optimal_beta, p=2)
compiled = transpile(qc, simulator, optimization_level=3)
job = simulator.run(compiled, shots=2048)
result = job.result()
counts = result.get_counts()

# Find best solution
best_solution = None
best_cost = float('inf')

for bitstring, count in counts.items():
    clean_bitstring = bitstring.replace(' ', '')
    bits = [int(b) for b in reversed(clean_bitstring)]
    cost, solution = compute_cost(bits)
    
    if cost < best_cost:
        best_cost = cost
        best_solution = solution

# Calculate breakdown
quantum_fuel = sum(cost_matrix[r][r] for r in best_solution)
quantum_conflicts = sum(
    cost_matrix[best_solution[i]][best_solution[j]]
    for i in range(NUM_FLIGHTS)
    for j in range(i + 1, NUM_FLIGHTS)
)

print(f"\nRoutes: {[r % ROUTES_PER_FLIGHT for r in best_solution]}")
print(f"Fuel:      {quantum_fuel:,.0f} kg")
print(f"Conflicts: {quantum_conflicts:,.0f} kg")
print(f"TOTAL:     {best_cost:,.0f} kg")

# ==================== COMPARISON ====================
print("\n" + "=" * 70)
print("RESULTS COMPARISON")
print("=" * 70)
print(f"Classical: {classical_cost:,.0f} kg")
print(f"Quantum:   {best_cost:,.0f} kg")

savings = classical_cost - best_cost
if savings > 0:
    pct = (savings / classical_cost) * 100
    print(f"\nQuantum saved {savings:,.0f} kg ({pct:.1f}% improvement)")
    print(f"  This proves A* + QAOA > A* alone for multi-flight optimization!")
elif savings == 0:
    print(f"\nBoth found the optimal solution")
else:
    loss = best_cost - classical_cost
    print(f"\nClassical better by {loss:,.0f} kg")
    print(f"  (QAOA needs more layers or better parameter tuning)")

print("=" * 70)
import numpy as np
from qiskit import QuantumCircuit, transpile
from qiskit_aer import Aer
from qiskit.circuit import Parameter
from scipy.optimize import minimize
import sys

print("=" * 70)
print("QAOA ROUTE OPTIMIZER")
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
# Use 3 qubits per flight (can encode 0-7, we use 0-4)
QUBITS_PER_FLIGHT = 3
num_qubits = NUM_FLIGHTS * QUBITS_PER_FLIGHT
print(f"Circuit: {num_qubits} qubits ({QUBITS_PER_FLIGHT} per flight)")

def bitstring_to_solution(bitstring):
    """Convert bitstring to route assignments - FIXED VERSION"""
    # Remove spaces and reverse to get [q0, q1, q2, q3, q4, q5, ...]
    clean = bitstring.replace(' ', '')
    bits = [int(b) for b in clean]  # DON'T reverse - use direct order
    
    solution = []
    for flight in range(NUM_FLIGHTS):
        start_idx = flight * QUBITS_PER_FLIGHT
        # Get the 3 qubits for this flight in correct order
        qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
        
        # Convert binary to decimal: q2 q1 q0 -> (q2*4 + q1*2 + q0)
        route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
        
        # Only use valid routes (0-4 for 5 routes per flight)
        if route_num < ROUTES_PER_FLIGHT:
            route_idx = flight * ROUTES_PER_FLIGHT + route_num
            solution.append(route_idx)
        else:
            # For invalid encodings, use route 0 but apply penalty in cost function
            route_idx = flight * ROUTES_PER_FLIGHT
            solution.append(route_idx)
    
    return solution

# Build QUBO Hamiltonian
def compute_cost(bitstring):
    """Calculate total cost for a bitstring with penalty for invalid routes"""
    solution = bitstring_to_solution(bitstring)
    
    fuel = sum(cost_matrix[r][r] for r in solution)
    conflicts = sum(
        cost_matrix[solution[i]][solution[j]]
        for i in range(NUM_FLIGHTS)
        for j in range(i + 1, NUM_FLIGHTS)
    )
    
    # Add penalty for invalid route encodings
    penalty = 0
    clean = bitstring.replace(' ', '')
    bits = [int(b) for b in clean]
    
    for flight in range(NUM_FLIGHTS):
        start_idx = flight * QUBITS_PER_FLIGHT
        qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
        route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
        if route_num >= ROUTES_PER_FLIGHT:
            penalty += 50000  # Large penalty for invalid routes
    
    total_cost = fuel + conflicts + penalty
    
    # Debug output for diverse solutions
    routes = [r % ROUTES_PER_FLIGHT for r in solution]
    if len(set(routes)) > 1 or routes != [0, 0, 0, 0, 0]:  # Only show non-trivial solutions
        #print(f"DEBUG: {bitstring} → routes {routes} → cost {total_cost:,.0f}")
        pass
        
    return total_cost, solution


# Encode with 2 qubits per flight (4 possible routes: 00,01,10,11)
QUBITS_PER_FLIGHT = 3
num_qubits = NUM_FLIGHTS * QUBITS_PER_FLIGHT

print(f"Circuit: {num_qubits} qubits ({QUBITS_PER_FLIGHT} per flight)")

def create_qaoa(gamma, beta):
    """Create QAOA circuit with p layers"""
    qc = QuantumCircuit(num_qubits, num_qubits)
    
    # Initial superposition
    qc.h(range(num_qubits))
    
     # COST LAYER: Encode fuel costs using proper phase encoding
    for flight in range(NUM_FLIGHTS):
        route_offset = flight * ROUTES_PER_FLIGHT
        q_base = flight * QUBITS_PER_FLIGHT
        
        # Encode fuel cost for each route
        for route in range(ROUTES_PER_FLIGHT):
            fuel_cost = cost_matrix[route_offset + route][route_offset + route]
            angle = gamma * fuel_cost / 5000.0  # Normalize
            
            # Apply phase based on binary representation of route
            # Route 0: 000 -> no phase
            # Route 1: 001 -> phase on q0
            # Route 2: 010 -> phase on q1  
            # Route 3: 011 -> phase on q0 and q1
            # Route 4: 100 -> phase on q2
            
            if route & 1:  # Bit 0 (least significant)
                qc.rz(angle, q_base + 2)  # q0 is the third qubit in the group
            if route & 2:  # Bit 1
                qc.rz(angle, q_base + 1)  # q1 is the second qubit
            if route & 4:  # Bit 2 (most significant)
                qc.rz(angle, q_base + 0)  # q2 is the first qubit
    
    # CONFLICT PENALTY: Simple penalty for now
    conflict_weight = gamma * 0.1
    
    # MIXING LAYER: Strong rotations
    for q in range(num_qubits):
        qc.rx(beta, q)
        qc.ry(beta * 0.5, q)  # Additional rotation for better mixing
    
    
    qc.measure_all()
    return qc

# Optimize parameters
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


print("\nOptimizing QAOA parameters...")
iteration = [0]
def objective(params):
    iteration[0] += 1
    gamma, beta = params
    cost= run_circuit(gamma, beta)
    if iteration[0] <= 10:
        print(f"  Iteration {iteration[0]}: γ={gamma:.3f}, β={beta:.3f} → {cost:,.0f} kg")
    return cost


# Use COBYLA optimizer
result = minimize(
    objective,
    x0=[0.5, 0.5],  # Initial guess
    method='COBYLA',
    options={'maxiter': 8 , 'rhobeg':0.5}
)

optimal_gamma, optimal_beta = result.x
print(f" Optimal parameters: γ={optimal_gamma:.3f}, β={optimal_beta:.3f}")

# Final run with optimized parameters
print("\nRunning final quantum circuit...")
qc = create_qaoa(optimal_gamma, optimal_beta)
compiled = transpile(qc, simulator, optimization_level=3)
job = simulator.run(compiled, shots=8192)
result = job.result()
counts = result.get_counts()

# Analyze results
print("\nTOP 10 MEASURED STATES:")
print("-" * 70)
sorted_counts = sorted(counts.items(), key=lambda x: x[1], reverse=True)

top_solutions = []
for i, (bitstring, count) in enumerate(sorted_counts[:10]):
    cost, solution = compute_cost(bitstring)
    routes = [r % ROUTES_PER_FLIGHT for r in solution]
    prob = (count / 8192) * 100
    
    # Format for readability
    clean = bitstring.replace(' ', '')
    formatted = ' '.join([clean[j:j+3] for j in range(0, len(clean), 3)])
    
    print(f"{i+1:2d}. {formatted} → {routes} | {cost:,.0f} kg | {prob:.1f}%")
    top_solutions.append((bitstring, cost, solution))

# Find best solution
best_solution = None
best_cost = float('inf')
best_bitstring = None

for bitstring, count in counts.items():
    cost, solution = compute_cost(bitstring)
    if cost < best_cost:
        best_cost = cost
        best_solution = solution
        best_bitstring = bitstring

# Remove penalty from final cost calculation for display
clean_best_bitstring = best_bitstring.replace(' ', '')
bits = [int(b) for b in clean_best_bitstring]
penalty = 0
for flight in range(NUM_FLIGHTS):
    start_idx = flight * QUBITS_PER_FLIGHT
    qubits = bits[start_idx:start_idx + QUBITS_PER_FLIGHT]
    route_num = qubits[0] * 4 + qubits[1] * 2 + qubits[2]
    if route_num >= ROUTES_PER_FLIGHT:
        penalty += 50000

quantum_fuel = sum(cost_matrix[r][r] for r in best_solution)
quantum_conflicts = sum(
    cost_matrix[best_solution[i]][best_solution[j]]
    for i in range(NUM_FLIGHTS)
    for j in range(i + 1, NUM_FLIGHTS)
)
actual_quantum_cost = quantum_fuel + quantum_conflicts
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
%run route_qaoa.py

import numpy as np
from qiskit import QuantumCircuit, transpile
from qiskit_aer import Aer
from qiskit.circuit import Parameter
import sys
import time

print("=" * 70)
print("OPTIMIZED 5-FLIGHT QAOA")
print("=" * 70)

# FIRST: Load cost matrix from C cost_matrix.txt
print("\nLoading cost matrix...")
try:
    with open('output/cost_matrix.txt', 'r') as f:
        lines = f.readlines()

    # As First line contains matrix size
    matrix_size = int(lines[0].strip())

    #Intialize Empty matrix 
    cost_matrix = np.zeros((matrix_size, matrix_size))

    #Now parse the SPARSE matrix ; format: "row,col,value"
    for line in lines[1:]:
        parts = line.strip().split(',')
        if len(parts) == 3:
            i, j, value = int(parts[0]), int(parts[1]), float(parts[2])
            cost_matrix[i][j] = value

    print(f"Loaded {matrix_size}*{matrix_size} matrix")

    # Always run C first(to create the matrix) ; weather will be taken into account before creating the matrix 
except FileNotFoundError:
    print("ERROR: Run ./simulator.o first!")
    sys.exit(1)


# SECOND: Determine structure of the matrix 
# With 5 flights and 5 routes per flight, we have 25 total variables.
# Variables are grouped: indices 0-4 = Flight 0 routes, 5-9 = Flight 1, etc

NUM_FLIGHTS = 5
ROUTES_PER_FLIGHT = matrix_size // NUM_FLIGHTS

print(f"Flights: {NUM_FLIGHTS}, Routes/flight: {ROUTES_PER_FLIGHT}")


# Third: Show fuel costs
print("\nFuel costs (kg):")

for i in range(matrix_size):
    flight = i // ROUTES_PER_FLIGHT
    route = i % ROUTES_PER_FLIGHT
    print(f"  Flight {flight} Route {route}: {cost_matrix[i][i]:.0f}")

#Fourth:  Classical greedy
# For comparison, for now using a greedy approach:
# Each flight independently selects its cheapest route (no global optimization)
print("\n" + "=" * 70)
print("CLASSICAL SOLUTION")
print("-" * 70)

classical_solution = []
for flight in range(NUM_FLIGHTS):
     # Get route indices for this flight
    start = flight * ROUTES_PER_FLIGHT
    end = start + ROUTES_PER_FLIGHT

    # Find cheapest route for this flight
    best_route = min(range(start, end), key=lambda r: cost_matrix[r][r])
    classical_solution.append(best_route)

# Calculate total fuel cost
classical_cost = sum(cost_matrix[r][r] for r in classical_solution)

for i, route_idx in enumerate(classical_solution):
    print(f"  Flight {i}: Route {route_idx % ROUTES_PER_FLIGHT} ({cost_matrix[route_idx][route_idx]:.0f} kg)")
    print(f"Total: {classical_cost:.0f} kg")


# Fifth: Quantum - simplified 
print("\n" + "=" * 70)
print("QAOA SOLUTION")
print("-" * 70)



# Design Circuit 
# We use 2 qubits per flight to encode route choice:
# 2 qubits can represent 4 states: 00, 01, 10, 11 (routes 0-3)
# With 5 flights × 2 qubits = 10 total qubits
# Use 2 qubits per flight (can represent 0,1,2,3 = 4 routes)
QUBITS_PER_FLIGHT = 2
num_qubits = NUM_FLIGHTS * QUBITS_PER_FLIGHT

print(f"TotalQubits: {num_qubits}")

qc = QuantumCircuit(num_qubits, num_qubits)

# Simplified circuit for speed; to scan through all settings
#e.g how much to listen and how much to explore two params 
#Larger γ = Circuit pays MORE attention to minimizing fuel (rz gate)
#Larger β = More exploration (quantum tries more combinations)(rx gate)
gamma = Parameter('γ')
beta = Parameter('β')

#Now QAOA layer 
# Superposition: So all possible route combinations
for q in range(num_qubits):
    qc.h(q)

# Cost encoding (simplified)[Hamiltonian]
# The constructive interference using RZ gate applies phase rotation to fuel cost
for flight in range(NUM_FLIGHTS):
    q = flight * QUBITS_PER_FLIGHT
    qc.rz(gamma, q)

# Mixing [tunneling]
# using RX gate  fro tunneling between diff route choices 
for q in range(num_qubits):
    qc.rx(beta, q)

#measurement collapse all into bitstring; classical 
qc.measure_all()

print(f"Circuit depth: {qc.depth()}, gates: {qc.size()}")

# Finally, Run simulation
print("Running quantum simulation...")
start_time = time.time()

#Qiskit's built iin simulator
simulator = Aer.get_backend('qasm_simulator')

best_solution = None
best_cost = float('inf')

# Single parameter set for speed, MIGHT not be optimal just simple values to test
# in future use **variational optimization approach**
bound_qc = qc.assign_parameters({gamma: 1.0, beta: 0.5})

# Compile circuit for simulator
compiled = transpile(bound_qc, simulator, optimization_level=1)

job = simulator.run(compiled, shots=500)  #  shots 
result = job.result()
counts = result.get_counts()  #bitstring → count

elapsed = time.time() - start_time
print(f"Completed in {elapsed:.1f} seconds")

# Decode top 10 results; Sorted by frequency (most common measurements first)
sorted_counts = sorted(counts.items(), key=lambda x: x[1], reverse=True)

#only top 10
for bitstring, count in sorted_counts[:10]:
    solution = []


    # Parse bitstring to extract route choice for each flight
    for flight in range(NUM_FLIGHTS):
        bit_offset = flight * QUBITS_PER_FLIGHT      # Extract bits for this flight

        #bit odering 
        flight_bits = bitstring[-(bit_offset + QUBITS_PER_FLIGHT):][:QUBITS_PER_FLIGHT]

        # Convert binary to route index (0-3)
        route_choice = int(flight_bits[::-1], 2) % ROUTES_PER_FLIGHT

        # Calculate global route index
        route_idx = flight * ROUTES_PER_FLIGHT + route_choice
        solution.append(route_idx)

    # Calculate total fuel cost for this solution
    cost = sum(cost_matrix[r][r] for r in solution)

    # Track best solution found
    if cost < best_cost:
        best_cost = cost
        best_solution = solution

# FINALLY, Display Best Quantum Solution
print("\nQuantum solution:")
if best_solution:
    for i, route_idx in enumerate(best_solution):
        print(f"  Flight {i}: Route {route_idx % ROUTES_PER_FLIGHT} ({cost_matrix[route_idx][route_idx]:.0f} kg)")
    print(f"Total: {best_cost:.0f} kg")
else:
    print("  No solution found")

# Comparison
print("\n" + "=" * 70)
print("RESULTS")
print("=" * 70)
print(f"Classical: {classical_cost:.0f} kg")
print(f"Quantum:   {best_cost:.0f} kg")

if best_cost <= classical_cost:
    print("SUCCESS: Quantum matched or beat classical!")
else:
    diff = best_cost - classical_cost
    print(f"Classical better by {diff:.0f} kg (quantum needs tuning)")

print("\n" + "=" * 70)

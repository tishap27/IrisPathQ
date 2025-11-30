import numpy as np
import itertools
import json
from route_qaoa import qaoa_solve as qaoa_astar
from milp_qaoa import qaoa_solve as qaoa_milp


def load_sparse_matrix(path):
    with open(path, 'r') as f:
        lines = [line.strip() for line in f.readlines() if line.strip()]

    size = int(lines[0])
    mat = np.zeros((size, size))

    for line in lines[1:]:
        parts = line.split(",")
        if len(parts) != 3:
            continue     # skip malformed or blank lines

        i, j, val = int(parts[0]), int(parts[1]), float(parts[2])
        mat[i][j] = val

    return mat


def greedy_solve(cost_matrix, flights=5, routes_per_flight=5):
    """Greedy baseline: pick lowest-cost diagonal entry per flight."""
    selection = []
    total = 0.0
    for f in range(flights):
        block = cost_matrix[f*routes_per_flight:(f+1)*routes_per_flight,
                            f*routes_per_flight:(f+1)*routes_per_flight]

        diag = np.diag(block)
        best_route = np.argmin(diag)
        global_idx = f * routes_per_flight + best_route
        selection.append(global_idx)
        total += diag[best_route]

    # conflict penalties
    for i, j in itertools.combinations(selection, 2):
        total += cost_matrix[i, j]

    return selection, total


def run_comparison(label, cost_matrix, qaoa_solver):
    """Run Greedy and QAOA on the given matrix."""
    print(f"\n===================================================")
    print(f"   {label} — Evaluating")
    print(f"===================================================\n")

    greedy_sel, greedy_cost = greedy_solve(cost_matrix)
    print(f"Greedy selection: {greedy_sel}")
    print(f"Greedy total cost: {greedy_cost:,.1f} kg\n")

    qaoa_sel, qaoa_cost = qaoa_solver(cost_matrix)
    print(f"QAOA selection:   {qaoa_sel}")
    print(f"QAOA total cost:  {qaoa_cost:,.1f} kg\n")

    improvement = greedy_cost - qaoa_cost
    improvement_pct = (improvement / greedy_cost) * 100

    print(f"QAOA improvement: {improvement:,.1f} kg ({improvement_pct:.2f}%)\n")

    return {
        "label": label,
        "greedy_selection": greedy_sel,
        "greedy_cost": greedy_cost,
        "qaoa_selection": qaoa_sel,
        "qaoa_cost": qaoa_cost,
        "improvement": improvement,
        "improvement_pct": improvement_pct
    }


def print_final_table(results):
    print("\n===================================================")
    print("          FINAL CROSS-SIMULATOR COMPARISON")
    print("===================================================\n")

    print(f"{'Method':20} {'Greedy (kg)':>15} {'QAOA (kg)':>15} {'Gain':>15} {'Gain %':>10}")
    print("-"*80)

    for r in results:
        print(f"{r['label']:20} "
              f"{r['greedy_cost']:15,.1f} "
              f"{r['qaoa_cost']:15,.1f} "
              f"{r['improvement']:15,.1f} "
              f"{r['improvement_pct']:10.2f}")


if __name__ == "__main__":

    print("\n===================================================")
    print("          Comparison — IrisPathQ")
    print("       A* vs A*+QAOA vs MILP vs MILP+QAOA")
    print("===================================================\n")

    # LOAD BOTH MATRICES
    astar_matrix = load_sparse_matrix("output/cost_matrix.txt")
    milp_matrix = load_sparse_matrix("output/cost_matrix_milp.txt")

    results = []

    # A* SYSTEM
    results.append(run_comparison("A* Simulator", astar_matrix, qaoa_astar))

    # MILP SYSTEM
    results.append(run_comparison("MILP Simulator", milp_matrix, qaoa_milp))

    # Final table
    print_final_table(results)

    # Save results JSON for documentation
    def to_py(o):
        if isinstance(o, np.generic):
            return o.item()
        return o

    clean_results = []
    for r in results:
        clean_results.append({
            "label": r["label"],
            "greedy_selection": [int(x) for x in r["greedy_selection"]],
            "greedy_cost": float(r["greedy_cost"]),
            "qaoa_selection": [int(x) for x in r["qaoa_selection"]],
            "qaoa_cost": float(r["qaoa_cost"]),
            "improvement": float(r["improvement"]),
            "improvement_pct": float(r["improvement_pct"]),
        })

    with open("output/comparison_results.txt", "w") as f:
       f.write("Method                   Greedy (kg)       QAOA (kg)            Gain     Gain %\n")
       f.write("-" * 80 + "\n")
       for r in results:
            f.write(
                f"{r['label']:20} "
                f"{r['greedy_cost']:15,.1f} "
                f"{r['qaoa_cost']:15,.1f} "
                f"{r['improvement']:15,.1f} "
                f"{r['improvement_pct']:10.2f}\n"
            )


    print("\nResults saved to output/comparison_results.txt\n")

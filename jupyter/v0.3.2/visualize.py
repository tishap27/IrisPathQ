"""
IrisPathQ Results Visualization
Professional charts for thesis presentation
"""
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Rectangle
from qiskit import QuantumCircuit

# Set professional style
plt.style.use('seaborn-v0_8-darkgrid')
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.size'] = 11

# ============================================================
# 1. BAR CHART: A* vs A* + QAOA
# ============================================================
def plot_comparison():
    fig, ax = plt.subplots(figsize=(8, 5))

    methods = ['A* Greedy\n(Baseline)', 'A* + QAOA\n(Quantum)']
    fuel_costs = [55272, 81669]
    conflict_costs = [50000, 20000]
    total_costs = [105272, 101669]

    x = np.arange(len(methods))
    width = 0.5

    # Stacked bars
    p1 = ax.bar(x, fuel_costs, width, label='Fuel Cost', 
                color='#2E86AB', alpha=0.85)
    p2 = ax.bar(x, conflict_costs, width, bottom=fuel_costs,
                label='Conflict Penalty', color='#A23B72', alpha=0.85)

    # Add total cost labels on top
    for i, total in enumerate(total_costs):
        ax.text(i, total + 3000, f'{total:,} kg', 
                ha='center', va='bottom', fontweight='bold', fontsize=12)

    # Improvement annotation
    savings = 105272 - 101669
    pct = (savings / 105272) * 100
    ax.annotate(f'−{savings:,} kg\n({pct:.1f}% better)', 
                xy=(1, 101669), xytext=(0.5, 115000),
                arrowprops=dict(arrowstyle='->', lw=2, color='green'),
                fontsize=11, color='green', fontweight='bold',
                ha='center')

    ax.set_ylabel('Total Cost (kg)', fontweight='bold')
    ax.set_title('Multi-Flight Route Optimization: Classical vs Quantum', 
                 fontweight='bold', fontsize=13, pad=15)
    ax.set_xticks(x)
    ax.set_xticklabels(methods)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.set_ylim(0, 130000)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    plt.tight_layout()
    plt.savefig('output/comparison_chart.png', dpi=300, bbox_inches='tight')
    print("✓ Saved: output/comparison_chart.png")

# ============================================================
# 2. QAOA CIRCUIT DIAGRAM (Simple Version)
# ============================================================
def plot_circuit():
    # Create simple QAOA circuit for 2 flights (6 qubits)
    qc = QuantumCircuit(6)

    # Layer 0: Initial superposition
    qc.h(range(6))
    qc.barrier()

    # Layer 1: Cost Hamiltonian (example rotations)
    qc.rz(0.5, 0)
    qc.rz(-0.3, 1)
    qc.rz(0.2, 2)
    qc.rz(0.7, 3)
    qc.rz(-0.4, 4)
    qc.rz(0.1, 5)
    qc.barrier()

    # Entanglement (conflict encoding)
    qc.cx(0, 3)
    qc.cx(1, 4)
    qc.cx(2, 5)
    qc.barrier()

    # Layer 2: Mixing Hamiltonian
    qc.rx(1.0, 0)
    qc.rx(1.0, 1)
    qc.rx(1.0, 2)
    qc.rx(1.0, 3)
    qc.rx(1.0, 4)
    qc.rx(1.0, 5)
    qc.barrier()

    # Measurement
    qc.measure_all()

    # Draw circuit
    fig = qc.draw('mpl', style='iqp', fold=-1)
    plt.savefig('output/qaoa_circuit.png', dpi=300, bbox_inches='tight')
    print("✓ Saved: output/qaoa_circuit.png")

# ============================================================
# 3. FORMULA VISUALIZATION
# ============================================================
def plot_formulas():
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.axis('off')

    # Title
    ax.text(0.5, 0.95, 'Mathematical Formulation', 
            ha='center', fontsize=16, fontweight='bold')

    # Cost Function
    ax.text(0.1, 0.85, '1. Total Cost Function', fontsize=13, fontweight='bold')
    ax.text(0.15, 0.78, r'$\mathcal{C}_{total} = \sum_{i=1}^{5} \text{Fuel}(r_i) + \sum_{i<j} \text{Conflict}(r_i, r_j)$',
            fontsize=11, family='monospace')

    # Fuel Cost
    ax.text(0.1, 0.68, '2. Fuel Cost per Route', fontsize=13, fontweight='bold')
    ax.text(0.15, 0.61, r'$\text{Fuel}(r) = \frac{\text{Distance}(r)}{\text{CruiseSpeed}} \times \text{FuelBurnRate}$',
            fontsize=11, family='monospace')

    # Weather Penalty
    ax.text(0.1, 0.51, '3. Weather Penalty', fontsize=13, fontweight='bold')
    ax.text(0.15, 0.44, r'$\text{EdgeCost}_{weather} = \text{Distance} \times \begin{cases} 5.0 & \text{if thunderstorm} \\ 1.0 & \text{otherwise} \end{cases} + \text{WindPenalty}$',
            fontsize=11, family='monospace')

    # Conflict Penalty
    ax.text(0.1, 0.31, '4. Conflict Penalty', fontsize=13, fontweight='bold')
    ax.text(0.15, 0.24, r'$\text{Conflict}(r_i, r_j) = \begin{cases} 10,000 \text{ kg} & \text{if } r_i \cap r_j \neq \emptyset \\ 0 & \text{otherwise} \end{cases}$',
            fontsize=11, family='monospace')

    # QAOA State
    ax.text(0.1, 0.14, '5. QAOA State Encoding', fontsize=13, fontweight='bold')
    ax.text(0.15, 0.07, r'$|\psi(\gamma, \beta)\rangle = e^{-i\beta \hat{H}_m} e^{-i\gamma \hat{H}_c} |+\rangle^{\otimes 15}$',
            fontsize=11, family='monospace')
    ax.text(0.15, 0.01, r'where $\hat{H}_c$ encodes costs and $\hat{H}_m = \sum_i \hat{X}_i$',
            fontsize=10, family='monospace', style='italic')

    plt.savefig('output/formulas.png', dpi=300, bbox_inches='tight')
    print("✓ Saved: output/formulas.png")

# ============================================================
# 4. WEATHER SCENARIO COMPARISON
# ============================================================
def plot_weather_scenarios():
    fig, ax = plt.subplots(figsize=(9, 5))

    scenarios = ['Light Storms\n(weatherTS)', 
                 'Heavy Storms\n(weatherTS2)', 
                 'Mixed Conditions\n(weatherTS3)']
    greedy = [105272, 90935, 112606]
    qaoa = [101669, 79598, 109132]
    improvements = [(g - q) / g * 100 for g, q in zip(greedy, qaoa)]

    x = np.arange(len(scenarios))
    width = 0.35

    p1 = ax.bar(x - width/2, greedy, width, label='A* Greedy', 
                color='#E63946', alpha=0.8)
    p2 = ax.bar(x + width/2, qaoa, width, label='A* + QAOA', 
                color='#06A77D', alpha=0.8)

    # Add improvement percentages
    for i, imp in enumerate(improvements):
        ax.text(i, max(greedy[i], qaoa[i]) + 3000, 
                f'↓{imp:.1f}%', ha='center', fontsize=10, 
                color='green', fontweight='bold')

    ax.set_ylabel('Total Cost (kg)', fontweight='bold')
    ax.set_title('Performance Across Different Weather Conditions', 
                 fontweight='bold', fontsize=13, pad=15)
    ax.set_xticks(x)
    ax.set_xticklabels(scenarios)
    ax.legend(loc='upper right', framealpha=0.9)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    plt.tight_layout()
    plt.savefig('output/weather_comparison.png', dpi=300, bbox_inches='tight')
    print("✓ Saved: output/weather_comparison.png")

# ============================================================
# RUN ALL
# ============================================================
if __name__ == "__main__":
    print("\n" + "="*60)
    print("Generating Visualizations for Thesis")
    print("="*60 + "\n")

    plot_comparison()
    plot_circuit()
    plot_formulas()
    plot_weather_scenarios()

    print("\n" + "="*60)
    print("✓ All visualizations saved to output/")
    print("="*60 + "\n")

    print("Files generated:")
    print("  1. comparison_chart.png     (Main result)")
    print("  2. qaoa_circuit.png          (Circuit diagram)")
    print("  3. formulas.png              (Mathematical notation)")
    print("  4. weather_comparison.png    (3 scenarios)")

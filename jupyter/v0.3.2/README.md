# IrisPathQ - Quantum-Enhanced Flight Route Optimisation  
## Version 0.3.2 - Weather Integration

## Overview  
Hybrid classical-quantum system for multi-flight route optimisation with real-world aviation constraints.

## Features  
- **A* Pathfinding**: Weather-aware route generation with dynamic cost penalties  
- **QAOA Optimisation**: Quantum circuit for conflict-free route selection  
- **Weather Integration**: Thunderstorm avoidance, wind penalties, turbulence handling  
- **Conflict Detection**: Spatial overlap penalty system (10,000 kg per conflict)  

## Architecture  
```
┌─────────────┐ ┌──────────────┐ ┌─────────────┐
│ A* Path │─────▶│ Cost Matrix │─────▶│ QAOA  │
│ Generation │ │ (25×25) │ │ Quantum           │
│ (C code) │ │ │ │ Optimizer                   │
└─────────────┘ └──────────────┘ └─────────────┘
      │               │                │
      │               │                │

Weather Data       Conflicts      Route Selection
Thunderstorms     (166 total)     (3-12% better)

```

## Results  
```
| Method      | Fuel Cost | Conflicts | Total Cost | Improvement |
|-------------|-----------|-----------|------------|-------------|
| A* Greedy   | 55,272 kg | 50,000 kg | 105,272 kg | Baseline    |
| A* + QAOA   | 81,669 kg | 20,000 kg | 101,669 kg | 3.4% ✅     |
```

- Key Achievement: QAOA reduces conflicts by 60% (50k → 20k kg) by exploring entangled state space.

## Mathematical Formulation  

**Cost Function**  
```bash
Total Cost = Fuel Cost + Conflict Penalty

Fuel(route) = (Distance / CruiseSpeed) × FuelBurnRate

Conflict(r₁, r₂) = {
  10,000 kg  if routes share waypoints
  0 kg       otherwise
}
```

Weather penalties on edge cost:  
```bash
- Edge Cost = Base Distance \times Weather Multiplier + Wind Penalty
- Thunderstorm Multiplier = 5.0  
- Wind Penalty = (WindSpeed / 10.0) × (Distance / 1000.0) × 100.0 kgkg}\) 
``` 



2. Run QAOA optimization:
```bash
cd ..
python route_qaoa.py
```

3. Visualize results:  
```bash
python visualize.py
```

## File Structure 
``` 
IrisPathQ/
├── simulator/
│ ├── astar.c # A* pathfinding with weather
│ ├── utils.c # Distance, fuel, wind calculations
│ ├── data_loader.c # CSV parsers
│ └── main.c # Orchestrator
├── data/
│ ├── flights.csv # 5 flights (AC101, WS202, ...)
│ ├── waypoints.csv # 10 Canadian airports
│ ├── weatherTS.csv # Scenario 1 (light storms)
│ ├── weatherTS2.csv # Scenario 2 (heavy storms)
│ └── weatherTS3.csv # Scenario 3 (mixed conditions)
├── output/
│ ├── cost_matrix.txt # Sparse format (25×25)
│ └── full_matrix.txt # Human-readable format
└── route_qaoa.py # QAOA implementation (Qiskit)

```

## Dependencies  
- C Compiler: gcc/clang  
- Python 3.8+  
- Qiskit 1.0+: `pip install qiskit qiskit-aer`  
- SciPy: `pip install scipy`  
- Matplotlib (for visualisation): `pip install matplotlib`

## Future Work  
- Temporal conflict detection (time-based separation)  
- MILP solver integration (GLPK/Pulp)  
- Real-time rerouting with updated weather  


---

<br>

**IrisPathQ**  
November 2025  
Version 0.3.2
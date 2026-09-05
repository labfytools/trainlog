# Dashboard graph-only layout

## Status

```text
DASHBOARD_GRAPH_ONLY=IMPLEMENTED
```

The dashboard removes redundant body-stat text and uses the available space for
one global body evolution graph.

Graph semantics:

- X axis: recorded dates;
- Y axis: percentage change from each metric's first real value;
- every series keeps a color and a unique symbol;
- legend uses human labels plus units;
- the right side of each legend entry shows current percentage evolution;
- missing body values are never converted to zero.

Examples:

```text
P Poids (kg)                 -1.8%
T Tour de taille (cm)        -4.1%
C Poitrine (cm)              +1.7%
```

The four dashboard actions are compressed into one horizontal navigation row.

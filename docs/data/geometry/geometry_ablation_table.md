# Geometry Ablation Table

Median command latency in microseconds (lower is better).

| Metric | Device | N | Q | Inline literal | Prebuilt no readback | Prebuilt with readback | Inline->Prebuilt gain | Readback overhead |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| distance matrix | cpu | 2048 | 256 | 3684.09 | 2236.36 | 10995100.00 | 1.65x | 4916.52x |
| distance matrix | cuda:0 | 2048 | 256 | 2597.62 | 313.57 | 11243700.00 | 8.28x | 35856.95x |
| nearest point | cpu | 2048 | 256 | 3967.97 | 1755.48 | 11056.10 | 2.26x | 6.30x |
| nearest point | cuda:0 | 2048 | 256 | 2992.23 | 506.83 | 10588.60 | 5.90x | 20.89x |
| circle range count | cpu | 2048 | 256 | 2758.81 | 543.84 | 4668.86 | 5.07x | 8.58x |
| circle range count | cuda:0 | 2048 | 256 | 2742.81 | 219.88 | 5177.61 | 12.47x | 23.55x |

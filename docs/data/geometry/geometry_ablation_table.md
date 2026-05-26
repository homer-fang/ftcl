# Geometry Ablation Table

Median command latency in microseconds (lower is better).

| Metric | Device | N | Q | Inline literal | Prebuilt no readback | Prebuilt with readback | Inline->Prebuilt gain | Readback overhead |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| distance matrix | cpu | 2048 | 256 | 2857.14 | 1728.47 | 9070460.00 | 1.65x | 5247.68x |
| distance matrix | cuda:0 | 2048 | 256 | 1850.88 | 261.75 | 9117650.00 | 7.07x | 34833.56x |
| nearest point | cpu | 2048 | 256 | 2278.65 | 1000.45 | 7680.29 | 2.28x | 7.68x |
| nearest point | cuda:0 | 2048 | 256 | 2204.96 | 489.64 | 7174.19 | 4.50x | 14.65x |
| circle range count | cpu | 2048 | 256 | 1562.68 | 293.92 | 3597.77 | 5.32x | 12.24x |
| circle range count | cuda:0 | 2048 | 256 | 1810.89 | 206.98 | 3540.10 | 8.75x | 17.10x |

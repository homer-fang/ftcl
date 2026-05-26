# Geometry Ablation Table

Median command latency in microseconds (lower is better).

| Metric | Device | N | Q | Inline literal | Prebuilt no readback | Prebuilt with readback | Inline->Prebuilt gain | Readback overhead |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| distance matrix | cpu | 2048 | 256 | 5678.99 | 3827.43 | 10287500.00 | 1.48x | 2687.83x |
| distance matrix | cuda:0 | 2048 | 256 | 1819.93 | 259.23 | 9784770.00 | 7.02x | 37745.22x |
| nearest point | cpu | 2048 | 256 | 4267.77 | 1006.38 | 7516.01 | 4.24x | 7.47x |
| nearest point | cuda:0 | 2048 | 256 | 2090.31 | 488.97 | 7110.99 | 4.27x | 14.54x |
| circle range count | cpu | 2048 | 256 | 1567.91 | 295.97 | 3599.26 | 5.30x | 12.16x |
| circle range count | cuda:0 | 2048 | 256 | 1806.42 | 204.72 | 3527.16 | 8.82x | 17.23x |

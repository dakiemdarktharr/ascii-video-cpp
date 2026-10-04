# Measuring conversion

```sh
QT_QPA_PLATFORM=offscreen ./build/ascii-benchmark input.mp4 output.mp4 4 100
```

Arguments after the paths are worker count and ASCII columns. Windows PowerShell uses
`$env:QT_QPA_PLATFORM = 'offscreen'` and `.\build\ascii-benchmark.exe`.

JSON reports decode time (including initial probe), grayscale/resize time, glyph rendering time,
encode time (pipe writes, waits, finalization and output commit), total wall time, actual processing
FPS and peak occupied ring slots. Preprocessing/rendering sum the work across workers; stages
overlap, so their totals must not be added or interpreted as a CPU profile. Decode time is finalized
after EOF. Encoding can dominate wall time while workers are backpressured.

The occupancy regression compares 16 and 320 frames at fixed resolution and capacity.
It verifies retained frame slots, not total process RSS or memory inside OpenCV/FFmpeg.
Use scripts/measure_memory.ps1 for process-tree working-set samples on Windows with longer inputs.
No fixed speed target is asserted.

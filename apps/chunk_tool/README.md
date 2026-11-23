# chunk_tool

M3's local demonstration app. It chunks and hashes a local file, saves the verified chunks and manifest below a data directory, prints a pretend-peer placement plan, and reconstructs the file with per-chunk and full-file verification. It does not use UDP or claim that remote peers confirmed storage.

```bash
./build/apps/chunk_tool/chunk_tool share <source-file> --data-dir <directory> [--peers A,B,C,D]
./build/apps/chunk_tool/chunk_tool catalog --data-dir <directory>
./build/apps/chunk_tool/chunk_tool rebuild <64-digit-file-id> --data-dir <directory> --output <path> [--force]
```

`share` defaults to four pretend peers named A-D. It leaves the input untouched and stores managed files under `<directory>/chunk_store/`. `rebuild` refuses to replace an existing output unless `--force` is passed. It writes to a temporary output, verifies the complete SHA-256, then renames it into place.

See [M3 storage format](../../docs/storage-format.md) for the design, file bounds and replica rules.

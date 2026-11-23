#pragma once // ELI5: prevent duplicate inclusion.

#include "filestore/chunk_store.hpp" // ELI5: expose local piece storage.
#include "filestore/file_store.hpp" // ELI5: expose the coordinator that shares files.
#include "filestore/manifest.hpp" // ELI5: expose file descriptions and chunking helpers.
#include "filestore/placement.hpp" // ELI5: expose the deterministic peer assignment helper.
#include "filestore/wire.hpp" // ELI5: expose the M3 network message formats.

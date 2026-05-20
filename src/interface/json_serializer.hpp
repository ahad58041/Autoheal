// src/interface/json_serializer.hpp
//
// Turn the shared state structs into one JSON payload suitable for the
// dashboard. nlohmann::json takes care of all escaping; we additionally
// length-cap any free-form strings before they go in.

#pragma once

#include "../common/snapshot.hpp"
#include <string>

namespace autoheal {

class JsonSerializer {
public:
    // Build a JSON blob of the form:
    // {
    //   "ts": <unix-ms>,
    //   "processes": [ { pid, comm, state, cpu, rss_kb, vsize_kb, uid } ... ],
    //   "interventions": [ { at, pid, comm, reason, stage, signal, outcome } ... ]
    // }
    static std::string build(const SnapshotBuffer& buffer,
                             const InterventionLog& log);
};

}  // namespace autoheal

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "log_entry.h"

namespace raft {

struct AppendEntriesRequest {
    uint64_t term;
    std::string leader_id;
    uint64_t prev_log_index;
    uint64_t prev_log_term;
    std::vector<LogEntry> entries;
    uint64_t leader_commit;

    AppendEntriesRequest()
        : term(0)
        , prev_log_index(0)
        , prev_log_term(0)
        , leader_commit(0)
    {}
};

struct AppendEntriesResponse {
    uint64_t term;
    bool success;
    uint64_t conflict_index;
    uint64_t conflict_term;

    AppendEntriesResponse()
        : term(0)
        , success(false)
        , conflict_index(0)
        , conflict_term(0)
    {}
};

}

#pragma once

#include <cstdint>
#include <string>

namespace raft {

/**
 * @brief RequestVote RPC 请求结构体。
 *
 * 用于候选人在选举过程中请求其他节点投票。
 */
struct RequestVoteRequest {
    uint64_t term;              ///< 候选人的任期号
    std::string candidate_id;   ///< 请求投票的候选人ID
    uint64_t last_log_index;    ///< 候选人最后一条日志的索引
    uint64_t last_log_term;     ///< 候选人最后一条日志的任期号
};

/**
 * @brief RequestVote RPC 响应结构体。
 *
 * 用于节点对投票请求的响应。
 */
struct RequestVoteResponse {
    uint64_t term;          ///< 响应者的任期号，用于候选人更新自己的任期
    bool vote_granted;      ///< 是否投票给候选人
};

}

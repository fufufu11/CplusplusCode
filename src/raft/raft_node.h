#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "append_entries.h"
#include "log_entry.h"

namespace raft {

/**
 * @brief Raft 节点状态枚举。
 *
 * 定义 Raft 共识算法中节点可能处于的三种状态。
 */
enum class State : uint8_t {
    kFollower,   ///< 跟随者状态：被动响应 Leader 的心跳和日志复制
    kCandidate,  ///< 候选人状态：发起选举，尝试成为 Leader
    kLeader      ///< 领导者状态：处理客户端请求，复制日志到其他节点
};

/**
 * @brief 将 State 枚举转换为字符串视图。
 *
 * @param state Raft 节点状态。
 * @return std::string_view 状态的字符串表示。
 */
inline std::string_view state_to_string(State state) {
    switch (state) {
        case State::kFollower:
            return "Follower";
        case State::kCandidate:
            return "Candidate";
        case State::kLeader:
            return "Leader";
        default:
            return "Unknown";
    }
}

/**
 * @brief Raft 节点核心数据结构。
 *
 * 封装 Raft 共识算法中单个节点的状态和行为。包含持久化状态、
 * 易失性状态以及状态转换和超时检测方法。
 *
 * @note 该类遵循 RAII 原则，资源在构造时初始化，析构时释放。
 */
class RaftNode {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::milliseconds;

    static constexpr Duration kDefaultElectionTimeoutMin{150};
    static constexpr Duration kDefaultElectionTimeoutMax{300};
    static constexpr Duration kDefaultHeartbeatInterval{50};

    /**
     * @brief 构造一个 Raft 节点。
     *
     * @param node_id 节点的唯一标识符。
     * @param election_timeout_min 选举超时最小值（毫秒）。
     * @param election_timeout_max 选举超时最大值（毫秒）。
     */
    RaftNode(std::string node_id,
             Duration election_timeout_min = kDefaultElectionTimeoutMin,
             Duration election_timeout_max = kDefaultElectionTimeoutMax)
        : node_id_(std::move(node_id))
        , current_term_(0)
        , voted_for_(std::nullopt)
        , state_(State::kFollower)
        , voted_count_(0)
        , commit_index_(0)
        , last_applied_(0)
        , cluster_size_(1)
        , election_timeout_min_(election_timeout_min)
        , election_timeout_max_(election_timeout_max)
        , heartbeat_interval_(kDefaultHeartbeatInterval)
        , last_heartbeat_sent_(Clock::now())
        , last_heartbeat_(Clock::now()) {
        reset_election_timeout();
    }

    ~RaftNode() = default;

    RaftNode(const RaftNode&) = delete;
    RaftNode& operator=(const RaftNode&) = delete;

    RaftNode(RaftNode&&) = default;
    RaftNode& operator=(RaftNode&&) = default;

    [[nodiscard]] const std::string& node_id() const noexcept { return node_id_; }

    [[nodiscard]] uint64_t current_term() const noexcept { return current_term_; }

    [[nodiscard]] const std::optional<std::string>& voted_for() const noexcept { return voted_for_; }

    [[nodiscard]] State state() const noexcept { return state_; }

    [[nodiscard]] uint64_t voted_count() const noexcept { return voted_count_; }

    [[nodiscard]] TimePoint last_heartbeat() const noexcept { return last_heartbeat_; }

    [[nodiscard]] Duration election_timeout() const noexcept { return election_timeout_; }

    [[nodiscard]] uint64_t commit_index() const noexcept { return commit_index_; }

    [[nodiscard]] uint64_t last_applied() const noexcept { return last_applied_; }

    [[nodiscard]] size_t cluster_size() const noexcept { return cluster_size_; }

    [[nodiscard]] const LogManager& log_manager() const noexcept { return log_manager_; }

    [[nodiscard]] LogManager& log_manager() noexcept { return log_manager_; }

    [[nodiscard]] const std::unordered_map<std::string, uint64_t>& next_index() const noexcept {
        return next_index_;
    }

    [[nodiscard]] const std::unordered_map<std::string, uint64_t>& match_index() const noexcept {
        return match_index_;
    }

    void set_current_term(uint64_t term) { current_term_ = term; }

    void set_voted_for(const std::optional<std::string>& candidate_id) {
        voted_for_ = candidate_id;
    }

    void set_voted_count(uint64_t count) { voted_count_ = count; }

    void set_commit_index(uint64_t index) { commit_index_ = index; }

    void set_last_applied(uint64_t index) { last_applied_ = index; }

    void set_cluster_size(size_t size) { cluster_size_ = size; }

    void increment_voted_count() { ++voted_count_; }

    /**
     * @brief 转换为 Leader 状态。
     *
     * 将当前节点状态设置为 Leader，并重置相关状态。
     * 仅在选举获胜时调用。
     */
    void become_leader() {
        state_ = State::kLeader;
        voted_count_ = 0;
        initialize_leader_state();
    }

    /**
     * @brief 转换为 Follower 状态。
     *
     * 将当前节点状态设置为 Follower，重置投票计数，
     * 并更新最后心跳时间。
     *
     * @param new_term 新的任期号（如果收到更高任期的消息）。
     */
    void become_follower(uint64_t new_term) {
        state_ = State::kFollower;
        voted_count_ = 0;
        if (new_term > current_term_) {
            current_term_ = new_term;
            voted_for_ = std::nullopt;
        }
        last_heartbeat_ = Clock::now();
        next_index_.clear();
        match_index_.clear();
    }

    /**
     * @brief 转换为 Candidate 状态。
     *
     * 将当前节点状态设置为 Candidate，增加任期号，
     * 投票给自己，并重置选举超时。
     */
    void become_candidate() {
        state_ = State::kCandidate;
        ++current_term_;
        voted_for_ = node_id_;
        voted_count_ = 1;
        reset_election_timeout();
        next_index_.clear();
        match_index_.clear();
    }

    /**
     * @brief 重置选举超时时间。
     *
     * 在 [election_timeout_min_, election_timeout_max_] 范围内
     * 随机选择一个超时时间，以避免多个节点同时发起选举。
     */
    void reset_election_timeout() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());

        std::uniform_int_distribution<int64_t> dist(
            election_timeout_min_.count(),
            election_timeout_max_.count()
        );

        election_timeout_ = Duration(dist(gen));
        last_heartbeat_ = Clock::now();
    }

    /**
     * @brief 检查选举是否超时。
     *
     * 判断自上次心跳以来是否已经超过选举超时时间。
     * 仅在 Follower 或 Candidate 状态下有效。
     *
     * @return true 如果选举超时，需要发起新的选举。
     * @return false 如果尚未超时。
     */
    [[nodiscard]] bool check_election_timeout() const {
        if (state_ == State::kLeader) {
            return false;
        }

        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - last_heartbeat_);
        return elapsed >= election_timeout_;
    }

    /**
     * @brief 检查是否需要发送心跳。
     *
     * @return true 如果需要发送心跳。
     * @return false 如果尚未到心跳时间。
     */
    [[nodiscard]] bool should_send_heartbeat() const {
        if (state_ != State::kLeader) {
            return false;
        }

        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - last_heartbeat_sent_);
        return elapsed >= heartbeat_interval_;
    }

    /**
     * @brief 更新最后心跳时间。
     *
     * 收到有效的心跳或日志复制消息时调用。
     */
    void update_heartbeat() {
        last_heartbeat_ = Clock::now();
    }

    /**
     * @brief 更新最后发送心跳的时间。
     */
    void update_heartbeat_sent() {
        last_heartbeat_sent_ = Clock::now();
    }

    /**
     * @brief 检查是否可以投票给指定候选人。
     *
     * @param candidate_id 候选人 ID。
     * @param candidate_term 候选人任期。
     * @return true 如果可以投票。
     * @return false 如果不能投票。
     */
    [[nodiscard]] bool can_vote_for(const std::string& candidate_id, uint64_t candidate_term) const {
        if (candidate_term < current_term_) {
            return false;
        }

        if (candidate_term > current_term_) {
            return true;
        }

        if (!voted_for_.has_value() || voted_for_.value() == candidate_id) {
            return true;
        }

        return false;
    }

    /**
     * @brief 投票给指定候选人。
     *
     * @param candidate_id 候选人 ID。
     * @param term 投票时的任期号。
     */
    void vote_for(const std::string& candidate_id, uint64_t term) {
        voted_for_ = candidate_id;
        if (term > current_term_) {
            current_term_ = term;
        }
    }

    /**
     * @brief 初始化 Leader 状态。
     *
     * 成为 Leader 后初始化 next_index 和 match_index。
     */
    void initialize_leader_state() {
        uint64_t last_idx = log_manager_.last_log_index();
        for (auto& [peer, idx] : next_index_) {
            idx = last_idx + 1;
        }
        for (auto& [peer, idx] : match_index_) {
            idx = 0;
        }
    }

    /**
     * @brief 添加集群节点。
     *
     * @param peer_id 节点 ID。
     */
    void add_peer(const std::string& peer_id) {
        if (next_index_.find(peer_id) == next_index_.end()) {
            uint64_t last_idx = log_manager_.last_log_index();
            next_index_[peer_id] = last_idx + 1;
            match_index_[peer_id] = 0;
        }
    }

    /**
     * @brief 构建 AppendEntries 请求。
     *
     * @param peer_id 目标 Follower 的 ID。
     * @return AppendEntriesRequest 构建好的请求。
     */
    [[nodiscard]] AppendEntriesRequest build_append_entries_request(const std::string& peer_id) const {
        AppendEntriesRequest req;
        req.term = current_term_;
        req.leader_id = node_id_;
        req.leader_commit = commit_index_;

        auto it = next_index_.find(peer_id);
        if (it == next_index_.end()) {
            return req;
        }

        uint64_t next_idx = it->second;
        req.prev_log_index = (next_idx > 0) ? next_idx - 1 : 0;

        if (req.prev_log_index > 0) {
            auto prev_entry = log_manager_.get(req.prev_log_index);
            if (prev_entry.has_value()) {
                req.prev_log_term = prev_entry->term;
            }
        }

        req.entries = log_manager_.get_entries_from(next_idx);

        return req;
    }

    /**
     * @brief 处理 AppendEntries 响应。
     *
     * @param peer_id Follower 的 ID。
     * @param req 发送的请求。
     * @param resp 收到的响应。
     * @return true 如果响应处理成功。
     * @return false 如果需要退位。
     */
    bool handle_append_entries_response(
        const std::string& peer_id,
        const AppendEntriesRequest& req,
        const AppendEntriesResponse& resp) {

        if (resp.term > current_term_) {
            become_follower(resp.term);
            return false;
        }

        if (state_ != State::kLeader) {
            return false;
        }

        if (resp.success) {
            match_index_[peer_id] = req.prev_log_index + req.entries.size();
            next_index_[peer_id] = match_index_[peer_id] + 1;
            update_commit_index();
            return true;
        }

        if (resp.conflict_index > 0) {
            next_index_[peer_id] = resp.conflict_index;
        } else {
            next_index_[peer_id] = std::max(1ULL, next_index_[peer_id] - 1);
        }

        return true;
    }

    /**
     * @brief 处理 AppendEntries 请求（Follower 端）。
     *
     * @param req AppendEntries 请求。
     * @return AppendEntriesResponse 响应。
     */
    [[nodiscard]] AppendEntriesResponse handle_append_entries(const AppendEntriesRequest& req) {
        AppendEntriesResponse resp;
        resp.term = current_term_;

        if (req.term < current_term_) {
            resp.success = false;
            return resp;
        }

        if (req.term > current_term_) {
            current_term_ = req.term;
            voted_for_ = std::nullopt;
        }

        if (state_ != State::kFollower) {
            become_follower(current_term_);
        }
        update_heartbeat();

        if (req.prev_log_index > 0) {
            auto prev_entry = log_manager_.get(req.prev_log_index);

            if (!prev_entry.has_value()) {
                resp.success = false;
                resp.conflict_index = log_manager_.last_log_index() + 1;
                return resp;
            }

            if (prev_entry->term != req.prev_log_term) {
                resp.success = false;
                resp.conflict_term = prev_entry->term;
                resp.conflict_index = find_first_index_of_term(resp.conflict_term);
                return resp;
            }
        }

        for (const auto& entry : req.entries) {
            auto existing = log_manager_.get(entry.index);

            if (existing.has_value()) {
                if (existing->term != entry.term) {
                    log_manager_.truncate_from(entry.index);
                    log_manager_.append(entry);
                }
            } else {
                log_manager_.append(entry);
            }
        }

        if (req.leader_commit > commit_index_) {
            commit_index_ = std::min(req.leader_commit, log_manager_.last_log_index());
        }

        resp.success = true;
        return resp;
    }

    /**
     * @brief 更新 commit_index。
     */
    void update_commit_index() {
        if (state_ != State::kLeader) {
            return;
        }

        for (uint64_t n = log_manager_.last_log_index(); n > commit_index_; --n) {
            auto entry = log_manager_.get(n);
            if (!entry.has_value() || entry->term != current_term_) {
                continue;
            }

            size_t count = 1;
            for (const auto& [peer, match_idx] : match_index_) {
                if (match_idx >= n) {
                    ++count;
                }
            }

            if (count > (cluster_size_ / 2)) {
                commit_index_ = n;
                break;
            }
        }
    }

    /**
     * @brief 接收客户端请求并追加日志。
     *
     * @param command 客户端命令。
     * @return uint64_t 新日志条目的索引，如果不是 Leader 返回 0。
     */
    uint64_t propose(const Command& command) {
        if (state_ != State::kLeader) {
            return 0;
        }

        uint64_t new_index = log_manager_.last_log_index() + 1;
        LogEntry entry(current_term_, new_index, command);
        log_manager_.append(entry);

        match_index_[node_id_] = new_index;
        next_index_[node_id_] = new_index + 1;

        return new_index;
    }

private:
    /**
     * @brief 找到指定任期的第一个日志索引。
     *
     * @param term 任期号。
     * @return uint64_t 第一个日志索引，如果没有返回 0。
     */
    [[nodiscard]] uint64_t find_first_index_of_term(uint64_t term) const {
        for (uint64_t i = 1; i <= log_manager_.last_log_index(); ++i) {
            auto entry = log_manager_.get(i);
            if (entry.has_value() && entry->term == term) {
                return i;
            }
        }
        return 0;
    }

    std::string node_id_;

    uint64_t current_term_;
    std::optional<std::string> voted_for_;

    State state_;
    uint64_t voted_count_;

    LogManager log_manager_;
    uint64_t commit_index_;
    uint64_t last_applied_;

    size_t cluster_size_;

    std::unordered_map<std::string, uint64_t> next_index_;
    std::unordered_map<std::string, uint64_t> match_index_;

    Duration election_timeout_min_;
    Duration election_timeout_max_;
    Duration election_timeout_;
    Duration heartbeat_interval_;
    TimePoint last_heartbeat_sent_;
    TimePoint last_heartbeat_;
};

}

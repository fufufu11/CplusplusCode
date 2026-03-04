#include <gtest/gtest.h>
#include "raft/raft_node.h"
#include "raft/election.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>

using namespace raft;

class RaftNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        node_ = std::make_unique<RaftNode>("node1");
    }

    void TearDown() override {
        node_.reset();
    }

    std::unique_ptr<RaftNode> node_;
};

TEST_F(RaftNodeTest, InitialStateIsFollower) {
    EXPECT_EQ(node_->state(), State::kFollower);
}

TEST_F(RaftNodeTest, InitialTermIsZero) {
    EXPECT_EQ(node_->current_term(), 0);
}

TEST_F(RaftNodeTest, InitialVotedForIsEmpty) {
    EXPECT_FALSE(node_->voted_for().has_value());
}

TEST_F(RaftNodeTest, InitialVotedCountIsZero) {
    EXPECT_EQ(node_->voted_count(), 0);
}

TEST_F(RaftNodeTest, BecomeFollowerChangesState) {
    node_->become_candidate();
    EXPECT_EQ(node_->state(), State::kCandidate);

    node_->become_follower(1);
    EXPECT_EQ(node_->state(), State::kFollower);
}

TEST_F(RaftNodeTest, BecomeFollowerUpdatesTerm) {
    node_->become_follower(5);
    EXPECT_EQ(node_->current_term(), 5);
}

TEST_F(RaftNodeTest, BecomeFollowerResetsVotedCount) {
    node_->become_candidate();
    node_->increment_voted_count();
    EXPECT_GT(node_->voted_count(), 0);

    node_->become_follower(1);
    EXPECT_EQ(node_->voted_count(), 0);
}

TEST_F(RaftNodeTest, BecomeFollowerClearsVotedForOnHigherTerm) {
    node_->vote_for("other_node", 1);
    ASSERT_TRUE(node_->voted_for().has_value());

    node_->become_follower(2);
    EXPECT_FALSE(node_->voted_for().has_value());
}

TEST_F(RaftNodeTest, BecomeFollowerKeepsVotedForOnSameTerm) {
    node_->set_current_term(5);
    node_->vote_for("candidate_a", 5);

    node_->become_follower(5);
    EXPECT_TRUE(node_->voted_for().has_value());
    EXPECT_EQ(node_->voted_for().value(), "candidate_a");
}

TEST_F(RaftNodeTest, BecomeCandidateChangesState) {
    node_->become_candidate();
    EXPECT_EQ(node_->state(), State::kCandidate);
}

TEST_F(RaftNodeTest, BecomeCandidateIncrementsTerm) {
    EXPECT_EQ(node_->current_term(), 0);

    node_->become_candidate();
    EXPECT_EQ(node_->current_term(), 1);

    node_->become_candidate();
    EXPECT_EQ(node_->current_term(), 2);
}

TEST_F(RaftNodeTest, BecomeCandidateVotesForSelf) {
    node_->become_candidate();
    ASSERT_TRUE(node_->voted_for().has_value());
    EXPECT_EQ(node_->voted_for().value(), "node1");
}

TEST_F(RaftNodeTest, BecomeCandidateSetsVotedCountToOne) {
    node_->become_candidate();
    EXPECT_EQ(node_->voted_count(), 1);
}

TEST_F(RaftNodeTest, BecomeLeaderChangesState) {
    node_->become_candidate();
    node_->become_leader();
    EXPECT_EQ(node_->state(), State::kLeader);
}

TEST_F(RaftNodeTest, BecomeLeaderResetsVotedCount) {
    node_->become_candidate();
    node_->increment_voted_count();
    node_->increment_voted_count();

    node_->become_leader();
    EXPECT_EQ(node_->voted_count(), 0);
}

TEST_F(RaftNodeTest, ElectionTimeoutWithinRange) {
    constexpr int kIterations = 1000;
    constexpr auto kMin = RaftNode::kDefaultElectionTimeoutMin;
    constexpr auto kMax = RaftNode::kDefaultElectionTimeoutMax;

    for (int i = 0; i < kIterations; ++i) {
        node_->reset_election_timeout();
        auto timeout = node_->election_timeout();
        EXPECT_GE(timeout, kMin);
        EXPECT_LE(timeout, kMax);
    }
}

TEST_F(RaftNodeTest, ResetElectionTimeoutProducesDifferentValues) {
    std::vector<RaftNode::Duration> timeouts;
    timeouts.reserve(100);

    for (int i = 0; i < 100; ++i) {
        node_->reset_election_timeout();
        timeouts.push_back(node_->election_timeout());
    }

    bool has_different = false;
    for (size_t i = 1; i < timeouts.size(); ++i) {
        if (timeouts[i] != timeouts[0]) {
            has_different = true;
            break;
        }
    }
    EXPECT_TRUE(has_different);
}

TEST_F(RaftNodeTest, CheckElectionTimeoutReturnsFalseImmediately) {
    node_->reset_election_timeout();
    EXPECT_FALSE(node_->check_election_timeout());
}

TEST_F(RaftNodeTest, CheckElectionTimeoutReturnsTrueAfterTimeout) {
    RaftNode quick_node("quick_node", 
                        std::chrono::milliseconds(10),
                        std::chrono::milliseconds(20));

    quick_node.reset_election_timeout();
    EXPECT_FALSE(quick_node.check_election_timeout());

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_TRUE(quick_node.check_election_timeout());
}

TEST_F(RaftNodeTest, CheckElectionTimeoutAlwaysFalseForLeader) {
    RaftNode quick_node("quick_node",
                        std::chrono::milliseconds(10),
                        std::chrono::milliseconds(20));

    quick_node.become_candidate();
    quick_node.become_leader();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(quick_node.check_election_timeout());
}

TEST_F(RaftNodeTest, UpdateHeartbeatResetsTimeout) {
    RaftNode quick_node("quick_node",
                        std::chrono::milliseconds(10),
                        std::chrono::milliseconds(20));

    quick_node.reset_election_timeout();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    quick_node.update_heartbeat();

    EXPECT_FALSE(quick_node.check_election_timeout());
}

TEST_F(RaftNodeTest, SetCurrentTerm) {
    node_->set_current_term(42);
    EXPECT_EQ(node_->current_term(), 42);

    node_->set_current_term(100);
    EXPECT_EQ(node_->current_term(), 100);
}

TEST_F(RaftNodeTest, SetVotedFor) {
    node_->set_voted_for("candidate_x");
    ASSERT_TRUE(node_->voted_for().has_value());
    EXPECT_EQ(node_->voted_for().value(), "candidate_x");

    node_->set_voted_for(std::nullopt);
    EXPECT_FALSE(node_->voted_for().has_value());
}

TEST_F(RaftNodeTest, SetVotedCount) {
    node_->set_voted_count(5);
    EXPECT_EQ(node_->voted_count(), 5);
}

TEST_F(RaftNodeTest, IncrementVotedCount) {
    EXPECT_EQ(node_->voted_count(), 0);

    node_->increment_voted_count();
    EXPECT_EQ(node_->voted_count(), 1);

    node_->increment_voted_count();
    EXPECT_EQ(node_->voted_count(), 2);
}

TEST_F(RaftNodeTest, CanVoteForWhenNotVoted) {
    EXPECT_TRUE(node_->can_vote_for("candidate_a", 1));
}

TEST_F(RaftNodeTest, CanVoteForSameCandidate) {
    node_->vote_for("candidate_a", 1);
    EXPECT_TRUE(node_->can_vote_for("candidate_a", 1));
}

TEST_F(RaftNodeTest, CannotVoteForDifferentCandidate) {
    node_->vote_for("candidate_a", 1);
    EXPECT_FALSE(node_->can_vote_for("candidate_b", 1));
}

TEST_F(RaftNodeTest, CannotVoteForLowerTerm) {
    node_->set_current_term(5);
    EXPECT_FALSE(node_->can_vote_for("candidate_a", 3));
}

TEST_F(RaftNodeTest, CanVoteForHigherTerm) {
    node_->set_current_term(5);
    node_->vote_for("candidate_a", 5);

    EXPECT_TRUE(node_->can_vote_for("candidate_b", 6));
}

TEST_F(RaftNodeTest, VoteForSetsVotedFor) {
    node_->vote_for("candidate_x", 1);
    ASSERT_TRUE(node_->voted_for().has_value());
    EXPECT_EQ(node_->voted_for().value(), "candidate_x");
}

TEST_F(RaftNodeTest, VoteForUpdatesTermIfHigher) {
    node_->set_current_term(5);
    node_->vote_for("candidate_x", 10);
    EXPECT_EQ(node_->current_term(), 10);
}

TEST_F(RaftNodeTest, VoteForKeepsTermIfLower) {
    node_->set_current_term(10);
    node_->vote_for("candidate_x", 5);
    EXPECT_EQ(node_->current_term(), 10);
}

TEST_F(RaftNodeTest, NodeIdIsCorrect) {
    EXPECT_EQ(node_->node_id(), "node1");

    RaftNode node2("node2");
    EXPECT_EQ(node2.node_id(), "node2");
}

TEST_F(RaftNodeTest, CustomElectionTimeoutRange) {
    RaftNode custom_node("custom",
                         std::chrono::milliseconds(100),
                         std::chrono::milliseconds(200));

    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        custom_node.reset_election_timeout();
        auto timeout = custom_node.election_timeout();
        EXPECT_GE(timeout, std::chrono::milliseconds(100));
        EXPECT_LE(timeout, std::chrono::milliseconds(200));
    }
}

TEST_F(RaftNodeTest, LastHeartbeatUpdatedOnReset) {
    auto before = node_->last_heartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    node_->reset_election_timeout();
    auto after = node_->last_heartbeat();

    EXPECT_GT(after, before);
}

TEST_F(RaftNodeTest, LastHeartbeatUpdatedOnBecomeFollower) {
    auto before = node_->last_heartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    node_->become_follower(1);
    auto after = node_->last_heartbeat();

    EXPECT_GT(after, before);
}

TEST_F(RaftNodeTest, StateTransitionSequence) {
    EXPECT_EQ(node_->state(), State::kFollower);
    EXPECT_EQ(node_->current_term(), 0);

    node_->become_candidate();
    EXPECT_EQ(node_->state(), State::kCandidate);
    EXPECT_EQ(node_->current_term(), 1);
    EXPECT_TRUE(node_->voted_for().has_value());
    EXPECT_EQ(node_->voted_count(), 1);

    node_->become_leader();
    EXPECT_EQ(node_->state(), State::kLeader);
    EXPECT_EQ(node_->voted_count(), 0);

    node_->become_follower(2);
    EXPECT_EQ(node_->state(), State::kFollower);
    EXPECT_EQ(node_->current_term(), 2);
    EXPECT_EQ(node_->voted_count(), 0);
}

TEST_F(RaftNodeTest, MultipleCandidatesCanCoexist) {
    RaftNode node1("node1");
    RaftNode node2("node2");
    RaftNode node3("node3");

    node1.become_candidate();
    node2.become_candidate();
    node3.become_candidate();

    EXPECT_EQ(node1.current_term(), 1);
    EXPECT_EQ(node2.current_term(), 1);
    EXPECT_EQ(node3.current_term(), 1);

    EXPECT_EQ(node1.voted_for().value(), "node1");
    EXPECT_EQ(node2.voted_for().value(), "node2");
    EXPECT_EQ(node3.voted_for().value(), "node3");
}

TEST_F(RaftNodeTest, FollowerRespondsToHigherTerm) {
    node_->vote_for("old_leader", 1);
    ASSERT_TRUE(node_->voted_for().has_value());

    node_->become_follower(5);
    EXPECT_EQ(node_->current_term(), 5);
    EXPECT_FALSE(node_->voted_for().has_value());
}

TEST_F(RaftNodeTest, ElectionTimeoutAfterStateChange) {
    RaftNode quick_node("quick_node",
                        std::chrono::milliseconds(10),
                        std::chrono::milliseconds(20));

    quick_node.become_candidate();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_TRUE(quick_node.check_election_timeout());
}

TEST(StateToStringTest, ReturnsCorrectStrings) {
    EXPECT_EQ(state_to_string(State::kFollower), "Follower");
    EXPECT_EQ(state_to_string(State::kCandidate), "Candidate");
    EXPECT_EQ(state_to_string(State::kLeader), "Leader");
}

TEST(StateToStringTest, ReturnsUnknownForInvalidValue) {
    auto invalid_state = static_cast<State>(255);
    EXPECT_EQ(state_to_string(invalid_state), "Unknown");
}

class RaftPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        node_ = std::make_unique<RaftNode>("perf_node");
    }

    void TearDown() override {
        node_.reset();
    }

    std::unique_ptr<RaftNode> node_;
};

TEST_F(RaftPerformanceTest, StateTransitionLatencyUnder1ms) {
    constexpr int kIterations = 10000;
    constexpr auto kMaxLatency = std::chrono::microseconds(1000);

    auto measure_become_follower = [this]() {
        auto start = std::chrono::high_resolution_clock::now();
        node_->become_follower(1);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    };

    auto measure_become_candidate = [this]() {
        auto start = std::chrono::high_resolution_clock::now();
        node_->become_candidate();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    };

    auto measure_become_leader = [this]() {
        auto start = std::chrono::high_resolution_clock::now();
        node_->become_leader();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    };

    std::vector<std::chrono::nanoseconds> follower_latencies;
    std::vector<std::chrono::nanoseconds> candidate_latencies;
    std::vector<std::chrono::nanoseconds> leader_latencies;

    follower_latencies.reserve(kIterations);
    candidate_latencies.reserve(kIterations);
    leader_latencies.reserve(kIterations);

    for (int i = 0; i < kIterations; ++i) {
        node_->become_follower(1);
        follower_latencies.push_back(measure_become_follower());

        candidate_latencies.push_back(measure_become_candidate());

        leader_latencies.push_back(measure_become_leader());
    }

    auto max_follower = *std::max_element(follower_latencies.begin(), follower_latencies.end());
    auto max_candidate = *std::max_element(candidate_latencies.begin(), candidate_latencies.end());
    auto max_leader = *std::max_element(leader_latencies.begin(), leader_latencies.end());

    EXPECT_LT(max_follower, kMaxLatency)
        << "become_follower() max latency: " << max_follower.count() << "ns";

    EXPECT_LT(max_candidate, kMaxLatency)
        << "become_candidate() max latency: " << max_candidate.count() << "ns";

    EXPECT_LT(max_leader, kMaxLatency)
        << "become_leader() max latency: " << max_leader.count() << "ns";
}

TEST_F(RaftPerformanceTest, ElectionTimeoutPrecisionErrorUnder10Percent) {
    constexpr int kIterations = 100;
    constexpr double kMaxErrorPercent = 10.0;

    RaftNode quick_node("quick_node",
                        std::chrono::milliseconds(50),
                        std::chrono::milliseconds(50));

    std::vector<double> error_percents;
    error_percents.reserve(kIterations);

    for (int i = 0; i < kIterations; ++i) {
        quick_node.reset_election_timeout();
        auto expected_timeout = quick_node.election_timeout();

        auto start = std::chrono::steady_clock::now();
        while (!quick_node.check_election_timeout()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        auto end = std::chrono::steady_clock::now();

        auto actual_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        double error_percent = std::abs(actual_elapsed.count() - expected_timeout.count())
                             * 100.0 / expected_timeout.count();

        error_percents.push_back(error_percent);
    }

    double max_error = *std::max_element(error_percents.begin(), error_percents.end());
    double avg_error = std::accumulate(error_percents.begin(), error_percents.end(), 0.0)
                     / error_percents.size();

    EXPECT_LT(max_error, kMaxErrorPercent)
        << "Max election timeout precision error: " << max_error << "%";

    EXPECT_LT(avg_error, kMaxErrorPercent)
        << "Avg election timeout precision error: " << avg_error << "%";
}

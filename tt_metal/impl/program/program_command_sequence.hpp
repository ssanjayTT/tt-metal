// SPDX-FileCopyrightText: © 2024 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "tt_metal/impl/dispatch/device_command.hpp"

struct CQDispatchWritePackedCmd;
struct launch_msg_t;

namespace tt::tt_metal {

class CircularBuffer;

constexpr uint32_t UncachedStallSequenceIdx = 0;
constexpr uint32_t CachedStallSequenceIdx = 1;

struct ProgramCommandSequence {
    struct RtaUpdate {
        const void* src;
        void* dst;
        uint32_t size;
    };
    struct LaunchMsgData {
        const bool is_multicast = false;
        // The original value of the launch message that was generated by assemble_device_commands.
        const launch_msg_t original_msg;
        // A pointer to the copy of the launch message inside `launch_msg_command_sequence`. This copy may be modified
        // at "update" time.
        launch_msg_t* msg_ptr = nullptr;
    };
    HostMemDeviceCommand preamble_command_sequence;
    uint32_t current_stall_seq_idx = 0;
    HostMemDeviceCommand stall_command_sequences[2];
    std::vector<HostMemDeviceCommand> runtime_args_command_sequences;
    HostMemDeviceCommand program_config_buffer_command_sequence;
    HostMemDeviceCommand program_binary_setup_prefetcher_cache_command;
    HostMemDeviceCommand program_binary_command_sequence;
    HostMemDeviceCommand launch_msg_command_sequence;
    HostMemDeviceCommand go_msg_command_sequence;
    std::vector<uint32_t*> cb_configs_payloads;
    std::vector<std::vector<std::shared_ptr<CircularBuffer>>> circular_buffers_on_core_ranges;
    // Note: some RTAs may be have their RuntimeArgsData modified so the source-of-truth of their data is the command
    // sequence. They won't be listed in rta_updates.
    std::vector<RtaUpdate> rta_updates;
    std::vector<LaunchMsgData> launch_messages;
    std::vector<CQDispatchWritePackedCmd*> launch_msg_write_packed_cmd_ptrs;
    std::vector<CQDispatchWritePackedCmd*> unicast_launch_msg_write_packed_cmd_ptrs;
    CQDispatchGoSignalMcastCmd* mcast_go_signal_cmd_ptr;

    bool prefetcher_cache_used = false;
    uint32_t kernel_bins_sizeB = 0;
    uint32_t kernel_bins_base_addr = 0;

    uint32_t get_rt_args_size() const {
        return std::accumulate(
            runtime_args_command_sequences.begin(),
            runtime_args_command_sequences.end(),
            0,
            [](int acc, const HostMemDeviceCommand& cmd) { return cmd.size_bytes() + acc; });
    }

    uint32_t get_one_shot_fetch_size(bool stall_first, bool stall_before_program, bool send_binary) const {
        uint32_t one_shot_fetch_size =
            ((stall_before_program || stall_first) ? stall_command_sequences[current_stall_seq_idx].size_bytes() : 0) +
            preamble_command_sequence.size_bytes() + program_config_buffer_command_sequence.size_bytes() +
            get_rt_args_size() +
            (send_binary ? program_binary_command_sequence.size_bytes() +
                               program_binary_setup_prefetcher_cache_command.size_bytes()
                         : 0) +
            launch_msg_command_sequence.size_bytes() + go_msg_command_sequence.size_bytes();
        return one_shot_fetch_size;
    }
};

}  // namespace tt::tt_metal

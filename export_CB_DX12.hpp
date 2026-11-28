///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
// export constant buffers functions
// ----------------------------------------------------------------------------------------
// 
// (c) Lefuneste.
//
// All rights reserved.
// https://github.com/xxx
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// This software is using part of code or algorithms provided by
// * Crosire https://github.com/crosire/reshade  
// * FransBouma https://github.com/FransBouma/ShaderToggler
// * ShortFuse https://github.com/clshortfuse/renodx
// 
/////////////////////////////////////////////////////////////////////////

#include <reshade.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <thread>
#include <vector>

#include "to_string.hpp"
#include "config.hpp"
#include "addon_logs.h"

namespace fs = std::filesystem;

extern SharedState* g_shared_state;

#define DELAY_BETWEEN_CB_EXPORT_MS 50

#ifdef _DEBUG
class ConstantBufferExporter {
private:
    static constexpr uint64_t MAX_CB_SIZE = 65536;   // 64 KB

    struct pending_cb {
        reshade::api::resource staging;
        uint64_t               size;
        uint64_t               frame;
        std::string            filepath;
    };
    std::vector<pending_cb> pending;
    std::mutex pending_mutex;
    uint64_t frame_counter = 0;

    uint64_t get_reshade_timestamp() {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
    }

    std::string generate_filename(uint64_t ps_hash, uint32_t draw_num, uint64_t timestamp, uint32_t binding_index,
        const std::string& extension = "txt")
    {
        std::ostringstream oss;
        oss << "CB_"
            << "PS_0x" << std::hex << std::setfill('0') << std::setw(16) << ps_hash
            << "_draw_" << std::dec << draw_num
            << "_ts_" << timestamp
            << "_binding_" << binding_index
            << "." << extension;
        return oss.str();
    }

    static bool write_text_file(const std::string& filepath, const void* data, uint64_t size)
    {
        std::ofstream file(filepath);
        if (!file.is_open()) { log_error_create_file(filepath); return false; }

        file << "Constant Buffer Dump\n";
        file << "Size: " << size << " bytes\n";
        file << "Data (as float4 registers):\n\n";

        const float* f = static_cast<const float*>(data);
        const uint64_t num_floats = size / sizeof(float);
        const uint64_t num_registers = num_floats / 4;

        for (uint64_t i = 0; i < num_registers; ++i) {
            file << "cb" << i << ": ("
                << std::fixed << std::setprecision(6)
                << f[i * 4 + 0] << ", " << f[i * 4 + 1] << ", "
                << f[i * 4 + 2] << ", " << f[i * 4 + 3] << ")\n";
        }
        const uint64_t remaining = num_floats % 4;
        if (remaining > 0) {
            file << "cb" << num_registers << ": (";
            for (uint64_t i = 0; i < remaining; ++i) {
                file << f[num_registers * 4 + i];
                if (i < remaining - 1) file << ", ";
            }
            file << ")\n";
        }
        return true;
    }

    bool save_constant_buffer_as_text(reshade::api::device* dev, reshade::api::command_list* cmd_list,
        reshade::api::buffer_range cb, const std::string& filepath)
    {
        using namespace reshade::api;

        const resource_desc desc = dev->get_resource_desc(cb.buffer);
        if (desc.type != resource_type::buffer) { log_error_not_buffer(); return false; }

        // a CBV is a slice of the buffer : only dump [offset, offset + size)
        uint64_t size = cb.size;
        if (size == 0 || size == UINT64_MAX || cb.offset + size > desc.buffer.size)
            size = desc.buffer.size > cb.offset ? desc.buffer.size - cb.offset : 0;
        if (size == 0) { log_error_CB_size(); return false; }
        if (size > MAX_CB_SIZE) size = MAX_CB_SIZE;

        // CPU visible buffer (upload heap = usual DX12 case) : direct read, no GPU copy, no barrier
        if (desc.heap == memory_heap::cpu_to_gpu || desc.heap == memory_heap::cpu_only) {
            void* data = nullptr;
            if (!dev->map_buffer_region(cb.buffer, cb.offset, size, map_access::read_only, &data) || !data) {
                log_error_map_staging();
                return false;
            }
            const bool ok = write_text_file(filepath, data, size);
            dev->unmap_buffer_region(cb.buffer);
            return ok;
        }

        // GPU only buffer : copy into a readback buffer, written to file later by flush()
        resource staging = {};
        const resource_desc sd(size, memory_heap::gpu_to_cpu, resource_usage::copy_dest);
        if (!dev->create_resource(sd, nullptr, resource_usage::copy_dest, &staging)) {
            log_error_create_ressource();
            return false;
        }

        cmd_list->barrier(cb.buffer, resource_usage::constant_buffer, resource_usage::copy_source);
        cmd_list->copy_buffer_region(cb.buffer, cb.offset, staging, 0, size);
        cmd_list->barrier(cb.buffer, resource_usage::copy_source, resource_usage::constant_buffer);

        std::lock_guard<std::mutex> lock(pending_mutex);
        pending.push_back({ staging, size, frame_counter, filepath });
        return true;
    }

public:
    // called at draw time : one update = one CBV (update.binding = b register)
    void export_constant_buffers(
        reshade::api::command_list* cmd_list,
        reshade::api::shader_stage stages,
        reshade::api::pipeline_layout layout,
        uint32_t param_index,
        const reshade::api::descriptor_table_update& update,
        uint64_t ps_hash,
        uint32_t vr_draw_num,
        bool export_as_text = true)
    {
        using namespace reshade::api;

        if (update.type != descriptor_type::constant_buffer) return;

        device* dev = cmd_list->get_device();
        const uint64_t timestamp = get_reshade_timestamp();

        std::filesystem::path path = g_shared_state->g_vrem_base_path;
        path /= VREM_CB_SAVE_DIR;
        if (!fs::exists(path)) fs::create_directories(path);

        for (uint32_t i = 0; i < update.count; ++i) {
            const buffer_range buff = static_cast<const buffer_range*>(update.descriptors)[i];
            if (buff.buffer.handle == 0) continue;

            const fs::path filepath = path / generate_filename(ps_hash, vr_draw_num, timestamp, update.binding + i);

            if (save_constant_buffer_as_text(dev, cmd_list, buff, filepath.string()))
                log_exported_CB(filepath.string());
        }
    }

    void flush(reshade::api::device* dev, reshade::api::command_queue* queue)
    {
        using namespace reshade::api;

        std::vector<pending_cb> ready;
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            for (auto it = pending.begin(); it != pending.end(); ) {
                if (it->frame < frame_counter) { ready.push_back(*it); it = pending.erase(it); }
                else ++it;
            }
            ++frame_counter;
        }
        if (ready.empty()) return;

        queue->wait_idle();

        for (auto& p : ready) {
            void* data = nullptr;
            if (dev->map_buffer_region(p.staging, 0, p.size, map_access::read_only, &data) && data) {
                if (write_text_file(p.filepath, data, p.size)) log_exported_CB(p.filepath);
                dev->unmap_buffer_region(p.staging);
            }
            else log_error_map_staging();

            dev->destroy_resource(p.staging);
        }
    }

    void release_all(reshade::api::device* dev)
    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        for (auto& p : pending) dev->destroy_resource(p.staging);
        pending.clear();
    }
};
#endif
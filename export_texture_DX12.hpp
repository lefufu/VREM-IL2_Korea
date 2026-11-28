///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
// export textures functions
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
#include <chrono>

#include "config.hpp"
#include "to_string.hpp"

namespace fs = std::filesystem;

extern SharedState* g_shared_state;
#ifdef _DEBUG
#ifdef _DEBUG
class TextureExporter {
private:
    // state of a texture bound as SRV at draw time. If the debug layer complains about
    // barriers, try resource_usage::shader_resource_pixel
    static constexpr reshade::api::resource_usage SRC_STATE = reshade::api::resource_usage::shader_resource;

    struct pending_texture {
        reshade::api::resource      staging;    // readback buffer
        reshade::api::resource_desc desc;       // description of the source texture
        uint32_t                    row_pitch;
        uint64_t                    frame;
        std::string                 filepath;
    };
    std::vector<pending_texture> pending;
    std::mutex pending_mutex;
    uint64_t frame_counter = 0;

    uint64_t get_reshade_timestamp() {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
    }

    std::string generate_filename(uint64_t ps_hash, uint32_t draw_num, uint64_t timestamp, uint32_t texture_index,
        const std::string& extension = RESHADE_ADDON_TEXTURE_SAVE_FORMAT)
    {
        std::ostringstream oss;
        oss << "PS_0x" << std::hex << std::setfill('0') << std::setw(16) << ps_hash
            << "_draw_" << std::dec << draw_num
            << "_ts_" << timestamp
            << "_idx_" << texture_index
            << extension;
        return oss.str();
    }

    // record the copy of the texture in a readback buffer. The file is written later by flush()
    bool queue_texture_export(reshade::api::device* dev, reshade::api::command_list* cmd_list,
        reshade::api::resource texture, const std::string& filepath)
    {
        using namespace reshade::api;

        const resource_desc desc = dev->get_resource_desc(texture);

        if (desc.type != resource_type::texture_2d) {
            reshade::log::message(reshade::log::level::info, "export_texture - skipped : not a 2D texture; ");
            return false;
        }
        if (desc.texture.samples > 1) {
            reshade::log::message(reshade::log::level::info, "export_texture - skipped : MSAA texture; ");
            return false;
        }

        uint32_t row_pitch = format_row_pitch(desc.texture.format, desc.texture.width);
        if (dev->get_api() == device_api::d3d12)
            row_pitch = (row_pitch + 255) & ~255u;           // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT

        resource staging = {};
        const resource_desc buf_desc(uint64_t(row_pitch) * desc.texture.height, memory_heap::gpu_to_cpu, resource_usage::copy_dest);
        if (!dev->create_resource(buf_desc, nullptr, resource_usage::copy_dest, &staging)) {
            log_error_staging("export_texture");
            return false;
        }

        cmd_list->barrier(texture, SRC_STATE, resource_usage::copy_source);
        cmd_list->copy_texture_to_buffer(texture, 0, nullptr, staging, 0, 0, 0);
        cmd_list->barrier(texture, resource_usage::copy_source, SRC_STATE);

        std::lock_guard<std::mutex> lock(pending_mutex);
        pending.push_back({ staging, desc, row_pitch, frame_counter, filepath });
        return true;
    }

public:
    // called at draw time : one update = one SRV (update.binding = t register)
    void export_descriptors(
        reshade::api::command_list* cmd_list,
        reshade::api::shader_stage stages,
        reshade::api::pipeline_layout layout,
        uint32_t param_index,
        const reshade::api::descriptor_table_update& update,
        uint64_t ps_hash,
        uint32_t vr_draw_num)
    {
        using namespace reshade::api;

        if (update.type != descriptor_type::shader_resource_view) return;

        device* dev = cmd_list->get_device();
        const uint64_t timestamp = get_reshade_timestamp();

        std::filesystem::path path = g_shared_state->g_vrem_base_path;
        path /= RESHADE_ADDON_TEXTURE_SAVE_DIR;
        if (!fs::exists(path)) fs::create_directories(path);

        for (uint32_t i = 0; i < update.count; ++i) {
            const resource_view srv = static_cast<const resource_view*>(update.descriptors)[i];

            const fs::path filepath = path / generate_filename(ps_hash, vr_draw_num, timestamp, update.binding + i);

            if (srv.handle == 0) { log_error_srv_handle_null(filepath.string()); continue; }

            const resource tex = dev->get_resource_from_view(srv);
            if (tex.handle == 0) { log_error_txt_handle_null(filepath.string()); continue; }

            queue_texture_export(dev, cmd_list, tex, filepath.string());
        }
    }

    // called at each present : write the files of the exports recorded in a previous frame
    void flush(reshade::api::device* dev, reshade::api::command_queue* queue)
    {
        using namespace reshade::api;

        std::vector<pending_texture> ready;
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            for (auto it = pending.begin(); it != pending.end(); ) {
                if (it->frame < frame_counter) { ready.push_back(*it); it = pending.erase(it); }
                else ++it;
            }
            ++frame_counter;
        }
        if (ready.empty()) return;

        queue->wait_idle();                                   // debug capture only : the GPU copies are done

        for (auto& p : ready) {
            void* data = nullptr;
            if (dev->map_buffer_region(p.staging, 0, UINT64_MAX, map_access::read_only, &data) && data) {
                subresource_data sd = {};
                sd.data = data;
                sd.row_pitch = p.row_pitch;
                sd.slice_pitch = p.row_pitch * p.desc.texture.height;

                if (save_texture_image(p.desc, sd, p.filepath))
                    reshade::log::message(reshade::log::level::info, ("export_texture - saved " + p.filepath + "; ").c_str());

                dev->unmap_buffer_region(p.staging);
            }
            else log_error_map_staging();

            dev->destroy_resource(p.staging);
        }
    }

    // destroy pending staging buffers without writing (reload / device destroyed)
    void release_all(reshade::api::device* dev)
    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        for (auto& p : pending) dev->destroy_resource(p.staging);
        pending.clear();
    }
};
#endif
#endif


///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  all functions regarding constant buffer injection and handling
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
#include <sstream>
#include "loader_addon_shared.h"
#include "addon_objects.h"

using namespace reshade::api;

// ---------------------------------------------------------------
// creation of CB (called in init_device and when reload)
void vrem_ensure_cb_buffer(device* dev)
{
    if (dev == nullptr || g_shared_state == nullptr) return;

    auto& cb = g_shared_state->VREM_cb;
    std::unique_lock lock(cb.mutex);
    if (cb.ptr) return;

    resource_desc desc(BUFFER_SIZE, memory_heap::cpu_to_gpu, resource_usage::constant_buffer);
    if (!dev->create_resource(desc, nullptr, resource_usage::cpu_access, &cb.buf)) {
        reshade::log::message(reshade::log::level::info, "addon - ensure_cb_buffer : create_resource ECHEC; ");
        return;
    }

    void* p = nullptr;
    if (!dev->map_buffer_region(cb.buf, 0, BUFFER_SIZE, map_access::write_only, &p) || !p) {
        reshade::log::message(reshade::log::level::info, "addon - ensure_cb_buffer : map_buffer_region ECHEC; ");
        dev->destroy_resource(cb.buf);
        cb.buf = {};
        return;
    }
    cb.ptr = static_cast<uint8_t*>(p);
    cb.range[0] = { cb.buf, 0,         SLOT_SIZE };
    cb.range[1] = { cb.buf, SLOT_SIZE, SLOT_SIZE };
    memset(cb.ptr, 0, BUFFER_SIZE);

    std::stringstream s;
    s << "addon - ensure_cb_buffer : OK buf=0x" << std::hex << cb.buf.handle
        << " ptr=" << (void*)cb.ptr << "; ";
    reshade::log::message(reshade::log::level::info, s.str().c_str());
}
//*******************************************************************************
// destroy the CB buffer : only when the DEVICE is destroyed, never at reload
void vrem_destroy_cb_buffer(device* dev)
{
    if (g_shared_state == nullptr) return;

    auto& cb = g_shared_state->VREM_cb;
    std::unique_lock lock(cb.mutex);
    if (cb.ptr && dev) {
        dev->unmap_buffer_region(cb.buf);
        dev->destroy_resource(cb.buf);
    }
    cb.buf = {};
    cb.ptr = nullptr;
    cb.range[0] = {};
    cb.range[1] = {};
    // layouts and pipelines die with the device
    cb.layout_param_index.clear();
    cb.pipeline_layout.clear();
}

//*******************************************************************************
// update the buffer with the current settings, one slot per eye (call from on_present)
void vrem_write_cb()
{
    if (g_shared_state == nullptr) return;

    auto& cb = g_shared_state->VREM_cb;
    std::shared_lock lock(cb.mutex);
    if (!cb.ptr) return;

    for (int eye = 0; eye < 2; ++eye) {
        ShaderInjectData d = a_shared.cb_inject_values;    // local copy, source untouched
        d.eye = float(eye);
        memcpy(cb.ptr + uint64_t(eye) * SLOT_SIZE, &d, sizeof(d));
    }
}

// ---------------------------------------------------------------

void vrem_cb_prepare(CmdData& data, pipeline_layout layout, uint32_t action)
{
    data.cb_active = false;
    if (!(action & action_injectCB)) return;

    auto& cb = g_shared_state->VREM_cb;
    std::shared_lock lock(cb.mutex);
    auto it = cb.layout_param_index.find(layout.handle);
    if (it == cb.layout_param_index.end()) return;

    data.cb_layout = layout;
    data.cb_index = it->second;
    data.cb_active = true;
}

// ---------------------------------------------------------------

void vrem_cb_push(command_list* cmd, shader_stage stage)
{
    auto& data = cmd->get_private_data<CmdData>();
    if (!data.cb_active) return;

    auto& cb = g_shared_state->VREM_cb;
    if (!cb.ptr) return;

    const buffer_range& range = cb.range[data.eye & 1];
    descriptor_table_update upd = {};
    upd.binding = 0;
    upd.count = 1;
    upd.type = descriptor_type::constant_buffer;
    upd.descriptors = &range;
    cmd->push_descriptors(stage, data.cb_layout, data.cb_index, upd);
}
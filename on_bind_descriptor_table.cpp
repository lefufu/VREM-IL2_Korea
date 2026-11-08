///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  descriptor_tables binding, modiy and copy functions
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
#include <unordered_map>
#include <shared_mutex>
#include <vector>

#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_objects.h"
#include "addon_logs.h"

#include "to_string.hpp"

static void ensure_size(std::vector<tracked_descriptor>& v, size_t n)
{
	if (v.size() < n) v.resize(n);
}

using namespace reshade::api;

#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT void vrem_on_bind_descriptor_tables(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_table* tables) {

#if _DEBUG_LOGS
		//too verbose
        // log_bind_descriptor_tables(cmd_list, stages, layout, first, count, tables);
#endif

		// ignore compute binds : you only target graphics
        if ((stages & reshade::api::shader_stage::all_graphics) == 0) return;

        auto& st = cmd_list->get_private_data<CmdData>();

		// layout changed, reset all tables
        if (st.layout.handle != layout.handle) {
            st.layout = layout;
            st.table_count = 0;
            for (auto& t : st.tables) t = {};
        }

		if (first + count > _countof(st.tables)) return;   //security check

        for (uint32_t i = 0; i < count; ++i)
            st.tables[first + i] = tables[i];

        if (first + count > st.table_count)
            st.table_count = first + count;


	}
#ifdef _DEBUG
}
#endif


#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT bool vrem_on_update_descriptor_tables(device* device, uint32_t count, const descriptor_table_update* updates) {

#if _DEBUG_LOGS
        //too verbose
		// log_update_descriptor_tables(device, count, updates);
#endif
		
        auto& vp = g_shared_state->VREM_pipelines;

        for (uint32_t i = 0; i < count; ++i) {
            const auto& u = updates[i];

            // seuls ces types nous intéressent (les samplers n'ont pas de ressource)
            const bool is_view = (u.type == descriptor_type::shader_resource_view ||
                u.type == descriptor_type::unordered_access_view);
            const bool is_cbv = (u.type == descriptor_type::constant_buffer);
            if (!is_view && !is_cbv) continue;

            descriptor_heap heap; uint32_t base;
            device->get_descriptor_heap_offset(u.table, u.binding, u.array_offset, &heap, &base);

            std::unique_lock lk(vp.heaps_mutex);
            auto& slots = vp.descriptor_heaps[heap.handle];
            ensure_size(slots, size_t(base) + u.count);

            for (uint32_t j = 0; j < u.count; ++j) {
                tracked_descriptor& t = slots[base + j];
                t.type = u.type;
                t.valid = true;
                if (is_cbv) {
                    t.range = static_cast<const buffer_range*>(u.descriptors)[j];
                    t.view = {};
                }
                else {
                    t.view = static_cast<const resource_view*>(u.descriptors)[j];
                    t.range = {};
                }
            }
        }
		
		
		return false;

	}
#ifdef _DEBUG
}
#endif

#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT bool vrem_on_copy_descriptor_tables(device* device, uint32_t count, const descriptor_table_copy* copies) {

#if _DEBUG_LOGS
        //too verbose
        // log_copy_descriptor_tables(device, count, copies);
#endif

        auto& vp = g_shared_state->VREM_pipelines;

        for (uint32_t i = 0; i < count; ++i) {
            const auto& c = copies[i];

            descriptor_heap sh, dh; uint32_t so, d_o;
            device->get_descriptor_heap_offset(c.source_table, c.source_binding, c.source_array_offset, &sh, &so);
            device->get_descriptor_heap_offset(c.dest_table, c.dest_binding, c.dest_array_offset, &dh, &d_o);

            std::unique_lock lk(vp.heaps_mutex);

            // 1. copie temporaire de la source : source et destination peuvent être
            //    le même vecteur (et se chevaucher), et le resize peut le réallouer
            std::vector<tracked_descriptor> tmp(c.count);   // valid=false par défaut
            auto sit = vp.descriptor_heaps.find(sh.handle);
            if (sit != vp.descriptor_heaps.end())
                for (uint32_t j = 0; j < c.count; ++j)
                    if (so + j < sit->second.size())
                        tmp[j] = sit->second[so + j];

            // 2. écriture dans la destination
            auto& dst = vp.descriptor_heaps[dh.handle];
            ensure_size(dst, size_t(d_o) + c.count);
            for (uint32_t j = 0; j < c.count; ++j)
                dst[d_o + j] = tmp[j];
        }

		return false;

	}
#ifdef _DEBUG
}
#endif


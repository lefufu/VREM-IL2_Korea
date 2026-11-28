///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  dump_descriptors : used to dump descriptors for a pipeline in draw* functions
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

#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_objects.h"
#include "addon_logs.h"
#include "to_string.hpp"

#ifdef _DEBUG
#include "export_texture_DX12.hpp"
#include "export_CB_DX12.hpp"

using namespace reshade::api;

static TextureExporter        g_texture_exporter;
static ConstantBufferExporter g_cb_exporter;

//*******************************************************************************
// to handle push_descriptor
void track_root_descriptors(command_list* cmd, pipeline_layout layout, uint32_t param_index, const descriptor_table_update& update)
{
	if (param_index >= ROOT_PARAM_MAX || update.count == 0 || update.descriptors == nullptr) return;

	auto& rd = cmd->get_private_data<CmdData>().root_desc[param_index];
	rd.layout = layout;
	rd.type = update.type;
	rd.valid = false;

	if (update.type == descriptor_type::constant_buffer) {
		rd.range = static_cast<const buffer_range*>(update.descriptors)[0];
		rd.valid = rd.range.buffer.handle != 0;
	}
	else if (update.type == descriptor_type::shader_resource_view) {
		rd.view = static_cast<const resource_view*>(update.descriptors)[0];
		rd.valid = rd.view.handle != 0;
	}
}


//*******************************************************************************
// dump the textures (SRV) and constant buffers bound by the descriptor tables at draw time.
// same table -> (heap, offset) -> tracked descriptor resolution as log_bound_tables()
void dump_bound_descriptors(command_list* cmd)
{
	auto& st = cmd->get_private_data<CmdData>();

	const bool want_tex = st.flag_texture_dump && g_shared_state->save_texture_flag;
	const bool want_cb = st.flag_cb_dump && g_shared_state->save_cb_flag;
	if (!want_tex && !want_cb) return;

	device* dev = cmd->get_device();
	auto& vp = g_shared_state->VREM_pipelines;
	auto& cbd = g_shared_state->VREM_cb;

	// layout of the pipeline being drawn
	uint64_t layout_handle = 0;
	{
		std::shared_lock lk(cbd.mutex);
		auto pl = cbd.pipeline_layout.find(st.pipeline.handle);
		if (pl != cbd.pipeline_layout.end()) layout_handle = pl->second;
	}
	if (layout_handle == 0) layout_handle = st.layout.handle;
	if (layout_handle == 0) return;

	std::vector<saved_layout_param> params;
	{
		std::shared_lock lk(vp.layouts_mutex);
		auto it = vp.saved_layouts.find(layout_handle);
		if (it == vp.saved_layouts.end()) return;
		params = it->second.params;
	}

	const uint64_t ps_hash = st.current_hash ? st.current_hash : st.pipeline.handle;
	const uint32_t draw_num = (uint32_t)a_shared.count_display;

	// 1. descriptor tables (SRV in this game)
	if (st.layout.handle == layout_handle) {
		for (uint32_t p = 0; p < st.table_count && p < params.size(); ++p) {

			const saved_layout_param& sp = params[p];
			if (sp.type != pipeline_layout_param_type::descriptor_table &&
				sp.type != pipeline_layout_param_type::descriptor_table_with_static_samplers) continue;
			if (st.tables[p].handle == 0) continue;

			uint32_t binding = 0;
			for (const saved_descriptor_range& rg : sp.ranges) {

				const bool is_cb = rg.type == descriptor_type::constant_buffer;
				const bool is_srv = rg.type == descriptor_type::shader_resource_view;

				if ((is_cb && want_cb) || (is_srv && want_tex)) {
					for (uint32_t k = 0; k < rg.count && k < 64; ++k) {

						descriptor_heap heap = {};
						uint32_t off = 0;
						dev->get_descriptor_heap_offset(st.tables[p], binding + k, 0, &heap, &off);
						if (heap.handle == 0) continue;

						tracked_descriptor t;
						bool found = false;
						{
							std::shared_lock lk(vp.heaps_mutex);
							auto hit = vp.descriptor_heaps.find(heap.handle);
							if (hit != vp.descriptor_heaps.end() && off < hit->second.size() && hit->second[off].valid) {
								t = hit->second[off];
								found = true;
							}
						}
						if (!found) continue;

						descriptor_table_update upd = {};
						upd.binding = rg.dx_register_index + k;
						upd.count = 1;

						if (is_cb) {
							upd.type = descriptor_type::constant_buffer;
							upd.descriptors = &t.range;
							g_cb_exporter.export_constant_buffers(cmd, rg.visibility, st.layout, p, upd, ps_hash, draw_num, true);
						}
						else if (t.view.handle != 0) {
							upd.type = descriptor_type::shader_resource_view;
							upd.descriptors = &t.view;
							g_texture_exporter.export_descriptors(cmd, rg.visibility, st.layout, p, upd, ps_hash, draw_num);
						}
					}
				}
				binding += rg.count;
			}
		}
	}

	// 2. root descriptors pushed by the game (CBV in this game). Root arguments persist until they are
	// replaced or the root signature changes : keep only those pushed with the current layout
	for (uint32_t p = 0; p < params.size() && p < ROOT_PARAM_MAX; ++p) {

		const root_descriptor& rd = st.root_desc[p];
		if (!rd.valid || rd.layout.handle != layout_handle) continue;
		if (params[p].type != pipeline_layout_param_type::push_descriptors) continue;

		const descriptor_range& pd = params[p].push_descriptors;

		descriptor_table_update upd = {};
		upd.binding = pd.dx_register_index;               // b register in the file name
		upd.count = 1;

		if (rd.type == descriptor_type::constant_buffer && want_cb) {
			buffer_range r = rd.range;
			if (r.size == 0 || r.size == UINT64_MAX || r.size > ROOT_CB_DUMP_SIZE) r.size = ROOT_CB_DUMP_SIZE;
			upd.type = descriptor_type::constant_buffer;
			upd.descriptors = &r;
			g_cb_exporter.export_constant_buffers(cmd, pd.visibility, rd.layout, p, upd, ps_hash, draw_num, true);
		}
		else if (rd.type == descriptor_type::shader_resource_view && want_tex) {
			upd.type = descriptor_type::shader_resource_view;
			upd.descriptors = &rd.view;
			g_texture_exporter.export_descriptors(cmd, pd.visibility, rd.layout, p, upd, ps_hash, draw_num);
		}
	}
}

//*******************************************************************************
// to be called at each present : writes the files of the previous frame exports
void flush_pending_exports(effect_runtime* runtime)
{
	device* dev = runtime->get_device();
	command_queue* queue = runtime->get_command_queue();
	g_texture_exporter.flush(dev, queue);
	g_cb_exporter.flush(dev, queue);
}

//*******************************************************************************
// reload or device destroyed
void release_pending_exports(device* dev)
{
	if (dev == nullptr) return;
	g_texture_exporter.release_all(dev);
	g_cb_exporter.release_all(dev);
}
#endif
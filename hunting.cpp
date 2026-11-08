///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
// hunting functions
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
#include <vector>
#include <filesystem>
#include <fstream>

#include "config.hpp"
#include "addon_logs.h"
#include "to_string.hpp"	
#include "addon_functions.h"

#include "ps_solid_dxbc.h"   // contain constant color shader code for PS 4.0-5.0
#include "ps_solid_dxil.h"   // contain constant color shader code for PS 6.0

using namespace reshade::api;

thread_local bool g_creating_replacement = false;   

static std::mutex g_solid_mutex;
static std::unordered_map<uint64_t, reshade::api::pipeline> g_solid_pipelines;

// *******************************************************************************************************
// functions to replace shader code by constant color

static bool is_dxil(const void* code, size_t size)
{
	auto p = static_cast<const uint8_t*>(code);
	if (size < 32 || memcmp(p, "DXBC", 4) != 0) return false;
	uint32_t chunks = *reinterpret_cast<const uint32_t*>(p + 28);
	for (uint32_t i = 0; i < chunks && 32 + 4 * (i + 1) <= size; ++i) {
		uint32_t off = *reinterpret_cast<const uint32_t*>(p + 32 + 4 * i);
		if (off + 4 <= size && memcmp(p + off, "DXIL", 4) == 0) return true;
	}
	return false;
}

// *******************************************************************************************************
reshade::api::pipeline get_solid_replacement(uint64_t handle)
{
	std::lock_guard lock(g_solid_mutex);

	auto it = g_solid_pipelines.find(handle);
	if (it != g_solid_pipelines.end())
		return it->second;                       // déjà créé (ou échec mémorisé : handle 0)

	save_pipeline* sp = find__pipeline_per_handle(handle);
	if (sp == nullptr || sp->ps_desc.code_size == 0) {
		g_solid_pipelines[handle] = {};
		return {};
	}

	// PS solid dans le même format que le PS d'origine
	const bool dxil = is_dxil(sp->ps_desc.code, sp->ps_desc.code_size);
	reshade::api::shader_desc ps = {};
	ps.code = dxil ? (const void*)g_ps_dxil : (const void*)g_ps_dxbc;
	ps.code_size = dxil ? sizeof(g_ps_dxil) : sizeof(g_ps_dxbc);
	ps.entry_point = "main";

	// reconstruit les subobjects en pointant sur les membres de sp (pas sur les anciens pointeurs)
	std::vector<reshade::api::pipeline_subobject> subs;
	subs.reserve(sp->subobject_count);
	for (uint32_t i = 0; i < sp->subobject_count; ++i)
	{
		reshade::api::pipeline_subobject s = sp->subobjects[i];
		switch (s.type) {
		case reshade::api::pipeline_subobject_type::pixel_shader:            s.data = &ps; break;
		case reshade::api::pipeline_subobject_type::vertex_shader:           s.data = &sp->vs_desc; break;
		case reshade::api::pipeline_subobject_type::geometry_shader:         s.data = &sp->gs_desc; break;
		case reshade::api::pipeline_subobject_type::hull_shader:             s.data = &sp->hs_desc; break;
		case reshade::api::pipeline_subobject_type::domain_shader:           s.data = &sp->ds_desc; break;
		case reshade::api::pipeline_subobject_type::input_layout:            s.data = sp->input_elements.data(); break;
		case reshade::api::pipeline_subobject_type::primitive_topology:      s.data = &sp->topology; break;
		case reshade::api::pipeline_subobject_type::rasterizer_state:        s.data = &sp->rasterizer; break;
		case reshade::api::pipeline_subobject_type::blend_state:             s.data = &sp->blend; break;
		case reshade::api::pipeline_subobject_type::depth_stencil_state:     s.data = &sp->depth_stencil; break;
		case reshade::api::pipeline_subobject_type::render_target_formats:   s.data = sp->render_target_formats.data(); break;
		case reshade::api::pipeline_subobject_type::depth_stencil_format:    s.data = &sp->depth_stencil_format; break;
		case reshade::api::pipeline_subobject_type::dynamic_pipeline_states: s.data = sp->dynamic_states.data(); break;
		default: continue;   // type non sauvegardé : son pointeur serait périmé, on l'omet
		}
		subs.push_back(s);
	}

	reshade::api::pipeline np = {};
	g_creating_replacement = true;
	const bool ok = sp->device->create_pipeline(sp->layout, (uint32_t)subs.size(), subs.data(), &np);
	g_creating_replacement = false;

	if (!ok) {
		reshade::log::message(reshade::log::level::warning, "solid PS: create_pipeline failed");
		np = {};
	}
	g_solid_pipelines[handle] = np;
	return np;
}

// *******************************************************************************************************

void release_solid_replacement(reshade::api::device* device)
{
	std::lock_guard lock(g_solid_mutex);
	//for (const save_pipeline& sp : g_shared_state->VREM_pipelines.saved_pipelines)
	for (const auto& it : g_solid_pipelines)
	{
		//auto it = g_solid_pipelines.find(sp.pipeline.handle);
		// if (it == g_solid_pipelines.end()) return;
		// if (it->second.handle != 0)
		if (it.second.handle != 0)
		{
			if (g_shared_state->debug_log)
			{
				std::stringstream s;
				s << "addon - deleting cloned pipeline for hunting, handle  = " << std::hex << it.second.handle << "; ";
				reshade::log::message(reshade::log::level::info, s.str().c_str());
			}
			
			device->destroy_pipeline(it.second);
		}
		g_solid_pipelines.erase(it.first);
	}
}

// *******************************************************************************************************
//hunting function called when binding a pipeline
void on_bind_pipeline_hunting(command_list* commandList, pipeline_stage stages, pipeline pipelineHandle)
{
		auto & state = commandList->get_private_data<CmdData>();
	
	state.do_not_draw = false;
	
	// build list of PS shaders and log messages if capture is active
	if (flag_capture)
	{
				
		if (stages == pipeline_stage::all)
		{
			
			
			// handle only pipeline with PS
			save_pipeline* saved_pipe = find__pipeline_per_handle(pipelineHandle.handle);
			if (saved_pipe == nullptr)
			{
				std::stringstream s;
				s << "*** shader hunter : pipeline not found :   " << std::hex << pipelineHandle.handle << ";";
				reshade::log::message(reshade::log::level::info, s.str().c_str());
				return;
			}
			
			
			if (saved_pipe->ps_hash != 0)
			{
								
				auto it = std::find(g_shared_state->PSshader_list.begin(), g_shared_state->PSshader_list.end(), pipelineHandle.handle);

				if (it == g_shared_state->PSshader_list.end()) {
					// new handle, store it
					g_shared_state->PSshader_list.push_back(pipelineHandle.handle);
				}
			}
			
		}
		

		// log bind pipeline
#if _DEBUG_LOGS  
		log_hunting_bind_pipeline(commandList, stages, pipelineHandle);
#endif
		
	}

	// if current pipeline is hunted, eiter replace it by constant color or skip draw call
	if (g_shared_state->PSshader_list.size() > 0)
	{
		if (pipelineHandle.handle == g_shared_state->PSshader_list[g_shared_state->PSshader_index])
		{
			/*if (g_shared_state->color_PS)
			{
				//color
				commandList->bind_pipeline(stages, a_shared.cloned_constant_color_pipeline);
			}
			else
			{
				// skip next draw call
				state.do_not_draw = true;
			} */

			if (g_shared_state->color_PS)
			{
				// remplace le PS par le PS "solid" dans un clone de la pipeline courante
				pipeline repl = get_solid_replacement(pipelineHandle.handle);
				if (repl.handle != 0)
					commandList->bind_pipeline(stages, repl);
				else
					state.do_not_draw = true;   // clone impossible : on masque le draw pour que ça reste visible
			}
			else
			{
				// skip next draw call
				state.do_not_draw = true;
			}
		}
	}

}
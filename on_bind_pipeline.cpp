///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on bind pipeline : do all actions when binding a pipeline
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


#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_objects.h"
#include "addon_logs.h"

#include "to_string.hpp"


using namespace reshade::api;

static uint32_t last_pipeline_hash_PS = 0;

//*******************************************************************************
// prepare the push of the mod settings CB : done at bind, executed before the draw
static void vrem_cb_prepare(CmdData& st, const pipeline& pipe, const FilteredPipelineData& data)
{
	bool wanted = false;
	for (const auto& def : data.shader_definitions)
	{
		if (def.action & action_inject_settings) { wanted = true; break; }
	}
	if (!wanted) return;

	auto& cb = g_shared_state->VREM_cb;
	std::shared_lock lock(cb.mutex);

	auto pl = cb.pipeline_layout.find(pipe.handle);
	if (pl == cb.pipeline_layout.end())
	{
		static int n = 0;
		if (n++ < 5) {
			log_error_cb_prepare(pipe.handle);

		}
		return;
	}
	auto li = cb.layout_param_index.find(pl->second);
	if (li == cb.layout_param_index.end()) return;

	st.cb_layout.handle = pl->second;
	st.cb_index = li->second;
	st.cb_active = true;

}



#ifdef _DEBUG
extern "C" {
#endif
	//*******************************************************************************
	// on bind pipeline : do all actions when binding a pipeline
	VREM_EXPORT void vrem_on_bind_pipeline(command_list* commandList, pipeline_stage stages, pipeline pipe)
	{


#if _DEBUG_CRASH 
		reshade::log::message(reshade::log::level::info, "***** addon - vrem_on_bind_pipeline started");
#endif

#ifdef _DEBUG		


		// store handle if shader hunting and PS, replace pipleine or skip the draw call
		if (g_shared_state->shader_hunter)
		{
			on_bind_pipeline_hunting(commandList, stages, pipe);
			// skip processing if shader hunting
#if _DEBUG_CRASH  
			reshade::log::message(reshade::log::level::info, "***** addon - vrem_on_bind_pipeline ended (hunting)");
#endif
			return;
		}
#endif

		//store the current pipeline handle in the command list private data for later use in on_draw() and on_dispatch()
		auto& st = commandList->get_private_data<CmdData>();
		st.pipeline = pipe;
		st.to_process_in_draw = false;
		st.cb_active = false;               // reset : otherwise the previous pipeline's state would be kept
		st.flag_cb_dump = false;             // reset : otherwise the previous pipeline's state would be kept
		st.flag_texture_dump = false;             // reset : otherwise the previous pipeline's state would be kept

		// identify if the pipeline is to be processed
		auto it = filtered_pipeline.find(pipe.handle);

		if (it == filtered_pipeline.end()) [[likely]]
		{
#if _DEBUG_CRASH 
			reshade::log::message(reshade::log::level::info, "***** addon - vrem_on_bind_pipeline ended (not used)");
#endif
			return;
		}

#if _DEBUG_LOGS  
		log_shader_binded(pipe.handle, it->second.shader_definitions);
#endif

		st.to_process_in_draw = process_action_bind(st, it->second, commandList, stages, pipe);

		// mod settings CB, pushed in on_draw*()
		vrem_cb_prepare(st, pipe, it->second);


#if _DEBUG_CRASH 
		reshade::log::message(reshade::log::level::info, "***** addon - vrem_on_bind_pipeline ended");
#endif
	}
#ifdef _DEBUG
}
#endif
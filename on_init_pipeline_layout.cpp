
///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on_init_pipeline_layout : called once per game session, initialize pipeline layout
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

//----------------------------------------------------------------------------------------
// Saves the exact HLSL register/space <-> root parameter mapping of a pipeline_layout.
// Call this from the reshade::addon_event::init_pipeline_layout hook.
// This is the function that gives you the "clean" mapping that bind_descriptor_table
// cannot provide (it only knows heap offsets, not HLSL registers).
void save_pipeline_layout_in_list(
    reshade::api::device* device,
    uint32_t param_count,
    const reshade::api::pipeline_layout_param* params,
    reshade::api::pipeline_layout layout) {

    // already known (a layout can be reused by several pipelines): nothing to do
    if (g_shared_state->VREM_pipelines.saved_layouts.count(layout.handle) > 0)
        return;

    saved_pipeline_layout entry;
    entry.device = device;
    entry.layout = layout;
    entry.params.resize(param_count);

    for (uint32_t i = 0; i < param_count; i++) {
        const auto& p = params[i];
        entry.params[i].type = p.type;

        switch (p.type) {
        case reshade::api::pipeline_layout_param_type::descriptor_table:
        case reshade::api::pipeline_layout_param_type::descriptor_table_with_static_samplers: {
            const uint32_t range_count = p.descriptor_table.count;
            entry.params[i].ranges.resize(range_count);

            for (uint32_t r = 0; r < range_count; r++) {
                const auto& range = p.descriptor_table.ranges[r];
                entry.params[i].ranges[r] = saved_descriptor_range{
                    range.type,
                    range.dx_register_index,
                    range.dx_register_space,
                    range.count,
                    range.visibility
                };

            }
            break;
        }

        case reshade::api::pipeline_layout_param_type::push_constants: {
            entry.params[i].push_constants = p.push_constants;
            break;
        }

        case reshade::api::pipeline_layout_param_type::push_descriptors: {
            entry.params[i].push_descriptors = p.push_descriptors;
            break;
        }

        default:
            break;
        }
    }

    g_shared_state->VREM_pipelines.saved_layouts.emplace(layout.handle, std::move(entry));
}




#ifdef _DEBUG
extern "C" {
#endif

	// *******************************************************************************************************
	/// create_CB_layout()
	///  create a CB layout for CB created or modified by VREM 


	//*******************************************************************************
	VREM_EXPORT void vrem_on_init_pipeline_layout(device* dev, uint32_t paramCount, const pipeline_layout_param* params, pipeline_layout layout) {

#if _DEBUG_LOGS
        //very verobse and also displayed in pipeline
		//log_init_pipeline_layout(paramCount, params, layout);

#endif
        save_pipeline_layout_in_list(dev, paramCount, params,layout);
	}
#ifdef _DEBUG
}
#endif



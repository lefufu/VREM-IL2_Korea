
///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on_create_pipeline_layout : modify pipeline layout to add a constant buffer for mod settings
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

static thread_local std::vector<pipeline_layout_param> s_params;

// *******************************************************************************************************
static uint32_t layout_cost(uint32_t count, const pipeline_layout_param* p)
{
    uint32_t cost = 0;
    for (uint32_t i = 0; i < count; ++i) {
        switch (p[i].type) {
        case pipeline_layout_param_type::push_constants:
            cost += p[i].push_constants.count; break;
        case pipeline_layout_param_type::descriptor_table:
        case pipeline_layout_param_type::descriptor_table_with_static_samplers:
            cost += 1; break;
        default:
            cost += 2; break;
        }
    }
    return cost;
}


#ifdef _DEBUG
extern "C" {
#endif

	// *******************************************************************************************************
	/// create_CB_layout()
	///  create a CB layout for CB created or modified by VREM 
    /// 
    // static thread_local std::vector<pipeline_layout_param> s_params;
    // static thread_local uint32_t s_added_index;

	//*******************************************************************************
	VREM_EXPORT bool vrem_on_create_pipeline_layout(device* dev, uint32_t& param_count, pipeline_layout_param*& params) {

#if _DEBUG_LOGS
		
		log_on_create_pipeline_layout();

#endif
        
		constexpr uint32_t MAX_PARAMS = 64;
		static thread_local pipeline_layout_param s_buf[MAX_PARAMS + 1];

		uint32_t cost = layout_cost(param_count, params);
		if (param_count > MAX_PARAMS || cost + 2 > 64) {
			std::stringstream s;
			s << "!!! addon - create_pipeline_layout : SKIP n=" << param_count << " cost=" << cost << "; ";
			reshade::log::message(reshade::log::level::info, s.str().c_str());
			return false;
		}

		for (uint32_t i = 0; i < param_count; ++i)
			s_buf[i] = params[i];

		pipeline_layout_param p = {};
		p.type = pipeline_layout_param_type::push_descriptors;
		p.push_descriptors.binding = 0;
		p.push_descriptors.count = 1;
		p.push_descriptors.type = descriptor_type::constant_buffer;
		p.push_descriptors.visibility = shader_stage::all;
		p.push_descriptors.dx_register_index = CBINDEX;
		p.push_descriptors.dx_register_space = CBSPACE;
		s_buf[param_count] = p;

		params = s_buf;
		param_count = param_count + 1;
		return true;
		
	}
#ifdef _DEBUG
}
#endif
///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on_create_pipeline : save shader code
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
#include "addon_functions.h"
#include "addon_objects.h"
#include "addon_logs.h"
#include "to_string.hpp"

extern std::unordered_map<uint32_t, std::vector<uint8_t>> shader_code_cache;

// hash of the code AFTER static replacement -> hash of the original shader
std::unordered_map<uint32_t, uint32_t> replaced_hash_origin;

using namespace reshade::api;

// *******************************************************************************************************
// DXIL (SM6) or DXBC (SM4/5) ? (a DXIL container is a DXBC container with a DXIL chunk)
// NB : same helper exists in hunting.cpp, move it to a common header
static bool is_dxil_code(const void* code, size_t size)
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
// load_shader_code_static() : replace shader code once for all at creation as done by crosire for usage in create_pipeline
// update shader_by_hash with new hash
// shader_hash = hash of orig_desc computed by the caller (not recomputed here)
// 
bool load_shader_code_static(device_api device_type, shader_desc& orig_desc, uint32_t shader_hash)
{
	// stage absent or empty : normal in DX12, nothing to do (no log, too verbose)
	if (orig_desc.code == nullptr || orig_desc.code_size == 0)
		return false;

	auto it = shader_by_hash.find(shader_hash);
	// no static replace if debug mode (to allow hot reload of the mod)
	if (it == shader_by_hash.end() || !(it->second.action & action_replace) || g_shared_state->debug)
		return false;

	auto it2 = shader_code_cache.find(shader_hash);
	if (it2 == shader_code_cache.end() || it2->second.empty())
		return false;

	// replace code by the one from cso
	const std::vector<uint8_t>& shader_code = it2->second;

	// DX12 : the replaced shader must keep the bytecode format of the original one (DXBC or DXIL),
	// otherwise the game's own CreatePipelineState fails
	if (device_type == device_api::d3d12 &&
		is_dxil_code(shader_code.data(), shader_code.size()) != is_dxil_code(orig_desc.code, orig_desc.code_size))
	{
		log_error_shader_type(shader_hash);
		return false;
	}

	orig_desc.code = shader_code.data();
	orig_desc.code_size = shader_code.size();

	const uint32_t new_shader_hash = calculateShaderHash(orig_desc);
	replaced_hash_origin[new_shader_hash] = shader_hash;

#if _DEBUG_LOGS  
	// log BEFORE emplace : an insertion may rehash the map and invalidate 'it'
	log_replaced_shader_code(shader_hash, it, new_shader_hash);
#endif

	// add a new entry in shader_by_hash with new hash if not existing
	if (shader_by_hash.find(new_shader_hash) == shader_by_hash.end())
	{
		Shader_Definition new_shader_def = it->second;   // copy before emplace
		new_shader_def.hash = new_shader_hash;
		shader_by_hash.emplace(new_shader_hash, std::move(new_shader_def));
	}
	return true;
}



#ifdef _DEBUG
extern "C" {
#endif


	//*******************************************************************************
	// Save shaders code and replace statically shader code (if option setup)
	VREM_EXPORT  bool vrem_on_create_pipeline(device* device, pipeline_layout layout, uint32_t subobject_count, const pipeline_subobject* subobjects) {

		const device_api device_type = device->get_api();

		bool replaced_stages = false;
#if _DEBUG_CRASH  
		reshade::log::message(reshade::log::level::info, "addon - vrem_on_create_pipeline starting");
#endif		
		
#if _DEBUG_LOGS
		//too verbose
		log_on_create_pipeline(layout, subobject_count);
#endif
		
		for (uint32_t i = 0; i < subobject_count; ++i)
		{
			
			const auto& sub = subobjects[i];
			if (ALLOWED_SHADERS.count(sub.type) > 0)
			{
				auto* desc = static_cast<shader_desc*>(subobjects[i].data);
				if (desc == nullptr || desc->code == nullptr || desc->code_size == 0)
					continue;   // stage absent (DX12 : normal)

				const uint32_t shader_hash = calculateShaderHash(*desc);

#if _DEBUG_LOGS
				//too verbose
				log_on_create_pipeline_objects(i, subobjects[i], shader_hash);
#endif

				/*
				// afficher le hash de ce stage (vertex_shader, pixel_shader, ...)
				std::stringstream s;
				s << "addon - vrem_on_create_pipeline, pipeline_layout =  " << to_string(sub.type) << " hash = 0x" << std::hex << shader_hash;
				reshade::log::message(reshade::log::level::info, s.str().c_str());

				//log hash of objects
				*/
				// save shader code for debug
				if (g_shared_state->debug) {
					//save_shader_code(device_type, *static_cast<const shader_desc*>(subobjects[i].data));
					save_shader_code(device_type, *desc);
				}
				// handle replacement of code if option setup for this shader
				replaced_stages |= load_shader_code_static(device_type, *desc, shader_hash);
				//break;
			}
		}

#if _DEBUG_CRASH  
		reshade::log::message(reshade::log::level::info, "addon - vrem_on_create_pipeline ending");
#endif
	// Return whether any shader code was replaced
	return replaced_stages;

	}
#ifdef _DEBUG
}
#endif

///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  setup list of pipeline to process, regarding mod option. Clone pipelines if needed
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

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>
#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_logs.h"
#include "to_string.hpp"

extern std::unordered_map<uint32_t, std::vector<uint8_t>> shader_code_cache;

// *******************************************************************************************************
/// <summary>
/// Check if the shader definition is related to an active option in the mod settings
/// </summary>

bool check_if_active_option(const Shader_Definition& shader_def)
{
	for (uint32_t index : shader_def.VREM_options)
	{
		if (a_shared.VREM_setting[index] != 0)
			return true;
	}
	return false;
}

// *******************************************************************************************************
/// <summary>
/// Find the mod definition matching ONE shader hash.
/// Assumes shader_by_hash is indexed by shader hash (adapt if is_in_mod_hash does something else).
/// </summary>

static std::optional<Shader_Definition> find_mod_shader_for_hash(uint32_t hash)
{
	auto it = shader_by_hash.find(hash);
	if (it == shader_by_hash.end())
		return std::nullopt;
	return it->second;
}

static bool is_shader_subobject(reshade::api::pipeline_subobject_type type)
{
	using T = reshade::api::pipeline_subobject_type;
	return type == T::vertex_shader || type == T::pixel_shader || type == T::geometry_shader
		|| type == T::hull_shader || type == T::domain_shader || type == T::compute_shader;
}

// *******************************************************************************************************
/// <summary>
/// setup filtered pipelines regarding mod settings.
/// filtered_pipeline : std::unordered_map<uint64_t, std::vector<Shader_Definition>>
/// </summary>

bool setup_filtered_pipelines(reshade::api::device* device, reshade::api::effect_runtime* runtime)
{
	for (auto& p : g_shared_state->VREM_pipelines.saved_pipelines)
	{
		// Validate pipeline handle
		if (p.pipeline.handle == 0)
			continue;

		// pipeline not processed yet?
		if (filtered_pipeline.find(p.pipeline.handle) == filtered_pipeline.end())
		{
			// ------------------------------------------------------------------
			// 1) collect all definitions related to this pipeline
			// ------------------------------------------------------------------
			std::vector<Shader_Definition> defs;
			std::unordered_set<uint32_t> seen_hashes;

			// a) by hash: one lookup per shader subobject
			for (uint32_t k = 0; k < p.subobject_count; ++k)
			{
				if (!is_shader_subobject(p.subobjects[k].type))
					continue;

				std::optional<Shader_Definition> opt = find_mod_shader_for_hash(p.hash[k]);
				if (opt.has_value() && seen_hashes.insert(p.hash[k]).second)
				{
					opt->hash = p.hash[k];
					defs.push_back(*opt);
				}
			}

			// b) static replacement: bytecode comparison
			for (auto& [orig_hash, shader_def] : shader_by_hash)
			{
				if (!(shader_def.action & action_replace)) continue;
				if (seen_hashes.count(orig_hash)) continue;

				auto it_cso = shader_code_cache.find(orig_hash);
				if (it_cso == shader_code_cache.end()) continue;

				if (p.ps_bytecode == it_cso->second || p.vs_bytecode == it_cso->second)
				{
					Shader_Definition def = shader_def;
					def.hash = orig_hash;
					seen_hashes.insert(orig_hash);
					defs.push_back(def);
				}
			}

			// ------------------------------------------------------------------
			// 2) keep only active definitions
			// ------------------------------------------------------------------
			std::vector<Shader_Definition> active_defs;
			for (auto& d : defs)
			{
				bool active = check_if_active_option(d);
#if _DEBUG_LOGS
				log_shader(p.pipeline, d, active);
#endif
				if (active)
					active_defs.push_back(d);
			}

			if (!active_defs.empty())
			{
				// --------------------------------------------------------------
				// 3) a SINGLE clone, with only the active hashes
				//    (0 = "do not replace this subobject", 0 must not be a key of shader_code_cache)
				// --------------------------------------------------------------
				std::vector<uint32_t> clone_hashes(p.subobject_count, 0);
				bool need_clone = false;

				for (auto& d : active_defs)
				{
					if (!(d.action & action_replace_bind))
						continue;

					for (uint32_t k = 0; k < p.subobject_count; ++k)
					{
						if (is_shader_subobject(p.subobjects[k].type) && p.hash[k] == d.hash)
						{
							clone_hashes[k] = d.hash;
							need_clone = true;
						}
					}
				}

				if (need_clone)
				{
					reshade::api::pipeline cloned_pipeline =
						clone_pipeline(p.device, p.layout, p.subobject_count,
							p.subobjects, p.pipeline, clone_hashes.data());

					if (cloned_pipeline.handle == 0)
					{
						log_pipeline_clone_error(p.pipeline.handle);
					}
					else
					{
						// kept so the clones can be destroyed when the addon is reloaded
						cloned_pipeline_list.emplace(cloned_pipeline.handle, cloned_pipeline);

						// same substitute pipeline for all "bind" definitions
						for (auto& d : active_defs)
							if (d.action & action_replace_bind)
								d.substitute_pipeline = cloned_pipeline;
					}
				}

				filtered_pipeline.emplace(p.pipeline.handle, std::move(active_defs));
#if _DEBUG_LOGS
				log_filtered_added(p.pipeline.handle);
#endif
			}
		}

		// ----------------------------------------------------------------------
		// "constant color" pipeline for hunting
		// IMPORTANT: clone_pipeline reads hash[i] for i < subobject_count,
		// so the array must have subobject_count elements (otherwise out-of-bounds read in DX12).
		// ----------------------------------------------------------------------
		if (p.pipeline.handle == a_shared.first_PS_pipeline_handle)
		{
			std::vector<uint32_t> hash_color(p.subobject_count, 0);
			for (uint32_t k = 0; k < p.subobject_count; ++k)
			{
				if (p.subobjects[k].type == reshade::api::pipeline_subobject_type::pixel_shader)
					hash_color[k] = CONSTANT_HASH;
			}

			a_shared.cloned_constant_color_pipeline =
				clone_pipeline(p.device, p.layout, p.subobject_count,
					p.subobjects, p.pipeline, hash_color.data());
		}
	}

	return false;
}




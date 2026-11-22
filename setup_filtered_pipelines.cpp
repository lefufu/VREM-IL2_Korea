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
#include <vector>
#include <algorithm>
#include <array>
#include <sstream>
#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_logs.h"
#include "to_string.hpp"

extern std::unordered_map<uint32_t, std::vector<uint8_t>> shader_code_cache;
extern std::unordered_map<uint32_t, uint32_t> replaced_hash_origin;   // defined in on_create_pipeline.cpp

// *******************************************************************************************************
// hash of an already statically replaced shader -> hash of the original shader (otherwise unchanged)
static uint32_t resolve_hash(uint32_t h)
{
	auto it = replaced_hash_origin.find(h);
	return (it != replaced_hash_origin.end()) ? it->second : h;
}

// *******************************************************************************************************
static bool is_null_definition(const Shader_Definition& d)
{
	return d.action == action_null;
}

// *******************************************************************************************************
// Add a definition, or replace the existing one if it is a "null" definition
static void add_or_upgrade_definition(std::vector<Shader_Definition>& defs,
	const Shader_Definition& def, uint32_t hash)
{
	auto it = std::find_if(defs.begin(), defs.end(),
		[hash](const Shader_Definition& d) { return d.hash == hash; });

	if (it != defs.end())
	{
		if (is_null_definition(*it) && !is_null_definition(def))
		{
			*it = def;
			it->hash = hash;
		}
		return;
	}

	Shader_Definition d = def;
	d.hash = hash;
	defs.push_back(std::move(d));
}

// *******************************************************************************************************
static Shader_Definition make_null_definition(uint32_t hash)
{
	Shader_Definition d{};
	d.hash = hash;
	d.action = action_null;
	d.feature = Feature::Null;
	return d;
}

// *******************************************************************************************************
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
// Does the (non empty) bytecode of one of the 5 stages of the pipeline match the .cso ?
static bool pipeline_has_bytecode(const save_pipeline& p, const std::vector<uint8_t>& cso)
{
	if (cso.empty())
		return false;

	const std::array<const std::vector<uint8_t>*, 5> stages =
	{ &p.vs_bytecode, &p.ps_bytecode, &p.gs_bytecode, &p.hs_bytecode, &p.ds_bytecode };

	for (const auto* code : stages)
	{
		if (!code->empty() && *code == cso)
			return true;
	}
	return false;
}

// *******************************************************************************************************
/// <summary>
/// setup filtered pipelines regarding mod settings (DX12 : several shaders per pipeline)
/// </summary>
bool setup_filtered_pipelines(reshade::api::device* device, reshade::api::effect_runtime* runtime)
{
	for (auto& p : g_shared_state->VREM_pipelines.saved_pipelines)
	{
		if (p.pipeline.handle == 0)
			continue;

		// number of valid entries in p.hash[] / p.subobjects
		const uint32_t hc = (std::min<uint32_t>)(p.subobject_count, MAX_PIPELINE_OBJECTS);

		if (filtered_pipeline.find(p.pipeline.handle) == filtered_pipeline.end())
		{
			std::vector<Shader_Definition> defs;

			// ---------------------------------------------------------------
			// 1) Search by hash on all shaders of the pipeline
			//    (hash[] + vs_hash/ps_hash for safety, duplicates are removed)
			//    Unknown hash or inactive option => null definition
			// ---------------------------------------------------------------
			std::vector<uint32_t> candidates;
			candidates.reserve(MAX_PIPELINE_OBJECTS + 2);

			for (uint32_t i = 0; i < hc; ++i)
				candidates.push_back(p.hash[i]);
			candidates.push_back(p.vs_hash);
			candidates.push_back(p.ps_hash);

			for (uint32_t h : candidates)
			{
				if (h == 0)
					continue;

				h = resolve_hash(h);   // 0x7ebd... (replaced code) -> 0xbaf5... (original)

				auto it = shader_by_hash.find(h);
				if (it != shader_by_hash.end() && check_if_active_option(it->second))
					add_or_upgrade_definition(defs, it->second, h);
				else
					add_or_upgrade_definition(defs, make_null_definition(h), h);
			}

			// ---------------------------------------------------------------
			// 2) Static replacement : pipeline bytecode vs .cso
			// ---------------------------------------------------------------
			for (auto& [orig_hash, shader_def] : shader_by_hash)
			{
				if (!(shader_def.action & action_replace)) continue;
				if (!check_if_active_option(shader_def)) continue;

				auto it_cso = shader_code_cache.find(orig_hash);
				if (it_cso == shader_code_cache.end()) continue;

				if (pipeline_has_bytecode(p, it_cso->second))
					add_or_upgrade_definition(defs, shader_def, orig_hash);
			}

			// ---------------------------------------------------------------
			// 3) Registration + single clone if needed
			// ---------------------------------------------------------------
			constexpr bool STORE_ALL_PIPELINES = false;   // true = diagnostic

			const bool has_active = std::any_of(defs.begin(), defs.end(),
				[](const Shader_Definition& d) { return !is_null_definition(d); });

			if (!defs.empty() && (has_active || STORE_ALL_PIPELINES))
			{
				FilteredPipelineData data{};
				data.cloned_pipeline = {};

				const bool need_clone = std::any_of(defs.begin(), defs.end(),
					[](const Shader_Definition& d) { return (d.action & action_replace_bind) != 0; });

				bool clone_failed = false;

				if (need_clone)
				{
					// hashes to replace, aligned on the subobject index.
					// 0 = keep the original shader.
					uint32_t clone_hash[MAX_PIPELINE_OBJECTS] = {};

					for (uint32_t i = 0; i < hc; ++i)
					{
						const uint32_t h = resolve_hash(p.hash[i]);
						if (h == 0)
							continue;

						const bool to_replace = std::any_of(defs.begin(), defs.end(),
							[h](const Shader_Definition& d)
							{ return d.hash == h && (d.action & action_replace_bind) != 0; });

						if (to_replace)
							clone_hash[i] = h;
					}

					// rebuilt from the deep-copied members of save_pipeline (like hunting.cpp)
					pipeline cloned_pipeline = clone_pipeline(p, clone_hash, hc);

					if (cloned_pipeline.handle != 0)
					{
						data.cloned_pipeline = cloned_pipeline;
						cloned_pipeline_list.emplace(cloned_pipeline.handle, cloned_pipeline);
					}
					else
					{
						clone_failed = true;
						std::stringstream s;
						s << "addon - setup_filtered_pipelines: clone failed for pipeline 0x"
							<< std::hex << p.pipeline.handle << ", pipeline not registered";
						reshade::log::message(reshade::log::level::error, s.str().c_str());
					}
				}

				if (!clone_failed)
				{
					data.shader_definitions = std::move(defs);
					filtered_pipeline.emplace(p.pipeline.handle, std::move(data));

#if _DEBUG_LOGS
					// ***************************************************************                   
					// removed because too verbose !
					log_filtered_added(p.pipeline.handle);
#endif
				}
			}
		}
	}

	return false;
}

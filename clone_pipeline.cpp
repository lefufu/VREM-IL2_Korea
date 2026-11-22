///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// ----------------------------------------------------------------------------------------
//  clone pipeline and add reference of the cloned pipeline in the original 
// ----------------------------------------------------------------------------------------
// (c) Lefuneste. All rights reserved. (see license header of the other source files)
/////////////////////////////////////////////////////////////////////////

#include <reshade.hpp>
#include "config.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "addon_objects.h"
#include "addon_logs.h"

extern std::unordered_map<uint32_t, std::vector<uint8_t>> shader_code_cache;
std::unordered_map<uint64_t, reshade::api::pipeline> cloned_pipeline_list;

// defined in hunting.cpp : on_init_pipeline must NOT save pipelines created while it is true
extern thread_local bool g_creating_replacement;

using namespace reshade::api;

// *******************************************************************************************************
/// <summary>
/// clone a saved pipeline, replacing the code of the shaders whose hash is given.
/// The subobjects are rebuilt from the deep-copied members of save_pipeline
/// (sp.subobjects[i].data is stale after init_pipeline, only type/count are used).
/// hash[i] is aligned on the subobject index, 0 = keep original shader.
/// </summary>
pipeline clone_pipeline(save_pipeline& sp, const uint32_t* hash, uint32_t hashCount)
{
    pipeline clone = {};

    // local copies of the shader descs : code replaced below if needed
    shader_desc vs = sp.vs_desc, ps = sp.ps_desc, gs = sp.gs_desc, hs = sp.hs_desc, ds = sp.ds_desc;

    std::vector<pipeline_subobject> subs;
    subs.reserve(sp.subobject_count);

    for (uint32_t i = 0; i < sp.subobject_count; ++i)
    {
        pipeline_subobject s = sp.subobjects[i];     // type and count only
        shader_desc* sd = nullptr;

        switch (s.type)
        {
        case pipeline_subobject_type::vertex_shader:           sd = &vs; break;
        case pipeline_subobject_type::pixel_shader:            sd = &ps; break;
        case pipeline_subobject_type::geometry_shader:         sd = &gs; break;
        case pipeline_subobject_type::hull_shader:             sd = &hs; break;
        case pipeline_subobject_type::domain_shader:           sd = &ds; break;
        case pipeline_subobject_type::input_layout:            s.data = sp.input_elements.data(); break;
        case pipeline_subobject_type::primitive_topology:      s.data = &sp.topology; break;
        case pipeline_subobject_type::rasterizer_state:        s.data = &sp.rasterizer; break;
        case pipeline_subobject_type::blend_state:             s.data = &sp.blend; break;
        case pipeline_subobject_type::depth_stencil_state:     s.data = &sp.depth_stencil; break;
        case pipeline_subobject_type::render_target_formats:   s.data = sp.render_target_formats.data(); break;
        case pipeline_subobject_type::depth_stencil_format:    s.data = &sp.depth_stencil_format; break;
        case pipeline_subobject_type::dynamic_pipeline_states: s.data = sp.dynamic_states.data(); break;
        default: continue;   // type not saved : its pointer would be stale, omit it
        }

        if (sd != nullptr)
        {
            if (i < hashCount && hash[i] != 0)
            {
                auto it = shader_code_cache.find(hash[i]);
                if (it != shader_code_cache.end() && !it->second.empty())
                {
                    sd->code = it->second.data();
                    sd->code_size = it->second.size();

                    /*
                    std::stringstream ss;
                    ss << "clone_pipeline: subobject[" << i << "] replaced by cso, hash=0x"
                       << std::hex << hash[i] << ", size=" << std::dec << it->second.size();
                    reshade::log::message(reshade::log::level::info, ss.str().c_str());
                    */
                }
            }
            s.data = sd;
        }
        subs.push_back(s);
    }

    g_creating_replacement = true;   // do not save our own clone in on_init_pipeline
    const bool ok = sp.device->create_pipeline(sp.layout, (uint32_t)subs.size(), subs.data(), &clone);
    g_creating_replacement = false;

    if (ok)
    {
#if _DEBUG_LOGS  
        log_pipeline_clone_OK(sp.pipeline.handle, clone.handle);
#endif
    }
    else
    {
        clone = {};
        log_pipeline_clone_error(sp.pipeline.handle);
    }
    return clone;
}

// *******************************************************************************************************
/// <summary>
/// delete cloned pipelines
/// </summary>
void delete_cloned_pipelines(reshade::api::device* dev)
{
    for (auto& [handle, pipeline] : cloned_pipeline_list)
    {
        if (pipeline.handle != 0)
        {
#if _DEBUG_LOGS  
            log_delete_cloned_pipeline(pipeline.handle);
#endif
            dev->destroy_pipeline(pipeline);
        }
    }
    cloned_pipeline_list.clear();
}

@echo off

REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/model.vert.spv model.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/model.geom.spv model.geom	
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/model.frag.spv model.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_model.vert.spv lp_notex_model.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_model.geom.spv lp_notex_model.geom	
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_model.frag.spv lp_notex_model.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_animated.vert.spv lp_notex_animated.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_animated.geom.spv lp_notex_animated.geom	
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_animated.frag.spv lp_notex_animated.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/model_shadow.vert.spv model_shadow.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/model_shadow.frag.spv model_shadow.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_model_shadow.vert.spv lp_notex_model_shadow.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/lp_notex_model_shadow.frag.spv lp_notex_model_shadow.frag 
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/deferred_lighting.vert.spv deferred_lighting.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/deferred_lighting.frag.spv deferred_lighting.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/atmosphere.vert.spv atmosphere.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/atmosphere.frag.spv atmosphere.frag
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/atmosphere.geom.spv atmosphere.geom
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/render_atmosphere.vert.spv render_atmosphere.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/render_atmosphere.frag.spv render_atmosphere.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain.vert.spv terrain.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain.geom.spv terrain.geom	
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain.frag.spv terrain.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain_shadow.vert.spv terrain_shadow.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain_shadow.frag.spv terrain_shadow.frag 
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain_pointer.vert.spv terrain_pointer.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/terrain_pointer.frag.spv terrain_pointer.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/debug_frustum.vert.spv debug_frustum.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/debug_frustum.frag.spv debug_frustum.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/screen_quad.vert.spv screen_quad.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/screen_quad.frag.spv screen_quad.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/pfx_ssr.vert.spv pfx_ssr.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/pfx_ssr.frag.spv pfx_ssr.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/pfx_final.vert.spv pfx_final.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/pfx_final.frag.spv pfx_final.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/uiquad.vert.spv uiquad.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/uiquad.frag.spv uiquad.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/uifontquad.vert.spv uifontquad.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/uifontquad.frag.spv uifontquad.frag
REM 
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/hitbox_render.vert.spv hitbox_render.vert
REM C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe -V -o SPV/hitbox_render.frag.spv hitbox_render.frag

make

popd
echo build shader

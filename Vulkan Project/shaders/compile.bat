
glslangValidator.exe -V -o compiled/process/equi_to_cube.vert.spv process/equi_to_cube.vert
glslangValidator.exe -V -o compiled/process/equi_to_cube.frag.spv process/equi_to_cube.frag

glslangValidator.exe -V -o compiled/deferred/geometry_pass.vert.spv deferred/geometry_pass.vert
glslangValidator.exe -V -o compiled/deferred/geometry_pass.frag.spv deferred/geometry_pass.frag
glslangValidator.exe -V -o compiled/deferred/lighting_pass.vert.spv deferred/lighting_pass.vert
glslangValidator.exe -V -o compiled/deferred/lighting_pass.frag.spv deferred/lighting_pass.frag

glslangValidator.exe -V -o compiled/forward/skybox.vert.spv forward/skybox.vert
glslangValidator.exe -V -o compiled/forward/skybox.frag.spv forward/skybox.frag


pause
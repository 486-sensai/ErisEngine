glslangValidator.exe -V triangle.vert -o vert.spv
glslangValidator.exe -V triangle.frag -o frag.spv
glslangValidator.exe -V skybox.vert -o sky_vert.spv
glslangValidator.exe -V skybox.frag -o sky_frag.spv
glslangValidator.exe -V grid.vert -o grid_vert.spv
glslangValidator.exe -V grid.frag -o grid_frag.spv
glslangValidator.exe -V shadow.vert -o shadow_vert.spv

glslangValidator.exe -V gtao.comp -o gtao.spv

glslangValidator.exe -V Lumen/main.vert -o Lumen/main_vert.spv
glslangValidator.exe -V Lumen/main.frag -o Lumen/main_frag.spv
glslangValidator.exe -V Lumen/gbuffer.vert -o Lumen/gbuffer_vert.spv
glslangValidator.exe -V Lumen/gbuffer.frag -o Lumen/gbuffer_frag.spv
pause

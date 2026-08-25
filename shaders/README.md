When a new shader is added here, make sure to compile it to a SpriV file.
If available update the CMakeLists.txt to auto compile it at build.

use glslangvalidator to compile a .comp, .vert, .frag to a spv file:
    glslangvalidator -V shader.vert -o shader.vert.spv

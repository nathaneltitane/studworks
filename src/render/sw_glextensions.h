#pragma once

void swInitializeGLExtensions(const QOpenGLContext* Context);

extern bool gSupportsVertexBufferObject;
extern bool gSupportsFramebufferObject;
extern bool gSupportsBlendFuncSeparate;
extern bool gSupportsAnisotropic;
extern GLfloat gMaxAnisotropy;

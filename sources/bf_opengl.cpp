void Check_OpenGL_Errors() {
    auto err = glGetError();
    if (!err)
        return;

    const char* error = nullptr;

    switch (err) {
    case GL_INVALID_ENUM:
        error = "INVALID_ENUM";
        break;
    case GL_INVALID_VALUE:
        error = "INVALID_VALUE";
        break;
    case GL_INVALID_OPERATION:
        error = "INVALID_OPERATION";
        break;
    case GL_STACK_OVERFLOW:
        error = "STACK_OVERFLOW";
        break;
    case GL_STACK_UNDERFLOW:
        error = "STACK_UNDERFLOW";
        break;
    case GL_OUT_OF_MEMORY:
        error = "OUT_OF_MEMORY";
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        error = "INVALID_FRAMEBUFFER_OPERATION";
        break;
    default:
        DEBUG_Print("Unknown OpenGL error occurred!\n");
        INVALID_PATH;
        break;
    }

    DEBUG_Print("OpenGL error occurred! %s\n", error);
    INVALID_PATH;
}

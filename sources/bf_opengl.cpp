void Check_OpenGL_Errors() {
    auto err = glGetError();
    if (err)
        INVALID_PATH;
}

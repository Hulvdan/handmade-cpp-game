void Check_OpenGL_Errors() {
    auto err = glGetError();
    if (err)
        assert(false);
}

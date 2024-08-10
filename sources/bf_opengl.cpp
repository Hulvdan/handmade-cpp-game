#define BF_FLOAT GL_FLOAT
#define BF_FLOAT GL_FLOAT

//----------------------------------------------------------------------------------
// Vertex arrays and buffers.
//----------------------------------------------------------------------------------

// TODO: Usage
uint BFGL_Load_Vertex_Array() {
    uint vao_id = 0;
    glGenVertexArrays(1, &vao_id);
    return vao_id;
}

void BFGL_Unload_Vertex_Array(uint vao_id) {
    glDeleteVertexArrays(1, &vao_id);
}

// TODO: Usage
uint BFGL_Load_Vertex_Buffer(const void* buffer, int size, bool dynamic) {
    uint id = 0;

    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(
        GL_ARRAY_BUFFER, size, buffer, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
    );

    return id;
}

// TODO: Usage
void BFGL_Set_Vertex_Attribute(
    uint attribute_index,
    int  instances_of_type_per_vertex,
    int  type,  // (BF_FLOAT, etc.)
    bool normalized,
    int  stride_bytes,
    u64  starting_offset_bytes
) {
    glVertexAttribPointer(
        attribute_index,
        instances_of_type_per_vertex,
        type,
        normalized,
        stride_bytes,
        Ptr_From_Int(starting_offset_bytes)
    );
}

void BFGL_Enable_Vertex_Attribute(uint attribute_index) {
    glEnableVertexAttribArray(attribute_index);
}

void BFGL_Enable_Vertex_Array(uint vao_id) {
    glBindVertexArray(vao_id);
}

void BFGL_Disable_Vertex_Array() {
    glBindVertexArray(0);
}

//----------------------------------------------------------------------------------
// Shaders.
//----------------------------------------------------------------------------------
struct Get_Shader_Info_Log_Result {
    bool  success;
    char* log_text;
};

const char* Get_Shader_Info_Log(GLuint shader_id, Arena& trash_arena) {
    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    GLchar* info_log = nullptr;

    if (log_length) {
        info_log = Allocate_Array(trash_arena, GLchar, log_length);
        glGetShaderInfoLog(shader_id, log_length, nullptr, info_log);
    }

    return info_log;
}

struct BFGL_Create_Shader_Result {
    bool success = {};
    uint program = {};

    struct {
        const char* vertex_log   = {};
        const char* fragment_log = {};
        const char* program_log  = {};
    } logs;
};

BFGL_Create_Shader_Result BFGL_Create_Shader_Program(
    const char* vertex_code,
    const char* fragment_code,
    Arena&      trash_arena
) {
    TEMP_USAGE(trash_arena);

    BFGL_Create_Shader_Result result = {};

    // Vertex.
    auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    defer {
        glDeleteShader(vertex_shader);
    };
    glShaderSource(vertex_shader, 1, scast<const GLchar* const*>(&vertex_code), nullptr);
    glCompileShader(vertex_shader);
    GLint vertex_success = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_success);
    result.logs.vertex_log = Get_Shader_Info_Log(vertex_shader, trash_arena);

    // Fragment.
    auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    defer {
        glDeleteShader(fragment_shader);
    };
    glShaderSource(
        fragment_shader, 1, scast<const GLchar* const*>(&fragment_code), nullptr
    );
    glCompileShader(fragment_shader);
    GLint fragment_success = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_success);
    result.logs.fragment_log = Get_Shader_Info_Log(fragment_shader, trash_arena);

    if (!vertex_success || !fragment_success)
        return result;

    // Program compilation.
    auto program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    // Program's logs.
    if (log_length) {
        auto info_log = Allocate_Array(trash_arena, GLchar, log_length);
        glGetProgramInfoLog(program, log_length, nullptr, info_log);
        result.logs.program_log = info_log;
    }

    GLint program_success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &program_success);

    result.success = program_success;
    if (program_success)
        result.program = program;

    return result;
}

void BFGL_Delete_Shader_Program(uint program) {
    if (program)  // TODO: мб это не нужно
        glDeleteProgram(program);
}

void BFGL_Check_Errors() {
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

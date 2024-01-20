#include <initializer_list>
#include "glm/glm.hpp"
#include "glew.h"
#include "wglew.h"
#include "glm/vec2.hpp"
#include "glm/mat3x3.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"

#include "bf_types.h"

void DrawSprite(
    u16 x0,
    u16 y0,
    u16 x1,
    u16 y1,
    glm::vec2 pos,
    glm::vec2 size,
    float rotation,
    const glm::mat3& projection)
{
    assert(x0 < x1);
    assert(y0 < y1);

    auto model = glm::mat3(1);
    model = glm::translate(model, pos);
    model = glm::rotate(model, rotation);
    model = glm::scale(model, size / 2.0f);

    auto matrix = projection * model;
    // TODO(hulvdan): How bad is it that there are vec3, but not vec2?
    glm::vec3 vertices[] = {{-1, -1, 1}, {-1, 1, 1}, {1, -1, 1}, {1, 1, 1}};
    for (auto& vertex : vertices) {
        vertex = matrix * vertex;
        // Converting from homogeneous to eucledian
        // vertex.x = vertex.x / vertex.z;
        // vertex.y = vertex.y / vertex.z;
    }

    u16 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    for (ptrdiff_t i : {0, 1, 2, 2, 1, 3}) {
        // TODO(hulvdan): How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

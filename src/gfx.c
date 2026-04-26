#include "gfx.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "shader.h"
#include <cglm/cglm.h>

void gfx_packet_static_request(Render_request* r) { // pos, rot and scale will be ignored, this is only cache setup
    VBO vbo;
    VAO vao;
    EBO ebo;

    vao_create(&vao);
    vao_bind(&vao);
    
    vbo_create(&vbo, r->data, r->data_size);
    ebo_create(&ebo, r->triangles, r->tri_count * 3 * sizeof(int));

    vbo_attr(0, 3, 5 * sizeof(float), 0);
    vbo_attr(1, 2, 5 * sizeof(float), 3);

    r->cache.vbo = vbo;
    r->cache.vao = vao;
    r->cache.ebo = ebo;
}

void gfx_render(Render_request *r, Program* active_program) {
    mat4 model_matrix;

    glm_mat4_identity(model_matrix);

    glm_translate(model_matrix, r->pos);
    glm_rotate(model_matrix, r->rot, (vec3){0.0f, 1.0f, 0.0f});
    glm_scale(model_matrix, r->scale);

    program_use(active_program);
    program_set_mat4(active_program, "model", (float*)model_matrix);

    vao_bind(&r->cache.vao);
    glDrawElements(GL_TRIANGLES, r->tri_count * 3, GL_UNSIGNED_INT, NULL);
}

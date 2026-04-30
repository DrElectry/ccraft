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

    vbo_attr(0, 3, 8 * sizeof(float), 0);
    vbo_attr(1, 2, 8 * sizeof(float), 3);
    vbo_attr(2, 3, 8 * sizeof(float), 5);

    r->cache.vbo = vbo;
    r->cache.vao = vao;
    r->cache.ebo = ebo;
}

void gfx_render(Render_request *r, Program* active_program) {
    mat4 model_matrix;

    glm_mat4_identity(model_matrix);

    glm_translate(model_matrix, r->pos);
    
    glm_rotate(model_matrix, r->rot[0], (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(model_matrix, r->rot[1], (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(model_matrix, r->rot[2], (vec3){0.0f, 0.0f, 1.0f});
    
    glm_scale(model_matrix, r->scale);

    program_use(active_program);
    program_set_mat4(active_program, "model", (float*)model_matrix);

    vao_bind(&r->cache.vao);
    glDrawElements(GL_TRIANGLES, r->tri_count * 3, GL_UNSIGNED_INT, NULL);
}

void gfx_draw_fullscreen_quad() {
    static VAO vao;
    static VBO vbo;
    static EBO ebo;
    static int initialized = 0;

    if (!initialized) {
        float vertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f 
        };

        int indices[] = {
            0, 1, 2,
            2, 0, 3
        };

        vao_create(&vao);
        vao_bind(&vao);

        vbo_create(&vbo, vertices, sizeof(vertices));
        ebo_create(&ebo, indices, sizeof(indices));

        vbo_attr(0, 2, 4 * sizeof(float), 0);
        vbo_attr(1, 2, 4 * sizeof(float), 2);

        initialized = 1;
    }

    vao_bind(&vao);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL); // i like how render() function in vao.h becomes useless
}
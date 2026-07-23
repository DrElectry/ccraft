#include "core/gfx.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "core/skin.h"
#include "gl/shader.h"
#include "utils/file.h"
#include "core/main.h"
#include <cglm/cglm.h>

void gfx_packet_static_request(Render_request* r) { // pos, rot and scale will be ignored, this is only cache setup
    VBO vbo;
    VAO vao;
    EBO ebo;

    vao_create(&vao);
    vao_bind(&vao);
    
    vbo_create(&vbo, r->data, r->data_size);
    ebo_create(&ebo, r->triangles, r->tri_count * 3 * sizeof(int));

    // pos (3) uv (2) norm (3)
    vbo_attr(0, 3, 8 * sizeof(float), 0);
    vbo_attr(1, 2, 8 * sizeof(float), 3);
    vbo_attr(2, 3, 8 * sizeof(float), 5);


    r->cache.vbo = vbo;
    r->cache.vao = vao;
    r->cache.ebo = ebo;
}

void gfx_chunk_packet_static_request(Render_request* r) {
    VBO vbo;
    VAO vao;
    EBO ebo;

    vao_create(&vao);
    vao_bind(&vao);
    
    vbo_create(&vbo, r->data, r->data_size);
    ebo_create(&ebo, r->triangles, r->tri_count * 3 * sizeof(int));

    // vertex format (tangent space packed by chunk builder):
    // pos (3) uv (2) norm (3) light(1) tangent(3) bitangent(3) => 15 floats
    vbo_attr(0, 3, 15 * sizeof(float), 0);
    vbo_attr(1, 2, 15 * sizeof(float), 3);
    vbo_attr(2, 3, 15 * sizeof(float), 5);
    vbo_attr(3, 1, 15 * sizeof(float), 8);
    vbo_attr(4, 3, 15 * sizeof(float), 9);
    vbo_attr(5, 3, 15 * sizeof(float), 12);

    r->cache.vbo = vbo;
    r->cache.vao = vao;
    r->cache.ebo = ebo;
}

void gfx_program_create(Program* a, char* vsrc, char* fsrc) {
    Shader vertex, fragment;

    vertex.type = GL_VERTEX_SHADER;
    fragment.type = GL_FRAGMENT_SHADER;

    File vsr, fsr;

    vsr = file_open(vsrc);
    fsr = file_open(fsrc);

    shader_create(&vertex, vsr.data);
    shader_create(&fragment, fsr.data);

    program_create(a, &vertex, &fragment);
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

void gfx_skinned_render(Skinned_render_request* r, Program* active_program) {
    if (!r) return;
    if (!r->skinned || !r->look.enabled) return;

    Skinned* sk = r->skinned;
    sk->gpu.skeleton = r->skeleton;
    sk->gpu.anim = r->anim;

    program_use(active_program);
    skinned_render(sk, active_program, 0.0f, &r->look);
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

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL); // i like how render() function in vao.h IS NOW useless
}

void gfx_canvas_packet_static_request(Canvas_Render_Request* r) {
    if (r->initialized) return;
    
    VBO vbo;
    VAO vao;
    EBO ebo;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    vao_create(&vao);
    vao_bind(&vao);
    
    vbo_create(&vbo, vertices, sizeof(vertices));
    ebo_create(&ebo, indices, sizeof(indices));

    // pos (2) uv (2)
    vbo_attr(0, 2, 4 * sizeof(float), 0);
    vbo_attr(1, 2, 4 * sizeof(float), 2);

    r->cache.vbo = vbo;
    r->cache.vao = vao;
    r->cache.ebo = ebo;
    r->initialized = 1;
}

void gfx_set_screen_projection(Program* program) {
    mat4 projection;
    glm_mat4_identity(projection);
    
    glm_ortho(0.0f, 1280.0f, 720.0f, 0.0f, -1.0f, 1.0f, projection);
    
    program_set_mat4(program, "projection", (float*)projection);
}

void gfx_canvas_render(Canvas_Render_Request* r, Program* active_program) {
    if (!r->initialized) {
        gfx_canvas_packet_static_request(r);
    }
    
    mat4 model_matrix;
    glm_mat4_identity(model_matrix);
    
    glm_translate(model_matrix, (vec3){r->pos[0], r->pos[1], 0.0f});
    glm_rotate(model_matrix, r->rotation, (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model_matrix, (vec3){r->scale[0], r->scale[1], 1.0f});
    
    program_use(active_program);
    program_set_mat4(active_program, "model", (float*)model_matrix);
    program_set_float(active_program, "alpha", r->alpha);
    
    vao_bind(&r->cache.vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
}

void gfx_canvas_render_batch(Canvas_Render_Request* requests, int count, Program* active_program) {
    if (!requests || count <= 0) return;
    
    program_use(active_program);
    
    for (int i = 0; i < count; i++) {
        Canvas_Render_Request* r = &requests[i];
        
        if (!r->initialized) {
            gfx_canvas_packet_static_request(r);
        }
        
        mat4 model_matrix;
        glm_mat4_identity(model_matrix);
        
        glm_translate(model_matrix, (vec3){r->pos[0], r->pos[1], 0.0f});
        glm_rotate(model_matrix, r->rotation, (vec3){0.0f, 0.0f, 1.0f});
        glm_scale(model_matrix, (vec3){r->scale[0], r->scale[1], 1.0f});
        
        program_set_mat4(active_program, "model", (float*)model_matrix);
        program_set_float(active_program, "alpha", r->alpha);
        
        vao_bind(&r->cache.vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
    }
}
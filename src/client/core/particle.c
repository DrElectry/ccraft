#include "particle.h"
#include <string.h>

static int find_dead_particle(const ParticleSystem* ps) {
    for (int i = 0; i < MAX_PARTICLES; ++i)
        if (ps->data[i].lifetime <= 0.0f)
            return i;
    return -1;
}

void particle_manager_init(ParticleManager* pm) {
    memset(pm, 0, sizeof(ParticleManager));
}

ParticleSystem* particle_system_create(ParticleManager* pm, vec3 gravity, vec3 start_vel, float lifetime, Render_request* model) {
    if (pm->count >= MAX_PARTICLE_SYSTEMS) return NULL;

    ParticleSystem* ps = &pm->systems[pm->count];
    memset(ps, 0, sizeof(ParticleSystem));
    glm_vec3_copy(gravity, ps->gravity);
    glm_vec3_copy(start_vel, ps->start_vel);
    ps->lifetime = lifetime;
    ps->model = model;
    ps->active = 1;

    glm_vec3_zero(ps->vel_spread);
    glm_vec3_zero(ps->pos_spread);
    ps->lifetime_spread = 0.0f;

    ps->emit_rate = 0.0f;
    ps->emit_timer = 0.0f;
    glm_vec3_zero(ps->emit_pos);

    pm->count++;
    return ps;
}

void particle_system_set_vel_spread(ParticleSystem* ps, vec3 spread) {
    glm_vec3_copy(spread, ps->vel_spread);
}

void particle_system_set_pos_spread(ParticleSystem* ps, vec3 spread) {
    glm_vec3_copy(spread, ps->pos_spread);
}

void particle_system_set_lifetime_spread(ParticleSystem* ps, float spread) {
    ps->lifetime_spread = spread;
}

int particle_emit(ParticleSystem* ps, vec3 position) {
    int idx = find_dead_particle(ps);
    if (idx < 0) return 0;

    Particle* p = &ps->data[idx];
    vec3 offset;

    glm_vec3_copy(position, p->pos);
    if (glm_vec3_norm(ps->pos_spread) > 0.0f) {
        offset[0] = (rng_float() - 0.5f) * 2.0f * ps->pos_spread[0];
        offset[1] = (rng_float() - 0.5f) * 2.0f * ps->pos_spread[1];
        offset[2] = (rng_float() - 0.5f) * 2.0f * ps->pos_spread[2];
        glm_vec3_add(p->pos, offset, p->pos);
    }

    glm_vec3_copy(ps->start_vel, p->vel);
    if (glm_vec3_norm(ps->vel_spread) > 0.0f) {
        offset[0] = (rng_float() - 0.5f) * 2.0f * ps->vel_spread[0];
        offset[1] = (rng_float() - 0.5f) * 2.0f * ps->vel_spread[1];
        offset[2] = (rng_float() - 0.5f) * 2.0f * ps->vel_spread[2];
        glm_vec3_add(p->vel, offset, p->vel);
    }

    p->lifetime = ps->lifetime;
    if (ps->lifetime_spread > 0.0f) {
        float delta = (rng_float() - 0.5f) * 2.0f * ps->lifetime_spread;
        p->lifetime += delta;
        if (p->lifetime < 0.0f) p->lifetime = 0.0f;
    }

    glm_vec3_zero(p->rot);
    return 1;
}

void particle_system_set_emission(ParticleSystem* ps, float rate, vec3 position) {
    ps->emit_rate = (rate > 0.0f) ? rate : 0.0f;
    glm_vec3_copy(position, ps->emit_pos);
    ps->emit_timer = 0.0f;
}

void particle_manager_update(ParticleManager* pm, float dt) {
    for (int s = 0; s < pm->count; ++s) {
        ParticleSystem* ps = &pm->systems[s];
        if (!ps->active) continue;

        ps->alive_count = 0;
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            Particle* p = &ps->data[i];
            if (p->lifetime <= 0.0f) continue;

            glm_vec3_muladds(ps->gravity, dt, p->vel);
            glm_vec3_muladds(p->vel, dt, p->pos);
            p->lifetime -= dt;
            ps->alive_count++;
        }

        if (ps->emit_rate > 0.0f) {
            float interval = 1.0f / ps->emit_rate;
            ps->emit_timer += dt;
            while (ps->emit_timer >= interval) {
                if (!particle_emit(ps, ps->emit_pos))
                    break;
                ps->emit_timer -= interval;
            }
        }
    }
}

void particle_manager_render(ParticleManager* pm, Program* active_program) {
    for (int s = 0; s < pm->count; ++s) {
        ParticleSystem* ps = &pm->systems[s];
        if (!ps->active || ps->alive_count == 0) continue;

        for (int i = 0; i < MAX_PARTICLES; ++i) {
            Particle* p = &ps->data[i];
            if (p->lifetime <= 0.0f) continue;

            glm_vec3_copy(p->pos, ps->model->pos);
            gfx_render(ps->model, active_program);
        }
    }
}
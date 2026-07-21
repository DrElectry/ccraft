#include "particle.h"
#include <string.h>
#include <math.h>

static int is_solid(uint16_t b) {
    return b != 0 && lookup_ignorecollision[b] == 0;
}

static int find_dead_particle(const ParticleSystem* ps) {
    for (int i = 0; i < MAX_PARTICLES; ++i)
        if (ps->data[i].lifetime <= 0.0f)
            return i;
    return -1;
}

static void sync_particle_aabb(Particle* p) {
    if (!p->has_collision) return;
    p->aabb.x = p->pos[0] - p->aabb.w * 0.5f;
    p->aabb.y = p->pos[1];
    p->aabb.z = p->pos[2] - p->aabb.d * 0.5f;
}

static int particle_aabb_collides(World* world, Particle* p) {
    if (!p->has_collision) return 0;
    
    int minX = (int)floorf(p->aabb.x + EPSILON);
    int maxX = (int)floorf(p->aabb.x + p->aabb.w - EPSILON);
    int minY = (int)floorf(p->aabb.y + EPSILON);
    int maxY = (int)floorf(p->aabb.y + p->aabb.h - EPSILON);
    int minZ = (int)floorf(p->aabb.z + EPSILON);
    int maxZ = (int)floorf(p->aabb.z + p->aabb.d - EPSILON);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                vec3 pnt = { (float)x, (float)y, (float)z };
                if (is_solid(world_get_block_at(world, pnt)))
                    return 1;
            }
        }
    }
    return 0;
}

static int particle_on_ground(World* world, Particle* p) {
    if (!p->has_collision) return 0;
    float original_y = p->pos[1];
    p->pos[1] -= EPSILON;
    sync_particle_aabb(p);
    int grounded = particle_aabb_collides(world, p);
    p->pos[1] = original_y;
    sync_particle_aabb(p);
    return grounded;
}

static void move_particle_axis(World* world, Particle* p, ParticleSystem* ps, int axis, float delta) {
    if (delta == 0.0f || !p->has_collision) return;
    
    p->pos[axis] += delta;
    sync_particle_aabb(p);
    if (particle_aabb_collides(world, p)) {
        p->pos[axis] -= delta;
        p->vel[axis] *= -ps->bounce_factor;
        sync_particle_aabb(p);
        if (delta > 0.0f)
            p->pos[axis] -= EPSILON;
        else if (delta < 0.0f)
            p->pos[axis] += EPSILON;
        sync_particle_aabb(p);
        
        if (axis == 1 && delta < 0.0f) {
            p->on_ground = 1;
        }
    }
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
    ps->has_collision = 0;
    ps->bounce_factor = 0.5f;
    ps->ground_drag = 0.0f;
    ps->ground_friction = 0.0f;

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

void particle_system_set_collision(ParticleSystem* ps, vec3 aabb_size, float bounce_factor, float ground_drag, float ground_friction) {
    glm_vec3_copy(aabb_size, ps->aabb_size);
    ps->has_collision = 1;
    ps->bounce_factor = bounce_factor;
    ps->ground_drag = ground_drag;
    ps->ground_friction = ground_friction;
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

    p->has_collision = ps->has_collision;
    p->on_ground = 0;
    if (p->has_collision) {
        p->aabb.w = ps->aabb_size[0];
        p->aabb.h = ps->aabb_size[1];
        p->aabb.d = ps->aabb_size[2];
        sync_particle_aabb(p);
    }

    glm_vec3_zero(p->rot);
    return 1;
}

void particle_system_set_emission(ParticleSystem* ps, float rate, vec3 position) {
    ps->emit_rate = (rate > 0.0f) ? rate : 0.0f;
    glm_vec3_copy(position, ps->emit_pos);
    ps->emit_timer = 0.0f;
}

void particle_manager_update(ParticleManager* pm, World* world, float dt) {
    for (int s = 0; s < pm->count; ++s) {
        ParticleSystem* ps = &pm->systems[s];
        if (!ps->active) continue;

        ps->alive_count = 0;
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            Particle* p = &ps->data[i];
            if (p->lifetime <= 0.0f) continue;

            if (p->has_collision && world) {
                p->on_ground = particle_on_ground(world, p);
                
                if (p->on_ground) {
                    p->vel[0] *= (1.0f - ps->ground_drag * dt);
                    p->vel[2] *= (1.0f - ps->ground_drag * dt);
                    
                    float speed = sqrtf(p->vel[0] * p->vel[0] + p->vel[2] * p->vel[2]);
                    if (speed > 0.0f) {
                        float friction_force = ps->ground_friction * fminf(1.0f, fabsf(p->vel[1]) * 0.1f);
                        float friction_factor = 1.0f - friction_force * dt;
                        if (friction_factor < 0.0f) friction_factor = 0.0f;
                        p->vel[0] *= friction_factor;
                        p->vel[2] *= friction_factor;
                    }
                    
                    p->vel[1] = 0.0f;
                } else {
                    glm_vec3_muladds(ps->gravity, dt, p->vel);
                }
            } else {
                glm_vec3_muladds(ps->gravity, dt, p->vel);
            }
            
            vec3 delta;
            glm_vec3_scale(p->vel, dt, delta);
            
            if (p->has_collision && world) {
                move_particle_axis(world, p, ps, 1, delta[1]);
                move_particle_axis(world, p, ps, 0, delta[0]);
                move_particle_axis(world, p, ps, 2, delta[2]);
            } else {
                glm_vec3_add(p->pos, delta, p->pos);
            }

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
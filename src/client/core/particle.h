#ifndef PARTICLE_H
#define PARTICLE_H

#include <cglm/cglm.h>
#include "core/gfx.h"
#include "utils/rand.h"

#define MAX_PARTICLES 4096
#define MAX_PARTICLE_SYSTEMS 64

typedef struct {
    vec3 pos, rot, vel;
    float lifetime;
} Particle;

typedef struct {
    Particle data[MAX_PARTICLES];
    float lifetime;
    vec3 gravity, start_vel;
    vec3 vel_spread;
    vec3 pos_spread;
    float lifetime_spread;
    Render_request* model;
    int active;
    int alive_count;
    float emit_rate;
    float emit_timer;
    vec3 emit_pos;
} ParticleSystem;

typedef struct {
    ParticleSystem systems[MAX_PARTICLE_SYSTEMS];
    int count;
} ParticleManager;

void particle_manager_init(ParticleManager* pm);
ParticleSystem* particle_system_create(ParticleManager* pm, vec3 gravity, vec3 start_vel, float lifetime, Render_request* model);
int particle_emit(ParticleSystem* ps, vec3 position);
void particle_system_set_emission(ParticleSystem* ps, float rate, vec3 position);
void particle_system_set_vel_spread(ParticleSystem* ps, vec3 spread);
void particle_system_set_pos_spread(ParticleSystem* ps, vec3 spread);
void particle_system_set_lifetime_spread(ParticleSystem* ps, float spread);
void particle_manager_update(ParticleManager* pm, float dt);
void particle_manager_render(ParticleManager* pm, Program* active_program);

#endif
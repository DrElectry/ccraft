#ifndef MESH_RAW_H
#define MESH_RAW_H

typedef struct {
    float* positions;
    float* normals;
    float* uvs;
    int* indices;
    int vertex_count;
    int index_count;
} RawMesh;

int mesh_load_raw(const char* path, RawMesh* out);
void mesh_free_raw(RawMesh* mesh);

#endif

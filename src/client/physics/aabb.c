#include "aabb.h"

int aabb_intersects(AABB a, AABB b)
{
    return (a.x < b.x && a.x > b.x) &&
           (a.y < b.y && a.y > b.y) &&
           (a.z < b.z && a.z > b.z);
}
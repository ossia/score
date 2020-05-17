///////////////////////////////////////////////////////////////////////////////
// Icosphere.cpp
// =============
// Polyhedron subdividing icosahedron (20 tris) by N-times iteration
// The icosphere with N=1 (default) has 80 triangles by subdividing a triangle
// of icosahedron into 4 triangles. If N=0, it is identical to icosahedron.
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2018-07-23
// UPDATED: 2019-12-18
///////////////////////////////////////////////////////////////////////////////


#include <cmath>
#include "Icosphere.h"



// constants //////////////////////////////////////////////////////////////////




///////////////////////////////////////////////////////////////////////////////
// ctor
///////////////////////////////////////////////////////////////////////////////
Icosphere::Icosphere(float radius, int sub, bool smooth) : radius(radius), subdivision(sub), smooth(smooth), interleavedStride(32)
{
    if(smooth)
        buildVerticesSmooth();
    else
        buildVerticesFlat();
}



///////////////////////////////////////////////////////////////////////////////
// setters
///////////////////////////////////////////////////////////////////////////////
void Icosphere::setRadius(float radius)
{
    this->radius = radius;
    updateRadius(); // update vertex positions only
}

void Icosphere::setSubdivision(int iteration)
{
    this->subdivision = iteration;
    // rebuild vertices
    if(smooth)
        buildVerticesSmooth();
    else
        buildVerticesFlat();
}

void Icosphere::setSmooth(bool smooth)
{
    if(this->smooth == smooth)
        return;

    this->smooth = smooth;
    if(smooth)
        buildVerticesSmooth();
    else
        buildVerticesFlat();
}



///////////////////////////////////////////////////////////////////////////////
// print itself
///////////////////////////////////////////////////////////////////////////////
void Icosphere::printSelf() const
{

    std::cout << "===== Icosphere =====\n"
              << "        Radius: " << radius << "\n"
              << "   Subdivision: " << subdivision << "\n"
              << "    Smoothness: " << (smooth ? "true" : "false") << "\n"
              << "Triangle Count: " << getTriangleCount() << "\n"
              << "   Index Count: " << getIndexCount() << "\n"
              << "  Vertex Count: " << getVertexCount() << "\n"
              << "  Normal Count: " << getNormalCount() << "\n"
              << "TexCoord Count: " << getTexCoordCount() << std::endl;
}






///////////////////////////////////////////////////////////////////////////////
// update vertex positions only
///////////////////////////////////////////////////////////////////////////////
void Icosphere::updateRadius()
{
    float scale = computeScaleForLength(&vertices[0], radius);

    std::size_t i, j;
    std::size_t count = vertices.size();
    for(i = 0, j = 0; i < count; i += 3, j += 8)
    {
        vertices[i]   *= scale;
        vertices[i+1] *= scale;
        vertices[i+2] *= scale;

        // for interleaved array
        interleavedVertices[j]   *= scale;
        interleavedVertices[j+1] *= scale;
        interleavedVertices[j+2] *= scale;
    }
}



///////////////////////////////////////////////////////////////////////////////
// compute 12 vertices of icosahedron using spherical coordinates
// The north pole is at (0, 0, r) and the south pole is at (0,0,-r).
// 5 vertices are placed by rotating 72 deg at elevation 26.57 deg (=atan(1/2))
// 5 vertices are placed by rotating 72 deg at elevation -26.57 deg
///////////////////////////////////////////////////////////////////////////////
std::vector<float> Icosphere::computeIcosahedronVertices()
{
    const float PI = acos(-1);
    const float H_ANGLE = PI / 180 * 72;    // 72 degree = 360 / 5
    const float V_ANGLE = atanf(1.0f / 2);  // elevation = 26.565 degree

    std::vector<float> vertices(12 * 3);    // 12 vertices
    int i1, i2;                             // indices
    float z, xy;                            // coords
    float hAngle1 = -PI / 2 - H_ANGLE / 2;  // start from -126 deg at 2nd row
    float hAngle2 = -PI / 2;                // start from -90 deg at 3rd row

    // the first top vertex (0, 0, r)
    vertices[0] = 0;
    vertices[1] = 0;
    vertices[2] = radius;

    // 10 vertices at 2nd and 3rd rows
    for(int i = 1; i <= 5; ++i)
    {
        i1 = i * 3;         // for 2nd row
        i2 = (i + 5) * 3;   // for 3rd row

        z = radius * sinf(V_ANGLE);             // elevaton
        xy = radius * cosf(V_ANGLE);

        vertices[i1] = xy * cosf(hAngle1);      // x
        vertices[i2] = xy * cosf(hAngle2);
        vertices[i1 + 1] = xy * sinf(hAngle1);  // x
        vertices[i2 + 1] = xy * sinf(hAngle2);
        vertices[i1 + 2] = z;                   // z
        vertices[i2 + 2] = -z;

        // next horizontal angles
        hAngle1 += H_ANGLE;
        hAngle2 += H_ANGLE;
    }

    // the last bottom vertex (0, 0, -r)
    i1 = 11 * 3;
    vertices[i1] = 0;
    vertices[i1 + 1] = 0;
    vertices[i1 + 2] = -radius;

    return vertices;
}



///////////////////////////////////////////////////////////////////////////////
// generate vertices with flat shading
// each triangle is independent (no shared vertices)
///////////////////////////////////////////////////////////////////////////////
void Icosphere::buildVerticesFlat()
{
    //const float S_STEP = 1 / 11.0f;         // horizontal texture step
    //const float T_STEP = 1 / 3.0f;          // vertical texture step
    const float S_STEP = 186 / 2048.0f;     // horizontal texture step
    const float T_STEP = 322 / 1024.0f;     // vertical texture step

    // compute 12 vertices of icosahedron
    std::vector<float> tmpVertices = computeIcosahedronVertices();

    // clear memory of prev arrays
    std::vector<float>().swap(vertices);
    std::vector<float>().swap(normals);
    std::vector<float>().swap(texCoords);
    std::vector<unsigned int>().swap(indices);
    std::vector<unsigned int>().swap(lineIndices);

    const float *v0, *v1, *v2, *v3, *v4, *v11;          // vertex positions
    float n[3];                                         // face normal
    float t0[2], t1[2], t2[2], t3[2], t4[2], t11[2];    // texCoords
    unsigned int index = 0;

    // compute and add 20 tiangles of icosahedron first
    v0 = &tmpVertices[0];       // 1st vertex
    v11 = &tmpVertices[11 * 3]; // 12th vertex
    for(int i = 1; i <= 5; ++i)
    {
        // 4 vertices in the 2nd row
        v1 = &tmpVertices[i * 3];
        if(i < 5)
            v2 = &tmpVertices[(i + 1) * 3];
        else
            v2 = &tmpVertices[3];

        v3 = &tmpVertices[(i + 5) * 3];
        if((i + 5) < 10)
            v4 = &tmpVertices[(i + 6) * 3];
        else
            v4 = &tmpVertices[6 * 3];

        // texture coords
        t0[0] = (2 * i - 1) * S_STEP;   t0[1] = 0;
        t1[0] = (2 * i - 2) * S_STEP;   t1[1] = T_STEP;
        t2[0] = (2 * i - 0) * S_STEP;   t2[1] = T_STEP;
        t3[0] = (2 * i - 1) * S_STEP;   t3[1] = T_STEP * 2;
        t4[0] = (2 * i + 1) * S_STEP;   t4[1] = T_STEP * 2;
        t11[0]= 2 * i * S_STEP;         t11[1]= T_STEP * 3;

        // add a triangle in 1st row
        Icosphere::computeFaceNormal(v0, v1, v2, n);
        addVertices(v0, v1, v2);
        addNormals(n, n, n);
        addTexCoords(t0, t1, t2);
        addIndices(index, index+1, index+2);

        // add 2 triangles in 2nd row
        Icosphere::computeFaceNormal(v1, v3, v2, n);
        addVertices(v1, v3, v2);
        addNormals(n, n, n);
        addTexCoords(t1, t3, t2);
        addIndices(index+3, index+4, index+5);

        Icosphere::computeFaceNormal(v2, v3, v4, n);
        addVertices(v2, v3, v4);
        addNormals(n, n, n);
        addTexCoords(t2, t3, t4);
        addIndices(index+6, index+7, index+8);

        // add a triangle in 3rd row
        Icosphere::computeFaceNormal(v3, v11, v4, n);
        addVertices(v3, v11, v4);
        addNormals(n, n, n);
        addTexCoords(t3, t11, t4);
        addIndices(index+9, index+10, index+11);

        // add 6 edge lines per iteration
        //  i
        //  /   /   /   /   /       : (i, i+1)                              //
        // /__ /__ /__ /__ /__                                              //
        // \  /\  /\  /\  /\  /     : (i+3, i+4), (i+3, i+5), (i+4, i+5)    //
        //  \/__\/__\/__\/__\/__                                            //
        //   \   \   \   \   \      : (i+9,i+10), (i+9, i+11)               //
        //    \   \   \   \   \                                             //
        lineIndices.push_back(index);       // (i, i+1)
        lineIndices.push_back(index+1);       // (i, i+1)
        lineIndices.push_back(index+3);     // (i+3, i+4)
        lineIndices.push_back(index+4);
        lineIndices.push_back(index+3);     // (i+3, i+5)
        lineIndices.push_back(index+5);
        lineIndices.push_back(index+4);     // (i+4, i+5)
        lineIndices.push_back(index+5);
        lineIndices.push_back(index+9);     // (i+9, i+10)
        lineIndices.push_back(index+10);
        lineIndices.push_back(index+9);     // (i+9, i+11)
        lineIndices.push_back(index+11);

        // next index
        index += 12;
    }

    // subdivide icosahedron
    subdivideVerticesFlat();

    // generate interleaved vertex array as well
    buildInterleavedVertices();
}



///////////////////////////////////////////////////////////////////////////////
// generate vertices with smooth shading
// NOTE: The north and south pole vertices cannot be shared for smooth shading
// because they have same position and normal, but different texcoords per face
// And, the first vertex on each row is also not shared.
///////////////////////////////////////////////////////////////////////////////
void Icosphere::buildVerticesSmooth()
{
    //const float S_STEP = 1 / 11.0f;         // horizontal texture step
    //const float T_STEP = 1 / 3.0f;          // vertical texture step
    const float S_STEP = 186 / 2048.0f;     // horizontal texture step
    const float T_STEP = 322 / 1024.0f;     // vertical texture step

    // compute 12 vertices of icosahedron
    // NOTE: v0 (top), v11(bottom), v1, v6(first vert on each row) cannot be
    // shared for smooth shading (they have different texcoords)
    std::vector<float> tmpVertices = computeIcosahedronVertices();

    // clear memory of prev arrays
    std::vector<float>().swap(vertices);
    std::vector<float>().swap(normals);
    std::vector<float>().swap(texCoords);
    std::vector<unsigned int>().swap(indices);
    std::vector<unsigned int>().swap(lineIndices);
    std::map<std::pair<float, float>, unsigned int>().swap(sharedIndices);

    float v[3];                             // vertex
    float n[3];                             // normal
    float scale;                            // scale factor for normalization

    // smooth icosahedron has 14 non-shared (0 to 13) and
    // 8 shared vertices (14 to 21) (total 22 vertices)
    //  00  01  02  03  04          //
    //  /\  /\  /\  /\  /\          //
    // /  \/  \/  \/  \/  \         //
    //10--14--15--16--17--11        //
    // \  /\  /\  /\  /\  /\        //
    //  \/  \/  \/  \/  \/  \       //
    //  12--18--19--20--21--13      //
    //   \  /\  /\  /\  /\  /       //
    //    \/  \/  \/  \/  \/        //
    //    05  06  07  08  09        //
    // add 14 non-shared vertices first (index from 0 to 13)
    addVertex(tmpVertices[0], tmpVertices[1], tmpVertices[2]);      // v0 (top)
    addNormal(0, 0, 1);
    addTexCoord(S_STEP, 0);

    addVertex(tmpVertices[0], tmpVertices[1], tmpVertices[2]);      // v1
    addNormal(0, 0, 1);
    addTexCoord(S_STEP * 3, 0);

    addVertex(tmpVertices[0], tmpVertices[1], tmpVertices[2]);      // v2
    addNormal(0, 0, 1);
    addTexCoord(S_STEP * 5, 0);

    addVertex(tmpVertices[0], tmpVertices[1], tmpVertices[2]);      // v3
    addNormal(0, 0, 1);
    addTexCoord(S_STEP * 7, 0);

    addVertex(tmpVertices[0], tmpVertices[1], tmpVertices[2]);      // v4
    addNormal(0, 0, 1);
    addTexCoord(S_STEP * 9, 0);

    addVertex(tmpVertices[33], tmpVertices[34], tmpVertices[35]);   // v5 (bottom)
    addNormal(0, 0, -1);
    addTexCoord(S_STEP * 2, T_STEP * 3);

    addVertex(tmpVertices[33], tmpVertices[34], tmpVertices[35]);   // v6
    addNormal(0, 0, -1);
    addTexCoord(S_STEP * 4, T_STEP * 3);

    addVertex(tmpVertices[33], tmpVertices[34], tmpVertices[35]);   // v7
    addNormal(0, 0, -1);
    addTexCoord(S_STEP * 6, T_STEP * 3);

    addVertex(tmpVertices[33], tmpVertices[34], tmpVertices[35]);   // v8
    addNormal(0, 0, -1);
    addTexCoord(S_STEP * 8, T_STEP * 3);

    addVertex(tmpVertices[33], tmpVertices[34], tmpVertices[35]);   // v9
    addNormal(0, 0, -1);
    addTexCoord(S_STEP * 10, T_STEP * 3);

    v[0] = tmpVertices[3];  v[1] = tmpVertices[4];  v[2] = tmpVertices[5];  // v10 (left)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(0, T_STEP);

    addVertex(v[0], v[1], v[2]);                                            // v11 (right)
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 10, T_STEP);

    v[0] = tmpVertices[18]; v[1] = tmpVertices[19]; v[2] = tmpVertices[20]; // v12 (left)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP, T_STEP * 2);

    addVertex(v[0], v[1], v[2]);                                            // v13 (right)
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 11, T_STEP * 2);

    // add 8 shared vertices to array (index from 14 to 21)
    v[0] = tmpVertices[6];  v[1] = tmpVertices[7];  v[2] = tmpVertices[8];  // v14 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 2, T_STEP);
    sharedIndices[std::make_pair(S_STEP * 2, T_STEP)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[9];  v[1] = tmpVertices[10]; v[2] = tmpVertices[11]; // v15 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 4, T_STEP);
    sharedIndices[std::make_pair(S_STEP * 4, T_STEP)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[12]; v[1] = tmpVertices[13]; v[2] = tmpVertices[14]; // v16 (shared)
    scale = Icosphere::computeScaleForLength(v, 1);
    n[0] = v[0] * scale;    n[1] = v[1] * scale;    n[2] = v[2] * scale;
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 6, T_STEP);
    sharedIndices[std::make_pair(S_STEP * 6, T_STEP)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[15]; v[1] = tmpVertices[16]; v[2] = tmpVertices[17]; // v17 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 8, T_STEP);
    sharedIndices[std::make_pair(S_STEP * 8, T_STEP)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[21]; v[1] = tmpVertices[22]; v[2] = tmpVertices[23]; // v18 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 3, T_STEP * 2);
    sharedIndices[std::make_pair(S_STEP * 3, T_STEP * 2)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[24]; v[1] = tmpVertices[25]; v[2] = tmpVertices[26]; // v19 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 5, T_STEP * 2);
    sharedIndices[std::make_pair(S_STEP * 5, T_STEP * 2)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[27]; v[1] = tmpVertices[28]; v[2] = tmpVertices[29]; // v20 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 7, T_STEP * 2);
    sharedIndices[std::make_pair(S_STEP * 7, T_STEP * 2)] = texCoords.size() / 2 - 1;

    v[0] = tmpVertices[30]; v[1] = tmpVertices[31]; v[2] = tmpVertices[32]; // v21 (shared)
    Icosphere::computeVertexNormal(v, n);
    addVertex(v[0], v[1], v[2]);
    addNormal(n[0], n[1], n[2]);
    addTexCoord(S_STEP * 9, T_STEP * 2);
    sharedIndices[std::make_pair(S_STEP * 9, T_STEP * 2)] = texCoords.size() / 2 - 1;

    // build index list for icosahedron (20 triangles)
    addIndices( 0, 10, 14);      // 1st row (5 tris)
    addIndices( 1, 14, 15);
    addIndices( 2, 15, 16);
    addIndices( 3, 16, 17);
    addIndices( 4, 17, 11);
    addIndices(10, 12, 14);      // 2nd row (10 tris)
    addIndices(12, 18, 14);
    addIndices(14, 18, 15);
    addIndices(18, 19, 15);
    addIndices(15, 19, 16);
    addIndices(19, 20, 16);
    addIndices(16, 20, 17);
    addIndices(20, 21, 17);
    addIndices(17, 21, 11);
    addIndices(21, 13, 11);
    addIndices( 5, 18, 12);      // 3rd row (5 tris)
    addIndices( 6, 19, 18);
    addIndices( 7, 20, 19);
    addIndices( 8, 21, 20);
    addIndices( 9, 13, 21);

    // add edge lines of icosahedron
    lineIndices.push_back(0);   lineIndices.push_back(10);       // 00 - 10
    lineIndices.push_back(1);   lineIndices.push_back(14);       // 01 - 14
    lineIndices.push_back(2);   lineIndices.push_back(15);       // 02 - 15
    lineIndices.push_back(3);   lineIndices.push_back(16);       // 03 - 16
    lineIndices.push_back(4);   lineIndices.push_back(17);       // 04 - 17
    lineIndices.push_back(10);  lineIndices.push_back(14);       // 10 - 14
    lineIndices.push_back(14);  lineIndices.push_back(15);       // 14 - 15
    lineIndices.push_back(15);  lineIndices.push_back(16);       // 15 - 16
    lineIndices.push_back(16);  lineIndices.push_back(17);       // 10 - 14
    lineIndices.push_back(17);  lineIndices.push_back(11);       // 17 - 11
    lineIndices.push_back(10);  lineIndices.push_back(12);       // 10 - 12
    lineIndices.push_back(12);  lineIndices.push_back(14);       // 12 - 14
    lineIndices.push_back(14);  lineIndices.push_back(18);       // 14 - 18
    lineIndices.push_back(18);  lineIndices.push_back(15);       // 18 - 15
    lineIndices.push_back(15);  lineIndices.push_back(19);       // 15 - 19
    lineIndices.push_back(19);  lineIndices.push_back(16);       // 19 - 16
    lineIndices.push_back(16);  lineIndices.push_back(20);       // 16 - 20
    lineIndices.push_back(20);  lineIndices.push_back(17);       // 20 - 17
    lineIndices.push_back(17);  lineIndices.push_back(21);       // 17 - 21
    lineIndices.push_back(21);  lineIndices.push_back(11);       // 21 - 11
    lineIndices.push_back(12);  lineIndices.push_back(18);       // 12 - 18
    lineIndices.push_back(18);  lineIndices.push_back(19);       // 18 - 19
    lineIndices.push_back(19);  lineIndices.push_back(20);       // 19 - 20
    lineIndices.push_back(20);  lineIndices.push_back(21);       // 20 - 21
    lineIndices.push_back(21);  lineIndices.push_back(13);       // 21 - 13
    lineIndices.push_back(5);   lineIndices.push_back(12);       // 05 - 12
    lineIndices.push_back(6);   lineIndices.push_back(18);       // 06 - 18
    lineIndices.push_back(7);   lineIndices.push_back(19);       // 07 - 19
    lineIndices.push_back(8);   lineIndices.push_back(20);       // 08 - 20
    lineIndices.push_back(9);   lineIndices.push_back(21);       // 09 - 21

    // subdivide icosahedron
    subdivideVerticesSmooth();

    // generate interleaved vertex array as well
    buildInterleavedVertices();
}



///////////////////////////////////////////////////////////////////////////////
// divide a trinage into 4 sub triangles and repeat N times
// If subdivision=0, do nothing.
///////////////////////////////////////////////////////////////////////////////
void Icosphere::subdivideVerticesFlat()
{
    std::vector<float> tmpVertices;
    std::vector<float> tmpTexCoords;
    std::vector<unsigned int> tmpIndices;
    int indexCount;
    const float *v1, *v2, *v3;          // ptr to original vertices of a triangle
    const float *t1, *t2, *t3;          // ptr to original texcoords of a triangle
    float newV1[3], newV2[3], newV3[3]; // new vertex positions
    float newT1[2], newT2[2], newT3[2]; // new texture coords
    float normal[3];                    // new face normal
    unsigned int index = 0;             // new index value
    int i, j;

    // iteration
    for(i = 1; i <= subdivision; ++i)
    {
        // copy prev arrays
        tmpVertices = vertices;
        tmpTexCoords = texCoords;
        tmpIndices = indices;

        // clear prev arrays
        vertices.clear();
        normals.clear();
        texCoords.clear();
        indices.clear();
        lineIndices.clear();

        index = 0;
        indexCount = (int)tmpIndices.size();
        for(j = 0; j < indexCount; j += 3)
        {
            // get 3 vertice and texcoords of a triangle
            v1 = &tmpVertices[tmpIndices[j] * 3];
            v2 = &tmpVertices[tmpIndices[j + 1] * 3];
            v3 = &tmpVertices[tmpIndices[j + 2] * 3];
            t1 = &tmpTexCoords[tmpIndices[j] * 2];
            t2 = &tmpTexCoords[tmpIndices[j + 1] * 2];
            t3 = &tmpTexCoords[tmpIndices[j + 2] * 2];

            // get 3 new vertices by spliting half on each edge
            computeHalfVertex(v1, v2, radius, newV1);
            computeHalfVertex(v2, v3, radius, newV2);
            computeHalfVertex(v1, v3, radius, newV3);
            computeHalfTexCoord(t1, t2, newT1);
            computeHalfTexCoord(t2, t3, newT2);
            computeHalfTexCoord(t1, t3, newT3);

            // add 4 new triangles
            addVertices(v1, newV1, newV3);
            addTexCoords(t1, newT1, newT3);
            computeFaceNormal(v1, newV1, newV3, normal);
            addNormals(normal, normal, normal);
            addIndices(index, index+1, index+2);

            addVertices(newV1, v2, newV2);
            addTexCoords(newT1, t2, newT2);
            computeFaceNormal(newV1, v2, newV2, normal);
            addNormals(normal, normal, normal);
            addIndices(index+3, index+4, index+5);

            addVertices(newV1, newV2, newV3);
            addTexCoords(newT1, newT2, newT3);
            computeFaceNormal(newV1, newV2, newV3, normal);
            addNormals(normal, normal, normal);
            addIndices(index+6, index+7, index+8);

            addVertices(newV3, newV2, v3);
            addTexCoords(newT3, newT2, t3);
            computeFaceNormal(newV3, newV2, v3, normal);
            addNormals(normal, normal, normal);
            addIndices(index+9, index+10, index+11);

            // add new line indices per iteration
            addSubLineIndices(index, index+1, index+4, index+5, index+11, index+9); //CCW

            // next index
            index += 12;
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
// divide a trianlge (v1-v2-v3) into 4 sub triangles by adding middle vertices
// (newV1, newV2, newV3) and repeat N times
// If subdivision=0, do nothing.
//         v1           //
//        / \           //
// newV1 *---* newV3    //
//      / \ / \         //
//    v2---*---v3       //
//        newV2         //
///////////////////////////////////////////////////////////////////////////////
void Icosphere::subdivideVerticesSmooth()
{
    std::vector<unsigned int> tmpIndices;
    int indexCount;
    unsigned int i1, i2, i3;            // indices from original triangle
    const float *v1, *v2, *v3;          // ptr to original vertices of a triangle
    const float *t1, *t2, *t3;          // ptr to original texcoords of a triangle
    float newV1[3], newV2[3], newV3[3]; // new subdivided vertex positions
    float newN1[3], newN2[3], newN3[3]; // new subdivided normals
    float newT1[2], newT2[2], newT3[2]; // new subdivided texture coords
    unsigned int newI1, newI2, newI3;   // new subdivided indices
    int i, j;

    // iteration for subdivision
    for(i = 1; i <= subdivision; ++i)
    {
        // copy prev indices
        tmpIndices = indices;

        // clear prev arrays
        indices.clear();
        lineIndices.clear();

        indexCount = (int)tmpIndices.size();
        for(j = 0; j < indexCount; j += 3)
        {
            // get 3 indices of each triangle
            i1 = tmpIndices[j];
            i2 = tmpIndices[j+1];
            i3 = tmpIndices[j+2];

            // get 3 vertex attribs from prev triangle
            v1 = &vertices[i1 * 3];
            v2 = &vertices[i2 * 3];
            v3 = &vertices[i3 * 3];
            t1 = &texCoords[i1 * 2];
            t2 = &texCoords[i2 * 2];
            t3 = &texCoords[i3 * 2];

            // get 3 new vertex attribs by spliting half on each edge
            computeHalfVertex(v1, v2, radius, newV1);
            computeHalfVertex(v2, v3, radius, newV2);
            computeHalfVertex(v1, v3, radius, newV3);
            computeHalfTexCoord(t1, t2, newT1);
            computeHalfTexCoord(t2, t3, newT2);
            computeHalfTexCoord(t1, t3, newT3);
            computeVertexNormal(newV1, newN1);
            computeVertexNormal(newV2, newN2);
            computeVertexNormal(newV3, newN3);

            // add new vertices/normals/texcoords to arrays
            // It will check if it is shared/non-shared and return index
            newI1 = addSubVertexAttribs(newV1, newN1, newT1);
            newI2 = addSubVertexAttribs(newV2, newN2, newT2);
            newI3 = addSubVertexAttribs(newV3, newN3, newT3);

            // add 4 new triangle indices
            addIndices(i1, newI1, newI3);
            addIndices(newI1, i2, newI2);
            addIndices(newI1, newI2, newI3);
            addIndices(newI3, newI2, i3);

            // add new line indices
            addSubLineIndices(i1, newI1, i2, newI2, i3, newI3); //CCW
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
// generate interleaved vertices: V/N/T
// stride must be 32 bytes
///////////////////////////////////////////////////////////////////////////////
void Icosphere::buildInterleavedVertices()
{
    std::vector<float>().swap(interleavedVertices);

    std::size_t i, j;
    std::size_t count = vertices.size();
    for(i = 0, j = 0; i < count; i += 3, j += 2)
    {
        interleavedVertices.push_back(vertices[i]);
        interleavedVertices.push_back(vertices[i+1]);
        interleavedVertices.push_back(vertices[i+2]);

        interleavedVertices.push_back(normals[i]);
        interleavedVertices.push_back(normals[i+1]);
        interleavedVertices.push_back(normals[i+2]);

        interleavedVertices.push_back(texCoords[j]);
        interleavedVertices.push_back(texCoords[j+1]);
    }
}



///////////////////////////////////////////////////////////////////////////////
// add single vertex to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addVertex(float x, float y, float z)
{
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
}



///////////////////////////////////////////////////////////////////////////////
// add 3 vertices of a triangle to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addVertices(const float v1[3], const float v2[3], const float v3[3])
{
    vertices.push_back(v1[0]);  // x
    vertices.push_back(v1[1]);  // y
    vertices.push_back(v1[2]);  // z
    vertices.push_back(v2[0]);
    vertices.push_back(v2[1]);
    vertices.push_back(v2[2]);
    vertices.push_back(v3[0]);
    vertices.push_back(v3[1]);
    vertices.push_back(v3[2]);
}



///////////////////////////////////////////////////////////////////////////////
// add single normal to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addNormal(float nx, float ny, float nz)
{
    normals.push_back(nx);
    normals.push_back(ny);
    normals.push_back(nz);
}



///////////////////////////////////////////////////////////////////////////////
// add 3 normals of a triangle to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addNormals(const float n1[3], const float n2[3], const float n3[3])
{
    normals.push_back(n1[0]);  // nx
    normals.push_back(n1[1]);  // ny
    normals.push_back(n1[2]);  // nz
    normals.push_back(n2[0]);
    normals.push_back(n2[1]);
    normals.push_back(n2[2]);
    normals.push_back(n3[0]);
    normals.push_back(n3[1]);
    normals.push_back(n3[2]);
}



///////////////////////////////////////////////////////////////////////////////
// add single texture coord to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addTexCoord(float s, float t)
{
    texCoords.push_back(s);
    texCoords.push_back(t);
}



///////////////////////////////////////////////////////////////////////////////
// add 3 texture coords of a triangle to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addTexCoords(const float t1[2], const float t2[2], const float t3[2])
{
    texCoords.push_back(t1[0]); // s
    texCoords.push_back(t1[1]); // t
    texCoords.push_back(t2[0]);
    texCoords.push_back(t2[1]);
    texCoords.push_back(t3[0]);
    texCoords.push_back(t3[1]);
}



///////////////////////////////////////////////////////////////////////////////
// add 3 indices to array
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addIndices(unsigned int i1, unsigned int i2, unsigned int i3)
{
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i3);
}



///////////////////////////////////////////////////////////////////////////////
// add 7 sub edge lines per triangle to array using 6 indices (CCW)
//     i1                                           //
//     /            : (i1, i2)                      //
//   i2---i6        : (i2, i6)                      //
//   / \  /         : (i2, i3), (i2, i4), (i6, i4)  //
// i3---i4---i5     : (i3, i4), (i4, i5)            //
///////////////////////////////////////////////////////////////////////////////
void Icosphere::addSubLineIndices(unsigned int i1,
                                  unsigned int i2,
                                  unsigned int i3,
                                  unsigned int i4,
                                  unsigned int i5,
                                  unsigned int i6)
{
    lineIndices.push_back(i1);      // i1 - i2
    lineIndices.push_back(i2);
    lineIndices.push_back(i2);      // i2 - i6
    lineIndices.push_back(i6);
    lineIndices.push_back(i2);      // i2 - i3
    lineIndices.push_back(i3);
    lineIndices.push_back(i2);      // i2 - i4
    lineIndices.push_back(i4);
    lineIndices.push_back(i6);      // i6 - i4
    lineIndices.push_back(i4);
    lineIndices.push_back(i3);      // i3 - i4
    lineIndices.push_back(i4);
    lineIndices.push_back(i4);      // i4 - i5
    lineIndices.push_back(i5);
}



///////////////////////////////////////////////////////////////////////////////
// add a subdivided vertex attribs (vertex, normal, texCoord) to arrays, then
// return its index value
// If it is a shared vertex, remember its index, so it can be re-used
///////////////////////////////////////////////////////////////////////////////
unsigned int Icosphere::addSubVertexAttribs(const float v[3], const float n[3], const float t[2])
{
    unsigned int index;     // return value;

    // check if is shared vertex or not first
    if(Icosphere::isSharedTexCoord(t))
    {
        // find if it does already exist in sharedIndices map using (s,t) key
        // if not in the list, add the vertex attribs to arrays and return its index
        // if exists, return the current index
        std::pair<float, float> key = std::make_pair(t[0], t[1]);
        std::map<std::pair<float, float>, unsigned int>::iterator iter = sharedIndices.find(key);
        if(iter == sharedIndices.end())
        {
            addVertex(v[0], v[1], v[2]);
            addNormal(n[0], n[1], n[2]);
            addTexCoord(t[0], t[1]);
            index = texCoords.size() / 2 - 1;
            sharedIndices[key] = index;
        }
        else
        {
            index = iter->second;
        }
    }
    // not shared
    else
    {
        addVertex(v[0], v[1], v[2]);
        addNormal(n[0], n[1], n[2]);
        addTexCoord(t[0], t[1]);
        index = texCoords.size() / 2 - 1;
    }

    return index;
}




// static functions ===========================================================
///////////////////////////////////////////////////////////////////////////////
// return face normal (4th param) of a triangle v1-v2-v3
// if a triangle has no surface (normal length = 0), then return a zero vector
///////////////////////////////////////////////////////////////////////////////
void Icosphere::computeFaceNormal(const float v1[3], const float v2[3], const float v3[3], float n[3])
{
    const float EPSILON = 0.000001f;

    // default return value (0, 0, 0)
    n[0] = n[1] = n[2] = 0;

    // find 2 edge vectors: v1-v2, v1-v3
    float ex1 = v2[0] - v1[0];
    float ey1 = v2[1] - v1[1];
    float ez1 = v2[2] - v1[2];
    float ex2 = v3[0] - v1[0];
    float ey2 = v3[1] - v1[1];
    float ez2 = v3[2] - v1[2];

    // cross product: e1 x e2
    float nx, ny, nz;
    nx = ey1 * ez2 - ez1 * ey2;
    ny = ez1 * ex2 - ex1 * ez2;
    nz = ex1 * ey2 - ey1 * ex2;

    // normalize only if the length is > 0
    float length = sqrtf(nx * nx + ny * ny + nz * nz);
    if(length > EPSILON)
    {
        // normalize
        float lengthInv = 1.0f / length;
        n[0] = nx * lengthInv;
        n[1] = ny * lengthInv;
        n[2] = nz * lengthInv;
    }
}



///////////////////////////////////////////////////////////////////////////////
// return vertex normal (2nd param) by mormalizing the vertex vector
///////////////////////////////////////////////////////////////////////////////
void Icosphere::computeVertexNormal(const float v[3], float normal[3])
{
    // normalize
    float scale = Icosphere::computeScaleForLength(v, 1);
    normal[0] = v[0] * scale;
    normal[1] = v[1] * scale;
    normal[2] = v[2] * scale;
}



///////////////////////////////////////////////////////////////////////////////
// get the scale factor for vector to resize to the given length of vector
///////////////////////////////////////////////////////////////////////////////
float Icosphere::computeScaleForLength(const float v[3], float length)
{
    // and normalize the vector then re-scale to new radius
    return length / sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}



///////////////////////////////////////////////////////////////////////////////
// find middle point of 2 vertices
// NOTE: new vertex must be resized, so the length is equal to the given length
///////////////////////////////////////////////////////////////////////////////
void Icosphere::computeHalfVertex(const float v1[3], const float v2[3], float length, float newV[3])
{
    newV[0] = v1[0] + v2[0];
    newV[1] = v1[1] + v2[1];
    newV[2] = v1[2] + v2[2];
    float scale = Icosphere::computeScaleForLength(newV, length);
    newV[0] *= scale;
    newV[1] *= scale;
    newV[2] *= scale;
}



///////////////////////////////////////////////////////////////////////////////
// find middle texcoords of 2 tex coords and return new coord (3rd param)
///////////////////////////////////////////////////////////////////////////////
void Icosphere::computeHalfTexCoord(const float t1[2], const float t2[2], float newT[2])
{
    newT[0] = (t1[0] + t2[0]) * 0.5f;
    newT[1] = (t1[1] + t2[1]) * 0.5f;
}



///////////////////////////////////////////////////////////////////////////////
// This function used 20 non-shared line segments to determine if the given
// texture coordinate is shared or no. If it is on the line segments, it is also
// non-shared point
//   00  01  02  03  04         //
//   /\  /\  /\  /\  /\         //
//  /  \/  \/  \/  \/  \        //
// 05  06  07  08  09   \       //
//   \   10  11  12  13  14     //
//    \  /\  /\  /\  /\  /      //
//     \/  \/  \/  \/  \/       //
//      15  16  17  18  19      //
///////////////////////////////////////////////////////////////////////////////
bool Icosphere::isSharedTexCoord(const float t[2])
{
    // 20 non-shared line segments
    const float S = 1.0f / 11;  // texture steps
    const float T = 1.0f / 3;
    static float segments[] = { S, 0,       0, T,       // 00 - 05
                                S, 0,       S*2, T,     // 00 - 06
                                S*3, 0,     S*2, T,     // 01 - 06
                                S*3, 0,     S*4, T,     // 01 - 07
                                S*5, 0,     S*4, T,     // 02 - 07
                                S*5, 0,     S*6, T,     // 02 - 08
                                S*7, 0,     S*6, T,     // 03 - 08
                                S*7, 0,     S*8, T,     // 03 - 09
                                S*9, 0,     S*8, T,     // 04 - 09
                                S*9, 0,     1, T*2,     // 04 - 14
                                0, T,       S*2, 1,     // 05 - 15
                                S*3, T*2,   S*2, 1,     // 10 - 15
                                S*3, T*2,   S*4, 1,     // 10 - 16
                                S*5, T*2,   S*4, 1,     // 11 - 16
                                S*5, T*2,   S*6, 1,     // 11 - 17
                                S*7, T*2,   S*6, 1,     // 12 - 17
                                S*7, T*2,   S*8, 1,     // 12 - 18
                                S*9, T*2,   S*8, 1,     // 13 - 18
                                S*9, T*2,   S*10, 1,    // 13 - 19
                                1, T*2,     S*10, 1 };  // 14 - 19

    // check the point with all 20 line segments
    // if it is on the line segment, it is non-shared
    int count = (int)(sizeof(segments) / sizeof(segments[0]));
    for(int i = 0, j = 2; i < count; i+=4, j+=4)
    {
        if(Icosphere::isOnLineSegment(&segments[i], &segments[j], t))
            return false;   // not shared
    }

    return true;
}



///////////////////////////////////////////////////////////////////////////////
// determine a point c is on the line segment a-b
///////////////////////////////////////////////////////////////////////////////
bool Icosphere::isOnLineSegment(const float a[2], const float b[2], const float c[2])
{
    const float EPSILON = 0.0001f;

    // cross product must be 0 if c is on the line
    float cross = ((b[0] - a[0]) * (c[1] - a[1])) - ((b[1] - a[1]) * (c[0] - a[0]));
    if(cross > EPSILON || cross < -EPSILON)
        return false;

    // c must be within a-b
    if((c[0] > a[0] && c[0] > b[0]) || (c[0] < a[0] && c[0] < b[0]))
        return false;
    if((c[1] > a[1] && c[1] > b[1]) || (c[1] < a[1] && c[1] < b[1]))
        return false;

    return true;    // all passed, it is on the line segment
}

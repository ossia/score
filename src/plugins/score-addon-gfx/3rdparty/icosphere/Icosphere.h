///////////////////////////////////////////////////////////////////////////////
// Icosphere.h
// ===========
// Polyhedron subdividing icosahedron (20 tris) by N-times iteration
// The icosphere with N=1 (default) has 80 triangles by subdividing a triangle
// of icosahedron into 4 triangles. If N=0, it is identical to icosahedron.
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2018-07-23
// UPDATED: 2019-12-28
///////////////////////////////////////////////////////////////////////////////

#ifndef GEOMETRY_ICOSPHERE_H
#define GEOMETRY_ICOSPHERE_H

#include <vector>
#include <map>

class Icosphere
{
public:
    // ctor/dtor
    Icosphere(float radius=1.0f, int subdivision=1, bool smooth=false);
    ~Icosphere() {}

    // getters/setters
    float getRadius() const                 { return radius; }
    void setRadius(float radius);
    int getSubdivision() const              { return subdivision; }
    void setSubdivision(int subdivision);
    bool getSmooth() const                  { return smooth; }
    void setSmooth(bool smooth);

    // for vertex data
    unsigned int getVertexCount() const     { return (unsigned int)vertices.size() / 3; }
    unsigned int getNormalCount() const     { return (unsigned int)normals.size() / 3; }
    unsigned int getTexCoordCount() const   { return (unsigned int)texCoords.size() / 2; }
    unsigned int getIndexCount() const      { return (unsigned int)indices.size(); }
    unsigned int getLineIndexCount() const  { return (unsigned int)lineIndices.size(); }
    unsigned int getTriangleCount() const   { return getIndexCount() / 3; }

    unsigned int getVertexSize() const      { return (unsigned int)vertices.size() * sizeof(float); }   // # of bytes
    unsigned int getNormalSize() const      { return (unsigned int)normals.size() * sizeof(float); }
    unsigned int getTexCoordSize() const    { return (unsigned int)texCoords.size() * sizeof(float); }
    unsigned int getIndexSize() const       { return (unsigned int)indices.size() * sizeof(unsigned int); }
    unsigned int getLineIndexSize() const   { return (unsigned int)lineIndices.size() * sizeof(unsigned int); }

    const float* getVertices() const        { return vertices.data(); }
    const float* getNormals() const         { return normals.data(); }
    const float* getTexCoords() const       { return texCoords.data(); }
    const unsigned int* getIndices() const  { return indices.data(); }
    const unsigned int* getLineIndices() const  { return lineIndices.data(); }

    // for interleaved vertices: V/N/T
    unsigned int getInterleavedVertexCount() const  { return getVertexCount(); }    // # of vertices
    unsigned int getInterleavedVertexSize() const   { return (unsigned int)interleavedVertices.size() * sizeof(float); }    // # of bytes
    int getInterleavedStride() const                { return interleavedStride; }   // should be 32 bytes
    const float* getInterleavedVertices() const     { return interleavedVertices.data(); }

    // draw in VertexArray mode
    void draw() const;
    void drawLines(const float lineColor[4]) const;
    void drawWithLines(const float lineColor[4]) const;

    // debug
    void printSelf() const;

protected:

private:
    // static functions
    static void computeFaceNormal(const float v1[3], const float v2[3], const float v3[3], float normal[3]);
    static void computeVertexNormal(const float v[3], float normal[3]);
    static float computeScaleForLength(const float v[3], float length);
    static void computeHalfVertex(const float v1[3], const float v2[3], float length, float newV[3]);
    static void computeHalfTexCoord(const float t1[2], const float t2[2], float newT[2]);
    static bool isSharedTexCoord(const float t[2]);
    static bool isOnLineSegment(const float a[2], const float b[2], const float c[2]);

    // member functions
    void updateRadius();
    std::vector<float> computeIcosahedronVertices();
    void buildVerticesFlat();
    void buildVerticesSmooth();
    void subdivideVerticesFlat();
    void subdivideVerticesSmooth();
    void buildInterleavedVertices();
    void addVertex(float x, float y, float z);
    void addVertices(const float v1[3], const float v2[3], const float v3[3]);
    void addNormal(float nx, float ny, float nz);
    void addNormals(const float n1[3], const float n2[3], const float n3[3]);
    void addTexCoord(float s, float t);
    void addTexCoords(const float t1[2], const float t2[2], const float t3[2]);
    void addIndices(unsigned int i1, unsigned int i2, unsigned int i3);
    void addSubLineIndices(unsigned int i1, unsigned int i2, unsigned int i3,
                           unsigned int i4, unsigned int i5, unsigned int i6);
    unsigned int addSubVertexAttribs(const float v[3], const float n[3], const float t[2]);

    // memeber vars
    float radius;                           // circumscribed radius
    int subdivision;
    bool smooth;
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texCoords;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> lineIndices;
    std::map<std::pair<float, float>, unsigned int> sharedIndices;   // indices of shared vertices, key is tex coord (s,t)

    // interleaved
    std::vector<float> interleavedVertices;
    int interleavedStride;                  // # of bytes to hop to the next vertex (should be 32 bytes)

};

#endif

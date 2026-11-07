// 3DGS classic — vertex stage paired with 02_DrawSplat.frag.
//
// Topology produced by 01_Decode.cs: VERTEX_COUNT=6, INSTANCE_COUNT=N
// with `pos / color / scale / rotation` as RATE=instance per-vertex
// inputs. gl_VertexIndex 0..5 picks a quad corner from a hardcoded
// LUT; gl_InstanceIndex is implicit via the per-instance bind rate
// (each instance reads its own pos/color/scale/rotation tuple).
//
// EWA covariance projection follows Kerbl et al. 2023 and its reference
// rasteriser (graphdeco-inria/diff-gaussian-rasterization, forward.cu
// computeCov2D / preprocessCUDA): cull at view depth <= 0.2, clamp tx/tz and
// ty/tz to +-1.3 tan(fov/2) before building the Jacobian, add 0.3 px^2 to the
// 2D covariance diagonal (low-pass dilation), eigenvalues with
// max(0.1, mid^2 - det). With `antialiased` on, opacity is additionally scaled
// by sqrt(det(cov) / det(cov + 0.3 I)) -- the Mip-Splatting 2D filter
// (Yu et al. CVPR 2024) / gsplat "antialiased" mode -- which is only correct
// for scenes TRAINED that way; vanilla 3DGS PLYs want it off.
// Output varyings: v_center (pixel-space splat center for the FS to
// compute distance from), v_color (rgba pre-blend), v_conic (the
// inverse 2D covariance packed as (a, b, c) for the FS's `power =
// -0.5 * (a*dx*dx + 2*b*dx*dy + c*dy*dy)` evaluation).

const vec2 quadCorners[6] = vec2[6](
    vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
    vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0));

mat3 quatToMat(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    return mat3(
        1.0 - 2.0*(y*y + z*z), 2.0*(x*y + w*z),       2.0*(x*z - w*y),
        2.0*(x*y - w*z),       1.0 - 2.0*(x*x + z*z), 2.0*(y*z + w*x),
        2.0*(x*z + w*y),       2.0*(y*z - w*x),       1.0 - 2.0*(x*x + y*y));
}

void main() {
    isf_vertShaderInit();
    int corner = gl_VertexIndex % 6;

    // Per-instance attributes (auto-bound at instance rate by the
    // upstream geometry's RATE=instance bindings).
    vec3 posWorld = pos;
    vec4 col      = color;
    vec3 sc       = scale;
    vec4 rot      = rotation;

    // Cull near-fully-transparent splats.
    if (col.a < 1.0/255.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    vec2 viewport = camera.renderSize.xy;
    vec4 viewPos  = camera.view * vec4(posWorld, 1.0);

    // Reconstruct focal lengths from the projection matrix
    // (proj[0][0] = 2*focalX/width, proj[1][1] = 2*focalY/height).
    float focalX = camera.projection[0][0] * viewport.x * 0.5;
    float focalY = camera.projection[1][1] * viewport.y * 0.5;
    float tanFovX = 0.5 * viewport.x / focalX;
    float tanFovY = 0.5 * viewport.y / focalY;

    vec4 clipPos = camera.projection * viewPos;
    // Kerbl in_frustum: p_view.z <= 0.2 is culled (GL view: w = -z_view).
    // There is deliberately NO x/y centre cull: upstream has it commented
    // out, and a large splat centred off-screen still covers the screen.
    if (clipPos.w <= 0.2) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    vec3 ndc = clipPos.xyz / clipPos.w;

    // Clamp the view-space position to the visible frustum extents
    // so off-screen splats don't blow up the Jacobian. Standard 3DGS
    // pre-projection trick.
    float limX = 1.3 * tanFovX;
    float limY = 1.3 * tanFovY;
    float txtz = viewPos.x / viewPos.z;
    float tytz = viewPos.y / viewPos.z;
    viewPos.x = clamp(txtz, -limX, limX) * viewPos.z;
    viewPos.y = clamp(tytz, -limY, limY) * viewPos.z;

    // 3D covariance Σ = R * S * Sᵀ * Rᵀ.
    mat3 R = quatToMat(rot);
    mat3 S = mat3(sc.x, 0.0, 0.0,  0.0, sc.y, 0.0,  0.0, 0.0, sc.z);
    mat3 M = R * S;
    mat3 Sigma = M * transpose(M);

    // 2D screen-space covariance via the EWA Jacobian + view rotation.
    mat3 W = mat3(camera.view);
    float z2 = viewPos.z * viewPos.z;
    mat3 J = mat3(
        focalX / viewPos.z, 0.0, 0.0,
        0.0, focalY / viewPos.z, 0.0,
        -focalX * viewPos.x / z2, -focalY * viewPos.y / z2, 0.0);

    mat3 T = J * W;
    mat3 cov = T * Sigma * transpose(T);

    float cov_xx = cov[0][0], cov_xy = cov[0][1], cov_yy = cov[1][1];

    // Mip-Splatting 2D filter (Yu et al. 2024): inflates the covariance
    // by `kernel_size` pixels² of isotropic noise, scales alpha by the
    // determinant ratio so total integrated brightness is preserved.
    float det_0 = max(0.0, cov_xx * cov_yy - cov_xy * cov_xy);
    cov_xx += kernelSize;
    cov_yy += kernelSize;
    float det = cov_xx * cov_yy - cov_xy * cov_xy;
    float mipCoef = antialiased ? sqrt(det_0 / max(det, 1e-12)) : 1.0;

    float mid = 0.5 * (cov_xx + cov_yy);
    float disc = max(0.1, mid * mid - det);
    float lambda1 = mid + sqrt(disc);
    float lambda2 = mid - sqrt(disc);

    // Eigenvector of cov for the major axis.
    vec2 eigVec1;
    if (abs(cov_xy) > 1e-6) {
        eigVec1 = normalize(vec2(cov_xy, lambda1 - cov_xx));
    } else {
        eigVec1 = (cov_xx >= cov_yy) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    }
    vec2 eigVec2 = vec2(-eigVec1.y, eigVec1.x);

    // Quad half-extents along the principal axes. Kerbl bins a splat into
    // the tiles of an axis-aligned square of half-side ceil(3 sqrt(lambda1))
    // and then drops every pixel with alpha < 1/255, i.e. with Mahalanobis^2
    // > 2 ln(255 opacity). The oriented quad covers exactly that ellipse,
    // capped at the same 3 sqrt(lambda1).
    float alpha0 = col.a * mipCoef;
    float k = sqrt(max(0.0, 2.0 * log(255.0 * alpha0)));
    float rMax = ceil(3.0 * sqrt(max(lambda1, 0.0)));
    float r1 = min(k * sqrt(max(lambda1, 0.0)), rMax) + 1.0;
    float r2 = min(k * sqrt(max(lambda2, 0.0)), rMax) + 1.0;

    if (det <= 0.0 || alpha0 < 1.0/255.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    float inv_det = 1.0 / det;
    v_conic = vec3(cov_yy * inv_det, -cov_xy * inv_det, cov_xx * inv_det);

    // Expand the quad in pixel space along the eigenvectors, convert
    // to NDC offset, ride out via clipSpaceCorrMatrix (Y-flip for
    // Vulkan / D3D / Metal vs OpenGL).
    vec2 quadPos = quadCorners[corner];
    vec2 pixelOffset = quadPos.x * r1 * eigVec1 + quadPos.y * r2 * eigVec2;
    vec2 ndcOffset = pixelOffset * 2.0 / viewport;

    gl_Position = clipSpaceCorrMatrix * vec4(ndc.xy + ndcOffset, ndc.z, 1.0);

    // FS needs the splat's pixel-space center to compute (dx, dy)
    // against gl_FragCoord. Compute it in OpenGL Y-up framebuffer
    // coords (the convention gl_FragCoord uses by default on every
    // backend QRhi targets — Vulkan/D3D/Metal all preserve OpenGL
    // gl_FragCoord semantics through QShaderBaker's cross-compile).
    // Going through `clipSpaceCorrMatrix` would put v_center in
    // Vulkan native framebuffer space (Y-down), which is the
    // OPPOSITE convention from gl_FragCoord — making d.y in the
    // FS only correct at the vertical screen middle where the two
    // conventions happen to agree. Everywhere else, d.y blows up
    // and the Gaussian collapses to a thin horizontal line at
    // mid-screen (the symptom we see if we get this wrong).
    v_center = (ndc.xy * 0.5 + 0.5) * viewport;

    v_color = vec4(col.rgb, alpha0);

    // Owns the single clip-space Y negation on Metal / D3D (SHADER-GUIDE rule 1).
    isf_vertShaderFinish();
}

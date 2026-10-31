#pragma once
#include <Gfx/Calibration/ProjectorModel.hpp>

// EPnP: Lepetit, Moreno-Noguer & Fua, "EPnP: An Accurate O(n) Solution to the
// PnP Problem", IJCV 81(2):155-166, 2009, doi:10.1007/s11263-008-0152-6.
// Four control points from the PCA of the world points; their camera-frame
// coordinates span the null space of M; the betas for 1 to 4 null vectors
// come from the control point distances, refined by Gauss-Newton; the lowest
// reprojection error wins. The beta approximations follow the authors' C++
// code as shipped in OpenCV (modules/calib3d/src/epnp.cpp).
namespace score::gfx::procam
{
namespace epnp
{
//! [R | t] from 4+ non-coplanar points on normalised image coordinates.
inline bool solve(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t)
{
  const int n = int(idx.size());
  if(n < 4)
    return false;

  std::array<Vec3, 4> cw;
  cw[0] = Vec3::Zero();
  for(int i : idx)
    cw[0] += X[i];
  cw[0] /= double(n);
  Mat3 cov = Mat3::Zero();
  for(int i : idx)
  {
    const Vec3 d = X[i] - cw[0];
    cov += d * d.transpose();
  }
  Eigen::SelfAdjointEigenSolver<Mat3> pca(cov);
  for(int k = 0; k < 3; k++)
  {
    const double l = pca.eigenvalues()(2 - k);
    if(!(l > 1e-12 * pca.eigenvalues()(2)))
      return false;
    cw[k + 1] = cw[0] + std::sqrt(l / double(n)) * pca.eigenvectors().col(2 - k);
  }

  Mat3 C;
  for(int k = 0; k < 3; k++)
    C.col(k) = cw[k + 1] - cw[0];
  const Mat3 Ci = C.inverse();
  std::vector<Eigen::Vector4d> alpha(n);
  Eigen::MatrixXd M(2 * n, 12);
  for(int k = 0; k < n; k++)
  {
    const Vec3 a = Ci * (X[idx[k]] - cw[0]);
    alpha[k] << 1. - a.sum(), a.x(), a.y(), a.z();
    const double u = xn[idx[k]].x(), v = xn[idx[k]].y();
    for(int j = 0; j < 4; j++)
    {
      M.block<1, 3>(2 * k, 3 * j) << alpha[k](j), 0., -alpha[k](j) * u;
      M.block<1, 3>(2 * k + 1, 3 * j) << 0., alpha[k](j), -alpha[k](j) * v;
    }
  }
  const Eigen::Matrix<double, 12, 12> MtM = M.transpose() * M;
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 12, 12>> es(MtM);
  std::array<Eigen::Matrix<double, 12, 1>, 4> v;
  for(int i = 0; i < 4; i++)
    v[i] = es.eigenvectors().col(i);

  static constexpr int pairs[6][2]{{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
  Eigen::Matrix<double, 6, 10> L;
  Eigen::Matrix<double, 6, 1> rho;
  for(int p = 0; p < 6; p++)
  {
    const int a = pairs[p][0], b = pairs[p][1];
    Vec3 dv[4];
    for(int i = 0; i < 4; i++)
      dv[i] = v[i].segment<3>(3 * a) - v[i].segment<3>(3 * b);
    L.row(p) << dv[0].dot(dv[0]), 2. * dv[0].dot(dv[1]), dv[1].dot(dv[1]),
        2. * dv[0].dot(dv[2]), 2. * dv[1].dot(dv[2]), dv[2].dot(dv[2]),
        2. * dv[0].dot(dv[3]), 2. * dv[1].dot(dv[3]), 2. * dv[2].dot(dv[3]),
        dv[3].dot(dv[3]);
    rho(p) = (cw[a] - cw[b]).squaredNorm();
  }

  auto gaussNewton = [&](Eigen::Vector4d& b) {
    for(int it = 0; it < 10; it++)
    {
      Eigen::Matrix<double, 6, 4> A;
      Eigen::Matrix<double, 6, 1> r;
      for(int p = 0; p < 6; p++)
      {
        const auto l = L.row(p);
        A(p, 0) = 2 * l(0) * b(0) + l(1) * b(1) + l(3) * b(2) + l(6) * b(3);
        A(p, 1) = l(1) * b(0) + 2 * l(2) * b(1) + l(4) * b(2) + l(7) * b(3);
        A(p, 2) = l(3) * b(0) + l(4) * b(1) + 2 * l(5) * b(2) + l(8) * b(3);
        A(p, 3) = l(6) * b(0) + l(7) * b(1) + l(8) * b(2) + 2 * l(9) * b(3);
        r(p) = rho(p)
               - (l(0) * b(0) * b(0) + l(1) * b(0) * b(1) + l(2) * b(1) * b(1)
                  + l(3) * b(0) * b(2) + l(4) * b(1) * b(2) + l(5) * b(2) * b(2)
                  + l(6) * b(0) * b(3) + l(7) * b(1) * b(3) + l(8) * b(2) * b(3)
                  + l(9) * b(3) * b(3));
      }
      const Eigen::Vector4d dx = A.colPivHouseholderQr().solve(r);
      if(!dx.allFinite())
        return;
      b += dx;
      if(dx.norm() < 1e-12 * (1. + b.norm()))
        return;
    }
  };

  auto poseFrom = [&](const Eigen::Vector4d& b, Mat3& Rk, Vec3& tk) {
    std::array<Vec3, 4> cc;
    for(int j = 0; j < 4; j++)
    {
      cc[j] = Vec3::Zero();
      for(int i = 0; i < 4; i++)
        cc[j] += b(i) * v[i].segment<3>(3 * j);
    }
    std::vector<Vec3> pc(n);
    for(int k = 0; k < n; k++)
    {
      pc[k] = Vec3::Zero();
      for(int j = 0; j < 4; j++)
        pc[k] += alpha[k](j) * cc[j];
    }
    int front = 0;
    for(const auto& p : pc)
      front += p.z() > 0. ? 1 : -1;
    if(front < 0)
      for(auto& p : pc)
        p = -p;

    Vec3 mw = Vec3::Zero(), mc = Vec3::Zero();
    for(int k = 0; k < n; k++)
    {
      mw += X[idx[k]];
      mc += pc[k];
    }
    mw /= double(n);
    mc /= double(n);
    Mat3 H = Mat3::Zero();
    for(int k = 0; k < n; k++)
      H += (pc[k] - mc) * (X[idx[k]] - mw).transpose();
    Eigen::JacobiSVD<Mat3> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Mat3 D = Mat3::Identity();
    if((svd.matrixU() * svd.matrixV().transpose()).determinant() < 0.)
      D(2, 2) = -1.;
    Rk = svd.matrixU() * D * svd.matrixV().transpose();
    tk = mc - Rk * mw;
    if(!Rk.allFinite() || !tk.allFinite())
      return std::numeric_limits<double>::infinity();
    double e = 0.;
    for(int k = 0; k < n; k++)
    {
      const Vec3 c = Rk * X[idx[k]] + tk;
      if(!(c.z() > 1e-12))
        return std::numeric_limits<double>::infinity();
      e += (Vec2(c.x() / c.z(), c.y() / c.z()) - xn[idx[k]]).squaredNorm();
    }
    return e;
  };

  auto signedRoot = [](double b11, double b22, double b12, Eigen::Vector4d& b) {
    if(b11 < 0.)
    {
      b(0) = std::sqrt(-b11);
      b(1) = b22 < 0. ? std::sqrt(-b22) : 0.;
    }
    else
    {
      b(0) = std::sqrt(b11);
      b(1) = b22 > 0. ? std::sqrt(b22) : 0.;
    }
    if(b12 < 0.)
      b(0) = -b(0);
  };

  std::vector<Eigen::Vector4d> candidates;
  {
    Eigen::Matrix<double, 6, 4> L4;
    L4 << L.col(0), L.col(1), L.col(3), L.col(6);
    const Eigen::Vector4d B = L4.colPivHouseholderQr().solve(rho);
    Eigen::Vector4d b;
    if(B(0) < 0.)
    {
      b(0) = std::sqrt(-B(0));
      b.tail<3>() = -B.tail<3>() / b(0);
    }
    else
    {
      b(0) = std::sqrt(B(0));
      b.tail<3>() = B.tail<3>() / (b(0) > 0. ? b(0) : 1.);
    }
    candidates.push_back(Eigen::Vector4d(b(0), b(1), b(2), b(3)));
  }
  {
    Eigen::Matrix<double, 6, 3> L3;
    L3 << L.col(0), L.col(1), L.col(2);
    const Vec3 B = L3.colPivHouseholderQr().solve(rho);
    Eigen::Vector4d b = Eigen::Vector4d::Zero();
    signedRoot(B(0), B(2), B(1), b);
    candidates.push_back(b);
  }
  {
    Eigen::Matrix<double, 6, 5> L5;
    L5 << L.col(0), L.col(1), L.col(2), L.col(3), L.col(4);
    const Eigen::Matrix<double, 5, 1> B = L5.colPivHouseholderQr().solve(rho);
    Eigen::Vector4d b = Eigen::Vector4d::Zero();
    signedRoot(B(0), B(2), B(1), b);
    b(2) = std::abs(b(0)) > 0. ? B(3) / b(0) : 0.;
    candidates.push_back(b);
  }
  {
    double num = 0., den = 0.;
    for(int p = 0; p < 6; p++)
    {
      const int a = pairs[p][0], c = pairs[p][1];
      const double dv = (v[0].segment<3>(3 * a) - v[0].segment<3>(3 * c)).norm();
      num += dv * std::sqrt(rho(p));
      den += dv * dv;
    }
    if(den > 0.)
      candidates.push_back(Eigen::Vector4d(num / den, 0., 0., 0.));
  }

  double best = std::numeric_limits<double>::infinity();
  for(auto b : candidates)
  {
    if(!b.allFinite())
      continue;
    gaussNewton(b);
    Mat3 Rk;
    Vec3 tk;
    const double e = poseFrom(b, Rk, tk);
    if(e < best)
    {
      best = e;
      R = Rk;
      t = tk;
    }
  }
  return std::isfinite(best);
}
}

//! Pose only, the intrinsics of `known` being fixed, from 4+ points: EPnP,
//! or posePlanar for coplanar points, then Levenberg-Marquardt on R and t.
//! With 6+ points, the better of this and solvePose (DLT) is kept.
inline Result solvePoseAny(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Model& known,
    const Options& opt = {})
{
  const int N = int(std::min(X.size(), x.size()));
  std::vector<int> all(N);
  std::iota(all.begin(), all.end(), 0);
  const bool planar = N >= 4 && coplanar(X, all);
  Result dlt;
  if(N >= 6 || planar)
  {
    dlt = solvePose(X, x, known, opt);
    if(planar || opt.ransac)
      return dlt;
  }

  Result res;
  res.mask.assign(N, 0);
  if(N < 4)
  {
    res.error = "at least 4 correspondences are needed";
    return res;
  }
  std::vector<Vec2> xn(N);
  for(int i = 0; i < N; i++)
    xn[i] = known.normalise(x[i]);
  res.model = known;
  if(!epnp::solve(X, xn, all, res.model.R, res.model.t))
  {
    if(dlt.ok)
      return dlt;
    res.error = "degenerate correspondences";
    return res;
  }
  refine(res.model, X, x, all, false, opt.maxIterations, true);
  res.mask.assign(N, 1);
  res.inliers = N;
  res.rms = rms(res.model, X, x, all);
  res.ok = std::isfinite(res.rms);
  if(!res.ok)
    res.error = "the refinement diverged or left a point behind the projector";
  if(dlt.ok && (!res.ok || dlt.rms < res.rms))
    return dlt;
  return res;
}
}

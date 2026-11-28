#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Eigen/Dense>

namespace score::gfx::procam
{

bool dlt(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const std::vector<int>& idx,
    Mat34& P)
{
  const int n = int(idx.size());
  if(n < 6)
    return false;
  const Eigen::Matrix4d T3 = detail::normalisation<3>(X, idx);
  const Mat3 T2 = detail::normalisation<2>(x, idx);

  Eigen::MatrixXd A(2 * n, 12);
  for(int k = 0; k < n; k++)
  {
    const int i = idx[k];
    const Eigen::Vector4d Xh = T3 * X[i].homogeneous();
    const Vec3 xh = T2 * x[i].homogeneous();
    const double u = xh.x() / xh.z(), v = xh.y() / xh.z();
    A.row(2 * k) << Xh.transpose(), Eigen::RowVector4d::Zero(), -u * Xh.transpose();
    A.row(2 * k + 1) << Eigen::RowVector4d::Zero(), Xh.transpose(), -v * Xh.transpose();
  }
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullV);
  const auto& sv = svd.singularValues();
  if(!(sv(0) > 0.) || sv(10) < 1e-9 * sv(0))
    return false;
  const Eigen::VectorXd p = svd.matrixV().col(11);
  Mat34 Pn;
  Pn << p.segment<4>(0).transpose(), p.segment<4>(4).transpose(),
      p.segment<4>(8).transpose();
  P = T2.inverse() * Pn * T3;
  return P.allFinite();
}

bool decompose(Mat34 P, Model& m)
{
  Mat3 M = P.leftCols<3>();
  if(M.determinant() < 0.)
  {
    P = -P;
    M = -M;
  }
  // RQ through QR of the row-reversed transpose.
  Mat3 J;
  J << 0, 0, 1, 0, 1, 0, 1, 0, 0;
  const Mat3 Mt = (J * M).transpose();
  Eigen::HouseholderQR<Mat3> qr(Mt);
  const Mat3 Q = qr.householderQ();
  const Mat3 Rr = qr.matrixQR().triangularView<Eigen::Upper>();
  Mat3 K = J * Rr.transpose() * J;
  Mat3 R = J * Q.transpose();
  for(int i = 0; i < 3; i++)
  {
    if(K(i, i) < 0.)
    {
      K.col(i) = -K.col(i);
      R.row(i) = -R.row(i);
    }
  }
  if(R.determinant() < 0. || !(std::abs(K(2, 2)) > 1e-15))
    return false;
  const Vec3 t = K.inverse() * P.col(3);
  K /= K(2, 2);
  m.fx = K(0, 0);
  m.fy = K(1, 1);
  m.cx = K(0, 2);
  m.cy = K(1, 2);
  m.R = R;
  m.t = t;
  m.k1 = m.k2 = 0.;
  return std::isfinite(m.fx) && std::isfinite(m.fy) && m.fx > 0. && m.fy > 0.
         && m.t.allFinite();
}

double rms(
    const Model& m, const std::vector<Vec3>& X, const std::vector<Vec2>& x,
    const std::vector<int>& idx)
{
  if(idx.empty())
    return 0.;
  double s = 0.;
  for(int i : idx)
  {
    const double e = reprojectionError(m, X[i], x[i]);
    if(!std::isfinite(e))
      return std::numeric_limits<double>::infinity();
    s += e * e;
  }
  return std::sqrt(s / double(idx.size()));
}

void refine(
    Model& m, const std::vector<Vec3>& X, const std::vector<Vec2>& x,
    const std::vector<int>& idx, bool distortion, int maxIterations,
    bool poseOnly)
{
  std::vector<int> active;
  if(!poseOnly)
    active = {0, 1, 2, 3};
  for(int j = 4; j < 10; j++)
    active.push_back(j);
  if(distortion && !poseOnly)
  {
    active.push_back(10);
    active.push_back(11);
  }
  const int np = int(active.size());
  const int n = int(idx.size());
  if(n * 2 < np)
    return;

  auto apply = [&](const Model& base, const Eigen::VectorXd& reduced) {
    Eigen::VectorXd d = Eigen::VectorXd::Zero(12);
    for(int k = 0; k < np; k++)
      d(active[k]) = reduced(k);
    Model r = base;
    r.fx += d(0);
    r.fy += d(1);
    r.cx += d(2);
    r.cy += d(3);
    const Vec3 w = d.segment<3>(4);
    const double a = w.norm();
    if(a > 0.)
      r.R = Eigen::AngleAxisd(a, w / a).toRotationMatrix() * base.R;
    r.t = base.t + d.segment<3>(7);
    r.k1 += d(10);
    r.k2 += d(11);
    return r;
  };
  auto residuals = [&](const Model& mm, Eigen::VectorXd& r) {
    r.resize(2 * n);
    for(int k = 0; k < n; k++)
    {
      Vec2 p;
      if(!mm.project(X[idx[k]], p))
        p = x[idx[k]] + Vec2(1e3, 1e3);
      r.segment<2>(2 * k) = p - x[idx[k]];
    }
  };

  Eigen::VectorXd r, r2;
  residuals(m, r);
  double cost = r.squaredNorm();
  double lambda = 1e-3;
  const double steps[12]
      = {1e-4 * m.fx, 1e-4 * m.fy, 1e-3, 1e-3, 1e-7, 1e-7, 1e-7,
         1e-7 * (1. + m.t.norm()), 1e-7 * (1. + m.t.norm()), 1e-7 * (1. + m.t.norm()),
         1e-7, 1e-7};

  Eigen::MatrixXd Jac(2 * n, np);
  for(int it = 0; it < maxIterations; it++)
  {
    for(int j = 0; j < np; j++)
    {
      Eigen::VectorXd d = Eigen::VectorXd::Zero(np);
      const double h = std::max(steps[active[j]], 1e-12);
      d(j) = h;
      Eigen::VectorXd rp, rm;
      residuals(apply(m, d), rp);
      d(j) = -h;
      residuals(apply(m, d), rm);
      Jac.col(j) = (rp - rm) / (2. * h);
    }
    const Eigen::MatrixXd JtJ = Jac.transpose() * Jac;
    const Eigen::VectorXd g = Jac.transpose() * r;
    if(g.lpNorm<Eigen::Infinity>() < 1e-12)
      break;

    bool improved = false;
    for(int tries = 0; tries < 12; tries++)
    {
      Eigen::MatrixXd A = JtJ;
      for(int j = 0; j < np; j++)
        A(j, j) += lambda * std::max(JtJ(j, j), 1e-12);
      const Eigen::VectorXd delta = A.ldlt().solve(-g);
      if(!delta.allFinite())
      {
        lambda *= 10.;
        continue;
      }
      const Model cand = apply(m, delta);
      residuals(cand, r2);
      const double c2 = r2.squaredNorm();
      if(c2 < cost)
      {
        const double rel = (cost - c2) / std::max(cost, 1e-300);
        m = cand;
        r = r2;
        cost = c2;
        lambda = std::max(lambda / 10., 1e-12);
        improved = true;
        if(rel < 1e-12)
          it = maxIterations;
        break;
      }
      lambda *= 10.;
    }
    if(!improved)
      break;
  }
}

PointSpread spread(const std::vector<Vec3>& X, const std::vector<int>& idx)
{
  PointSpread s;
  if(idx.empty())
    return s;
  for(int i : idx)
    s.centroid += X[i];
  s.centroid /= double(idx.size());
  Mat3 cov = Mat3::Zero();
  for(int i : idx)
  {
    const Vec3 d = X[i] - s.centroid;
    cov += d * d.transpose();
  }
  Eigen::SelfAdjointEigenSolver<Mat3> es(cov);
  s.variance = es.eigenvalues();
  s.axes = es.eigenvectors();
  return s;
}

Result solve(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Options& opt)
{
  Result res;
  const int N = int(std::min(X.size(), x.size()));
  res.mask.assign(N, 0);
  if(N < 6)
  {
    res.error = "at least 6 correspondences are needed";
    return res;
  }

  std::vector<int> all(N);
  std::iota(all.begin(), all.end(), 0);
  for(int i = 0; i < N; i++)
    if(!X[i].allFinite() || !x[i].allFinite())
    {
      res.error = "a correspondence is not finite";
      return res;
    }
  if(coplanar(X, all))
  {
    res.error = "the 3D points are coplanar: a projection with unknown intrinsics needs "
                "points off a single plane";
    return res;
  }

  std::vector<int> inl = all;
  if(opt.ransac)
  {
    std::mt19937 rng(opt.seed);
    std::uniform_int_distribution<int> pick(0, N - 1);
    std::vector<int> best;
    int maxIter = std::max(1, opt.ransacIterations);
    for(int it = 0; it < maxIter; it++)
    {
      std::vector<int> sample;
      while(int(sample.size()) < 6)
      {
        const int k = pick(rng);
        if(std::find(sample.begin(), sample.end(), k) == sample.end())
          sample.push_back(k);
      }
      Mat34 P;
      Model m;
      if(!dlt(X, x, sample, P) || !decompose(P, m))
        continue;
      std::vector<int> cur;
      for(int i = 0; i < N; i++)
        if(reprojectionError(m, X[i], x[i]) < opt.ransacThreshold)
          cur.push_back(i);
      if(cur.size() > best.size())
      {
        best = std::move(cur);
        const double w = double(best.size()) / N;
        const double pAll = std::pow(w, 6.);
        if(opt.ransacConfidence > 0. && pAll > 1e-12 && pAll < 1.)
        {
          const double need = std::min(
              1e8, std::log(1. - opt.ransacConfidence) / std::log(1. - pAll));
          maxIter = std::min(maxIter, int(std::ceil(need)) + it + 1);
        }
        else if(pAll >= 1.)
          break;
      }
    }
    if(best.size() < 6)
    {
      res.error = "RANSAC found no consensus";
      return res;
    }
    inl = std::move(best);
  }

  Mat34 P;
  if(!dlt(X, x, inl, P) || !decompose(P, res.model))
  {
    res.error = "degenerate correspondences";
    return res;
  }

  refine(res.model, X, x, inl, false, opt.maxIterations);
  if(opt.distortion)
    refine(res.model, X, x, inl, true, opt.maxIterations);

  if(opt.ransac)
  {
    std::vector<int> again;
    for(int i = 0; i < N; i++)
      if(reprojectionError(res.model, X[i], x[i]) < opt.ransacThreshold)
        again.push_back(i);
    if(again.size() >= 6 && again != inl)
    {
      inl = std::move(again);
      refine(res.model, X, x, inl, opt.distortion, opt.maxIterations);
    }
  }

  for(int i : inl)
    res.mask[i] = 1;
  res.inliers = int(inl.size());
  res.rms = rms(res.model, X, x, inl);
  res.ok = std::isfinite(res.rms) && res.model.fx > 0. && res.model.fy > 0.;
  if(!res.ok)
    res.error = "the refinement diverged or left a point behind the projector";
  return res;
}
namespace detail
{

bool poseDLT(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t)
{
  const int n = int(idx.size());
  if(n < 6)
    return false;
  const Eigen::Matrix4d T3 = normalisation<3>(X, idx);
  Eigen::MatrixXd A(2 * n, 12);
  for(int k = 0; k < n; k++)
  {
    const Eigen::Vector4d Xh = T3 * X[idx[k]].homogeneous();
    const double u = xn[idx[k]].x(), v = xn[idx[k]].y();
    A.row(2 * k) << Xh.transpose(), Eigen::RowVector4d::Zero(), -u * Xh.transpose();
    A.row(2 * k + 1) << Eigen::RowVector4d::Zero(), Xh.transpose(), -v * Xh.transpose();
  }
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullV);
  const auto& sv = svd.singularValues();
  if(!(sv(0) > 0.) || sv(10) < 1e-9 * sv(0))
    return false;
  const Eigen::VectorXd p = svd.matrixV().col(11);
  Mat34 P;
  P << p.segment<4>(0).transpose(), p.segment<4>(4).transpose(),
      p.segment<4>(8).transpose();
  P = P * T3;
  Mat3 M = P.leftCols<3>();
  if(M.determinant() < 0.)
  {
    P = -P;
    M = -M;
  }
  Eigen::JacobiSVD<Mat3> ms(M, Eigen::ComputeFullU | Eigen::ComputeFullV);
  const double scale = ms.singularValues().mean();
  if(!(scale > 1e-15))
    return false;
  R = ms.matrixU() * ms.matrixV().transpose();
  t = P.col(3) / scale;
  int front = 0;
  for(int i : idx)
    front += (R * X[i] + t).z() > 0. ? 1 : -1;
  if(front < 0)
    return false;
  return R.allFinite() && t.allFinite();
}

bool posePlanar(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t)
{
  const int n = int(idx.size());
  if(n < 4)
    return false;
  const auto sp = spread(X, idx);
  if(!(sp.variance(2) > 0.) || sp.variance(1) < 1e-8 * sp.variance(2))
    return false;
  const Vec3 c = sp.centroid;
  const Vec3 e1 = sp.axes.col(2).normalized();
  const Vec3 e3 = sp.axes.col(0).normalized();
  const Vec3 e2 = e3.cross(e1);

  std::vector<Vec2> plane(X.size());
  for(int i : idx)
    plane[i] = Vec2((X[i] - c).dot(e1), (X[i] - c).dot(e2));
  const Mat3 Tp = normalisation<2>(plane, idx);
  const Mat3 Tn = normalisation<2>(xn, idx);
  Eigen::MatrixXd A(2 * n, 9);
  for(int k = 0; k < n; k++)
  {
    const Vec3 a = Tp * plane[idx[k]].homogeneous();
    const Vec3 b = Tn * xn[idx[k]].homogeneous();
    const double u = b.x() / b.z(), v = b.y() / b.z();
    A.row(2 * k) << a.transpose(), Eigen::RowVector3d::Zero(), -u * a.transpose();
    A.row(2 * k + 1) << Eigen::RowVector3d::Zero(), a.transpose(), -v * a.transpose();
  }
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullV);
  const auto& sv = svd.singularValues();
  if(sv.size() >= 8 && !(sv(7) > 1e-10 * sv(0)))
    return false;
  const Eigen::VectorXd h = svd.matrixV().col(8);
  Mat3 H;
  H << h.segment<3>(0).transpose(), h.segment<3>(3).transpose(),
      h.segment<3>(6).transpose();
  H = Tn.inverse() * H * Tp;
  const double l = 2. / (H.col(0).norm() + H.col(1).norm());
  if(!std::isfinite(l))
    return false;
  Vec3 r1 = H.col(0) * l, r2 = H.col(1) * l, tp = H.col(2) * l;
  if(tp.z() < 0.)
  {
    r1 = -r1;
    r2 = -r2;
    tp = -tp;
  }
  Mat3 Rp;
  Rp.col(0) = r1;
  Rp.col(1) = r2;
  Rp.col(2) = r1.cross(r2);
  Eigen::JacobiSVD<Mat3> rs(Rp, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Rp = rs.matrixU() * rs.matrixV().transpose();
  if(Rp.determinant() < 0.)
    return false;
  Mat3 B;
  B.col(0) = e1;
  B.col(1) = e2;
  B.col(2) = e3;
  R = Rp * B.transpose();
  t = tp - R * c;
  return R.allFinite() && t.allFinite();
}
}

Result solvePose(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Model& known,
    const Options& opt)
{
  Result res;
  const int N = int(std::min(X.size(), x.size()));
  res.mask.assign(N, 0);
  std::vector<int> all(N);
  std::iota(all.begin(), all.end(), 0);
  if(N >= 2 && collinear(X, all))
  {
    res.error = "the 3D points are collinear";
    return res;
  }
  const bool planar = N >= 4 && coplanar(X, all);
  const int minimal = planar ? 4 : 6;
  if(N < minimal)
  {
    res.error = "at least 4 coplanar or 6 correspondences are needed";
    return res;
  }

  std::vector<Vec2> xn(N);
  for(int i = 0; i < N; i++)
    xn[i] = known.normalise(x[i]);

  auto estimate = [&](const std::vector<int>& idx, Model& m) {
    m = known;
    return planar ? detail::posePlanar(X, xn, idx, m.R, m.t)
                  : detail::poseDLT(X, xn, idx, m.R, m.t);
  };

  std::vector<int> inl = all;
  if(opt.ransac)
  {
    std::mt19937 rng(opt.seed);
    std::uniform_int_distribution<int> pick(0, N - 1);
    std::vector<int> best;
    int maxIter = std::max(1, opt.ransacIterations);
    for(int it = 0; it < maxIter; it++)
    {
      std::vector<int> sample;
      while(int(sample.size()) < minimal)
      {
        const int k = pick(rng);
        if(std::find(sample.begin(), sample.end(), k) == sample.end())
          sample.push_back(k);
      }
      Model m;
      if(!estimate(sample, m))
        continue;
      std::vector<int> cur;
      for(int i = 0; i < N; i++)
        if(reprojectionError(m, X[i], x[i]) < opt.ransacThreshold)
          cur.push_back(i);
      if(cur.size() > best.size())
      {
        best = std::move(cur);
        const double pAll = std::pow(double(best.size()) / N, double(minimal));
        if(opt.ransacConfidence > 0. && pAll > 1e-12 && pAll < 1.)
          maxIter = std::min(
              maxIter,
              int(std::ceil(std::min(
                  1e8, std::log(1. - opt.ransacConfidence) / std::log(1. - pAll))))
                  + it + 1);
        else if(pAll >= 1.)
          break;
      }
    }
    if(int(best.size()) < minimal)
    {
      res.error = "RANSAC found no consensus";
      return res;
    }
    inl = std::move(best);
  }

  if(!estimate(inl, res.model))
  {
    res.error = "degenerate correspondences";
    return res;
  }
  refine(res.model, X, x, inl, false, opt.maxIterations, true);

  if(opt.ransac)
  {
    std::vector<int> again;
    for(int i = 0; i < N; i++)
      if(reprojectionError(res.model, X[i], x[i]) < opt.ransacThreshold)
        again.push_back(i);
    if(int(again.size()) >= minimal && again != inl)
    {
      inl = std::move(again);
      refine(res.model, X, x, inl, false, opt.maxIterations, true);
    }
  }

  for(int i : inl)
    res.mask[i] = 1;
  res.inliers = int(inl.size());
  res.rms = rms(res.model, X, x, inl);
  res.ok = std::isfinite(res.rms);
  if(!res.ok)
    res.error = "the refinement diverged or left a point behind the projector";
  return res;
}
}

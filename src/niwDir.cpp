#include "niwDir.hpp"
#include "karcher.hpp"
#include <cmath>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/random/chi_squared_distribution.hpp>
#include <boost/random/normal_distribution.hpp>


template<typename T>
NIWDIR<T>::NIWDIR(const Matrix<T,Dynamic,Dynamic>& sigma, 
  const Matrix<T,Dynamic,Dynamic>& mu, T nu,  T kappa, boost::mt19937 &rndGen):
  Sigma_(sigma), mu_(mu), nu_(nu), kappa_(kappa), dim_(mu.size()), rndGen_(rndGen) //dim_ is dimParam defined in main.cpp
{
  muPos_ = mu_(seq(0, 1));
  SigmaPos_ = Sigma_(seq(0, 1), seq(0, 1));
  muDir_ = mu_(seq(2, 3));
  SigmaDir_ = Sigma_(2, 2);
};



template<typename T>
NIWDIR<T>::NIWDIR(const Matrix<T,Dynamic,1>& muPos, const Matrix<T,Dynamic,Dynamic>& SigmaPos,  
  const Matrix<T,Dynamic,1>& muDir, T SigmaDir,
  T nu, T kappa, T count, boost::mt19937 &rndGen):
  SigmaPos_(SigmaPos), SigmaDir_(SigmaDir), muPos_(muPos), muDir_(muDir), nu_(nu), kappa_(kappa), count_(count), rndGen_(rndGen) //dim_ is dimParam defined in main.cpp
{};



template<typename T>
NIWDIR<T>::~NIWDIR()
{};


template<typename T>
void NIWDIR<T>::getSufficientStatistics(const Matrix<T,Dynamic, Dynamic>& x_k)
{
  meanDir_ = karcherMean(x_k);
  meanPos_ = x_k(all, (seq(0, 1))).colwise().mean().transpose();  //!!!!later change the number 1 to accomodate for 3D data
 
  Matrix<T,Dynamic, Dynamic> x_k_mean;  //this is the value of each data point subtracted from the mean value calculated from the previous procedure
  x_k_mean = x_k.rowwise() - x_k.colwise().mean(); 
  ScatterPos_ = (x_k_mean.adjoint() * x_k_mean)(seq(0, 1), seq(0, 1)); //!!!!later change the number 1 to accomodate for 3D data
  ScatterDir_ = karcherScatter(x_k, meanDir_);  //!!! This is karcher scatter

  count_ = x_k.rows();
};


template<typename T>
NIWDIR<T> NIWDIR<T>::posterior(const Matrix<T,Dynamic, Dynamic>& x_k)
{
  getSufficientStatistics(x_k);
  return NIWDIR<T>(
    (kappa_*muPos_+ count_*meanPos_)/(kappa_+count_),
    SigmaPos_+ScatterPos_ + ((kappa_*count_)/(kappa_+count_))*(meanPos_-muPos_)*(meanPos_-muPos_).transpose(),
    meanDir_,
    SigmaDir_+ScatterDir_ + ((kappa_*count_)/(kappa_+count_))*pow(rie_log(meanDir_, muDir_).norm(), 2),
    nu_+count_,
    kappa_+count_,
    count_, 
    rndGen_);
};


template<class T>
NormalDir<T> NIWDIR<T>::samplePosteriorParameter(const Matrix<T,Dynamic, Dynamic>& x_k)
{
  NIWDIR<T> posterior = this ->posterior(x_k);
  return posterior.sampleParameter();
}


template<class T>
NormalDir<T> NIWDIR<T>::sampleParameter()
{
  int dim = 2;

  Matrix<T,Dynamic,1> meanPos(dim);
  Matrix<T,Dynamic,Dynamic> covPos(dim, dim);
  Matrix<T,Dynamic,1> meanDir(dim);
  T covDir;


  LLT<Matrix<T,Dynamic,Dynamic> > lltObj(SigmaPos_);
  Matrix<T,Dynamic,Dynamic> cholFacotor = lltObj.matrixL();

  Matrix<T,Dynamic,Dynamic> matrixA(dim, dim);
  matrixA.setZero();
  boost::random::normal_distribution<> gauss_;
  for (uint32_t i=0; i<dim; ++i)
  {
    boost::random::chi_squared_distribution<> chiSq_(nu_-i);
    matrixA(i,i) = sqrt(chiSq_(rndGen_)); 
    for (uint32_t j=i+1; j<dim; ++j)
    {
      matrixA(j, i) = gauss_(rndGen_);
    }
  }
  covPos = matrixA.inverse()*cholFacotor;
  covPos = covPos.transpose()*covPos;


  lltObj.compute(covPos);
  cholFacotor = lltObj.matrixL();

  for (uint32_t i=0; i<dim; ++i)
    meanPos[i] = gauss_(rndGen_);
  meanPos = cholFacotor * meanPos / sqrt(kappa_) + muPos_;


  // covDir = SigmaDir_;
  boost::random::chi_squared_distribution<> chiSq_(nu_);
  T inv_chi_sqrd = 1 / chiSq_(rndGen_);
  covDir = inv_chi_sqrd * SigmaDir_ / count_ * nu_;


  meanDir = muDir_; //the mean location of the normal distribution is sampled from the posterior mu of niw which is the data mean of the data points in cluster

  return NormalDir<T>(meanPos, covPos, meanDir, covDir, rndGen_);
};




template class NIWDIR<double>;

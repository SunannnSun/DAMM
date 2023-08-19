#pragma once

#include <Eigen/Dense>
#include <boost/random/mersenne_twister.hpp>
#include "normal.hpp"
#include <memory>


using namespace Eigen;

template<typename T>
class NIWDIR;


template<typename T>
class NIW
{
    public:
        NIW(const MatrixXd &sigma, const VectorXd &mu, T nu, T kappa, boost::mt19937 &rndGen, int base);
        NIW(const MatrixXd &sigma, const VectorXd &mu, T nu, T kappa, boost::mt19937 &rndGen);
        NIW(){};
        ~NIW(){};

        NIW<T> posterior(const Matrix<T,Dynamic, Dynamic> &x_k);
        void getSufficientStatistics(const Matrix<T,Dynamic, Dynamic> &x_k);
        Normal<T> samplePosteriorParameter(const Matrix<T,Dynamic, Dynamic> &x_k);
        Normal<T> sampleParameter();

 
    private:
        boost::mt19937 rndGen_;


        Matrix<T,Dynamic,Dynamic> sigma_;
        Matrix<T,Dynamic,1> mu_;
        T nu_,kappa_;
        uint32_t dim_;


        Matrix<T,Dynamic,Dynamic> scatter_;
        Matrix<T,Dynamic,1> mean_;
        uint16_t count_;
};






/*---------------------------------------------------*/
//-------------------Inactive Methods-----------------
/*---------------------------------------------------*/   
// T logPostPredProb(const Matrix<T,Dynamic,1>& x_i, const Matrix<T,Dynamic, Dynamic>& x_k);
// T logPredProb(const Matrix<T,Dynamic,1> &x_i);
// T predProb(const Matrix<T,Dynamic,1> &x_i);

/*---------------------------------------------------*/
//-------------------Inactive Members-----------------
/*---------------------------------------------------*/
// std::shared_ptr<NIWDIR<T>> NIWDIR_ptr;
// Matrix<T,Dynamic,Dynamic> SigmaPos_;
// T SigmaDir_;
// Matrix<T,Dynamic,1> muPos_;
// Matrix<T,Dynamic,1> muDir_;
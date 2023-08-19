#pragma once

#include <Eigen/Dense>
#include <boost/random/mersenne_twister.hpp>
#include "normalDir.hpp"
#include "niw.hpp"
#include <memory>



using namespace Eigen;


template<typename T>
class NIWDIR
{
    public:
        NIWDIR(const Matrix<T,Dynamic,Dynamic>& sigma, const Matrix<T,Dynamic,Dynamic>& mu, T nu, T kappa, T sigmaDir,
        boost::mt19937 &rndGen);
        NIWDIR(const Matrix<T,Dynamic,Dynamic>& sigmaPos, const Matrix<T,Dynamic,1>& muPos, T nu, T kappa, T sigmaDir, 
        const Matrix<T,Dynamic,1>& muDir, T count, boost::mt19937 &rndGen);        
        NIWDIR(){};
        ~NIWDIR(){};


        void getSufficientStatistics(const Matrix<T,Dynamic, Dynamic>& x_k);
        NIWDIR<T> posterior(const Matrix<T,Dynamic, Dynamic>& x_k);
        NormalDir<T> samplePosteriorParameter(const Matrix<T,Dynamic, Dynamic> &x_k);
        NormalDir<T> sampleParameter();
    
    public:
        std::shared_ptr<NIW<T>> NIW_ptr;

    private:
        boost::mt19937 rndGen_;

        // Hyperparameters
        Matrix<T,Dynamic,Dynamic> sigmaPos_;
        Matrix<T,Dynamic,1> muPos_;
        T nu_,kappa_;
        Matrix<T,Dynamic,1> muDir_;
        T sigmaDir_;
        uint32_t dim_;


        // Sufficient statistics
        Matrix<T,Dynamic,Dynamic> scatterPos_;
        T scatterDir_;
        Matrix<T,Dynamic,1> meanPos_;
        Matrix<T,Dynamic,1> meanDir_;
        uint16_t count_;
};




/*---------------------------------------------------*/
//-------------------Inactive Methods-----------------
/*---------------------------------------------------*/ 
// T logPostPredProb(const Vector<T,Dynamic>& x_i, const Matrix<T,Dynamic, Dynamic>& x_k);
// T logPredProb(const Matrix<T,Dynamic,1>& x_i);
// T predProb(const Matrix<T,Dynamic,1>& x_i);




/*---------------------------------------------------*/
//-------------------Inactive Members-----------------
/*---------------------------------------------------*/  
// Matrix<T,Dynamic,Dynamic> sigma_;
// Matrix<T,Dynamic,1> mu_;
// Matrix<T,Dynamic,Dynamic> SigmaPos_;
// T SigmaDir_;

#include <iostream>
#include <limits>
#include <boost/random/uniform_01.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/gamma_distribution.hpp>
#include <boost/random/variate_generator.hpp>


#include "dpmm.hpp"
#include "dpmmDir.hpp"
#include "niw.hpp"
#include "niwDir.hpp"


template <class dist_t> 
DPMMDIR<dist_t>::DPMMDIR(const MatrixXd &x, int init_cluster, double alpha, const dist_t &H, const boost::mt19937 &rndGen)
: alpha_(alpha), H_(H), rndGen_(rndGen), x_(x), N_(x.rows())
{
  VectorXi z(x.rows());

  if (init_cluster == 1) z.setZero();
  else if (init_cluster > 1)  {
    boost::random::uniform_int_distribution<> uni_(0, init_cluster-1);
    for (int ii=0; ii<N_; ++ii) z[ii] = uni_(rndGen_); 
  }
  else  { 
    cout<< "Number of initial clusters not supported yet" << endl;
    exit(1);
  }

  z_ = z;
  K_ = z_.maxCoeff() + 1; 
  this ->updateIndexLists();
};



template <class dist_t> 
DPMMDIR<dist_t>::DPMMDIR(const MatrixXd& x, const VectorXi& z, const vector<int> indexList, const double alpha, const dist_t& H, boost::mt19937 &rndGen)
: alpha_(alpha), H_(H), rndGen_(rndGen), x_(x), N_(x.rows()), z_(z), K_(z.maxCoeff() + 1), indexList_(indexList)
{
  
  // Initialize the data points of given indexList by randomly assigning them into one of the two clusters


  vector<int> indexList_i;
  vector<int> indexList_j;
  int z_i = z_.maxCoeff() + 1;
  int z_j = z_[indexList[0]];


  boost::random::uniform_int_distribution<> uni_01(0, 1);
  for (int ii = 0; ii<indexList_.size(); ++ii)
  {
    if (uni_01(rndGen_) == 0) {
        indexList_i.push_back(indexList_[ii]);
        z_[indexList_[ii]] = z_i;
      }
    else  {
        indexList_j.push_back(indexList_[ii]);
        z_[indexList_[ii]] = z_j;
      }
  }
  indexLists_.push_back(indexList_i);
  indexLists_.push_back(indexList_j);
};



template <class dist_t> 
void DPMMDIR<dist_t>::sampleCoefficients()
{
  VectorXd Pi(K_);
  for (uint32_t kk=0; kk<K_; ++kk)
  {
    boost::random::gamma_distribution<> gamma_(indexLists_[kk].size(), 1);
    Pi(kk) = gamma_(rndGen_);
  }
  Pi_ = Pi / Pi.sum();
}


template <class dist_t> 
void DPMMDIR<dist_t>::sampleParameters()
{ 
  parameters_.clear();
  components_.clear();

  for (uint32_t kk=0; kk<K_; ++kk)
  {
    parameters_.push_back(H_.posterior(x_(indexLists_[kk], all)));     
    components_.push_back(parameters_[kk].sampleParameter());          
  }
}


template <class dist_t> 
void DPMMDIR<dist_t>::sampleCoefficientsParameters()
{ 
  parameters_.clear();
  components_.clear();
  VectorXd Pi(K_);

  for (uint32_t kk=0; kk<K_; ++kk)
  {
    boost::random::gamma_distribution<> gamma_(indexLists_[kk].size(), 1);
    Pi(kk) = gamma_(rndGen_);
    parameters_.push_back(H_.posterior(x_(indexLists_[kk], all)));
    components_.push_back(parameters_[kk].sampleParameter());
  }

  Pi_ = Pi / Pi.sum();
}



template <class dist_t> 
void DPMMDIR<dist_t>::sampleLabels()
{
  double logLik = 0;
  #pragma omp parallel for num_threads(6) schedule(static) private(rndGen_)
  for(uint32_t ii=0; ii<N_; ++ii)
  {
    VectorXd prob(K_);
    double logLik_i = 0;

    for (uint32_t kk=0; kk<K_; ++kk)
    { 
      double logProb =  components_[kk].logProb(x_(ii, all));
      prob[kk] = log(Pi_[kk]) + logProb;
      logLik_i += Pi_[kk] * exp(logProb);
    }
    logLik += log(logLik_i);
   
    prob = (prob.array()-(prob.maxCoeff() + log((prob.array() - prob.maxCoeff()).exp().sum()))).exp().matrix();
    prob = prob / prob.sum();
    for (uint32_t kk = 1; kk < prob.size(); ++kk) prob[kk] = prob[kk-1]+ prob[kk];
    
    boost::random::uniform_01<> uni_;   
    double uni_draw = uni_(rndGen_);
    uint32_t kk = 0;
    while (prob[kk] < uni_draw) kk++;
    z_[ii] = kk;
  } 
  logLogLik_.push_back(logLik);
}


template <class dist_t> 
int DPMMDIR<dist_t>::splitProposal(const vector<int> &indexList)
{
  VectorXi z_launch = z_;
  VectorXi z_split = z_;
  uint32_t z_split_i = z_split.maxCoeff() + 1;
  uint32_t z_split_j = z_split[indexList[0]];

  
  NIW<double> H_NIW = * H_.NIW_ptr;  
  DPMM<NIW<double>> dpmm_split(x_, z_launch, indexList, alpha_, H_NIW, rndGen_);

  for (int tt=0; tt<50; ++tt)
  {
    if (dpmm_split.indexLists_[0].size()==1 || dpmm_split.indexLists_[1].size() ==1 || dpmm_split.indexLists_[0].empty()==true || dpmm_split.indexLists_[1].empty()==true)
    {
      // std::cout << "Component " << z_split_j <<": Split proposal Rejected" << std::endl;
      return 1;
    }
    dpmm_split.sampleCoefficientsParameters(indexList);
    dpmm_split.sampleLabels(indexList);
    // std::cout << "H" << std::endl;
    // dpmm_split.sampleLabelsCollapsed(indexList);
  }

  vector<int> indexList_i = dpmm_split.indexLists_[0];
  vector<int> indexList_j = dpmm_split.indexLists_[1];


  
  double logAcceptanceRatio = 0;
  // logAcceptanceRatio -= dpmm_split.logTransitionProb(indexList_i, indexList_j);
  // logAcceptanceRatio += dpmm_split.logPosteriorProb(indexList_i, indexList_j);;
  // if (logAcceptanceRatio < 0) 
  // {
  //   std::cout << "Component " << z_split_j <<": Split proposal Rejected with Log Acceptance Ratio " << logAcceptanceRatio << std::endl;
  //   return 1;
  // }
  
  for (int i = 0; i < indexList_i.size(); ++i)
  {
    z_split[indexList_i[i]] = z_split_i;
  }
  for (int i = 0; i < indexList_j.size(); ++i)
  {
    z_split[indexList_j[i]] = z_split_j;
  }

  z_ = z_split;
  // z_ = dpmm_split.z_;
  K_ += 1;
  logNum_.push_back(K_);
  // this -> updateIndexLists();
  std::cout << "Component " << z_split_j <<": Split proposal Aceepted with Log Acceptance Ratio " << logAcceptanceRatio << std::endl;
  
  return 0;
}


template <class dist_t> 
int DPMMDIR<dist_t>::mergeProposal(const vector<int> &indexList_i, const vector<int> &indexList_j)
{
  // VectorXi z_launch = z_;
  // VectorXi z_merge = z_;
  // uint32_t z_merge_i = z_merge[indexList_i[0]];
  // uint32_t z_merge_j = z_merge[indexList_j[0]];

  // vector<int> indexList;
  // indexList.reserve(indexList_i.size() + indexList_j.size() ); // preallocate memory
  // indexList.insert( indexList.end(), indexList_i.begin(), indexList_i.end() );
  // indexList.insert( indexList.end(), indexList_j.begin(), indexList_j.end() );

  // NIW<double> NIW_dist = * H_.NIW_;
  // DPMM<NIW<double>> dpmm_merge(x_, z_launch, indexList, alpha_, NIW_dist, rndGen_);

  // // DPMMDIR<NIW<double>> dpmm_merge(x_, z_launch, indexList, alpha_, NIW_dist, rndGen_);

  // // DPMMDIR<dist_t> dpmm_merge(x_, z_launch, indexList, alpha_, H_, rndGen_);
  // for (int tt=0; tt<50; ++tt)
  // {    
  //   if (dpmm_merge.indexLists_[0].size()==0 || dpmm_merge.indexLists_[1].size() ==0  || dpmm_merge.indexLists_[0].empty()==true || dpmm_merge.indexLists_[1].empty()==true)
  //   {
  //     // double logAcceptanceRatio = 0;
  //     // logAcceptanceRatio += log(dpmm_merge.transitionProb(indexList_i, indexList_j));
  //     // logAcceptanceRatio -= dpmm_merge.logPosteriorProb(indexList_i, indexList_j);;

  //     // std::cout << logAcceptanceRatio << std::endl;
  //     for (int i = 0; i < indexList_i.size(); ++i) z_merge[indexList_i[i]] = z_merge_j;
  //     z_ = z_merge;
  //     this -> reorderAssignments();
  //     std::cout << "Component " << z_merge_j << "and" << z_merge_i <<": Merge proposal Aceepted" << std::endl;
  //     return 0;
  //   };
  //   // dpmm_merge.sampleLabelsCollapsed(indexList);
  //   dpmm_merge.sampleCoefficientsParameters(indexList);
  //   dpmm_merge.sampleLabels(indexList);
  // }

  // return 1;

  // double logAcceptanceRatio = 0;
  // logAcceptanceRatio += log(dpmm_merge.transitionProb(indexList_i, indexList_j));
  // logAcceptanceRatio -= dpmm_merge.logPosteriorProb(indexList_i, indexList_j);;

  // std::cout << logAcceptanceRatio << std::endl;
  // if (logAcceptanceRatio > 0)
  // {
  //   for (int i = 0; i < indexList_i.size(); ++i) z_merge[indexList_i[i]] = z_merge_j;
  //   z_ = z_merge;
  //   this -> reorderAssignments();
  //   std::cout << "Component " << z_merge_j << "and" << z_merge_i <<": Merge proposal Aceepted" << std::endl;
  //   return 0;
  // }
  // std::cout << "Component " << z_merge_j << "and" << z_merge_i <<": Merge proposal Rejected" << std::endl;
  return 1;
}



template <class dist_t> 
void DPMMDIR<dist_t>::sampleCoefficientsParameters(vector<int> indexList)
{
  parameters_.clear();
  components_.clear();

  parameters_.push_back(H_.posterior(x_(indexLists_[0], all)));
  parameters_.push_back(H_.posterior(x_(indexLists_[1], all)));
  components_.push_back(parameters_[0].sampleParameter());
  components_.push_back(parameters_[1].sampleParameter());
  

  VectorXd Pi(2);
  for (uint32_t kk=0; kk<2; ++kk)
  {
    boost::random::gamma_distribution<> gamma_(indexLists_[kk].size(), 1);
    Pi(kk) = gamma_(rndGen_);
  }
  Pi_ = Pi / Pi.sum();
}


template <class dist_t> 
void DPMMDIR<dist_t>::sampleLabels(vector<int> indexList)
{
  indexLists_.clear();
  vector<int> indexList_i;
  vector<int> indexList_j;

  boost::random::uniform_01<> uni_;    
  for(uint32_t ii=0; ii<indexList.size(); ++ii)
  {
    VectorXd prob(2);

    for (uint32_t kk=0; kk<2; ++kk)
    {
      prob[kk] = log(Pi_[kk]) + components_[kk].logProb(x_(indexList[ii], all)); 
    }

    prob = (prob.array()-(prob.maxCoeff() + log((prob.array() - prob.maxCoeff()).exp().sum()))).exp().matrix();
    prob = prob / prob.sum();
    
    double uni_draw = uni_(rndGen_);
    if (uni_draw < prob[0]) indexList_i.push_back(indexList[ii]);
    else indexList_j.push_back(indexList[ii]);
  }

  indexLists_.push_back(indexList_i);
  indexLists_.push_back(indexList_j);
}



template <class dist_t>
void DPMMDIR<dist_t>::reorderAssignments()  //mainly called after clusters vanish during parallel sampling
{ 

  vector<uint8_t> rearrange_list;
  for (uint32_t ii=0; ii<N_; ++ii)
  {
    if (rearrange_list.empty()) rearrange_list.push_back(z_[ii]);
    vector<uint8_t>::iterator it;
    it = find (rearrange_list.begin(), rearrange_list.end(), z_[ii]);
    if (it == rearrange_list.end())
    {
      rearrange_list.push_back(z_[ii]);
      z_[ii] = rearrange_list.size() - 1;
    }
    else if (it != rearrange_list.end())
    {
      int index = it - rearrange_list.begin();
      z_[ii] = index;
    }
  }
  K_ = z_.maxCoeff() + 1;
  logNum_.push_back(K_);
}


template <class dist_t>
vector<vector<int>> DPMMDIR<dist_t>::getIndexLists()
{
  this ->updateIndexLists();
  return indexLists_;
}


template <class dist_t>
void DPMMDIR<dist_t>::updateIndexLists()
{
  vector<vector<int>> indexLists(K_);
  for (uint32_t ii = 0; ii<N_; ++ii) 
    indexLists[z_[ii]].push_back(ii); 
  
  indexLists_ = indexLists;
}


template <class dist_t> 
vector<vector<int>> DPMMDIR<dist_t>::computeSimilarity()
{
  // std::cout << "Compute similarity" << std::endl;
  int num_comp = K_;
  vector<vector<int>> indexLists = this-> getIndexLists();
  assert(indexLists.size()==num_comp);
  vector<MatrixXd>       muLists;

  for (int kk=0; kk< num_comp; ++kk)
  {
    MatrixXd x_k = x_(indexLists[kk],  seq(0, (x_.cols()/2)-1));
    // std::cout << x_k.colwise().mean() << std::endl;
    muLists.push_back(x_k.colwise().mean().transpose());
  }

  MatrixXd similarityMatrix = MatrixXd::Constant(num_comp, num_comp, numeric_limits<float>::infinity());
  // std::cout << similarityMatrix << std::endl;
  
  for (int ii=0; ii<num_comp; ++ii)
      for (int jj=ii+1; jj<num_comp; ++jj)
          similarityMatrix(ii, jj) = (muLists[ii] - muLists[jj]).norm();
  // std::cout << similarityMatrix<< std::endl;

  MatrixXd similarityMatrix_flattened;
  similarityMatrix_flattened = similarityMatrix.transpose(); 
  similarityMatrix_flattened.resize(1, (similarityMatrix.rows() * similarityMatrix.cols()) );  

  // std::cout << similarityMatrix_flattened<< std::endl;

  Eigen::MatrixXf::Index min_index;
  similarityMatrix_flattened.row(0).minCoeff(&min_index);
  // std::cout << similarityMatrix_flattened.row(0).minCoeff(&min_index) << std::endl;
  // std::cout << min_index << std::endl;

  int merge_i;
  int merge_j;
  int min_index_int = min_index;

  merge_i = min_index_int / num_comp;
  merge_j = min_index_int % num_comp;

  // std::cout << merge_i << std::endl;
  // std::cout << merge_j << std::endl;

  vector<vector<int>> merge_indexLists;

  merge_indexLists.push_back(indexLists[merge_i]);
  merge_indexLists.push_back(indexLists[merge_j]);

 return merge_indexLists;
}


template class DPMMDIR<NIWDIR<double>>;



/*---------------------------------------------------*/
//Following class methods are currently not being used 
/*---------------------------------------------------*/

/*

template <class dist_t> 
void DPMMDIR<dist_t>::sampleLabelsCollapsed(vector<int> indexList)
{
  int dimPos;
  if (x_.cols()==4) dimPos=1;
  else if (x_.cols()==6) dimPos=2;
  int index_i = z_[indexLists_[0][0]];
  int index_j = z_[indexLists_[1][0]];


  boost::random::uniform_01<> uni_;
  vector<int> indexList_i;
  vector<int> indexList_j;

  // #pragma omp parallel for num_threads(4) schedule(static) private(rndGen_)
  for(int i=0; i<indexList.size(); ++i)
  {
    VectorXd x_i;
    x_i = x_(indexList[i], seq(0,dimPos)); //current data point x_i from the index_list
    VectorXd prob(2);

    for (int ii=0; ii < indexList.size(); ++ii)
    {
      if (z_[indexList[ii]] == index_i && ii!=i) indexList_i.push_back(indexList[ii]);
      else if (z_[indexList[ii]] == index_j && ii!=i) indexList_j.push_back(indexList[ii]);
    }

    if (indexList_i.empty()==true || indexList_j.empty()==true)
    {
      indexLists_.clear();
      indexLists_.push_back(indexList_i);
      indexLists_.push_back(indexList_j);
      return;
    } 

    prob[0] = log(indexList_i.size()) + H_.logPosteriorProb(x_i, x_(indexList_i, seq(0,dimPos))); 
    prob[1] = log(indexList_j.size()) + H_.logPosteriorProb(x_i, x_(indexList_j, seq(0,dimPos))); 

    double prob_max = prob.maxCoeff();
    prob = (prob.array()-(prob_max + log((prob.array() - prob_max).exp().sum()))).exp().matrix();
    prob = prob / prob.sum();
    for (uint32_t ii = 1; ii < prob.size(); ++ii)
    {
      prob[ii] = prob[ii-1]+ prob[ii];
    }
    double uni_draw = uni_(rndGen_);
    if (uni_draw < prob[0]) z_[indexList[i]] = index_i;
    else z_[indexList[i]] = index_j;
    
    indexList_i.clear();
    indexList_j.clear();
  }


  for (int i=0; i < indexList.size(); ++i)
  {
    if (z_[indexList[i]] == index_i) indexList_i.push_back(indexList[i]);
    else if (z_[indexList[i]] == index_j)indexList_j.push_back(indexList[i]);
  }
  indexLists_.clear();
  indexLists_.push_back(indexList_i);
  indexLists_.push_back(indexList_j);
}


template <class dist_t> 
double DPMMDIR<dist_t>::transitionProb(vector<int> indexList_i, vector<int> indexList_j)
{
  double transitionProb = 1;

  for (uint32_t ii=0; ii < indexList_i.size(); ++ii)
  {
    transitionProb *= Pi_(0) * components_[0].prob(x_(indexList_i[ii], all))/
    (Pi_(0) * components_[0].prob(x_(indexList_i[ii], all)) + Pi_(1) *  components_[1].prob(x_(indexList_i[ii], all)));
  }

  for (uint32_t ii=0; ii < indexList_j.size(); ++ii)
  {
    transitionProb *= Pi_(0) * components_[0].prob(x_(indexList_j[ii], all))/
    (Pi_(0) * components_[0].prob(x_(indexList_j[ii], all)) + Pi_(1) *  components_[1].prob(x_(indexList_j[ii], all)));
  }
  
  // std::cout << transitionProb << std::endl;

  return transitionProb;
}


template <class dist_t> 
double DPMMDIR<dist_t>::logTransitionProb(vector<int> indexList_i, vector<int> indexList_j)
{
  double logTransitionProb = 0;

  for (uint32_t ii=0; ii < indexList_i.size(); ++ii)
  {
    logTransitionProb += log(Pi_(0) * components_[0].prob(x_(indexList_i[ii], all))) -
    log(Pi_(0) * components_[0].prob(x_(indexList_i[ii], all)) + Pi_(1) *  components_[1].prob(x_(indexList_i[ii], all)));
  }

  for (uint32_t ii=0; ii < indexList_j.size(); ++ii)
  {
    logTransitionProb += log(Pi_(0) * components_[0].prob(x_(indexList_j[ii], all))) -
    log(Pi_(0) * components_[0].prob(x_(indexList_j[ii], all)) + Pi_(1) *  components_[1].prob(x_(indexList_j[ii], all)));
  }
  
  // std::cout << transitionProb << std::endl;

  return logTransitionProb;
}


template <class dist_t>
double DPMMDIR<dist_t>::logPosteriorProb(vector<int> indexList_i, vector<int> indexList_j)
{
  vector<int> indexList;
  indexList.reserve(indexList_i.size() + indexList_j.size() ); // preallocate memory
  indexList.insert( indexList.end(), indexList_i.begin(), indexList_i.end() );
  indexList.insert( indexList.end(), indexList_j.begin(), indexList_j.end() );

  NormalDir<double> parameter_ij = H_.posterior(x_(indexList, all)).sampleParameter();
  NormalDir<double> parameter_i  = H_.posterior(x_(indexList_i, all)).sampleParameter();
  NormalDir<double> parameter_j  = H_.posterior(x_(indexList_j, all)).sampleParameter();

  double logPosteriorRatio = 0;
  for (uint32_t ii=0; ii < indexList_i.size(); ++ii)
  {
    logPosteriorRatio += log(indexList_i.size()) + parameter_i.logProb(x_(indexList_i[ii], all)) ;
    logPosteriorRatio -= log(indexList.size()) - parameter_ij.logProb(x_(indexList_i[ii], all));
  }
  for (uint32_t jj=0; jj < indexList_j.size(); ++jj)
  {
    logPosteriorRatio += log(indexList_j.size()) + parameter_j.logProb(x_(indexList_j[jj], all)) ;
    logPosteriorRatio -= log(indexList.size()) - parameter_ij.logProb(x_(indexList_j[jj], all));
  }

  return logPosteriorRatio;
}

*/
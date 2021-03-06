/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
* Author: Alex Teichman
*********************************************************************/

#ifndef MULTIBOOSTER_H
#define MULTIBOOSTER_H

#include <limits>

#include <iomanip>
#include <Eigen/Core>

#include <Eigen/Cholesky>
#include <Eigen/LU>
#include <eigen_extensions/eigen_extensions.h>

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <stdlib.h>
#include <stdint.h>
#include <sstream>
#include <fstream>
#include <float.h>
#include <math.h>
#include <cmath>
#include <string>
#include <algorithm>

#include <boost/shared_ptr.hpp>
#include <name_mapping/name_mapping.h>
#include <performance_statistics/performance_statistics.h>
#include <qpOASES/QProblem.hpp>
#include <pipeline/pipeline.h>
#include <unconstrained_optimization/common_functions.h>
#include <unconstrained_optimization/unconstrained_optimization.h>

#include <bag_of_tricks/high_res_timer.h>

#define CLASSIFIER_VERSION "#MULTIBOOSTER CLASSIFIER LOG v0.4"
#define DATASET_VERSION "#MULTIBOOSTER DATASET LOG v0.1"
#define WC_VERSION 0

/* class HighResTimer { */
/*  public: */
/*   timeval start_; */
/*   timeval end_; */
  
/*   void start() {gettimeofday(&start_, NULL);}  */
/*   void stop() {gettimeofday(&end_, NULL);} */
/*   double getMicroSec() {return 1e6*(end_.tv_sec - start_.tv_sec) + (end_.tv_usec - start_.tv_usec);} */
/*   double getMilliSec() {return getMicroSec() / 1000.;} */
/*   double getSec() {return getMilliSec() / 1000.;} */
/* }; */

class WeakClassifier {
 public:
  size_t serialization_version_;
  //! The id of the feature this weak classifier is concerned with.
  size_t fsid_;
  //! Center of the weak classifier hypersphere.
  Eigen::VectorXf center_;
  //! center_.dot(center_), cached for speed.
  float length_squared_; 
  //! radius squared.
  float theta_;
  //! The a_t^c's, i.e. the values of the weak classifier responses.
  Eigen::VectorXf vals_;
  //! to identify which was learned first, second, third, etc.
  size_t id_;

  WeakClassifier(const WeakClassifier& wc);
  WeakClassifier();
  WeakClassifier(std::istream& is);
  std::string serialize();
  std::string status(const NameMapping& class_map, const NameMapping& feature_map) const;
  //! NameTranslator(thisMapping, destinationMapping)
  void translate(const NameTranslator& class_translator, const NameTranslator& fs_translator);
  bool compare(const WeakClassifier& wc, bool verbose=false);
};  


typedef struct {
  Eigen::VectorXf* vector;
  float length_squared;
} descriptor;

class Object {
 public:
  //! -1 = background, -2 = unlabeled.
  int label_;
  std::vector<descriptor> descriptors_;
  
  std::string status(const NameMapping& class_map, const NameMapping& feature_map, bool showDescriptors=true);
  Object() {}
  Object(std::istream& in);
  ~Object() {
    for(size_t i=0; i<descriptors_.size(); ++i) {
      if(descriptors_[i].vector)
	delete descriptors_[i].vector;
    }
  }

  //! Allocates new memory for descriptors.
  Object(const Object& o);

  //! NameTranslator(thisMapping, destinationMapping)
  void translate(const NameTranslator& class_translator, const NameTranslator& fs_translator);
  void serialize(std::ostream& out) const;
  //! Strict equality.  Order of descriptors matters.
  bool equals(const Object& other) const;
  uint64_t numBytes() const; 

private:
  void deserialize(std::istream& in);
};

//! Computes the squared euclidean distance assuming that a'a and b'b have been cached. 
inline float fastEucSquared(const Eigen::VectorXf& a, const Eigen::VectorXf& b, float ata, float btb) {
  assert(a.rows() == b.rows());
  if(a.rows() != b.rows()) {
    std::cerr << "Mixing feature spaces." << std::endl;
    exit(0);
  }
  float r = 0;
  for(int i=0; i<a.rows(); i++) {
    r += b.coeff(i) * a.coeff(i);
  }
  float result = ata + btb - 2*r; 
  if(result < 0) //Rounding errors may cause this to be slightly negative.
    result = 0;
  return result;
}

inline float euc(const Eigen::MatrixXf& a, const Eigen::MatrixXf& b)
{
  assert(a.cols() == 1 && b.cols() == 1);
  assert(a.rows() == b.rows());

  float r = 0;
  float tmp = 0;
  for(int i=0; i<a.rows(); i++) {
    tmp = a.coeff(i,0) - b.coeff(i,0);
    r += tmp * tmp;
  }

  //assert(fabs((float)((a-b).NormFrobenius() - sqrt(r))) < 1e-3);  
  return sqrt(r);

  //Slow
/*   Eigen::MatrixXf c = (a - b).cwise().pow(2); */
/*   return sqrt(c.sum()); */
}

class WeakClassifierTree {
 public:
  ~WeakClassifierTree();
  //! User-friendly constructor.
  WeakClassifierTree(const std::vector<WeakClassifier*>& pwcs, size_t recursion_limit);
  //! Used for recursively building the tree.
  WeakClassifierTree(const std::vector<WeakClassifier*>& pwcs, const std::vector<WeakClassifier*>& consider, size_t recursion_limit, size_t level,
	 const std::vector<Eigen::VectorXf>& region_a, const std::vector<float>& region_b);
  void query(const Eigen::VectorXf& sample, float len_sq,
	     std::vector<WeakClassifier*>* activated, int* num_euc = NULL, int* num_dot = NULL);

 private:
  //! Weak classifiers that have centers inside this region.
  std::vector<WeakClassifier*> pwcs_;
  //! Weak classifiers with centers outside the region but that have some part of their hypersphere in this region.
  std::vector<WeakClassifier*> consider_;
  //! pwcs_ concatenated with consider_
  std::vector<WeakClassifier*> all_;
  //! Defines the split for this region, if there is one.
  Eigen::VectorXf a_;
  //! Defines the split for this region, if there is one.
  float b_;
  //! Defines this region. Empty vector = the entire space.
  std::vector<Eigen::VectorXf> region_a_;
  //! Defines this region.  Empty vector = the entire space.
  std::vector<float> region_b_;
  //! Cols are the pwcs_ centers.
  Eigen::MatrixXf X_; 
  //! The left child region is all x for a_'x <= b_
  WeakClassifierTree* lchild_;
  //! The right child region is all x for a_'x >= b_, or -a_'x <= -b_
  WeakClassifierTree* rchild_;
  //! Maximum depth allowed for this tree.
  size_t recursion_limit_;
  //! The depth at which this region lives.  Root = 0.
  size_t depth_;
  //! Max number of weak classifiers to call a region a leaf.
  size_t max_wcs_;
  float thresh_;
  bool is_leaf_;
  bool verbose_;

  void makeLeaf();
  void growTree();
  
};

//! Research version.
class WCTree {
 public:
  //! Weak classifiers that have centers inside this region.
  std::vector<WeakClassifier*> pwcs_;
  //! Weak classifiers with centers outside the region but that have some part of their hypersphere in this region.
  std::vector<WeakClassifier*> consider_;
  //! pwcs_ concatenated with consider_, (TODO) in descending order based on which exclude the most others.
  std::vector<WeakClassifier*> all_;
  //! Defines the split for this region, if there is one.
  Eigen::VectorXf a_;
  //! Defines the split for this region, if there is one.
  float b_;
  //! Defines this region. Empty vector = the entire space.
  std::vector<Eigen::VectorXf> region_a_;
  //! Defines this region.  Empty vector = the entire space.
  std::vector<float> region_b_;
  //! The projection of each weak classifier in the consider list for each split. consider_projections_[i][j] = the value of the projection of weak classifier j on split i.
  std::vector< std::vector<float> > consider_projections_;
  //! The projection of each weak classifier in the wc list for each split. pwcs_projections_[i][j] = the value of the projection of weak classifier j on split i.
  std::vector< std::vector<float> > pwcs_projections_;
  //! If weak classifier all_[i] is activated, then all_[require_[i][j]] for all j must also be activated.
  std::vector< std::vector<size_t> > require_;
  //! If weak classifier all_[i] is activated, then all_[exclude_[i][j]] for all j cannot be activated.
  std::vector< std::vector<size_t> > exclude_;

  
  //! Cols are the pwcs_ centers.
  Eigen::MatrixXf X_; 
  //! The left child region is all x for a_'x <= b_
  WCTree* lchild_;
  //! The right child region is all x for a_'x >= b_, or -a_'x <= -b_
  WCTree* rchild_;
  size_t recursion_limit_;
  size_t level_;
  size_t max_wcs_;
  float thresh_;
  bool is_leaf_;
  bool verbose_;
  
  ~WCTree();
  //! User-friendly constructor.
  WCTree(const std::vector<WeakClassifier*>& pwcs, size_t recursion_limit);
  //! Used for recursively building the tree.
  WCTree(const std::vector<WeakClassifier*>& pwcs, const std::vector<WeakClassifier*>& consider, size_t recursion_limit, size_t level,
	 const std::vector<Eigen::VectorXf>& region_a, const std::vector<float>& region_b);
  void computeSplit();
  float computeDistanceToSplit(const Eigen::VectorXf& vec);

  void query(const Eigen::VectorXf& sample, float len_sq,
	     std::vector<WeakClassifier*>* activated, int* num_euc = NULL, int* num_dot = NULL);
  
  void checkWCsWithExclusion(const Eigen::VectorXf& sample, float len_sq, std::vector<WeakClassifier*>* activated, bool use_exclusion_heuristic, int* num_euc);

 private:
  void makeLeaf();
  void growTree();
};

class MultiBoosterDataset {
 public:
  std::vector<Object*> objs_;
  //! Matrix of y_m^c values.  i.e. ymc_(c,m) = +1 if the label of training example m is c, and -1 otherwise.
  Eigen::MatrixXf ymc_;
  //! No background.  
  std::vector<size_t> num_objs_of_class_;
  //! Number of background points.
  size_t num_bg_;
  std::string version_string_;
  //! Links class name strings to consecutive id numbers.
  //! class_map_.getName(i) is the name of the label for an Object with obj.label=i.
  NameMapping class_map_;
  //! Links feature space strings to consecutive id numbers.
  //! feature_map_.getName(i) is the name of the feature space for obj.descriptors[i].
  NameMapping feature_map_;

  //! Creates an empty object with empty name mappings.
  MultiBoosterDataset();
  MultiBoosterDataset(std::string filename);
  MultiBoosterDataset(std::vector<std::string> classes, std::vector<std::string> feature_spaces);
  MultiBoosterDataset(const NameMapping& class_map, const NameMapping& descriptor_map);
  //! Deep-copies the Objects.
  MultiBoosterDataset(const MultiBoosterDataset& mbd);
  ~MultiBoosterDataset();

  
  std::string status();
  std::string displayObjects();
  std::string displayYmc();

  //! Translates all objects to use the provided mappings, then updates ymc_ and num_objs_of_class_.
  void applyNewMappings(const NameMapping& new_class_map, const NameMapping& new_feature_map);
  //! Sets the objs_ vector, then updates ymc_ and num_objs_of_class_.
  //! Objects are not deep copied.
  void setObjs(const std::vector<Object*> &objs);
  bool save(std::string filename);
  bool load(std::string filename, bool quiet=false);
  //! The name mappings in this dataset get augmented with any additional names in mbd2.
  //! mbd2's objects are copied and translated into this dataset's (possibly new) name mappings.
  bool join(const MultiBoosterDataset& mbd2);
  //! Strict compare: the mappings must be the same too.
  bool compare(const MultiBoosterDataset& mbd);
  //! Uses distance squared.
  boost::shared_ptr< std::vector< std::pair<float, int> > > computeSortedDistances(size_t fsid, Eigen::VectorXf center);

  //! Drops all but the first cropval * objs_.size() objects.
  void crop(float cropval);
  //! Drops all but a random decimation * objs_.size() objects.
  void decimate(float decimation);
  void dropFeature(std::string drop);
  void dropFeatures(std::vector<std::string> droplist);
};


class MultiBooster {
 public:
  std::string version_string_;
  //! Links class name strings to consecutive id numbers.
  //! class_map_.getName(i) is the name of the label for an Object with obj.label=i.
  NameMapping class_map_;
  //! Links feature space strings to consecutive id numbers.
  //! feature_map_.getName(i) is the name of the feature space for the weak classifier with fsid_ i.
  NameMapping feature_map_;
  //! battery_[i] contains all the wcs that are in feature space feature_map_.toName(i).
  std::vector< std::vector<WeakClassifier*> > battery_;
  //! Pointers to weak classifiers, in the order that they were learned.
  std::vector<WeakClassifier*> pwcs_;
  MultiBoosterDataset* mbd_;
  bool verbose_;
  //! total_objs_seen_[c] == the total number of objects of class c that this classifier has ever seen objects for. 
  std::vector<size_t> total_objs_seen_;
  size_t num_bg_seen_;
  Eigen::VectorXf prior_;
  std::vector<WeakClassifierTree*> trees_;
  //! The centers of weak classifiers, for locality of reference.
  std::vector<Eigen::MatrixXf> centers_;
  std::vector<Eigen::MatrixXd> H_pos_;
  std::vector<Eigen::MatrixXd> H_neg_;
  std::vector<Eigen::VectorXd> g_pos_;
  std::vector<Eigen::VectorXd> g_neg_;
  //! The maximum weak classifier id to use when classifying.  0 = no limitation.
  size_t wc_limiter_;
  
  // -- These things should maybe be in their own struct for mbd + mb data.
  //! nClasses x nTrEx.
  Eigen::MatrixXd log_weights_;
  int num_threads_;

  //! Sets the classifier's name mappings to be equal to those of mbd.
  //! Doesn't change mbd, and doesn't delete it on destruction.
  MultiBooster(MultiBoosterDataset* mbd, int num_threads = 1);
  MultiBooster(std::string filename, int num_threads = 1);
  MultiBooster(std::istream& in, int num_threads = 1);
  //! Makes a copy of the MultiBoosterDataset pointer, not the dataset itself.
  MultiBooster(const MultiBooster& mb);
  ~MultiBooster();
  
  //! debugHook will be called each time a new weak classifier is learned. 
  void train(int num_candidates, int max_secs, int max_wcs, double min_util = 0, double min_objective = 0, int min_wcs = 0, void (*debugHook)(WeakClassifier) = NULL);
  //! Relearns the weak classifier responses on a new labeled dataset
  void relearnResponses(MultiBoosterDataset* dataset, double min_util = 0, int max_wcs = 0);
  //! Relearns the weak classifier responses and exposes an indicator matrix (nWCs x nTrEx) for reuse.
  //! If min_util != 0, then WCs with utility less than this will be removed.
  //! If prune != 0, then prune * pwcs_.size() of the worst WCs will be removed.
  void relearnResponses2(MultiBoosterDataset* dataset,
			 Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator,
			 double min_util = 0,
			 double prune = 0);
  //! Augments class and feature names with those in the dataset (and applies these changes to weak classifiers), applies our mapping to
  //! the dataset, resets all fields specific to a dataset / classifier pairing, relearns the prior...  Also sets mbd_.
  //! If reset_statistics is true, then num_bg_seen_ and total_objs_seen_ are both reset.  Otherwise these accumulate.
  void useDataset(MultiBoosterDataset *mbd, bool reset_statistics);
  bool save(std::string filename);
  void serialize(std::ostream& out);
  std::string status(bool verbose = true);
  //! If strict, then only allow permutations.
  void applyNewMappings(const NameMapping& new_class_map, const NameMapping& new_feature_map, bool strict = true);
  //! Assumes that the object has the same name mappings as the classifier.
  Eigen::VectorXf classify(Object &obj);
  void computeTrees();
  Eigen::VectorXf treeClassify(const Object &obj, int* num_euc = NULL, int* num_dot = NULL, std::vector<WeakClassifier*>* activations = NULL) const;
  Eigen::VectorXf localClassify(Object &obj);
  double classify(MultiBoosterDataset* mbd, float threshold = 0, PerfStats* results = NULL, Eigen::MatrixXd* log_weights = NULL, bool tree = true);
  void classifySpeedTest(MultiBoosterDataset* mbd, PerfStats* results, PerfStats* treeResults, PerfStats* localResults);
  double computeTrainingSetAccuracy() const;
  void recomputeLogWeights();
  double computeObjective();
  
  void updateGradientAndHessian(MultiBoosterDataset* mbd);
  //! Thin wrapper for balanceResponses
  void balanceResponsesMEL(MultiBoosterDataset* dataset, Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator = NULL);
  //! Thin wrapper for balanceResponses
  void balanceResponsesMLS(MultiBoosterDataset* dataset, Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator = NULL);
  
  void findActivatedWCs(const Object& obj, size_t fsid, std::vector<WeakClassifier*>* activated);
  bool compare(const MultiBooster& mb, bool verbose = false);
  std::vector< boost::shared_ptr<WeakClassifier> > createRandomWeakClassifiers(int num_candidates);
  void learnWC(int num_candidates, float* util=NULL);
  //! Augments both the classifier's and the dataset's name maps, then tranlates the dataset to match the classifier.
  void augmentAndTranslate(MultiBoosterDataset* mbd);
  void learnPrior();
  void computeStatisticsForDataset(MultiBoosterDataset* mbd);
  //! Returns the percent of weak classifiers that have no response value above thresh.
  int pctWCsBelowThreshold(double thresh);
  
private:
  //! Should only be called in the construction process.
  void deserialize(std::istream& is);

  //! indicator.col(i) is {0,1}^(numWCs): which weak classifiers training example i is in.
  void computeIndicatorMatrix(const MultiBoosterDataset& dataset,
			      Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator) const;

  //! If mls == true:
  //!  Runs min_a 1/M sum_m log(1 + exp(-y_m a's_m)), where a is the vector of responses for all weak classifiers
  //!  and s_m is the indicator vector for training example m.
  //! else:
  //!  Runs min_a 1/M sum_m exp(-y_m a's_m))
  //! If an indicator matrix is provided (i.e. from relearnResponses2) then it won't be recomputed.  Doesn't delete indicator if provided.
  void balanceResponses(bool mls, MultiBoosterDataset* dataset, Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator = NULL);

//! Runs MLS response balancing for label.  Returns the optimal value.
  double balanceResponsesMLS(const MultiBoosterDataset& dataset,
			     Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator,
			     int label);

  //! Runs MEL response balancing for label.  Returns the optimal value.
  double balanceResponsesMEL(const MultiBoosterDataset& dataset,
			     Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator,
			     int label);

  void cleanUpRBData(const MultiBoosterDataset& dataset,
		     int label,
		     Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator);
  void setUpRBData(const MultiBoosterDataset& dataset,
		   int label,
		   Eigen::SparseMatrix<double, Eigen::ColMajor>* indicator,
		   Eigen::VectorXd* b);

  //! Updates indexing, sets battery_ and pwcs_, rebuilds trees.
  //! Doesn't do anything to log_weights_.  Doesn't delete old pwcs_.
  void installNewWeakClassifiers(std::vector<WeakClassifier*> pwcs);

};

class WCEvaluator : public pipeline::ComputeNode
{
 public:
  MultiBoosterDataset *mbd_;
  MultiBooster *mb_;
  boost::shared_ptr<WeakClassifier> wc_;

  float utility_;
  boost::shared_ptr< std::vector< std::pair<float, int> > > distance_idx_;
  int num_contained_tr_ex_;

  WCEvaluator(MultiBooster *mb, MultiBoosterDataset *mbd, boost::shared_ptr<WeakClassifier> wc);
  
 private:
  void _compute();
  std::string _getName() const;
  void _flush();
};
  

// -- Helper functions.
std::string serializeMatrix(const Eigen::MatrixXf& mat);
void deserializeMatrix(std::istream& is, Eigen::MatrixXf* target);
std::string serializeVector(const Eigen::VectorXf& mat);
void deserializeVector(std::istream& is, Eigen::VectorXf* target);

inline bool close(double x, double y, double tol) {
  if(x == 0 && y != 0 && fabs(y) < tol)
    return true;
  else if(y == 0 && x != 0 && fabs(x) < tol)
    return true;
  else if(y != 0 && x != 0 && fabs((x-y)/x) < tol && fabs((x-y)/y) < tol)
    return true;
  else if(x == 0 && y == 0)
    return true;
  else
    return false;
}

double computePointToPolytopeDistanceSquared(const std::vector<Eigen::VectorXf>& region_a_left, const std::vector<float>& region_b_left,
					     const Eigen::VectorXf& point);


double sampleFromGaussian(double stdev);

#endif

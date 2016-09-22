/*
Copyright (c) 2004 Gregory Kuhlmann, Peter Stone
University of Texas at Austin
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the University of Amsterdam nor the names of its
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SMDP_AGENT
#define SMDP_AGENT

#include <utility>
#include <vector>
#include <zconf.h>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include "SoccerTypes.h"

#define MAX_STATE_VARS         128
#define MAX_ACTIONS            128

class BasicPlayer;

enum AtomicActionType {
  AAT_None,
  AAT_Hold,
  AAT_PassTo, // 1, 2, 3, ..., k - 1
  AAT_Intercept,
  AAT_Stay,
  AAT_Move // Left, Right, In, Out
};

std::ostream& operator<<(std::ostream& out, const AtomicActionType value);

struct AtomicAction {
  AtomicActionType type;
  int parameter;

  AtomicAction(AtomicActionType t = AAT_None, int p = 0): type(t), parameter(p) { }

  std::string name() const {
    stringstream ss;
    ss << type << "_" << parameter;
    return ss.str();
  }

  virtual std::vector<int> parameters() { return {0}; }
  virtual bool terminated(double state[], int num_features) {
    return state[num_features - 1] > 0.5; // terminate when tms can kick the ball
  }

  virtual SoccerCommand execute(BasicPlayer *player) = 0;
  virtual AtomicAction *clone(int parameter) = 0;

  static int keepers;
};

#define CLONE(cls) \
  virtual AtomicAction *clone(int parameter) { \
    auto a = new cls(); \
    a->parameter = parameter; \
    return a; \
  }

struct Hold: public AtomicAction {
  Hold(): AtomicAction(AAT_Hold) { }

  virtual bool terminated(double state[], int num_features);
  virtual SoccerCommand execute(BasicPlayer *player);
  CLONE(Hold)
};

struct PassTo: public AtomicAction {
  PassTo(): AtomicAction(AAT_PassTo) { }

  virtual std::vector<int> parameters();
  virtual SoccerCommand execute(BasicPlayer *player);
  CLONE(PassTo)
};

struct Intercept: public AtomicAction {
  Intercept(): AtomicAction(AAT_Intercept) { }

  virtual SoccerCommand execute(BasicPlayer *player);
  CLONE(Intercept)
};

struct Stay: public AtomicAction {
  Stay(): AtomicAction(AAT_Stay) { }

  virtual SoccerCommand execute(BasicPlayer *player);
  CLONE(Stay)
};

struct Move: public AtomicAction {
  Move(): AtomicAction(AAT_Move) { }

  virtual std::vector<int> parameters();

  virtual SoccerCommand execute(BasicPlayer *player);

  CLONE(Move);
};

struct JointAction {
  std::vector<AtomicAction*> actions;
  int id;
  bool tmControllBall;

  JointAction(): id(0), tmControllBall(false) {
    actions.resize((std::size_t) AtomicAction::keepers);
  }

  JointAction(const JointAction &ja):
      actions(ja.actions),
      id(ja.id),
      tmControllBall(ja.tmControllBall) { }

  const JointAction &operator=(const JointAction &ja) {
    if (this != &ja) {
      actions = ja.actions;
      id = ja.id;
      tmControllBall = ja.tmControllBall;
    }
    return *this;
  }

  std::string name() const {
    stringstream ss;
    ss << "{";
    for (auto a : actions) ss << a->name() << ", ";
    ss << "}";
    return ss.str();
  }
};

class JointActionSpace {
private:
  JointActionSpace();
  void construct(
      bool tmControlBall, int k,
      std::vector<std::vector<AtomicAction *>> &actions, JointAction &ja);

public:
  static JointActionSpace &ins() {
    static JointActionSpace jas;
    return jas;
  }

  int sample(bool tmControlBall) const {
    return jointActions[tmControlBall][rand() % jointActions[tmControlBall].size()]->id;
  }

  int sample(double state[], int num_features) const {
    bool tmControllBall = state[num_features - 1] > 0.5;
    return sample(tmControllBall);
  }

  int numActions() const { return (int) jaMap.size(); }

  const JointAction *getJointAction(int id) { return jaMap[id].first; }

private:
  std::vector<JointAction*> jointActions[2];
  std::unordered_map<int, std::pair<JointAction*, std::string>> jaMap;
  int count;
};

class SMDPAgent
{
public:
  int m_numFeatures; /* number of state features <= MAX_STATE_VARS */
  int lastAction;
  int lastActionTime;

  int getNumFeatures() const { return m_numFeatures; }
  int getNumActions()  const { return JointActionSpace::ins().numActions();  }

public:
  SMDPAgent( int numFeatures)
  {
    m_numFeatures = numFeatures;
    lastAction = -1;
    lastActionTime = UnknownTime;
  }

  virtual ~SMDPAgent() {}

  // abstract methods to be supplied by implementing class
  virtual void sync() {}
  virtual int  startEpisode( int current_time, double state[] ) = 0;
  virtual int  step( int current_time, double reward, double state[] ) = 0;
  virtual void endEpisode( int current_time, double reward ) = 0;

  // Optional customization point.
  virtual void shutDown() {}
} ;

#endif

/******************************************************************************
 *                                                                            *
 *    Copyright 2020   Lawrence Livermore National Security, LLC and other    *
 *    Whole Cell Simulator Project Developers. See the top-level COPYRIGHT    *
 *    file for details.                                                       *
 *                                                                            *
 *    SPDX-License-Identifier: MIT                                            *
 *                                                                            *
 ******************************************************************************/

#pragma once

#include <vector>
#include <unordered_map>
#include <list>

//// Classes that make the whole API work

struct EventData; //will be defined by a perl script. 
class EventInterface;
typedef std::size_t Tag;

class LP {
 public:
   virtual EventInterface* create(EventData* e) = 0;
   virtual ~LP() {}

   template <typename Dependent>
   static void init_lp(void *state, tw_lp *me);
   template <typename Dependent>
   static void forward_event(void *state, tw_bf *bf, void *evt, tw_lp *thislp);
   static void backward_event(void *state, tw_bf *bf, void *evt, tw_lp *thislp);
   static void commit_event(void *state, tw_bf *bf, void *evt, tw_lp *thislp);
   template <typename Dependent>
   static void final_lp(void *state, tw_lp *me);
   
   struct ObserverData {
      tw_lpid dest;
      Tag slot;
   };
   tw_lp* twlp; //this has to be initialized properly! TODO
};

#define TAGIFY(NAME) (typeid(NAME).hash_code())

#define SIGNAL(name, type)                                              \
   void name(Time trecv, type& msg) {                                   \
      /*for all LPs attached to this signal*/                           \
      for (auto&& observerData : name##_listeners) {                    \
         my_event_send(observerData.dest, trecv, twlp, observerData.slot, msg); \
      }                                                                 \
   }                                                                    \
   std::list<LP::ObserverData> name##_listeners

#define CONNECT(sendingLP, signal, dest, slot) (sendingLP).signal##_listeners.push_back({(tw_lpid)(dest),TAGIFY(slot)})

class EventInterface {
 public:
   virtual void forward(Time tnow, tw_bf *bf) = 0;
   virtual void backward(Time tnow, tw_bf *bf) = 0;
   virtual void commit(Time tnow, tw_bf *bf) = 0;
   virtual ~EventInterface() {}
};

template <typename LPType, typename MsgType>
class EventBase : public EventInterface
{
 public:
   LPType* self;
   MsgType* msg;
};

enum CheckpointMode {FORWARD, BACKWARD};

template <typename T>
inline void checkpoint(CheckpointMode m, T& self, T& messageBuffer)
{
   if (m == FORWARD)
   {
      messageBuffer = self;
   }
   else
   {
      self = messageBuffer;
   }
}

////////////////////////////////////////////////////
/// User interface classes

typedef int SpeciesValue;
typedef int SpeciesTag;
typedef int ReactionTag;

typedef int ExpressionTag;
typedef double ExpressionValue;

struct SpeciesValueMsg {
   SpeciesTag tag;
   SpeciesValue value;
};
struct ExpressionValueMsg {
   ExpressionTag tag;
   ExpressionValue value;
};
struct ChangeMsg {
   int change;
};
struct NoopMsg {
};

////////////////////////////////////////////////////////////////////////////
// Code that will be automatically generated by a perl script
//
// I'm just writing it out here to make it clear what it would contain.

struct EventData {
   typedef std::size_t Tag;
   union {
      Tag tag;
      EventInterface* obj;
   };
   int cancel;
   union {
      char msg;
      SpeciesValueMsg spec;
      ExpressionValueMsg expr;
      ChangeMsg change;
      NoopMsg noop;
   };
};


template <typename TTT>
tw_event* my_event_send(tw_lpid dest, Time trecv, tw_lp* twlp, Tag tag, const TTT& msg)
{
   tw_event* evtptr = tw_event_new(dest, trecv, twlp);
   EventData* data = static_cast<EventData *>(tw_event_data(evtptr));
   data->cancel=0;
   data->tag = tag;
   memcpy(&data->msg, &msg, sizeof(msg));
   tw_event_send(evtptr);
   return evtptr;
}

void my_event_cancel(tw_event* evtptr) {
   EventData* data = static_cast<EventData *>(tw_event_data(evtptr));
   data->cancel++;
}

void my_event_uncancel(tw_event* evtptr) {
   EventData* data = static_cast<EventData *>(tw_event_data(evtptr));
   data->cancel--;
}
////////////////////////////////////////////////////////////////////////////


class Species : public LP {
 public:
   SpeciesTag tag;
   SpeciesValue value;

   class ChangeSlot : public EventBase<Species,ChangeMsg> {
    public:
      virtual void forward(Time tnow, tw_bf *bf) {
         self->value += msg->change;
         self->sendValueUpdates(tnow, bf);
      }
      virtual void backward(Time , tw_bf *) {
         self->value -= msg->change;
      }
      virtual void commit(Time , tw_bf *) {
      }
   };

   void sendValueUpdates(Time tnow, tw_bf *) {
      SpeciesValueMsg msg;
      msg.tag = tag;
      msg.value = value;
      valueChanged(tnow, msg);
   };
   virtual EventInterface* create(EventData* e); //will be created by a perl script.

   SIGNAL(valueChanged, SpeciesValueMsg);
};

typedef typename std::function<double(const int numSpecies, const SpeciesValue* const, const int numExpr, const ExpressionValue* const)> EvaluationFunction;

class Reaction : public LP {
 public:
   ReactionTag tag;
   std::vector<SpeciesValue> speciesValues;
   std::vector<ExpressionValue> exprValues;

   std::unordered_map<SpeciesTag, int> dependentSpecies;
   std::unordered_map<ExpressionTag, int> dependentExpr;

   std::unordered_map<SpeciesTag, int> rxnValences;

   SIGNAL(updateDependentRxns, NoopMsg);
   
   EvaluationFunction rateFunction;
   
   double prevTime;
   double rate;
   double countdown;
   tw_event* nextReaction;

   void init()
   {
      nextReaction=0;
      prevTime=0;
      rate=0;
      countdown=0;
      
   }
   
   class UpdateSpeciesSlot : public EventBase<Reaction, SpeciesValueMsg> {
    public:
      SpeciesValue prevValue;
      inline void checkpointMe(CheckpointMode m)
      {
         int speciesIndex = self->dependentSpecies[msg->tag];
         checkpoint(m,self->speciesValues[speciesIndex],prevValue);
      }
      virtual void forward(Time , tw_bf* ) {
         checkpointMe(FORWARD);
         int speciesIndex = self->dependentSpecies[msg->tag];
         self->speciesValues[speciesIndex] = msg->value;
      }
      virtual void backward(Time , tw_bf* ) {
         checkpointMe(BACKWARD);
      }
      virtual void commit(Time , tw_bf* ) {}
   };

   class UpdateExprSlot : public EventBase<Reaction, ExpressionValueMsg> {
    public:
      ExpressionValue prevValue;
      inline void checkpointMe(CheckpointMode m)
      {
         int index = self->dependentExpr[msg->tag];
         checkpoint(m,self->exprValues[index],prevValue);
      }
      virtual void forward(Time, tw_bf*) {
         checkpointMe(FORWARD);
         int index = self->dependentExpr[msg->tag];
         self->exprValues[index] = msg->value;
      }
      virtual void backward(Time, tw_bf*) {
         checkpointMe(BACKWARD);
      }
      virtual void commit(Time, tw_bf*) {}
   };

   class UpdateReactionRateSlot : public EventBase<Reaction, NoopMsg> {
      double prevTime;
      double rate;
      double countdown;
      tw_event* nextReaction;
      inline void checkpointMe(CheckpointMode m)
      {
         checkpoint(m,self->prevTime,prevTime);
         checkpoint(m,self->rate, rate);
         checkpoint(m,self->countdown, countdown);
         checkpoint(m,self->nextReaction, nextReaction);
      }
      virtual void forward(Time tnow, tw_bf *) {
         checkpointMe(FORWARD);
         if (this->nextReaction != NULL)
         {
            my_event_cancel(this->nextReaction);
         }
         self->updateReactionRate(tnow);
         
      }
      virtual void backward(Time , tw_bf *) {
         if (this->nextReaction != NULL)
         {
            my_event_uncancel(this->nextReaction);
         }
         checkpointMe(BACKWARD);
      }
      virtual void commit(Time , tw_bf *) {
         //Notice, nextReaction holds the previous message that's now invalid.
         //If we get here, the message went through and we can cancel the
         //other message.
         //if (nextReaction != NULL)
         //{
         //   tw_event_cancel(this->nextReaction);
         //}
            
      }
   };

   void updateReactionRate(Time tnow) {
      double current_time = tnow.t;
      countdown -= (current_time-prevTime)*rate;
         
      //Record this time as the current time.
      prevTime = current_time;
      //Compute the new rate
      rate = rateFunction(speciesValues.size(), &speciesValues[0], exprValues.size(), &exprValues[0]);
      //Compute the next time we're due to fire
      Time futureTime;
      futureTime.t = current_time + countdown/rate;
      futureTime.bits[0]=futureTime.bits[1]=0;
      
      //Send a message to future self to fire
      ChangeMsg newMsg;
      newMsg.change = 1; //FIXME: change this for reversible reactions?
      nextReaction = my_event_send(tag,futureTime,twlp,TAGIFY(Reaction::FireReactionSlot),newMsg);
   }
   
   class FireReactionSlot : public EventBase<Reaction, ChangeMsg> {
    public:
      double prevTime;
      double countdown;
      inline void checkpointMe(CheckpointMode m, tw_bf *)
      {
         checkpoint(m,self->prevTime,prevTime);
         checkpoint(m,self->countdown, countdown);
         if (m==BACKWARD) {
            tw_rand_reverse_unif(self->twlp->rng);
         }
      }
      virtual void forward(Time tnow, tw_bf *bf) {
         checkpointMe(FORWARD, bf);

         // Update the counts to all the upkeep versions
         for (auto iter : self->rxnValences)
         {
            ChangeMsg otherMsg;
            otherMsg.change = iter.second;
            Time tnow_shifted = tnow;
            tnow_shifted.bits[0] += 1;

            my_event_send(iter.first, tnow_shifted, self->twlp, TAGIFY(Species::ChangeSlot), otherMsg);
         }

         // Tell downstream reactions to update themselves
         {
            NoopMsg otherMsg;
            Time tnow_shifted = tnow;
            tnow_shifted.bits[0] += 2;
            self->updateDependentRxns(tnow_shifted, otherMsg);
         }
         self->updateCountdown(tnow);
      }
      virtual void backward(Time , tw_bf *bf) {
         checkpointMe(BACKWARD, bf);
      }
      virtual void commit(Time , tw_bf *) {
      }
   };

   void updateCountdown(Time tnow)
   {
      //Get a new random number
      countdown = tw_rand_exponential(twlp->rng, 1);
      prevTime = tnow.t;
   }
   
   virtual EventInterface* create(EventData* e); //will be created by a perl script.

};


////////////////////////////////////////////////////////////////////////////
// Code that will be automatically generated by a perl script
//
// I'm just writing it out here to make it clear what it would contain.

/////////////////////////////////////////////////////////////////////////////

#define FACTORY_FOR(LP, SLOT, MSG)                      \
   do {                                                 \
      if (e->tag == TAGIFY(LP::SLOT))                   \
      {                                                 \
         LP::SLOT* slot = new LP::SLOT();               \
         slot->self = this;                             \
         slot->msg = reinterpret_cast<MSG*>(&e->msg);   \
         return slot;                                   \
      }                                                 \
   }while(0)

EventInterface* Species::create(EventData* e) {
   FACTORY_FOR(Species, ChangeSlot, ChangeMsg);
   assert(0 && "Error, unrecognized slot");
   return NULL;
}

EventInterface* Reaction::create(EventData* e) {
   FACTORY_FOR(Reaction, UpdateSpeciesSlot, SpeciesValueMsg);
   FACTORY_FOR(Reaction, UpdateExprSlot, ExpressionValueMsg);
   FACTORY_FOR(Reaction, UpdateReactionRateSlot, NoopMsg);
   FACTORY_FOR(Reaction, FireReactionSlot, ChangeMsg);
   assert(0 && "Error, unrecognized slot");
   return NULL;   
}

template <typename Dependent>
inline LP* cast_as_LP(void* state) {
   return static_cast<LP*>(reinterpret_cast<Dependent*>(state));
}

template <typename Dependent>
void LP::init_lp(void *state, tw_lp *thislp) {
   Dependent* derivedObj = new (reinterpret_cast<Dependent *>(state)) Dependent();
   LP *lpptr = static_cast<LP*>(derivedObj);
   lpptr->twlp = thislp;
}

template <typename Dependent>
void LP::forward_event(void *state, tw_bf *bf, void *evt, tw_lp *thislp) {
   Time tnow = tw_now(thislp);
   EventData *e = reinterpret_cast<EventData *>(evt);
   if (e->cancel == 0)
   {
      LP *lpptr = static_cast<LP*>(reinterpret_cast<Dependent*>(state));
      e->obj = lpptr->create(e);
      e->obj->forward(tnow,bf);
   }
}

void LP::backward_event(void *, tw_bf *bf, void *evt, tw_lp *thislp) {
   Time tnow = tw_now(thislp);
   EventData *e = reinterpret_cast<EventData *>(evt);
   if (e->cancel == 0) {
      e->obj->backward(tnow,bf);
      delete e->obj;
   }
}

void LP::commit_event(void *, tw_bf *bf, void *evt, tw_lp *thislp) {
   Time tnow = tw_now(thislp);
   EventData *e = reinterpret_cast<EventData *>(evt);
   if (e->cancel == 0) {
      e->obj->commit(tnow,bf);
      delete e->obj;
   }
}  

template <typename Dependent>
void LP::final_lp(void *state, tw_lp *) {
   Dependent* derivedObj = reinterpret_cast<Dependent *>(state);
   derivedObj->~Dependent();
}

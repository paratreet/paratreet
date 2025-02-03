#ifndef PARATREET_OPPOSITEEFFECTSMANAGER_H_
#define PARATREET_OPPOSITEEFFECTSMANAGER_H_

#include "paratreet.decl.h"
#include "common.h"

template <typename Data>
class OppositeEffectsManager : public CBase_OppositeEffectsManager<Data> {
  std::map<int, std::map<Key, typename Data::Particle::Effect>> opposing_effects; // (partition, pKey, effect)
public:
  void applyOpposingEffect(const typename Data::Particle& part, const typename Data::Particle::Effect& effect);
  void applyAccumulatedOpposingEffects(PPHolder<Data> pp_holder);
};

template <typename Data>
void OppositeEffectsManager<Data>::applyOpposingEffect(const typename Data::Particle& part, const typename Data::Particle::Effect& effect) {
  opposing_effects[part.partition_idx][part.key] += effect;
}

template <typename Data>
void OppositeEffectsManager<Data>::applyAccumulatedOpposingEffects(PPHolder<Data> pp_holder) {
  for (auto && oe : opposing_effects) {
    std::vector<std::pair<Key, typename Data::Particle::Effect>> effects (oe.second.begin(), oe.second.end());
    pp_holder.proxy[oe.first].applyOpposingEffects(effects);
  }
  opposing_effects.clear();
}

#endif // PARATREET_OPPOSITEEFFECTSMANAGER_H_

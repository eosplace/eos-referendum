/* Licence: https://github.com/eos-referendum/master/LICENCE.md */

/*
 * File:   referendum.cpp
 * Author: Michael Fletcher | EOS42
 *
 * Created on 20 June 2018, 17:26
*/

#include "referendum.hpp"

namespace referendum {

void referendum::init(account_name publisher)
{
  refconfig ref_config;

  ref_config.min_part_p = MINIMUM_VOTE_PARTICIPATION_PERCENTAGE;
  ref_config.vote_period_d = REFERENDUM_VOTE_PERIOD_DAYS;
  ref_config.sust_vote_d = SUSTAINED_VOTE_PERIOD_DAYS;
  ref_config.yes_vote_w = YES_LEADING_VOTE_PERCENTAGE;

  referendum_config.set(ref_config, _self);
}


//@abi action
void referendum::vote(account_name voter_name, uint8_t vote_side){
  require_auth(voter_name);

  /* check if vote is active */
  eosio_assert(referendum_results.get().vote_active, "voting has finished");

  /* if they've staked, their will be a voter entry */
  auto voter = voter_info.find(voter_name);
  eosio_assert(voter == voter_info.end(), "user must stake before they can vote.");

  /* vote side yes = 1 or no = 0 */
  eosio_assert(validate_side(vote_side), "vote side is invalid");

  /* have they already voted */
  auto registered_voter = registered_voters.find(voter_name);
  eosio_assert(registered_voter == registered_voters.end(), "user has already voted"); 

  /* register vote */
  registered_voters.emplace(_self, [&](auto &voter_rec){
    voter_rec.name = voter_name;
    voter_rec.vote_side = vote_side;
  });
   
}

//@abi action
void referendum::unvote(account_name voter_name){
  require_auth(voter_name);

  /* check if vote is active */
  eosio_assert(referendum_results.get().vote_active, "voting has finished");

   /* have they voted */
  auto registered_voter = registered_voters.find(voter_name);
  eosio_assert(registered_voter != registered_voters.end(), "user has not voted");

  /* remove the user */
  registered_voters.erase(registered_voter);
}

//@abi action
void referendum::countvotes(account_name publisher){
  require_auth(publisher);

  /* check if vote is active */
  eosio_assert(referendum_results.get().vote_active, "voting has finished");
 
  bool vote_period_passed = false;
 
  /* count the votes */
  double total_votes_yes = 0;
  double total_votes_no = 0;

  /* check all the registered voters */
  for(auto itr = registered_voters.begin(); itr != registered_voters.end(); ++itr)
  {
    auto user_votes = voter_info.find(itr->name);
    if(user_votes == voter_info.end()){
      continue; // user has not staked 
    }

    /* count each side */
    switch(itr->vote_side)
    {
	case VOTE_SIDE_YES:
	  total_votes_yes += user_votes->staked;
	  break;

	case VOTE_SIDE_NO:
	  total_votes_no += user_votes->staked;
	  break;

	default:
	  continue;
	break;
    }
    
  }

  double total_votes = total_votes_yes + total_votes_no;

  /* TODO -> we can make this dynamic by looking up how many EOS currently exist. it will do for now */
  double total_network_vote_percentage = total_votes / TOTAL_AVAILABLE_EOS  * 100;
 
  /* calculate vote percentages */
  double yes_vote_percentage = total_votes_yes / total_votes * 100;
  double no_vote_percentage = total_votes_no / total_votes * 100;

  /* is it greater than the minimum pariticpation i.e 15%? */
  if(total_network_vote_percentage > MINIMUM_VOTE_PARTICIPATION_PERCENTAGE)
  {
    /* Do we have more yes votes than no */
    if(total_votes_yes > (total_votes_no + YES_LEADING_VOTE_PERCENTAGE))
    {
      vote_period_passed = true;
    }
  }   
  
  /* how many days have passed since the vote started + how many consecutive days has the vote been succesful */
  uint64_t total_days = referendum_results.get().total_days;
  uint64_t total_c_days = referendum_results.get().total_c_days;

  /* todays vote has passed */
  refinfo new_referendum_info;
  if(vote_period_passed){
    
    new_referendum_info.total_days = ++total_days;
    new_referendum_info.total_c_days = ++total_c_days;
    new_referendum_info.vote_active = true;
 
  } else {

    /* todays vote has failed, start again */
    new_referendum_info.total_days = ++total_days;
    new_referendum_info.total_c_days = 0;
    
    /* do we have enough time left within the vote period to complete a succesful vote if we start again? */
    if(new_referendum_info.total_days + SUSTAINED_VOTE_PERIOD_DAYS > REFERENDUM_VOTE_PERIOD_DAYS)
    {
      new_referendum_info.vote_active = false;
    } else {
      new_referendum_info.vote_active = true;
    }

  }

  /* Update the singleton storing referendum data */ 
  referendum_results.set(new_referendum_info, _self);

  /*submit transaction to count vote again in 24 hours*/
  eosio::transaction out;
  out.actions.emplace_back( eosio::permission_level{ _self, N(active) }, _self, N(countvotes), _self );
  out.delay_sec = 86400; 
  out.send(_self, _self, true);
}

bool referendum::validate_side(uint8_t vote_side){

  switch(vote_side){
    case VOTE_SIDE_YES:
    case VOTE_SIDE_NO:
	return true;

    default:
	return false;
  }

}

}
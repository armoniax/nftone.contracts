#include <pass.lock/pass.lock.hpp>
#include <pass.lock/pass.lock_db.hpp>
#include <pass.lock/amax.ntoken/amax.ntoken.hpp>
#include <pass.lock/amax.ntoken/amax.ntoken_db.hpp>
#include <pass.lock/libraries/utils.hpp>
#include <pass.lock/libraries/wasm_db.hpp>
#include <set>

using namespace eosio;
using namespace std;
using std::string;

static constexpr eosio::name active_permission{"active"_n};

namespace lock{
  
  void pass_lock::init( const name& nft_contract){
    
     require_auth( get_self() );
     CHECK( _gstate.started_at == time_point_sec(), "already init");
     CHECKC( is_account(nft_contract), err::ACCOUNT_INVALID,"new admin does not exist");
     _gstate.admin          = get_self();
     _gstate.nft_contract   = nft_contract;
     _gstate.started_at     = time_point_sec(current_time_point());
     _gstate.last_plan_id   = INITIALIZED_ID;
  }

  void pass_lock::addplan(const name& owner,
                         const string& title,
                         const nsymbol& asset_symbol,
                         const time_point_sec& unlock_times){
      
      CHECK( _gstate.started_at != time_point_sec(), "initialization not complete");
      CHECK( has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of maintainer or admin" );
      CHECK( title.size() <= MAX_TITLE_SIZE, "title size must be <= " + to_string(MAX_TITLE_SIZE) );
      //check( is_account(nft_contract), "asset contract account does not exist" );
      CHECK( asset_symbol.is_valid(), "Invalid asset symbol" );
  
      auto now = time_point_sec(current_time_point());
      //check(unlock_times > now,"The correct timestamp is required");

      nstats_t::idx_t nt( _gstate.nft_contract, _gstate.nft_contract.value );
      auto nt_itr = nt.find( asset_symbol.id);
      CHECKC( nt_itr != nt.end(), err::SYMBOL_MISMATCH,"nft not found, id:" + to_string(asset_symbol.id));

      plan_t::tbl_t plan_tbl(_self,_self.value);
      auto nft_idx = plan_tbl.get_index<"nsymbidx"_n>();
      auto plan_itr = nft_idx.find(asset_symbol.id);

      CHECKC( plan_itr == nft_idx.end(),err::RECORD_EXISTING, "The current NFT plan already exists , plan_id: " + to_string(plan_itr->id));

      _gstate.last_plan_id ++;
      auto plan_id = _gstate.last_plan_id;

      plan_tbl.emplace(_self,[&](auto& plan){
        plan.id                 = plan_id;
        plan.owner              = owner;
        plan.title              = title;
        plan.asset_contract     = _gstate.nft_contract;
        plan.asset_symbol       = asset_symbol;
        plan.total_issued       = nasset(0,asset_symbol);
        plan.total_unlocked     = nasset(0,asset_symbol);
        plan.status             = plan_status::enabled;
        plan.created_at         = now;
        plan.updated_at         = now;
        plan.unlock_times       = unlock_times;
      });
  }

  void pass_lock::setplanowner( const uint64_t& plan_id, const name& new_owner){

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) );
    CHECK( is_account(new_owner), "new_owner account does not exist" );
    CHECK( has_auth(plan_itr->owner) || has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of owner or maintainer or admin" );

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.owner = new_owner;
        plan.updated_at = current_time_point();
    });
  }

  void pass_lock::enableplan( const uint64_t& plan_id, bool enabled){
    
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( has_auth(plan_itr->owner) || has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of owner or maintainer or admin" );
    name new_status = enabled ? plan_status::enabled : plan_status::disabled;
    CHECK( plan_itr->status != new_status, "plan status is no changed" );

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.status = new_status;
        plan.updated_at = current_time_point();
    });

  }

  void pass_lock::ontransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo){

      if (from == get_self() || to != get_self()) return;

      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" );

      vector<string_view> memo_params = split(memo, ":");
      ASSERT(memo_params.size() > 0);
      if (memo_params[0] == "lock"){

        CHECKC( quants.size() == 1, err::OVERSIZED, "Only one NFT is allowed at a time point" );

        nasset quantity = quants[0];

        CHECKC( memo_params.size() == 2, err::MEMO_FORMAT_ERROR,"expected format 'lock : ${receiver} '");

        name receiver = name(string(memo_params[1]));
        CHECKC( is_account(receiver), err::NOT_POSITIVE, "receiver account not exist" );
      
        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto nft_idx = plan_tbl.get_index<"nsymbidx"_n>();
        auto plan_itr = nft_idx.find( quantity.symbol.id );

        CHECKC( plan_itr != nft_idx.end(), err::RECORD_NOT_FOUND , "lock plan not found, symbol id: " + to_string(quantity.symbol.id) );
        CHECKC( plan_itr->status == plan_status::enabled, err::STATUS_ERROR, "plan not enabled, status:" + plan_itr->status.to_string() );
        CHECKC( _gstate.nft_contract == get_first_receiver(),err::NO_AUTH, "issue asset contract mismatch" );
        CHECKC( from == plan_itr->owner, err::NO_AUTH, "owner mismatch" );

        auto now = time_point_sec(current_time_point());

        nft_idx.modify( plan_itr, get_self(), [&]( auto& plan ) {
          plan.total_issued += quantity;
          plan.updated_at   = now;
        });
        
        plan_t pt = plan_tbl.get(plan_itr->id);
        _add_lock( receiver, quantity, pt);
       
      }
      
  }

  void pass_lock::unlock(const name& owner,const uint64_t& plan_id){
    
    require_auth(owner);

    account_t::tbl_t account( get_self(), owner.value);
    auto itr = account.find(plan_id);

    CHECKC( itr != account.end(), err::RECORD_NOT_FOUND, "owner`s plan not find, id:" + to_string(plan_id));
    CHECKC( itr->locked.amount > 0, err::RECORD_NOT_FOUND,"overdrawn amount");
    CHECKC( itr->unlocked.amount + itr->locked.amount == itr->total_issued.amount, err::RECORD_NOT_FOUND,"Data exception");
    CHECKC( itr->status == lock_status::locked, err::STATUS_ERROR, "Current status: not unlocked")

    auto now = time_point_sec(current_time_point());
    
    CHECKC( itr->unlock_times <= now, err::STATUS_ERROR, "The unlocking date has not arrived" );

    auto quantity = itr->locked;
    account.modify(itr,owner,[&](auto& row){
        row.locked        -= quantity;
        row.unlocked      += quantity;
        row.updated_at    = now;
        row.status        = lock_status::unlocked;
    });

    plan_t::tbl_t plan_tbl( get_self(), get_self().value);
    auto plan_itr = plan_tbl.find( itr->plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string( itr->plan_id)  );
    CHECK( plan_itr->status == plan_status::enabled, "plan not enabled, status:" + plan_itr->status.to_string());

    vector<nasset> quants = { quantity };
    TRANSFER_N( plan_itr-> asset_contract, owner , quants , "unlock" );

    plan_tbl.modify( plan_itr, _self, [&](auto& row){
      row.total_unlocked    += quantity;
      row.updated_at        = now;
    });

  }

  void pass_lock::_add_lock( const name& owner, const nasset& quantity, const plan_t& plan){
    
      account_t::tbl_t a_tbl( get_self(), owner.value);
      auto itr = a_tbl.find( plan.id );

      auto now = time_point_sec(current_time_point());
      if ( itr == a_tbl.end() ){
        a_tbl.emplace( get_self(), [&](auto& row){
          row.total_issued      = quantity;
          row.locked            = quantity;
          row.unlocked          = nasset(0,quantity.symbol);
          row.asset_contract    = plan.asset_contract;
          row.created_at        = now;
          row.updated_at        = now;
          row.unlock_times      = plan.unlock_times;
          row.plan_id           = plan.id;
          row.status            = lock_status::locked;
        });
      }else {

        a_tbl.modify(itr,get_self(),[&](auto& row){
          row.total_issued      += quantity;
          row.locked            += quantity;
          row.updated_at        = now;
          row.unlock_times      = plan.unlock_times;
          // row.status            = lock_status::locked;
        });
      }
  }
}

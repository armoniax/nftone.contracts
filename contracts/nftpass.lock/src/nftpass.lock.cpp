#include <nftpass.lock/nftpass.lock.hpp>
#include <nftpass.lock/nftpass.lock_db.hpp>
#include <nftpass.lock/amax.ntoken/amax.ntoken.hpp>
#include <nftpass.lock/thirdparty/utils.hpp>
#include <nftpass.lock/thirdparty/wasm_db.hpp>
#include <set>

using namespace eosio;
using namespace std;
using std::string;

static constexpr eosio::name active_permission{"active"_n};

namespace amax{
  
  void nft_lock::init(){
     require_auth( get_self() );
     CHECK( _gstate.started_at == time_point_sec(), "already init");

     _gstate.admin          = get_self();
     _gstate.started_at     = time_point_sec(current_time_point());
     _gstate.last_plan_id   = INITIALIZED_ID;
     _gstate.last_issue_id  = INITIALIZED_ID;
  
  }

  void nft_lock::addplan(const name& owner,
                         const string& title,
                         const name& asset_contract,
                         const nsymbol& asset_symbol,
                         const time_point_sec& unlock_times){
      
      CHECK( _gstate.started_at != time_point_sec(), "initialization not complete");
      check( has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of maintainer or admin" );
      check( title.size() <= MAX_TITLE_SIZE, "title size must be <= " + to_string(MAX_TITLE_SIZE) );
      check( is_account(asset_contract), "asset contract account does not exist" );
      check( asset_symbol.is_valid(), "Invalid asset symbol" );
  
      auto now = time_point_sec(current_time_point());
      //check(unlock_times > now,"The correct timestamp is required");

      plan_t::tbl_t plan_tbl(_self,_self.value);
      auto nft_idx = plan_tbl.get_index<"nsyidx"_n>();
      auto plan_itr = nft_idx.find(asset_symbol.id);

      CHECKC( plan_itr == nft_idx.end(),err::RECORD_EXISTING, "The current NFT plan already exists , plan_id: " + to_string(plan_itr->id));
      _gstate.last_plan_id ++;
      auto plan_id = _gstate.last_plan_id;

      plan_tbl.emplace(_self,[&](auto& plan){
        plan.id                 = plan_id;
        plan.owner              = owner;
        plan.title              = title;
        plan.asset_contract     = asset_contract;
        plan.asset_symbol       = asset_symbol;
        plan.total_issued       = nasset(0,asset_symbol);
        plan.total_unlocked     = nasset(0,asset_symbol);
        plan.status             = plan_status::enabled;
        plan.created_at         = now;
        plan.updated_at         = now;
        plan.unlock_times       = unlock_times;
      });
  }

  void nft_lock::setplanowner(const name& owner,const uint64_t& plan_id, const name& new_owner){

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    check( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) );
    check( is_account(new_owner), "new_owner account does not exist" );
    check( has_auth(plan_itr->owner) || has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of owner or maintainer or admin" );

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.owner = new_owner;
        plan.updated_at = current_time_point();
    });
  }

  void nft_lock::enableplan(const name& owner, const uint64_t& plan_id, bool enabled){
    
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

  void nft_lock::onlocktransferntoken(const name& from, const name& to, const vector<nasset>& quants, const string& memo){

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
        auto nft_idx = plan_tbl.get_index<"nsyidx"_n>();
        auto plan_itr = nft_idx.find( quantity.symbol.id );

        CHECKC( plan_itr != nft_idx.end(), err::RECORD_NOT_FOUND , "plan not found, symbol id: " + to_string(quantity.symbol.id) );
        CHECKC( plan_itr->status == plan_status::enabled, err::STATUS_ERROR, "plan not enabled, status:" + plan_itr->status.to_string() );
        CHECKC( plan_itr->asset_contract == get_first_receiver(),err::NO_AUTH, "issue asset contract mismatch" );
        CHECKC( from == plan_itr->owner, err::NO_AUTH, "owner mismatch" );

        //issue_t::tbl_t issue_tbl(_self,_self.value);

        // _gstate.last_issue_id ++;
        // auto issue_id = _gstate.last_issue_id;

        auto now = time_point_sec(current_time_point());
      
        // issue_tbl.emplace(_self,[&](auto& row){
        //   row.id            = lock_id;
        //   row.plan_id       = plan_id;
        //   row.issuer        = from;
        //   row.receiver      = receiver;
        //   row.created_at    = now;
        //   row.locked        = quantity;
        //   row.unlocked      = nasset(0,quantity.symbol);
        //   row.status        = ISSUE_NORMAL;
        //   row.updated_at    = now;
        //   row.unlock_times  = plan_itr->unlock_times;
        // });

        nft_idx.modify( plan_itr, get_self(), [&]( auto& plan ) {
          plan.total_issued += quantity;
          plan.updated_at   = now;
        });
        
        plan_t pt = plan_tbl.get(plan_itr->id);
        _add_locked_amount( receiver, quantity, pt);
       
      }
      
  }

  void nft_lock::unlock(const name& owner,const uint64_t& symbol_id){
    
    require_auth(owner);

    // issue_t::tbl_t issue_tbl(_self,_self.value);
    // auto issue_itr = issue_tbl.find(issue_id);
    // check( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) );
    // check( owner == issue_tbl->receiver,"owner mismatch");
    // check( issue_itr->status == ISSUE_NORMAL,"issue not normal,status:" + to_string(issue_itr->status));

    // auto unlock_asset = issue_itr -> locked;
    // check( issue_itr->unlocked.amount > 0,"No unlocking data");

    // auto now = time_point_sec(current_time_point());
    // check( issue_itr->unlock_times <= now,  "Unlocking date not reached");

    account_t::tbl_t account( get_self(), owner.value);
    auto itr = account.find(symbol_id);

    CHECKC( itr != account.end(), err::RECORD_NOT_FOUND, "symbol not find, id:" + to_string(symbol_id));
    CHECKC( itr->locked.amount > 0, err::RECORD_NOT_FOUND,"overdrawn amount");
    CHECKC( itr->unlocked.amount + itr->locked.amount == itr->total_issued.amount, err::RECORD_NOT_FOUND,"Data exception");
    auto now = time_point_sec(current_time_point());
    
    CHECKC( itr->unlock_times <= now, err::STATUS_ERROR, "The unlocking date has not arrived" );

    auto quantity = itr->locked;
    account.modify(itr,owner,[&](auto& row){
        row.locked        -= quantity;
        row.unlocked      += quantity;
        row.updated_at    = now;
        row.status        = issue_status::unlocked;
    });

    plan_t::tbl_t plan_tbl( get_self(), get_self().value);
    auto plan_itr = plan_tbl.find( itr->plan_id);
    check( plan_itr != plan_tbl.end(), "plan not found: " + to_string( itr->plan_id)  );
    check( plan_itr->status == plan_status::enabled, "plan not enabled, status:" + plan_itr->status.to_string());

    vector<nasset> quants = { quantity };
    TRANSFER_N( plan_itr-> asset_contract, owner , quants,"unlock:" + to_string(symbol_id));

    plan_tbl.modify( plan_itr, _self, [&](auto& row){
      row.total_unlocked    += quantity;
      row.updated_at        = now;
    });

    // issue_tbl.modify( issue_itr, _self, [&](auto& row){
    //   row.unlocked          += unlock_asset;
    //   row.updated_at        = now;
    //   row.status            = issue_status::finished;
    //   row.locked.amount     = 0;
    // });

    //_sub_locked(owner,unlock_asset);
  }

  void nft_lock::_add_locked_amount( const name& owner, const nasset& quantity, const plan_t& plan){
    
      account_t::tbl_t a_tbl( get_self(), owner.value);
      auto itr = a_tbl.find(quantity.symbol.raw());

      auto now = time_point_sec(current_time_point());
      if ( itr == a_tbl.end() ){
        a_tbl.emplace( get_self(), [&](auto& row){
          row.total_issued      = quantity;
          row.locked            = quantity;
          row.unlocked          = nasset(0,quantity.symbol);
          row.created_at        = now;
          row.updated_at        = now;
          row.unlock_times      = plan.unlock_times;
          row.plan_id           = plan.id;
          row.status            = issue_status::locked;
        });
      }else {

        a_tbl.modify(itr,get_self(),[&](auto& row){
          row.total_issued      += quantity;
          row.locked            += quantity;
          row.updated_at        = now;
          row.unlock_times      = plan.unlock_times;
          row.plan_id           = plan.id;
          row.status            = issue_status::locked;
        });
      }
  }
}

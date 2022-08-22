#pragma once


#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>
#include <eosio/name.hpp>
#include <eosio/time.hpp>
#include <nftpass.lock/amax.ntoken/amax.ntoken_db.hpp>

#include <string>


static constexpr uint32_t MAX_TITLE_SIZE        = 64;

namespace amax{


using namespace amax;

#define LOCK_TBL  [[eosio::table, eosio::contract("nftpass.lock")]]

static constexpr uint32_t INITIALIZED_ID        = 0;

using std::string;
struct [[eosio::table("global"), eosio::contract("nftpass.lock")]] global_t {
    name                admin ;
    time_point_sec      started_at ;
    uint64_t            last_plan_id ;
    uint64_t            last_issue_id ;
    EOSLIB_SERIALIZE( global_t, (admin)(started_at)(last_plan_id)(last_issue_id))
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

namespace plan_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
};

namespace issue_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name locked             = "locked"_n;
    static constexpr eosio::name unlocked           = "unlocked"_n;
};

enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   MEMO_FORMAT_ERROR    = 6,
   PAUSED               = 7,
   NO_AUTH              = 8,
   NOT_POSITIVE         = 9,
   NOT_STARTED          = 10,
   OVERSIZED            = 11,
   TIME_EXPIRED         = 12,
   NOTIFY_UNRELATED     = 13,
   ACTION_REDUNDANT     = 14,
   ACCOUNT_INVALID      = 15,
   FEE_INSUFFICIENT     = 16,
   FIRST_CREATOR        = 17,
   STATUS_ERROR         = 18,
   RATE_OVERLOAD        = 19
};

struct LOCK_TBL plan_t{

    uint64_t                    id;
    name                        owner;
    string                      title;
    name                        asset_contract;
    nsymbol                     asset_symbol; 
    nasset                      total_issued;
    nasset                      total_unlocked;
    name                        status = plan_status::none;
    time_point_sec              unlock_times;
    time_point_sec              created_at;
    time_point_sec              updated_at;

    plan_t(){};
    plan_t(const uint64_t& pid) : id(pid){}

    uint64_t primary_key() const { return id; }
    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }
    uint64_t by_nsy_id() const     { return asset_symbol.id; }
    uint64_t by_parent_id()const    { return asset_symbol.parent_id; }


    typedef eosio::multi_index<"plans"_n, plan_t,
        indexed_by<"owneridx"_n,  const_mem_fun<plan_t, uint128_t, &plan_t::by_owner> >,
        indexed_by<"nsyidx"_n,  const_mem_fun<plan_t, uint64_t, &plan_t::by_nsy_id> >,
        indexed_by<"parentidx"_n,  const_mem_fun<plan_t, uint64_t, &plan_t::by_parent_id> >
    > tbl_t;

     EOSLIB_SERIALIZE(plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(total_issued)
                    (total_unlocked)(status)(unlock_times)(created_at)(updated_at))
};

struct LOCK_TBL issue_t{
    uint64_t                id;
    uint64_t                plan_id;
    name                    issuer;
    name                    receiver;
    nasset                  issued; 
    nasset                  locked; 
    nasset                  unlocked;  
    name                    status = issue_status::none;  
    time_point_sec          created_at;
    time_point_sec          unlock_times;
    time_point_sec          updated_at;
    
    issue_t(){}
    issue_t(const uint64_t& pid) : id(pid){}

    uint64_t primary_key()const     { return id; }
    uint64_t scope() const { return 0; }
    uint64_t by_plan_id() const {return plan_id;}
    uint64_t by_issuer()const       { return issuer.value; }
    uint64_t by_receiver()const       { return receiver.value; }
    uint64_t by_nsy_id() const     { return locked.symbol.id; }
    
     typedef eosio::multi_index
    < "issues"_n,  issue_t,
        indexed_by<"planidx"_n,       const_mem_fun<issue_t, uint64_t, &issue_t::by_plan_id> >,
        indexed_by<"issueridx"_n,       const_mem_fun<issue_t, uint64_t, &issue_t::by_issuer> >,
        indexed_by<"receiveridx"_n,       const_mem_fun<issue_t, uint64_t, &issue_t::by_receiver> >,
        indexed_by<"nsyidx"_n,      const_mem_fun<issue_t, uint64_t, &issue_t::by_nsy_id> >
    > tbl_t;

    EOSLIB_SERIALIZE(issue_t, (id)(plan_id)(issuer)(receiver)(locked)(unlocked)(status)
                                (created_at)(unlock_times)(updated_at))

};
//Scope: owner's account
struct LOCK_TBL account_t{
    nasset              total_issued;
    nasset              locked;
    nasset              unlocked;
    uint64_t            plan_id;
    name                asset_contract;
    name                status = issue_status::none;  
    time_point_sec      created_at;
    time_point_sec      updated_at;
    time_point_sec      unlock_times;

    account_t() {}
    account_t(const nasset& asset): total_issued(asset) {}

    uint64_t primary_key()const { return total_issued.symbol.raw(); }
    uint64_t scope() const { return 0; }
    
    typedef eosio::multi_index< "accounts"_n, account_t > tbl_t;

    EOSLIB_SERIALIZE(account_t, (total_issued)(locked)(unlocked)
                    (plan_id)(asset_contract)(status)(created_at)(updated_at)(unlock_times)
                    )
};

}

#pragma once

#include <amax.ntoken/amax.ntoken.hpp>
#include "commons/wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
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
   RATE_OVERLOAD        = 19,
   DATA_MISMATCH        = 20,
   OVERDRAWN            = 21,
   MISC                 = 255
};


namespace wasm { namespace db {

using namespace amax;

#define TBL struct [[eosio::table, eosio::contract("rndnft.mart")]]
#define TBL_NAME(name) struct [[eosio::table(name), eosio::contract("rndnft.mart")]]

static constexpr eosio::name active_permission{"active"_n};

TBL_NAME("global") global_t {
    name        admin;
    uint64_t    max_step            = 30;
    uint64_t    last_shop_id        = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(max_step)(last_shop_id) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

namespace shop_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
};

namespace nft_random_type {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name bynftids           = "bynftids"_n;
    static constexpr eosio::name bynftamt           = "bynftamt"_n;
};

TBL shop_t {
    uint64_t            id = 0;                                             //PK
    name                owner;                                              //shop owner
    string              title;                                              //shop title: <=64 chars                                
    name                nft_contract;
    name                fund_contract;
    asset               price; 
    name                fund_receiver;
    asset               fund_recd;
    uint64_t            nft_sold_amount             = 0;        
    uint64_t            nft_current_amount          = 0;                    // sum of amounts of all nfts
    uint64_t            random_base                 = 0;
    name                random_type                 = nft_random_type::none;        
    name                status                      = shop_status::none;    //status, see plan_status_t
    time_point_sec      created_at;                                         //creation time (UTC time)
    time_point_sec      updated_at;                                         //update time: last updated at
    time_point_sec      opened_at;                                          //opend time: opening time of blind box
    time_point_sec      closed_at;                                          //close time: close time of blind box
    

    shop_t() {}
    shop_t( const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"shops"_n, shop_t,
        indexed_by<"owneridx"_n,  const_mem_fun<shop_t, uint128_t, &shop_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( shop_t, (id)(owner)(title)(nft_contract)(fund_contract)(price)(fund_receiver)(fund_recd)
                              (nft_sold_amount)(nft_current_amount)(random_base)(random_type)(status)
                              (created_at)(updated_at)(opened_at)(closed_at) )

};

// scope = shop_id
TBL nft_box_t {
    nasset          nft;  
      
    nft_box_t() {}
    nft_box_t( const uint64_t& id ) { nft.symbol.id = id; }

    uint64_t primary_key() const { return nft.symbol.id; }

    typedef eosio::multi_index<"boxes"_n, nft_box_t
    > tbl_t;

    EOSLIB_SERIALIZE( nft_box_t,  (nft) )
};

struct deal_trace_s {
    uint64_t            shop_id;
    name                buyer;
    name                nft_contract;
    name                fund_contract;
    asset               paid_quant;
    nasset              sold_quant;
    time_point_sec      created_at;

    deal_trace_s() {}
    deal_trace_s(   const uint64_t& sid, const name& b, const name& nc, const name& fc,
                    const asset& pq, const nasset& sq, const time_point_sec c):
                shop_id(sid), buyer(b), nft_contract(nc), fund_contract(fc), paid_quant(pq), 
                sold_quant(sq), created_at(c) {}
};

} }
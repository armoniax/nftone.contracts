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

#define TBL struct [[eosio::table, eosio::contract("rndnft.swap")]]
#define TBL_NAME(name) struct [[eosio::table(name), eosio::contract("rndnft.swap")]]

static constexpr eosio::name active_permission{"active"_n};

TBL_NAME("global") global_t {
    name        admin;
    uint64_t    batch_swap_max_nfts                 = 30;
    uint64_t    last_booth_id                       = 0;

    EOSLIB_SERIALIZE( global_t, (admin)(batch_swap_max_nfts)(last_booth_id) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

namespace booth_status {
    static constexpr eosio::name none               = "none"_n;
    static constexpr eosio::name enabled            = "enabled"_n;
    static constexpr eosio::name disabled           = "disabled"_n;
};

struct booth_conf_s {
    name                owner;                                  //shop owner
    string              title;                                  //shop title: <=64 chars      
    name                base_nft_contract;
    name                quote_nft_contract;
    nasset              quote_nft_price;                        //PK  swap price in quote nft for 1 base nft (usually blindbox nft)
    time_point_sec      opened_at;                              //opend time: opening time of blind box
    time_point_sec      closed_at;                              //close time: close time of blind box

    booth_conf_s(){}
    booth_conf_s(const uint32_t& id) : quote_nft_price(id) {}
    booth_conf_s(const name& o, const string& t, const name& b, const name& qc,
                 const nasset& qn,const time_point_sec& ot,const time_point_sec& ct ) :
                 owner(o), title(t),base_nft_contract(b),quote_nft_contract(qc),
                 quote_nft_price(qn.amount,qn.symbol), opened_at(ot), closed_at(ct) {}

    EOSLIB_SERIALIZE( booth_conf_s, (owner)(title)(base_nft_contract)(quote_nft_contract)(quote_nft_price)
                                (opened_at)(closed_at) )
};

//scope: nft_contract
TBL booth_t {
    booth_conf_s        conf;
    uint64_t            id = 0;                               
    uint64_t            base_nft_sum;
    uint64_t            base_nft_num;
    uint64_t            base_nftbox_sum;
    uint64_t            base_nftbox_num;
    nasset              quote_nft_recd = nasset(0, conf.quote_nft_price.symbol);
    name                status = booth_status::enabled;         //status, see plan_status_t
    time_point_sec      created_at;                             //creation time (UTC time)
    time_point_sec      updated_at;                             //update time: last updated at

    booth_t() {}
    booth_t( const uint64_t& i): conf(i) {}

    uint64_t primary_key() const { return conf.quote_nft_price.symbol.id; }
    uint128_t by_owner() const { return (uint128_t)conf.owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"booths"_n, booth_t,
        indexed_by<"owneridx"_n,  const_mem_fun<booth_t, uint128_t, &booth_t::by_owner> >
    > idx_t;

    EOSLIB_SERIALIZE( booth_t,  (conf)(id)(base_nft_sum)(base_nft_num)(base_nftbox_sum)(base_nftbox_num)
                                (quote_nft_recd)(status)(created_at)(updated_at) )

};

//scope: booth_id
TBL booth_nftbox_t {
    uint64_t id;         //PK
    nasset  nfts;
      
    booth_nftbox_t() {}
    booth_nftbox_t( const uint64_t& i ): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t by_nft_id()const { return nfts.symbol.id; }

    typedef eosio::multi_index<"boothboxes"_n, booth_nftbox_t,
        indexed_by<"nftidx"_n,       const_mem_fun<booth_nftbox_t, uint64_t, &booth_nftbox_t::by_nft_id> >
    > idx_t;

    EOSLIB_SERIALIZE( booth_nftbox_t, (id)(nfts) )
};

struct deal_trace_s_s {
    uint64_t            booth_id;
    name                user;   //who swaps
    name                base_nft_contract;
    name                quote_nft_contract;
    nasset              paid_quant;
    nasset              sold_quant;
    time_point_sec      created_at;
};

} }
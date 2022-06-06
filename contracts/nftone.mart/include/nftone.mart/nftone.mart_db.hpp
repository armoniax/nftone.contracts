#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <amax.ntoken/amax.ntoken_db.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>



namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("nftone.mart")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("nftone.mart")]]

NTBL("global") global_t {
    name dev_fee_collector;
    float dev_fee_rate;

    EOSLIB_SERIALIZE( global_t, (dev_fee_collector)(dev_fee_rate) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


// price for NFT tokens
struct price_s {
    nsymbol symbol;
    float value;    //price value

    price_s() {}
    price_s(const nsymbol& symb, const float& v): symbol(symb), value(v) {}

    EOSLIB_SERIALIZE( price_s, (symbol)(value) )
};

//scope: nasset.symbol.id
TBL order_t {
    uint64_t        id;                 //PK
    price_s         price;
    int64_t         frozen;             //nft amount for sellers, cnyd amount for buyers
    name            maker;
    time_point_sec  created_at;
    time_point_sec  updated_at;
    bool            paused = false;     //if true, it can no longer be transferred

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    // uint128_t by_maker_nft()const { (uint128_t) maker.value | quantity.symbol.raw(); }
    uint64_t  by_small_price_first()const { return price.value; }
    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.value ); }

    EOSLIB_SERIALIZE( order_t, (id)(price)(frozen)(maker)(created_at)(updated_at)(paused) )
 
};

typedef eosio::multi_index
< "sellorders"_n,  order_t,
    // indexed_by<"makernfts"_n,  const_mem_fun<order_t, uint128_t, &order_t::by_maker_nft> >
    indexed_by<"priceidx"_n,   const_mem_fun<order_t, uint64_t, &order_t::by_small_price_first> >
> selloffer_idx;

//buyer to bid for the token ID
typedef eosio::multi_index
< "buyoffers"_n,  order_t,
    // indexed_by<"ownernfts"_n,  const_mem_fun<order_t, uint128_t, &order_t::by_maker_nft> >
    indexed_by<"priceidx"_n,  const_mem_fun<order_t, uint64_t, &order_t::by_large_price_first> >
> buyoffer_idx;

} //namespace amax
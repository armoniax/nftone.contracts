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

TBL offer_t {
    uint64_t        id;
    name            owner;
    nasset          quantity;
    asset           price;           //CNYD price
    time_point_sec  created_at;
    time_point_sec  sale_opened_at;
    bool            paused = false;   //if true, it can no longer be transferred

    offer_t() {}
    offer_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint128_t by_owner_nft()const { (uint128_t) owner.value | quantity.symbol.raw(); }

    EOSLIB_SERIALIZE(offer_t, (id)(owner)(quantity)(price)(created_at)(sale_opened_at)(paused) )

    typedef eosio::multi_index
    < "nftoffers"_n,  offer_t,
        indexed_by<"ownernfts"_n,  const_mem_fun<offer_t, uint128_t, &offer_t::by_owner_nft> >
    > nftoffer_idx;
};

} //namespace amax
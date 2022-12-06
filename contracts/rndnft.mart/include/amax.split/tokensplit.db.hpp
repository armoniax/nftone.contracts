#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <string>
#include <vector>
#include <type_traits>

namespace amax {

using namespace std;
using std::string;
using namespace eosio;

struct split_unit_s {
    eosio::name token_receiver;
    uint64_t token_split_amount; //rate or amount, amount must contain precision
};

//scope: sender account (usually a smart contract)
struct split_plan_t {
    uint64_t                    id;        //PK
    symbol                      token_symbol;
    bool                        split_by_rate   = false;  //rate boost by 10000
    std::vector<split_unit_s>   split_conf;     //receiver -> rate_or_amount

    split_plan_t() {}
    split_plan_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    typedef multi_index<"splitplans"_n, split_plan_t > tbl_t;

    EOSLIB_SERIALIZE( split_plan_t, (id)(token_symbol)(split_by_rate)(split_conf) )
};

} //namespace amax
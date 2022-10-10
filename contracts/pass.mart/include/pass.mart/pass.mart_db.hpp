#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>
#include <eosio/name.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include "pass.mart_const.hpp"
#include <string>

#define TBL struct [[eosio::table, eosio::contract("pass.mart")]]

namespace mart{

    using namespace amax;
    using std::string;

    struct [[eosio::table("global"), eosio::contract("pass.mart")]] global_t{
        name                 admin;
        name                 custody_contract;
        name                 nft_contract;
        name                 gift_nft_contract;
        name                 token_split_contract;
        uint64_t             last_pass_id = 0;
        uint64_t             last_order_id = 0;

        EOSLIB_SERIALIZE( global_t, (admin)(custody_contract)(nft_contract)(gift_nft_contract)(token_split_contract)
                                    (last_pass_id)(last_order_id) )
    };

    typedef eosio::singleton< "global"_n, global_t > global_singleton;

    TBL pass_t {
        uint64_t            id = 0;             //PK
        string              title;
        name                owner;
        nasset              nft_total;
        nasset              nft_available;
        asset               price;              //MUSDT
        uint64_t            custody_plan_id         = 0;
        name                status                  = pass_status::none;
        nasset              gift_nft_total          = nasset(0, nsymbol(0,0));        
        nasset              gift_nft_available      = nasset(0, nsymbol(0,0));
        uint64_t            token_split_plan_id;
        time_point_sec      created_at;
        time_point_sec      updated_at;
        time_point_sec      sell_started_at;
        time_point_sec      sell_ended_at;

       
        pass_t(){}
        pass_t(const uint64_t& pid): id(pid) {}

        uint64_t scope() const { return 0; }
        uint64_t primary_key()const { return id; }

        typedef eosio::multi_index<"passes"_n, pass_t
        > tbl_t;

        EOSLIB_SERIALIZE(pass_t, (id)(title)(owner)(nft_total)(nft_available)(price)(custody_plan_id)(status)
                                    (gift_nft_total)(gift_nft_available)(token_split_plan_id)
                                    (created_at)(updated_at)(sell_started_at)(sell_ended_at) )
    };

    struct order_s{
        uint64_t            id;
        uint64_t            pass_id;
        name                owner;
        asset               quantity;
        nasset              nft_quantity;
        nasset              gift_quantity;
        time_point_sec      created_at;
    };
}

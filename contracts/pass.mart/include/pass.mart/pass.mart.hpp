#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include "pass.mart_db.hpp"
#include <libraries/wasm_db.hpp>

using namespace eosio;
using namespace std;
using namespace wasm::db;

namespace mart {

class [[eosio::contract("pass.mart")]] pass_mart : public contract{

    public:
        using contract::contract;

        pass_mart(  eosio::name receiver, eosio::name code, datastream<const char*> ds):
                    contract(receiver, code, ds), _db(get_self()),
                    _global(get_self(), get_self().value)
        {
            _gstate = _global.exists() ? _global.get() : global_t{};
        }

        ~pass_mart() { _global.set( _gstate, get_self() ); }


        ACTION init( const name& admin, const name& nft_contract, const name& gift_nft_contract, const name& custody_contract, const name& token_split_contract );
        ACTION closepass( const uint64_t& pass_id);
        ACTION addpass( const name& owner, const string& title, const nsymbol& nft_symbol, const nsymbol& gift_symbol, 
                            const asset& price, const time_point_sec& started_at,
                            const time_point_sec& ended_at, uint64_t custody_plan_id, const uint64_t& token_split_plan_id);
        
        ACTION setendtime( const uint64_t& pass_id, const time_point_sec& sell_ended_at );
        
        ACTION ordertrace(const order_s& order);
        using order_srace_action = eosio::action_wrapper<"ordertrace"_n, &pass_mart::ordertrace>;

        [[eosio::on_notify("pass.ntoken::transfer")]]
        void nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo);

        [[eosio::on_notify("amax.mtoken::transfer")]]
        void mtoken_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

        [[eosio::on_notify("verso.mid::transfer")]]
        void verso_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo);
        
    private:
        global_singleton    _global;
        global_t            _gstate;
        dbc                 _db;

        void _on_order_trace( const order_s& order);
};
}

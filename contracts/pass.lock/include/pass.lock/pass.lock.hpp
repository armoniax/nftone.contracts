#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include "pass.lock_db.hpp"
#include <pass.lock/librarys/wasm_db.hpp>

using namespace eosio;
using namespace std;

namespace lock{
    class [[eosio::contract("pass.lock")]] pass_lock: public contract {

    private:
        global_singleton    _global;
        global_t            _gstate;
          
        void _add_lock( const name& owner, const nasset& quantity, const plan_t& plan);
        
    public:

        using contract::contract;
       

        pass_lock(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
        {
            _gstate = _global.exists() ? _global.get() : global_t{};
        }

        ~pass_lock() { _global.set( _gstate, get_self() ); }

        [[eosio::on_notify("*::transfer")]]
        void ontransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo);

        [[eosio::action]]
        void init( const name& nft_contract);
        
        [[eosio::action]]
        void addplan(const name& marker,
                         const string& title,
                         const nsymbol& asset_symbol,
                         const time_point_sec& unlock_times);
        
        [[eosio::action]]
        void setplanowner( const uint64_t& plan_id, const name& new_owner);

        [[eosio::action]]
        void enableplan( const uint64_t& plan_id, bool enabled);
        
        [[eosio::action]]
        void unlock(const name& owner,const uint64_t& lock_id);
    };

}

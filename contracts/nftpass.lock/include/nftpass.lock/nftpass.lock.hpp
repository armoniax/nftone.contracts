#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/time.hpp>
#include "nftpass.lock_db.hpp"
#include <nftpass.lock/thirdparty/wasm_db.hpp>

using namespace eosio;
using namespace std;

namespace amax{
    class [[eosio::contract("nftpass.lock")]] nft_lock: public contract {

    private:
        global_singleton    _global;
        global_t            _gstate;
          
        void _add_locked_amount( const name& owner, const nasset& quantity, const plan_t& plan);
        
    public:

        using contract::contract;
       

        nft_lock(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
        {
            _gstate = _global.exists() ? _global.get() : global_t{};
        }

        ~nft_lock() { _global.set( _gstate, get_self() ); }

        [[eosio::on_notify("*::transfer")]]
        void onlocktransferntoken(const name& from, const name& to, const vector<nasset>& quants, const string& memo);

        [[eosio::action]]
        void init();
        
        [[eosio::action]]
        void addplan(const name& owner,
                         const string& title,
                         const name& asset_contract,
                         const nsymbol& asset_symbol,
                         const time_point_sec& unlock_times);
        
        [[eosio::action]]
        void setplanowner(const name& owner,const uint64_t& plan_id, const name& new_owner);

        [[eosio::action]]
        void enableplan(const name& owner, const uint64_t& plan_id, bool enabled);
        
        [[eosio::action]]
        void unlock(const name& owner,const uint64_t& lock_id);
    };

}

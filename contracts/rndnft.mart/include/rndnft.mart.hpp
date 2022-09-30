#include "rndnft.mart.db.hpp"
#include <amax.ntoken/amax.ntoken.hpp>

using namespace std;
using namespace wasm::db;

class [[eosio::contract("rndnft.mart")]] rndnft_mart: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    rndnft_mart(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~rndnft_mart() { _global.set( _gstate, get_self() ); }

    [[eosio::action]]
    void init( const name& owner);

    [[eosio::action]]
    void createpool( const name& owner,const string& title,const name& asset_contract, const name& rndnft_mart_contract,
                           const asset& price, const name& fee_receiver, const bool& allow_to_buy_again,
                           const time_point_sec& opended_at, const uint64_t& opened_days);
    
    [[eosio::action]]
    void enableplan(const name& owner, const uint64_t& pool_id, bool enabled);
    
    [[eosio::action]]
    void editplantime(const name& owner, const uint64_t& pool_id, const time_point_sec& opended_at, const time_point_sec& closed_at);

    [[eosio::action]]
    void endpool(const name& owner, const uint64_t& pool_id);
    
    [[eosio::action]]
    void dealtrace(const deal_trace_t& trace);

    // [[eosio::action]]
    // void fillnftinc( const name& owner, const uint64_t& pool_id, const uint64_t& begin_id, const uint64_t& end_id);
    // [[eosio::action]]
    // void fillnftids(const name& owner, const uint64_t& pool_id, const vector<nasset>& quants);

    [[eosio::on_notify("*::transfer")]] void ontransnft(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    [[eosio::on_notify("amax.mtoken::transfer")]] void ontranstoken( const name& from, const name& to, const asset& quantity, const string& memo );
    
    using deal_trace_action = eosio::action_wrapper<"dealtrace"_n, &rndnft_mart::dealtrace>;
private:
    uint64_t _rand(uint64_t max_uint,  uint16_t min_unit, name owner , uint64_t pool_id);
    void _get_one_nft( pool_t& pool , const name& owner , deal_trace_t trace , const uint64_t& amount);
    void _on_deal_trace( const deal_trace_t& deal_trace);
    void _add_times( const uint64_t& pool_id, const name& owner);
    void _on_open_transfer( const name& from, const name& to, const asset& quantity, const string& memo);
    void _on_fill_transfer( const name& from, const name& to, const vector<nasset>& assets, const string& memo );
    void _add_nfts( pool_t& p, const vector<nasset>& quants );
}; //contract one.rndnft_mart
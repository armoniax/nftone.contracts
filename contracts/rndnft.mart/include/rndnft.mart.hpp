#include "rndnft.mart.db.hpp"
#include <amax.ntoken/amax.ntoken.hpp>

using namespace std;
using namespace wasm::db;

class [[eosio::contract("rndnft.mart")]] rndnft_mart: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _db;

public:
    using contract::contract;

    rndnft_mart(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value) {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~rndnft_mart() { _global.set( _gstate, get_self() ); }

    ACTION init( const name& owner);
    ACTION createshop(   const name& owner,const string& title,const name& fund_contract, const name& nft_contract,
                           const asset& price, const name& fund_receiver, const name& nft_random,
                           const time_point_sec& opened_at, const uint64_t& opened_days);
    ACTION enableshop( const name& owner, const uint64_t& shop_id, bool enabled);
    ACTION setshoptime( const name& owner, const uint64_t& shop_id, const time_point_sec& opened_at, const time_point_sec& closed_at);
    ACTION closeshop( const name& owner, const uint64_t& shop_id);
    ACTION dealtrace( const deal_trace_s& trace);

    /**
     * @brief show owner to send nft into one's own shop
     * 
     * @param from 
     * @param to 
     * @param assets 
     * @param memo 
     */
    [[eosio::on_notify("amax.ntoken::transfer")]] 
    void on_transfer_ntoken(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    /**
     * @brief buyer to buy shop nft
     * 
     * @param from 
     * @param to 
     * @param quantity 
     * @param memo 
     */
    [[eosio::on_notify("amax.mtoken::transfer")]] 
    void on_transfer_mtoken( const name& from, const name& to, const asset& quantity, const string& memo );
    
    using deal_trace_action = eosio::action_wrapper<"dealtrace"_n, &rndnft_mart::dealtrace>;

private:
    uint64_t _rand(const uint16_t& min_unit, const uint64_t& max_uint, const name& owner, const uint64_t& shop_id);
    void _one_nft( const name& owner, shop_t& shop, nasset& nft );
    void _on_deal_trace( const deal_trace_s& deal_trace);
    void _add_times( const uint64_t& shop_id, const name& owner);

};
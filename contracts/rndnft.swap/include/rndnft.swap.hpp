#include "rndnft.swap.db.hpp"
#include <amax.ntoken/amax.ntoken.hpp>

using namespace std;
using namespace wasm::db;


#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

class [[eosio::contract("rndnft.swap")]] rndnft_swap: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _db;

public:
    using contract::contract;

    rndnft_swap(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value),_db(get_self()) {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~rndnft_swap() { _global.set( _gstate, get_self() ); }

    ACTION init( const name& admin);
    ACTION createbooth( const booth_conf_s& conf );
    ACTION enablebooth( const name& owner, const name& quote_nft_contract, const uint64_t& symbol_id, bool enabled);
    ACTION setboothtime(const name& owner, const name& quote_nft_contract, const uint64_t& symbol_id, const time_point_sec& opened_at, 
                            const time_point_sec& closed_at);
    ACTION closebooth( const name& owner, const name& quote_nft_contract, const uint64_t& symbol_id);
    ACTION dealtrace( const deal_trace_s_s& trace);
    
    ACTION fixbox( const uint64_t& booth_id, const uint64_t& box_id, const nasset& nfts) {
        require_auth( _self );

        auto nftboxes = booth_nftbox_t::idx_t( _self, booth_id );
        auto nftbox = booth_nftbox_t(box_id);
        _db.get( booth_id, nftbox );

        nftbox.nfts = nfts;
        _db.set( booth_id, nftbox );
    }
    
    ACTION fixbooth( const name& nscope, const uint64_t& symbol_id, const uint64_t& num) {
        require_auth( _self );
        auto booths                    = booth_t( symbol_id );
        _db.get( nscope.value ,booths);
        booths.base_nft_num = num;
        _db.set( nscope.value ,booths);
    }
    /**
     * @brief booth owner to send nft into one's own booth
     * 
     * @param from 
     * @param to 
     * @param assets 
     * @param memo 
     */
    [[eosio::on_notify("*::transfer")]] 
    void on_transfer_ntoken(const name& from, const name& to, const vector<nasset>& assets, const string& memo);

    using deal_trace_s_action = eosio::action_wrapper<"dealtrace"_n, &rndnft_swap::dealtrace>;

private:
    uint64_t _rand( const uint64_t& range, const name& owner, const uint64_t& index );
    void _one_nft( const time_point_sec& now, const name& owner, booth_t& booth, nasset& nft , const uint64_t& nonce);
    void _on_deal_trace_s( const deal_trace_s_s& deal_trace_s);
    void _refuel_nft( const vector<nasset>& assets, booth_t& booth );
    void _swap_nft( const name& user, const nasset& paid_nft, booth_t& booth );

};
